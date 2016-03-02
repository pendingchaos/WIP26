#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>
#include <string.h>

#include "runtime.h"

#define FAIL(...) _fail((fputs("Failure: ", stderr), fprintf(stderr, __VA_ARGS__), fputc('\n', stderr), EXIT_FAILURE))
#define WARN(...) (fputs("Warning: ", stderr), fprintf(stderr, __VA_ARGS__), fputc('\n', stderr))
#define INFO(...) (fputs("Info: ", stdout), printf(__VA_ARGS__), putchar('\n'))

typedef struct {
    union {float vals[4];struct {float x, y, z, w;};};
} vec4_t;

typedef struct {
    union {float vals[3];struct {float x, y, z;};};
} vec3_t;

typedef struct {
    vec4_t column[4];
} mat4_t;

static const char* vertex_source = "#version 120\n"
"attribute float posx;\n"
"attribute float posy;\n"
"attribute float posz;\n"
"attribute float colr;\n"
"attribute float colg;\n"
"attribute float colb;\n"
"attribute float deleted;\n"
"uniform mat4 uView;\n"
"uniform mat4 uProj;\n"
"varying vec3 color;\n"
"void main() {\n"
"    gl_Position = uProj * uView * vec4(posx, posy, posz, 1.0);\n"
"    if (deleted > 0.9) {\n"
"        gl_Position.x = 2.0;\n"
"        gl_Position.w = 1.0;\n"
"    }"
"    color = vec3(colr, colg, colb);\n"
"}\n";
static const char* fragment_source = "#version 120\n"
"varying vec3 color;\n"
"void main() {\n"
"    gl_FragColor = vec4(color, 1.0);"
"}\n";

static bool glfw_init = false;
static bool runtime_init = false;
static bool program_init = false;
static bool system_init = false;
static bool gl_program_init = false;

static GLFWwindow* window;
static runtime_t runtime;
static program_t program;
static system_t particle_system;
static int posx_index;
static int posy_index;
static int posz_index;
static int velx_index;
static int vely_index;
static int velz_index;
static int colr_index;
static int colg_index;
static int colb_index;
static GLuint gl_program;
static float anglea = 0.0f;
static float angleb = 0.0f;
static float zoom = 3.0f;
static float frametime = 1.0 / 60.0;

static vec3_t create_vec3(float x, float y, float z) {
    vec3_t v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

static vec3_t sub_vec3(vec3_t a, vec3_t b) {
    return create_vec3(a.x-b.x, a.y-b.y, a.z-b.z);
}

static vec3_t cross_vec3(vec3_t a, vec3_t b) {
    return create_vec3(a.y*b.z - a.z*b.y,
                       a.z*b.x - a.x*b.z,
                       a.x*b.y - a.y*b.x);
}

static float dot_vec3(vec3_t a, vec3_t b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

static vec3_t normalize_vec3(vec3_t v) {
    float len = sqrt(dot_vec3(v, v));
    return create_vec3(v.x/len, v.y/len, v.z/len);
}

static mat4_t create_mat(vec3_t x, vec3_t y, vec3_t z, vec3_t t) {
    mat4_t res;
    res.column[0].vals[0] = x.x;
    res.column[1].vals[0] = x.y;
    res.column[2].vals[0] = x.z;
    res.column[3].vals[0] = t.x;
    
    res.column[0].vals[1] = y.x;
    res.column[1].vals[1] = y.y;
    res.column[2].vals[1] = y.z;
    res.column[3].vals[1] = t.y;
    
    res.column[0].vals[2] = z.x;
    res.column[1].vals[2] = z.y;
    res.column[2].vals[2] = z.z;
    res.column[3].vals[2] = t.z;
    
    res.column[0].vals[3] = 0.0f;
    res.column[1].vals[3] = 0.0f;
    res.column[2].vals[3] = 0.0f;
    res.column[3].vals[3] = 1.0f;
    return res;
}

static mat4_t lookat(vec3_t up, vec3_t at, vec3_t eye) {
    vec3_t f = normalize_vec3(sub_vec3(at, eye));
    vec3_t s = normalize_vec3(cross_vec3(f, normalize_vec3(up)));
    vec3_t u = normalize_vec3(cross_vec3(s, f));
    mat4_t res;
    memset(&res, 0, 64);
    res.column[0].vals[0] = s.x;
    res.column[1].vals[0] = s.y;
    res.column[2].vals[0] = s.z;
    res.column[0].vals[1] = u.x;
    res.column[1].vals[1] = u.y;
    res.column[2].vals[1] = u.z;
    res.column[0].vals[2] = -f.x;
    res.column[1].vals[2] = -f.y;
    res.column[2].vals[2] = -f.z;
    res.column[3].vals[0] = -dot_vec3(s, eye);
    res.column[3].vals[1] = -dot_vec3(u, eye);
    res.column[3].vals[2] = dot_vec3(f, eye);
    res.column[3].vals[3] = 1.0f;
    return res;
    vec3_t t = create_vec3(-dot_vec3(s, eye), -dot_vec3(u, eye), dot_vec3(f, eye));
    return create_mat(s, u, sub_vec3(create_vec3(0.0f, 0.0f, 0.0f), f), t);
}

static float radians(float deg) {
    return deg / 180.0f * M_PI;
}

static mat4_t perspective(float fov, float width, float height, float near, float far) {
    float h = cos(fov / 2.0f) / sin(fov / 2.0f);
    float w = h * height / width;
    mat4_t res;
    memset(&res, 0, 64);
    res.column[0].vals[0] = w;
    res.column[1].vals[1] = h;
    res.column[2].vals[2] = -(far+near) / (far-near);
    res.column[2].vals[3] = -1.0f;
    res.column[3].vals[2] = -(far*near*2.0f) / (far-near);
    res.column[3].vals[3] = 1.0f;
    return res;
}

static void on_scroll(GLFWwindow* window, double x, double y) {
    zoom -= y * 0.1f;
}

static void deinit() {
    if (gl_program_init) glDeleteProgram(gl_program);
    if (system_init) destroy_system(&particle_system);
    if (program_init) destroy_program(&program);
    if (runtime_init) destroy_runtime(&runtime);
    if (glfw_init) glfwTerminate();
}

static void _fail(int status) {
    deinit();
    exit(status);
}

static void init_particle(unsigned int index) {
    float posx = rand() / (double)RAND_MAX * 2.0 - 1.0;
    float posy = rand() / (double)RAND_MAX * 2.0 - 1.0;
    float posz = rand() / (double)RAND_MAX * 2.0 - 1.0;
    
    float velx, vely, velz, vlen=-1.0f;
    while (vlen <= 0.0f) {
        velx = rand() / (double)RAND_MAX * 2.0 - 1.0;
        vely = rand() / (double)RAND_MAX * 2.0 - 1.0;
        velz = rand() / (double)RAND_MAX * 2.0 - 1.0;
        vlen = sqrt(velx*velx + vely*vely + velz*velz);
    }
    
    float plen = sqrt(posx*posx + posy*posy + posz*posz);
    float dist = 0.0f; //rand() / (double)RAND_MAX;
    ((float*)particle_system.attributes[posx_index])[index] = posx/plen*dist;
    ((float*)particle_system.attributes[posy_index])[index] = posy/plen*dist;
    ((float*)particle_system.attributes[posz_index])[index] = posz/plen*dist;
    ((float*)particle_system.attributes[velx_index])[index] = velx/vlen*0.005f;
    ((float*)particle_system.attributes[vely_index])[index] = vely/vlen*0.005f;
    ((float*)particle_system.attributes[velz_index])[index] = velz/vlen*0.005f;
    ((uint8_t*)particle_system.attributes[colr_index])[index] = 0;
    ((uint8_t*)particle_system.attributes[colg_index])[index] = 0;
    ((uint8_t*)particle_system.attributes[colb_index])[index] = 0;
}

static void create_gl_program() {
    GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
    
    GLchar info_log[1024];
    
    glShaderSource(vertex, 1, &vertex_source, NULL);
    glCompileShader(vertex);
    glGetShaderInfoLog(vertex, sizeof(info_log), NULL, info_log);
    INFO("Vertex shader info log: %s", info_log);
    
    glShaderSource(fragment, 1, &fragment_source, NULL);
    glCompileShader(fragment);
    glGetShaderInfoLog(fragment, sizeof(info_log), NULL, info_log);
    INFO("Fragment shader info log: %s", info_log);
    
    gl_program = glCreateProgram();
    gl_program_init = true;
    glAttachShader(gl_program, vertex);
    glAttachShader(gl_program, fragment);
    glLinkProgram(gl_program);
    glValidateProgram(gl_program);
}

static void debug_callback(GLenum _1, GLenum _2, GLuint _3, GLenum severity,
                           GLsizei _5, const char* msg, const void* _6) {
    if (severity == GL_DEBUG_SEVERITY_HIGH_ARB) FAIL("OpenGL Error: %s", msg);
    else if (severity == GL_DEBUG_SEVERITY_MEDIUM_ARB)
        WARN("OpenGL Warning: %s", msg);
    else INFO("OpenGL Info: %s", msg);
}

int main() {
    assert(sizeof(mat4_t) == 64);
    
    if (!glfwInit())
        FAIL("glfwInit() failed");
    glfw_init = true;
    
    glfwWindowHint(GLFW_RESIZABLE, false);
    glfwWindowHint(GLFW_SRGB_CAPABLE, true);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 1);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
    
    window = glfwCreateWindow(800, 800, "", NULL, NULL);
    if (!window) FAIL("Failed to create window");
    
    int xpos, ypos;
    glfwGetMonitorPos(glfwGetPrimaryMonitor(), &xpos, &ypos);
    glfwSetWindowPos(window, xpos, ypos);
    
    glfwMakeContextCurrent(window);
    if (glewInit() != GLEW_OK) FAIL("Failed to initialize GLEW");
    
    glfwSetScrollCallback(window, on_scroll);
    
    if (GLEW_ARB_framebuffer_sRGB) glEnable(GL_FRAMEBUFFER_SRGB);
    else WARN("sRGB framebuffers not supported");
    
    if (GLEW_ARB_debug_output)
        glDebugMessageCallbackARB((GLDEBUGPROCARB)debug_callback, NULL);
    
    glEnable(GL_DEPTH_TEST);
    
    if (!create_runtime(&runtime, "auto"))
        FAIL("Failed to create runtime: %s", runtime.error);
    runtime_init = true;
    
    program.runtime = &runtime;
    if (!open_program("main.sim.bin", &program))
        FAIL("Failed to open main.sim.bin: %s", runtime.error);
    program_init = true;
    
    posx_index = get_attribute_index(&program, "pos.x");
    if (posx_index < 0) FAIL("Failed to find attribute \"pos.x\".");
    posy_index = get_attribute_index(&program, "pos.y");
    if (posy_index < 0) FAIL("Failed to find attribute \"pos.y\".");
    posz_index = get_attribute_index(&program, "pos.z");
    if (posz_index < 0) FAIL("Failed to find attribute \"pos.z\".");
    velx_index = get_attribute_index(&program, "vel.x");
    if (velx_index < 0) FAIL("Failed to find attribute \"vel.x\".");
    vely_index = get_attribute_index(&program, "vel.y");
    if (vely_index < 0) FAIL("Failed to find attribute \"vel.y\".");
    velz_index = get_attribute_index(&program, "vel.z");
    if (velz_index < 0) FAIL("Failed to find attribute \"vel.z\".");
    colr_index = get_attribute_index(&program, "col.x");
    if (colr_index < 0) FAIL("Failed to find attribute \"col.x\".");
    colg_index = get_attribute_index(&program, "col.y");
    if (colg_index < 0) FAIL("Failed to find attribute \"col.y\".");
    colb_index = get_attribute_index(&program, "col.z");
    if (colb_index < 0) FAIL("Failed to find attribute \"col.z\".");
    
    particle_system.runtime = &runtime;
    particle_system.pool_size = 300000;
    particle_system.sim_program = &program;
    particle_system.attribute_dtypes[posx_index] = ATTR_FLOAT32;
    particle_system.attribute_dtypes[posy_index] = ATTR_FLOAT32;
    particle_system.attribute_dtypes[posz_index] = ATTR_FLOAT32;
    particle_system.attribute_dtypes[velx_index] = ATTR_FLOAT32;
    particle_system.attribute_dtypes[vely_index] = ATTR_FLOAT32;
    particle_system.attribute_dtypes[velz_index] = ATTR_FLOAT32;
    particle_system.attribute_dtypes[colr_index] = ATTR_UINT8;
    particle_system.attribute_dtypes[colg_index] = ATTR_UINT8;
    particle_system.attribute_dtypes[colb_index] = ATTR_UINT8;
    if (!create_system(&particle_system))
        FAIL("Failed to create particle system: %s", runtime.error);
    system_init = true;
    
    for (size_t i = 0; i < 100000; i++) {
        int index = spawn_particle(&particle_system);
        if (index >= 0) init_particle(index);
        else FAIL("Failed to spawn particle: %s", runtime.error);
    }
    
    create_gl_program();
    
    glUseProgram(gl_program);
    
    GLint posx_loc = glGetAttribLocation(gl_program, "posx");
    GLint posy_loc = glGetAttribLocation(gl_program, "posy");
    GLint posz_loc = glGetAttribLocation(gl_program, "posz");
    GLint colr_loc = glGetAttribLocation(gl_program, "colr");
    GLint colg_loc = glGetAttribLocation(gl_program, "colg");
    GLint colb_loc = glGetAttribLocation(gl_program, "colb");
    GLint deleted_loc = glGetAttribLocation(gl_program, "deleted");
    glEnableVertexAttribArray(posx_loc);
    glEnableVertexAttribArray(posy_loc);
    glEnableVertexAttribArray(posz_loc);
    glEnableVertexAttribArray(colr_loc);
    glEnableVertexAttribArray(colg_loc);
    glEnableVertexAttribArray(colb_loc);
    glEnableVertexAttribArray(deleted_loc);
    float* posx = particle_system.attributes[posx_index];
    float* posy = particle_system.attributes[posy_index];
    float* posz = particle_system.attributes[posz_index];
    uint8_t* colr = particle_system.attributes[colr_index];
    uint8_t* colg = particle_system.attributes[colg_index];
    uint8_t* colb = particle_system.attributes[colb_index];
    uint8_t* deleted = particle_system.deleted_flags;
    glVertexAttribPointer(posx_loc, 1, GL_FLOAT, GL_FALSE, 0, posx);
    glVertexAttribPointer(posy_loc, 1, GL_FLOAT, GL_FALSE, 0, posy);
    glVertexAttribPointer(posz_loc, 1, GL_FLOAT, GL_FALSE, 0, posz);
    glVertexAttribPointer(colr_loc, 1, GL_UNSIGNED_BYTE, GL_TRUE, 0, colr);
    glVertexAttribPointer(colg_loc, 1, GL_UNSIGNED_BYTE, GL_TRUE, 0, colg);
    glVertexAttribPointer(colb_loc, 1, GL_UNSIGNED_BYTE, GL_TRUE, 0, colb);
    glVertexAttribPointer(deleted_loc, 1, GL_UNSIGNED_BYTE, GL_FALSE, 0, deleted);
    
    anglea = -0.652613f;
    angleb = -0.69614f;
    
    while (!glfwWindowShouldClose(window)) {
        double begin = glfwGetTime();
        
        if (glfwGetKey(window, GLFW_KEY_LEFT))
            anglea -= radians(30.0f) * frametime;
        if (glfwGetKey(window, GLFW_KEY_RIGHT))
            anglea += radians(30.0f) * frametime;
        if (glfwGetKey(window, GLFW_KEY_UP))
            angleb -= radians(30.0f) * frametime;
        if (glfwGetKey(window, GLFW_KEY_DOWN))
            angleb += radians(30.0f) * frametime;
        if (glfwGetKey(window, GLFW_KEY_S)) {
            for (size_t i = 0; i < 50000*frametime; i++) {
                int index = spawn_particle(&particle_system);
                if (index >= 0) init_particle(index);
                else {
                    WARN("Failed to spawn particle: %s", runtime.error);
                    break;
                }
            }
        }
        
        double sim_begin = glfwGetTime();
        if (!simulate_system(&particle_system))
            FAIL("Failed to simulate particle system: %s", runtime.error);
        double sim_end = glfwGetTime();
        
        vec3_t eye = create_vec3(zoom*sin(anglea)*cos(angleb),
                                 zoom*sin(anglea)*sin(angleb),
                                 zoom*cos(anglea));
        
        mat4_t view = lookat(create_vec3(0.0f, 1.0f, 0.0f),
                             create_vec3(0.0f, 0.0f, 0.0f),
                             eye);
        mat4_t proj = perspective(radians(45.0f), 1.0f, 1.0f, 0.1f, 100.0f);
        
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        GLint loc = glGetUniformLocation(gl_program, "uView");
        glUniformMatrix4fv(loc, 1, GL_FALSE, (const GLfloat*)&view);
        loc = glGetUniformLocation(gl_program, "uProj");
        glUniformMatrix4fv(loc, 1, GL_FALSE, (const GLfloat*)&proj);
        
        glDrawArrays(GL_POINTS, 0, particle_system.pool_size);
        
        glfwSwapBuffers(window);
        glfwPollEvents();
        
        double end = glfwGetTime();
        
        char title[256];
        frametime = end - begin;
        float usage = (double)particle_system.pool_usage/particle_system.pool_size;
        float nspp = (sim_end-sim_begin)*1000000000.0/particle_system.pool_usage;
        float ppms = particle_system.pool_usage / ((sim_end-sim_begin)*1000.0);
        static const char* format = "Frametime: %.0f mspf - Pool usage: %.0f%c - Simulated in %.0f ms (%.0f nspp %.0f ppms)";
        snprintf(title, 256, format, frametime*1000.0f, usage*100.0, '%', (sim_end-sim_begin)*1000.0f, nspp, ppms);
        glfwSetWindowTitle(window, title);
    }
    
    deinit();
    
    return EXIT_SUCCESS;
}

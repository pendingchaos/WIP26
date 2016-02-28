#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#include "runtime.h"

#define FAIL(...) _fail((fputs("Failure: ", stderr), fprintf(stderr, __VA_ARGS__), fputc('\n', stderr), EXIT_FAILURE))
#define WARN(...) (fputs("Warning: ", stderr), fprintf(stderr, __VA_ARGS__), fputc('\n', stderr))
#define INFO(...) (fputs("Info: ", stdout), printf(__VA_ARGS__), putchar('\n'))

static const char* vertex_source = "#version 120\n"
"attribute float posx;\n"
"attribute float posy;\n"
"attribute float posz;\n"
"attribute float deleted;\n"
"void main() {gl_Position = vec4(posx, posy, posz, deleted>127.0 ? 0.0 : 1.0);}\n";
static const char* fragment_source = "#version 120\n"
"void main() {gl_FragColor = vec4(1.0, 0.5, 0.5, 1.0);}\n";

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
static GLuint gl_program;

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
    float velx = rand() / (double)RAND_MAX * 2.0 - 1.0;
    float vely = rand() / (double)RAND_MAX * 2.0 - 1.0;
    float velz = rand() / (double)RAND_MAX * 2.0 - 1.0;
    float plen = sqrt(posx*posx + posy*posy + posz*posz);
    float vlen = sqrt(velx*velx + vely*vely + velz*velz);
    float dist = 0.0f; //rand() / (double)RAND_MAX;
    ((float*)particle_system.properties[posx_index])[index] = posx/plen*dist;
    ((float*)particle_system.properties[posy_index])[index] = posy/plen*dist;
    ((float*)particle_system.properties[posz_index])[index] = posz/plen*dist;
    ((float*)particle_system.properties[velx_index])[index] = velx/vlen*0.005f;
    ((float*)particle_system.properties[vely_index])[index] = vely/vlen*0.005f;
    ((float*)particle_system.properties[velz_index])[index] = velz/vlen*0.005f;
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
    if (!glfwInit())
        FAIL("glfwInit() failed");
    glfw_init = true;
    
    glfwWindowHint(GLFW_RESIZABLE, false);
    glfwWindowHint(GLFW_SRGB_CAPABLE, true);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 1);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
    
    window = glfwCreateWindow(800, 800, "Graphical WIP26 App", NULL, NULL);
    if (!window) FAIL("Failed to create window");
    
    int xpos, ypos;
    glfwGetMonitorPos(glfwGetPrimaryMonitor(), &xpos, &ypos);
    glfwSetWindowPos(window, xpos, ypos);
    
    glfwMakeContextCurrent(window);
    if (glewInit() != GLEW_OK) FAIL("Failed to initialize GLEW");
    
    if (GLEW_ARB_framebuffer_sRGB) glEnable(GL_FRAMEBUFFER_SRGB);
    else WARN("sRGB framebuffers not supported");
    
    if (GLEW_ARB_debug_output)
        glDebugMessageCallbackARB((GLDEBUGPROCARB)debug_callback, NULL);
    
    if (!create_runtime(&runtime, "auto"))
        FAIL("Failed to create runtime: %s", runtime.error);
    runtime_init = true;
    
    program.runtime = &runtime;
    if (!open_program("main.sim.bin", &program))
        FAIL("Failed to open main.sim.bin: %s", runtime.error);
    program_init = true;
    
    posx_index = get_property_index(&program, "pos.x");
    if (posx_index < 0) FAIL("Failed to find property \"pos.x\".");
    posy_index = get_property_index(&program, "pos.y");
    if (posy_index < 0) FAIL("Failed to find property \"pos.y\".");
    posz_index = get_property_index(&program, "pos.z");
    if (posz_index < 0) FAIL("Failed to find property \"pos.z\".");
    velx_index = get_property_index(&program, "vel.x");
    if (velx_index < 0) FAIL("Failed to find property \"vel.x\".");
    vely_index = get_property_index(&program, "vel.y");
    if (vely_index < 0) FAIL("Failed to find property \"vel.y\".");
    velz_index = get_property_index(&program, "vel.z");
    if (velz_index < 0) FAIL("Failed to find property \"vel.z\".");
    
    particle_system.runtime = &runtime;
    particle_system.pool_size = 1000000;
    particle_system.sim_program = &program;
    particle_system.property_dtypes[posx_index] = PROP_FLOAT32;
    particle_system.property_dtypes[posy_index] = PROP_FLOAT32;
    particle_system.property_dtypes[posz_index] = PROP_FLOAT32;
    particle_system.property_dtypes[velx_index] = PROP_FLOAT32;
    particle_system.property_dtypes[vely_index] = PROP_FLOAT32;
    particle_system.property_dtypes[velz_index] = PROP_FLOAT32;
    if (!create_system(&particle_system))
        FAIL("Failed to create particle system: %s", runtime.error);
    system_init = true;
    
    for (size_t i = 0; i < 100000; i++) {
        int index = spawn_particle(&particle_system);
        if (index < 0)
            FAIL("Failed to spawn particle: %s\n", runtime.error);
        init_particle(index);
    }
    
    create_gl_program();
    
    glUseProgram(gl_program);
    
    GLint posx_loc = glGetAttribLocation(gl_program, "posx");
    GLint posy_loc = glGetAttribLocation(gl_program, "posy");
    GLint posz_loc = glGetAttribLocation(gl_program, "posz");
    GLint deleted_loc = glGetAttribLocation(gl_program, "deleted");
    glEnableVertexAttribArray(posx_loc);
    glEnableVertexAttribArray(posy_loc);
    glEnableVertexAttribArray(posz_loc);
    glEnableVertexAttribArray(deleted_loc);
    float* posx = particle_system.properties[posx_index];
    float* posy = particle_system.properties[posy_index];
    float* posz = particle_system.properties[posz_index];
    uint8_t* deleted = particle_system.deleted_flags;
    glVertexAttribPointer(posx_loc, 1, GL_FLOAT, GL_FALSE, 0, posx);
    glVertexAttribPointer(posy_loc, 1, GL_FLOAT, GL_FALSE, 0, posy);
    glVertexAttribPointer(posz_loc, 1, GL_FLOAT, GL_FALSE, 0, posz);
    glVertexAttribPointer(deleted_loc, 1, GL_UNSIGNED_BYTE, GL_FALSE, 0, deleted);
    
    while (!glfwWindowShouldClose(window)) {
        double begin = glfwGetTime();
        
        if (!simulate_system(&particle_system))
            FAIL("Failed to simulate particle system: %s\n", runtime.error);
        
        glClear(GL_COLOR_BUFFER_BIT);
        
        glDrawArrays(GL_POINTS, 0, particle_system.pool_size);
        
        glfwSwapBuffers(window);
        glfwPollEvents();
        
        double end = glfwGetTime();
        
        char title[256];
        float fps = 1.0f / (end-begin);
        float usage = (double)particle_system.pool_usage/particle_system.pool_size;
        static const char* format = "Graphical WIP26 App - FPS: %.0f - Pool usage: %.0f%c";
        snprintf(title, 256, format, fps, usage*100.0, '%');
        glfwSetWindowTitle(window, title);
    }
    
    deinit();
    
    return EXIT_SUCCESS;
}

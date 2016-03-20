#define VM_COMPUTED_GOTO
#define VM_AVX

#if !defined(__AVX__) && defined(VM_AVX)
#undef VM_AVX
#endif

#include "runtime.h"

#include <string.h>
#include <math.h>
#include <endian.h>
#ifdef VM_AVX
#include <immintrin.h>
#endif

#ifdef VM_AVX
typedef __m256 simd8f_t;

static void simd8f_add(simd8f_t* dest, simd8f_t a, simd8f_t b) {
    *dest = _mm256_add_ps(a, b);
}

static void simd8f_sub(simd8f_t* dest, simd8f_t a, simd8f_t b) {
    *dest = _mm256_sub_ps(a, b);
}

static void simd8f_mul(simd8f_t* dest, simd8f_t a, simd8f_t b) {
    *dest = _mm256_mul_ps(a, b);
}

static void simd8f_div(simd8f_t* dest, simd8f_t a, simd8f_t b) {
    *dest = _mm256_div_ps(a, b);
}

static void simd8f_pow(simd8f_t* dest, simd8f_t a, simd8f_t b) {
    //*dest = _mm256_pow_ps(a, b);
    float af[8];
    _mm256_storeu_ps(af, a);
    float bf[8];
    _mm256_storeu_ps(bf, b);
    float df[8];
    for (uint_fast8_t i = 0; i < 8; i++) df[i] = powf(af[i], bf[i]);
    *dest = _mm256_loadu_ps(df);
}

static void simd8f_less(simd8f_t* dest, simd8f_t a, simd8f_t b) {
    *dest = _mm256_cmp_ps(a, b, _CMP_LT_OQ);
}

static void simd8f_greater(simd8f_t* dest, simd8f_t a, simd8f_t b) {
    *dest = _mm256_cmp_ps(a, b, _CMP_GT_OQ);
}

static void simd8f_equal(simd8f_t* dest, simd8f_t a, simd8f_t b) {
    *dest = _mm256_cmp_ps(a, b, _CMP_EQ_OQ);
}

static void simd8f_sqrt(simd8f_t* dest, simd8f_t a) {
    *dest = _mm256_sqrt_ps(a);
}

static void simd8f_bool_and(simd8f_t* dest, simd8f_t a, simd8f_t b) {
    uint32_t ai[8];
    _mm256_storeu_ps((float*)ai, a);
    uint32_t bi[8];
    _mm256_storeu_ps((float*)bi, b);
    uint32_t di[8];
    for (uint_fast8_t i = 0; i < 8; i++) di[i] = ai[i] && bi[i];
    *dest = _mm256_loadu_ps((float*)di);
}

static void simd8f_bool_or(simd8f_t* dest, simd8f_t a, simd8f_t b) {
    uint32_t ai[8];
    _mm256_storeu_ps((float*)ai, a);
    uint32_t bi[8];
    _mm256_storeu_ps((float*)bi, b);
    uint32_t di[8];
    for (uint_fast8_t i = 0; i < 8; i++) di[i] = ai[i] || bi[i];
    *dest = _mm256_loadu_ps((float*)di);
}

static void simd8f_bool_not(simd8f_t* dest, simd8f_t a) {
    uint32_t ai[8];
    _mm256_storeu_ps((float*)ai, a);
    uint32_t di[8];
    for (uint_fast8_t i = 0; i < 8; i++) di[i] = !ai[i];
    *dest = _mm256_loadu_ps((float*)di);
}

static void simd8f_sel(simd8f_t* dest, simd8f_t a, simd8f_t b, simd8f_t cond) {
    //__mmask8 mask = _mm256_cmp_epu32_mask(*(__m256i*)&cond, _mm256_set1_epi32(0), /*_MM_CMPINT_NEQ*/4);
    //*dest = _mm256_mask_blend_ps(mask, a, b);
    uint32_t* condi = (uint32_t*)&cond;
    float* af = (float*)&a;
    float* bf = (float*)&b;
    float destf[8];
    for (uint_fast8_t i = 0; i < 8; i++) destf[i] = condi[i] ? af[i] : bf[i];
    *dest = _mm256_loadu_ps(destf);
}

static void simd8f_init1(simd8f_t* dest, float v) {
    *dest = _mm256_set1_ps(v);
}

static void simd8f_init(simd8f_t* dest, const float* v) {
    *dest = _mm256_loadu_ps(v);
}

static void simd8f_get(simd8f_t v, float* dest) {
    _mm256_storeu_ps(dest, v);
}
#else
typedef struct {union {float v[8]; uint32_t i[8];};} simd8f_t;

static void simd8f_add(simd8f_t* dest, simd8f_t a, simd8f_t b) {
    for (uint_fast8_t i = 0; i < 8; i++) dest->v[i] = a.v[i] + b.v[i];
}

static void simd8f_sub(simd8f_t* dest, simd8f_t a, simd8f_t b) {
    for (uint_fast8_t i = 0; i < 8; i++) dest->v[i] = a.v[i] - b.v[i];
}

static void simd8f_mul(simd8f_t* dest, simd8f_t a, simd8f_t b) {
    for (uint_fast8_t i = 0; i < 8; i++) dest->v[i] = a.v[i] * b.v[i];
}

static void simd8f_div(simd8f_t* dest, simd8f_t a, simd8f_t b) {
    for (uint_fast8_t i = 0; i < 8; i++) dest->v[i] = a.v[i] / b.v[i];
}

static void simd8f_pow(simd8f_t* dest, simd8f_t a, simd8f_t b) {
    for (uint_fast8_t i = 0; i < 8; i++) dest->v[i] = powf(a.v[i], b.v[i]);
}

static void simd8f_sqrt(simd8f_t* dest, simd8f_t a) {
    for (uint_fast8_t i = 0; i < 8; i++) dest->v[i] = sqrt(a.v[i]);
}

static void simd8f_less(simd8f_t* dest, simd8f_t a, simd8f_t b) {
    for (uint_fast8_t i = 0; i < 8; i++) dest->i[i] = a.v[i] < b.v[i];
}

static void simd8f_greater(simd8f_t* dest, simd8f_t a, simd8f_t b) {
    for (uint_fast8_t i = 0; i < 8; i++) dest->i[i] = a.v[i] > b.v[i];
}

static void simd8f_equal(simd8f_t* dest, simd8f_t a, simd8f_t b) {
    for (uint_fast8_t i = 0; i < 8; i++) dest->i[i] = a.v[i] == b.v[i];
}

static void simd8f_bool_and(simd8f_t* dest, simd8f_t a, simd8f_t b) {
    for (uint_fast8_t i = 0; i < 8; i++) dest->i[i] = a.i[i] && b.i[i];
}

static void simd8f_bool_or(simd8f_t* dest, simd8f_t a, simd8f_t b) {
    for (uint_fast8_t i = 0; i < 8; i++) dest->i[i] = a.i[i] || b.i[i];
}

static void simd8f_bool_not(simd8f_t* dest, simd8f_t a) {
    for (uint_fast8_t i = 0; i < 8; i++) dest->i[i] = !a.i[i];
}

static void simd8f_sel(simd8f_t* dest, simd8f_t a, simd8f_t b, simd8f_t cond) {
    for (uint_fast8_t i = 0; i < 8; i++) dest->v[i] = cond.i[i] ? b.v[i] : a.v[i];
}

static void simd8f_init1(simd8f_t* dest, float v) {
    for (uint_fast8_t i = 0; i < 8; i++) dest->v[i] = v;
}

static void simd8f_init(simd8f_t* dest, const float* v) {
    memcpy(dest, v, sizeof(float)*8);
}

static void simd8f_get(simd8f_t v, float* dest) {
    memcpy(dest, &v, sizeof(float)*8);
}
#endif

#ifdef VM_COMPUTED_GOTO
#define DISPATCH goto* dispatch_table[*bc++]
#define BEGIN_CASE(op) op: {
#define END_CASE DISPATCH; }
#else
#define BEGIN_CASE(op) case op: {
#define END_CASE break;}
#endif

//TODO: Slow
void load_attr(float* val, void* attribute, attr_dtype_t dtype, size_t offset) {
    switch (dtype) {
    case ATTR_UINT8:
        for (size_t i = 0; i < 8; i++)
            val[i] = ((uint8_t*)attribute)[i+offset] / 255.0f;
        break;
    case ATTR_INT8:
        for (size_t i = 0; i < 8; i++)
            val[i] = ((int8_t*)attribute)[i+offset] / 127.0f;
        break;
    case ATTR_UINT16:
        for (size_t i = 0; i < 8; i++)
            val[i] = ((uint16_t*)attribute)[i+offset] / 65535.0f;
        break;
    case ATTR_INT16:
        for (size_t i = 0; i < 8; i++)
            val[i] = ((int16_t*)attribute)[i+offset] / 32767.0f;
        break;
    case ATTR_UINT32:
        for (size_t i = 0; i < 8; i++)
            val[i] = ((uint32_t*)attribute)[i+offset] / 4294967295.0;
        break;
    case ATTR_INT32:
        for (size_t i = 0; i < 8; i++)
            val[i] = ((int32_t*)attribute)[i+offset] / 2147483647.0;
        break;
    case ATTR_FLOAT32:
        memcpy(val, ((float*)attribute)+offset, sizeof(float)*8);
        break;
    case ATTR_FLOAT64:
        for (size_t i = 0; i < 8; i++)
            val[i] = ((double*)attribute)[i+offset];
        break;
    }
}

//TODO: Slow
void store_attr(const float* val, void* attribute, attr_dtype_t dtype, size_t offset) {
    switch (dtype) {
    case ATTR_UINT8:
        for (size_t i = 0; i < 8; i++)
            ((uint8_t*)attribute)[i+offset] = val[i] * 255.0f;
        break;
    case ATTR_INT8:
        for (size_t i = 0; i < 8; i++)
            ((int8_t*)attribute)[i+offset] = val[i] * 127.0f;
        break;
    case ATTR_UINT16:
        for (size_t i = 0; i < 8; i++)
            ((uint16_t*)attribute)[i+offset] = val[i] * 65535.0f;
        break;
    case ATTR_INT16:
        for (size_t i = 0; i < 8; i++)
            ((int16_t*)attribute)[i+offset] = val[i] * 32767.0f;
        break;
    case ATTR_UINT32:
        for (size_t i = 0; i < 8; i++)
            ((uint32_t*)attribute)[i+offset] = val[i] * 4294967295.0;
        break;
    case ATTR_INT32:
        for (size_t i = 0; i < 8; i++)
            ((int32_t*)attribute)[i+offset] = val[i] * 2147483647.0;
        break;
    case ATTR_FLOAT32:
        memcpy(((float*)attribute)+offset, val, sizeof(float)*8);
        break;
    case ATTR_FLOAT64:
        for (size_t i = 0; i < 8; i++)
            ((double*)attribute)[i+offset] = val[i];
        break;
    }
}

static bool vm_execute1(const uint8_t* bc, const uint8_t* deleted_flags, size_t index, system_t* system, float* regs, bool cond) {
    #ifdef VM_COMPUTED_GOTO
    static void* dispatch_table[] = {&&BC_OP_ADD, &&BC_OP_SUB, &&BC_OP_MUL,
                                     &&BC_OP_DIV, &&BC_OP_POW, &&BC_OP_MOVF,
                                     &&BC_OP_SQRT,
                                     &&BC_OP_DELETE, &&BC_OP_LESS, &&BC_OP_GREATER,
                                     &&BC_OP_EQUAL, &&BC_OP_BOOL_AND, &&BC_OP_BOOL_OR,
                                     &&BC_OP_BOOL_NOT, &&BC_OP_SEL, &&BC_OP_COND_BEGIN,
                                     &&BC_OP_COND_END, &&BC_OP_WHILE_BEGIN, &&BC_OP_WHILE_END_COND,
                                     &&BC_OP_WHILE_END, &&BC_OP_END};
    DISPATCH;
    #else
    while (true) {
        bc_op_t op = *bc++;
        switch (op) {
    #endif
        BEGIN_CASE(BC_OP_ADD)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            regs[d] = regs[a] + regs[b];
        END_CASE
        BEGIN_CASE(BC_OP_SUB)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            regs[d] = regs[a] - regs[b];
        END_CASE
        BEGIN_CASE(BC_OP_MUL)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            regs[d] = regs[a] * regs[b];
        END_CASE
        BEGIN_CASE(BC_OP_DIV)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            regs[d] = regs[a] / regs[b];
        END_CASE
        BEGIN_CASE(BC_OP_POW)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            regs[d] = powf(regs[a], regs[b]);
        END_CASE
        BEGIN_CASE(BC_OP_MOVF)
            uint8_t d = *bc++;
            float f = *(const float*)bc;
            bc += 4;
            regs[d] = f;
        END_CASE
        BEGIN_CASE(BC_OP_SQRT)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            regs[d] = sqrt(regs[a]);
        END_CASE
        BEGIN_CASE(BC_OP_DELETE)
            delete_particle(system->particles, index);
            return true;
        END_CASE
        BEGIN_CASE(BC_OP_LESS)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            ((uint32_t*)regs)[d] = regs[a] < regs[b];
        END_CASE
        BEGIN_CASE(BC_OP_GREATER)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            ((uint32_t*)regs)[d] = regs[a] > regs[b];
        END_CASE
        BEGIN_CASE(BC_OP_EQUAL)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            ((uint32_t*)regs)[d] = regs[a] == regs[b];
        END_CASE
        BEGIN_CASE(BC_OP_BOOL_AND)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            ((uint32_t*)regs)[d] = ((uint32_t*)regs)[a] && ((uint32_t*)regs)[b];
        END_CASE
        BEGIN_CASE(BC_OP_BOOL_OR)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            ((uint32_t*)regs)[d] = ((uint32_t*)regs)[a] && ((uint32_t*)regs)[b];
        END_CASE
        BEGIN_CASE(BC_OP_BOOL_NOT)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            ((uint32_t*)regs)[d] = !((uint32_t*)regs)[a];
        END_CASE
        BEGIN_CASE(BC_OP_SEL)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            uint8_t cond = *bc++;
            regs[d] = regs[((uint32_t*)regs)[cond] ? b : a];
        END_CASE
        BEGIN_CASE(BC_OP_COND_BEGIN)
            uint8_t c = *bc++;
            uint32_t count = *(uint32_t*)bc;
            bc += 6;
            
            if (deleted_flags[index]) bc += le32toh(count);
            else if (((uint32_t*)regs)[c]) vm_execute1(bc, deleted_flags, index, system, regs, true);
            else bc += le32toh(count);
        END_CASE
        BEGIN_CASE(BC_OP_COND_END)
            if (cond) return true;
        END_CASE
        BEGIN_CASE(BC_OP_WHILE_BEGIN)
            uint8_t c = *bc++;
            uint32_t cond_count = *(uint32_t*)bc;
            bc += 6;
            uint32_t body_count = *(uint32_t*)bc;
            bc += 6;
            
            const uint8_t* body_bc = bc + cond_count;
            
            if (deleted_flags[index])
                while (true) {
                    vm_execute1(bc, deleted_flags, index, system, regs, true);
                    if (!((uint32_t*)regs)[c]) break;
                    vm_execute1(body_bc, deleted_flags, index, system, regs, true);
                }
            
            bc = body_bc + body_count;
        END_CASE
        BEGIN_CASE(BC_OP_WHILE_END_COND)
            if (cond) return true;
        END_CASE
        BEGIN_CASE(BC_OP_WHILE_END)
            if (cond) return true;
        END_CASE
        BEGIN_CASE(BC_OP_END)
            return true;
        END_CASE
    #ifndef VM_COMPUTED_GOTO
        default: {break;}
        }
    }
    #endif
}

static bool vm_execute8(const program_t* program, size_t offset, system_t* system) {
    bool deleted = true;
    for (uint_fast8_t i = 0; i < 8; i++)
        deleted = deleted && system->particles->deleted_flags[offset+i];
    if (deleted) return true;
    
    const uint8_t* bc = program->bc;
    simd8f_t regs[256];
    
    for (size_t i = 0; i < program->attribute_count; i++) {
        float val[8];
        int index = system->sim_attribute_indices[i];
        load_attr(val, system->particles->attributes[index],
                  system->particles->attribute_dtypes[index], offset);
        simd8f_init(regs+program->attribute_load_regs[i], val);
    }
    
    for (size_t i = 0; i < program->uniform_count; i++)
        simd8f_init1(regs+program->uniform_regs[i], system->sim_uniforms[i]);
    
    #ifdef VM_COMPUTED_GOTO
    static void* dispatch_table[] = {&&BC_OP_ADD, &&BC_OP_SUB, &&BC_OP_MUL,
                                     &&BC_OP_DIV, &&BC_OP_POW, &&BC_OP_MOVF,
                                     &&BC_OP_SQRT,
                                     &&BC_OP_DELETE, &&BC_OP_LESS, &&BC_OP_GREATER,
                                     &&BC_OP_EQUAL, &&BC_OP_BOOL_AND, &&BC_OP_BOOL_OR,
                                     &&BC_OP_BOOL_NOT, &&BC_OP_SEL, &&BC_OP_COND_BEGIN,
                                     &&BC_OP_COND_END, &&BC_OP_WHILE_BEGIN, &&BC_OP_WHILE_END_COND,
                                     &&BC_OP_WHILE_END, &&BC_OP_END};
    DISPATCH;
    #else
    while (true) {
        bc_op_t op = *bc++;
        switch (op) {
    #endif
        BEGIN_CASE(BC_OP_ADD)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            simd8f_add(regs+d, regs[a], regs[b]);
        END_CASE
        BEGIN_CASE(BC_OP_SUB)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            simd8f_sub(regs+d, regs[a], regs[b]);
        END_CASE
        BEGIN_CASE(BC_OP_MUL)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            simd8f_mul(regs+d, regs[a], regs[b]);
        END_CASE
        BEGIN_CASE(BC_OP_DIV)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            simd8f_div(regs+d, regs[a], regs[b]);
        END_CASE
        BEGIN_CASE(BC_OP_POW)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            simd8f_pow(regs+d, regs[a], regs[b]);
        END_CASE
        BEGIN_CASE(BC_OP_MOVF)
            uint8_t d = *bc++;
            float f = *(const float*)bc;
            bc += 4;
            simd8f_init1(regs+d, f);
        END_CASE
        BEGIN_CASE(BC_OP_SQRT)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            simd8f_sqrt(regs+d, regs[a]);
        END_CASE
        BEGIN_CASE(BC_OP_DELETE)
            for (uint_fast8_t i = 0; i < 8; i++)
                if (!system->particles->deleted_flags[i])
                    delete_particle(system->particles, offset+i);
            goto end;
        END_CASE
        BEGIN_CASE(BC_OP_LESS)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            simd8f_less(regs+d, regs[a], regs[b]);
        END_CASE
        BEGIN_CASE(BC_OP_GREATER)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            simd8f_greater(regs+d, regs[a], regs[b]);
        END_CASE
        BEGIN_CASE(BC_OP_EQUAL)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            simd8f_equal(regs+d, regs[a], regs[b]);
        END_CASE
        BEGIN_CASE(BC_OP_BOOL_AND)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            simd8f_bool_and(regs+d, regs[a], regs[b]);
        END_CASE
        BEGIN_CASE(BC_OP_BOOL_OR)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            simd8f_bool_or(regs+d, regs[a], regs[b]);
        END_CASE
        BEGIN_CASE(BC_OP_BOOL_NOT)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            simd8f_bool_not(regs+d, regs[a]);
        END_CASE
        BEGIN_CASE(BC_OP_SEL)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            uint8_t cond = *bc++;
            simd8f_sel(regs+d, regs[a], regs[b], regs[cond]);
        END_CASE
        BEGIN_CASE(BC_OP_COND_BEGIN)
            uint8_t c = *bc++;
            uint32_t count = *(uint32_t*)bc;
            bc += 4;
            unsigned int rmin = *bc++;
            unsigned int rmax = *bc++;
            
            uint32_t v[8];
            simd8f_get(regs[c], (float*)v);
            for (uint_fast8_t i = 0; i < 8; i++) {
                if (system->particles->deleted_flags[offset+i]) continue;
                if (!v[i]) continue;
                
                float fregs[256];
                for (uint_fast16_t j = rmin; j < rmax+1; j++)
                    fregs[j] = ((float*)(regs+j))[i];
                if (!vm_execute1(bc, system->particles->deleted_flags,
                                 offset+i, system, fregs, true))
                    return false;
                for (uint_fast16_t j = rmin; j < rmax+1; j++)
                    ((float*)(regs+j))[i] = fregs[j];
            }
            bc += le32toh(count);
        END_CASE
        BEGIN_CASE(BC_OP_WHILE_END_COND)
        END_CASE
        BEGIN_CASE(BC_OP_COND_END)
        END_CASE
        BEGIN_CASE(BC_OP_WHILE_BEGIN)
            uint8_t c = *bc++;
            
            uint32_t cond_count = *(uint32_t*)bc;
            bc += 4;
            unsigned int crmin = *bc++;
            unsigned int crmax = *bc++;
            
            uint32_t body_count = *(uint32_t*)bc;
            bc += 4;
            unsigned int brmin = *bc++;
            unsigned int brmax = *bc++;
            
            const uint8_t* body_bc = bc + cond_count;
            
            for (uint_fast8_t i = 0; i < 8; i++) {
                if (system->particles->deleted_flags[offset+i]) continue;
                
                while (true) {
                    float fregs[256];
                    for (uint_fast16_t j = crmin; j < crmax+1; j++)
                        fregs[j] = ((float*)(regs+j))[i];
                    if (!vm_execute1(bc, system->particles->deleted_flags,
                                     offset+i, system, fregs, true))
                        return false;
                    for (uint_fast16_t j = crmin; j < crmax+1; j++)
                        ((float*)(regs+j))[i] = fregs[j];
                    
                    if (!((uint32_t*)fregs)[c]) break;
                    
                    for (uint_fast16_t j = brmin; j < brmax+1; j++)
                        fregs[j] = ((float*)(regs+j))[i];
                    if (!vm_execute1(body_bc, system->particles->deleted_flags,
                                     offset+i, system, fregs, true))
                        return false;
                    for (uint_fast16_t j = brmin; j < brmax+1; j++)
                        ((float*)(regs+j))[i] = fregs[j];
                }
            }
            
            bc = body_bc + body_count;
        END_CASE
        BEGIN_CASE(BC_OP_WHILE_END)
        END_CASE
        BEGIN_CASE(BC_OP_END)
            goto end;
        END_CASE
    #ifndef VM_COMPUTED_GOTO
        default: {break;}
        }
    }
    #endif
    
    end:
    for (size_t i = 0; i < program->attribute_count; i++) {
        int index = system->sim_attribute_indices[i];
        store_attr((const float*)(regs+program->attribute_store_regs[i]),
                   system->particles->attributes[index],
                   system->particles->attribute_dtypes[index], offset);
    }
    return true;
}

#ifdef VM_COMPUTED_GOTO
#undef DISPATCH
#endif
#undef BEGIN_CASE
#undef END_CASE

static bool vm_create(runtime_t* runtime) {
    return true;
}

static bool vm_destroy(runtime_t* runtime) {
    return true;
}

static size_t vm_get_attribute_padding(const runtime_t* runtime) {
    return 8;
}

static bool vm_create_program(program_t* program) {
    return true;
}

static bool vm_destroy_program(program_t* program) {
    return true;
}

static bool vm_simulate_system(system_t* system) {
    const program_t* p = system->sim_program;
    for (size_t i = 0; i < system->particles->pool_size; i += 8)
        if (!vm_execute8(p, i, system))
            return false;
    
    return true;
}

bool vm_backend(backend_t* backend) {
    backend->create = &vm_create;
    backend->destroy = &vm_destroy;
    backend->get_attribute_padding = &vm_get_attribute_padding;
    backend->create_program = &vm_create_program;
    backend->destroy_program = &vm_destroy_program;
    backend->simulate_system = &vm_simulate_system;
    return true;
}

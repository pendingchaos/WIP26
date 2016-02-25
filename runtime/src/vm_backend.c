#define VM_COMPUTED_GOTO
#define VM_AVX

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

static void simd8f_min(simd8f_t* dest, simd8f_t a, simd8f_t b) {
    *dest = _mm256_min_ps(a, b);
}

static void simd8f_max(simd8f_t* dest, simd8f_t a, simd8f_t b) {
    *dest = _mm256_max_ps(a, b);
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

static void simd8f_init1(simd8f_t* dest, float v) {
    *dest = _mm256_set1_ps(v);
}

static void simd8f_init(simd8f_t* dest, float* v) {
    *dest = _mm256_loadu_ps(v);
}

static void simd8f_get(simd8f_t v, float* dest) {
    _mm256_storeu_ps(dest, v);
}
#else
typedef struct {float v[8];} simd8f_t;

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

static void simd8f_min(simd8f_t* dest, simd8f_t a, simd8f_t b) {
    for (uint_fast8_t i = 0; i < 8; i++) dest->v[i] = fmin(a.v[i], b.v[i]);
}

static void simd8f_max(simd8f_t* dest, simd8f_t a, simd8f_t b) {
    for (uint_fast8_t i = 0; i < 8; i++) dest->v[i] = fmax(a.v[i], b.v[i]);
}

static void simd8f_sqrt(simd8f_t* dest, simd8f_t a) {
    for (uint_fast8_t i = 0; i < 8; i++) dest->v[i] = sqrt(a.v[i]);
}

static void simd8f_less(simd8f_t* dest, simd8f_t a, simd8f_t b) {
    for (uint_fast8_t i = 0; i < 8; i++) ((uint32_t*)dest->v)[i] = a.v[i] < b.v[i];
}

static void simd8f_greater(simd8f_t* dest, simd8f_t a, simd8f_t b) {
    for (uint_fast8_t i = 0; i < 8; i++) ((uint32_t*)dest->v)[i] = a.v[i] > b.v[i];
}

static void simd8f_equal(simd8f_t* dest, simd8f_t a, simd8f_t b) {
    for (uint_fast8_t i = 0; i < 8; i++) ((uint32_t*)dest->v)[i] = a.v[i] == b.v[i];
}

static void simd8f_bool_and(simd8f_t* dest, simd8f_t a, simd8f_t b) {
    for (uint_fast8_t i = 0; i < 8; i++) ((uint32_t*)dest->v)[i] = (uint32_t*)a.v)[i] && (uint32_t*)b.v)[i];
}

static void simd8f_bool_or(simd8f_t* dest, simd8f_t a, simd8f_t b) {
    for (uint_fast8_t i = 0; i < 8; i++) ((uint32_t*)dest->v)[i] = (uint32_t*)a.v)[i] || (uint32_t*)b.v)[i];
}

static void simd8f_bool_not(simd8f_t* dest, simd8f_t a) {
    for (uint_fast8_t i = 0; i < 8; i++) ((uint32_t*)dest->v)[i] = !(uint32_t*)a.v)[i];
}

static void simd8f_init1(simd8f_t* dest, float v) {
    for (uint_fast8_t i = 0; i < 8; i++) dest->v[i] = v;
}

static void simd8f_init(simd8f_t* dest, float* v) {
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

static bool vm_execute1(const uint8_t* bc, size_t index, uint8_t* deleted_flags, float** properties, float* regs, bool cond) {
    #ifdef VM_COMPUTED_GOTO
    static void* dispatch_table[] = {&&BC_OP_ADD_R_F, &&BC_OP_ADD_RR,
                                     &&BC_OP_SUB_R_F, &&BC_OP_SUB_F_R,
                                     &&BC_OP_SUB_RR, &&BC_OP_MUL_R_F,
                                     &&BC_OP_MUL_RR, &&BC_OP_DIV_R_F,
                                     &&BC_OP_DIV_F_R, &&BC_OP_DIV_RR,
                                     &&BC_OP_POW_R_F, &&BC_OP_POW_F_R,
                                     &&BC_OP_POW_RR, &&BC_OP_MOV_F,
                                     &&BC_OP_MIN_RR, &&BC_OP_MIN_R_F,
                                     &&BC_OP_MAX_RR, &&BC_OP_MAX_R_F,
                                     &&BC_OP_SQRT, &&BC_OP_LOAD_PROP,
                                     &&BC_OP_STORE_PROP, &&BC_OP_DELETE,
                                     &&BC_OP_LESS, &&BC_OP_GREATER, &&BC_OP_EQUAL,
                                     &&BC_OP_BOOL_AND, &&BC_OP_BOOL_OR, &&BC_OP_BOOL_NOT,
                                     &&BC_OP_COND_BEGIN, &&BC_OP_COND_END,
                                     &&BC_OP_END};
    DISPATCH;
    #else
    const uint8_t* end = bc + program->bc_size;
    while (bc != end) {
        bc_op_t op = *bc++;
        switch (op) {
    #endif
        BEGIN_CASE(BC_OP_ADD_R_F)
            uint8_t d = *bc++;
            uint8_t r = *bc++;
            float f = *(const float*)bc;
            bc += 4;
            regs[d] = regs[r] + f;
        END_CASE
        BEGIN_CASE(BC_OP_ADD_RR)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            regs[d] = regs[a] + regs[b];
        END_CASE
        BEGIN_CASE(BC_OP_SUB_R_F)
            uint8_t d = *bc++;
            uint8_t r = *bc++;
            float f = *(const float*)bc;
            bc += 4;
            regs[d] = regs[r] - f;
        END_CASE
        BEGIN_CASE(BC_OP_SUB_F_R)
            uint8_t d = *bc++;
            float f = *(const float*)bc;
            bc += 4;
            uint8_t r = *bc++;
            regs[d] = f - regs[r];
        END_CASE
        BEGIN_CASE(BC_OP_SUB_RR)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            regs[d] = regs[a] - regs[b];
        END_CASE
        BEGIN_CASE(BC_OP_MUL_R_F)
            uint8_t d = *bc++;
            uint8_t r = *bc++;
            float f = *(const float*)bc;
            bc += 4;
            regs[d] = regs[r] * f;
        END_CASE
        BEGIN_CASE(BC_OP_MUL_RR)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            regs[d] = regs[a] * regs[b];
        END_CASE
        BEGIN_CASE(BC_OP_DIV_R_F)
            uint8_t d = *bc++;
            uint8_t r = *bc++;
            float f = *(const float*)bc;
            bc += 4;
            regs[d] = regs[r] / f;
        END_CASE
        BEGIN_CASE(BC_OP_DIV_F_R)
            uint8_t d = *bc++;
            float f = *(const float*)bc;
            bc += 4;
            uint8_t r = *bc++;
            regs[d] = f / regs[r];
        END_CASE
        BEGIN_CASE(BC_OP_DIV_RR)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            regs[d] = regs[a] / regs[b];
        END_CASE
        BEGIN_CASE(BC_OP_POW_R_F)
            uint8_t d = *bc++;
            uint8_t r = *bc++;
            float f = *(const float*)bc;
            bc += 4;
            regs[d] = powf(regs[r], f);
        END_CASE
        BEGIN_CASE(BC_OP_POW_F_R)
            uint8_t d = *bc++;
            float f = *(const float*)bc;
            bc += 4;
            uint8_t r = *bc++;
            regs[d] = powf(f, regs[r]);
        END_CASE
        BEGIN_CASE(BC_OP_POW_RR)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            regs[d] = powf(regs[a], regs[b]);
        END_CASE
        BEGIN_CASE(BC_OP_MOV_F)
            uint8_t d = *bc++;
            float f = *(const float*)bc;
            bc += 4;
            regs[d] = f;
        END_CASE
        BEGIN_CASE(BC_OP_MIN_R_F)
            uint8_t d = *bc++;
            uint8_t r = *bc++;
            float f = *(const float*)bc;
            bc += 4;
            regs[d] = fmin(regs[r], f);
        END_CASE
        BEGIN_CASE(BC_OP_MIN_RR)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            regs[d] = fmin(regs[a], regs[b]);
        END_CASE
        BEGIN_CASE(BC_OP_MAX_R_F)
            uint8_t d = *bc++;
            uint8_t r = *bc++;
            float f = *(const float*)bc;
            bc += 4;
            regs[d] = fmax(f, regs[r]);
        END_CASE
        BEGIN_CASE(BC_OP_MAX_RR)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            regs[d] = fmax(regs[a], regs[b]);
        END_CASE
        BEGIN_CASE(BC_OP_SQRT)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            regs[d] = sqrt(regs[a]);
        END_CASE
        BEGIN_CASE(BC_OP_LOAD_PROP)
            uint8_t d = *bc++;
            uint8_t s = *bc++;
            regs[d] = properties[s][index];
        END_CASE
        BEGIN_CASE(BC_OP_STORE_PROP)
            uint8_t d = *bc++;
            uint8_t s = *bc++;
            properties[d][index] = regs[s];
        END_CASE
        BEGIN_CASE(BC_OP_DELETE)
            deleted_flags[index] = 1;
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
        BEGIN_CASE(BC_OP_COND_BEGIN)
            uint8_t c = *bc++;
            uint32_t count = *(uint32_t*)bc;
            bc += 6;
            if (regs[c] >= 0.5f) vm_execute1(bc, index, deleted_flags, properties, regs, cond);
            else bc += le32toh(count);
        END_CASE
        BEGIN_CASE(BC_OP_COND_END)
            return cond;
        END_CASE
        BEGIN_CASE(BC_OP_END)
            return true;
        END_CASE
    #ifndef VM_COMPUTED_GOTO
        default: {break;}
        }
    }
    #endif
    
    return true;
}

static bool vm_execute8(const program_t* program, size_t offset, uint8_t* deleted_flags, float** properties) {
    bool deleted = true;
    for (uint_fast8_t i = 0; i < 8; i++)
        deleted = deleted && deleted_flags[offset+i];
    if (deleted) return true;
    
    const uint8_t* bc = program->bc;
    simd8f_t regs[256];
    #ifdef VM_COMPUTED_GOTO
    static void* dispatch_table[] = {&&BC_OP_ADD_R_F, &&BC_OP_ADD_RR,
                                     &&BC_OP_SUB_R_F, &&BC_OP_SUB_F_R,
                                     &&BC_OP_SUB_RR, &&BC_OP_MUL_R_F,
                                     &&BC_OP_MUL_RR, &&BC_OP_DIV_R_F,
                                     &&BC_OP_DIV_F_R, &&BC_OP_DIV_RR,
                                     &&BC_OP_POW_R_F, &&BC_OP_POW_F_R,
                                     &&BC_OP_POW_RR, &&BC_OP_MOV_F,
                                     &&BC_OP_MIN_RR, &&BC_OP_MIN_R_F,
                                     &&BC_OP_MAX_RR, &&BC_OP_MAX_R_F,
                                     &&BC_OP_SQRT, &&BC_OP_LOAD_PROP,
                                     &&BC_OP_STORE_PROP, &&BC_OP_DELETE,
                                     &&BC_OP_LESS, &&BC_OP_GREATER, &&BC_OP_EQUAL,
                                     &&BC_OP_BOOL_AND, &&BC_OP_BOOL_OR, &&BC_OP_BOOL_NOT,
                                     &&BC_OP_COND_BEGIN, &&BC_OP_COND_END,
                                     &&BC_OP_END};
    DISPATCH;
    #else
    const uint8_t* end = bc + program->bc_size;
    while (bc != end) {
        bc_op_t op = *bc++;
        switch (op) {
    #endif
        BEGIN_CASE(BC_OP_ADD_R_F)
            uint8_t d = *bc++;
            uint8_t r = *bc++;
            simd8f_t f;
            simd8f_init1(&f, *(const float*)bc);
            bc += 4;
            simd8f_add(regs+d, regs[r], f);
        END_CASE
        BEGIN_CASE(BC_OP_ADD_RR)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            simd8f_add(regs+d, regs[a], regs[b]);
        END_CASE
        BEGIN_CASE(BC_OP_SUB_R_F)
            uint8_t d = *bc++;
            uint8_t r = *bc++;
            simd8f_t f;
            simd8f_init1(&f, *(const float*)bc);
            bc += 4;
            simd8f_sub(regs+d, regs[r], f);
        END_CASE
        BEGIN_CASE(BC_OP_SUB_F_R)
            uint8_t d = *bc++;
            simd8f_t f;
            simd8f_init1(&f, *(const float*)bc);
            bc += 4;
            uint8_t r = *bc++;
            simd8f_sub(regs+d, f, regs[r]);
        END_CASE
        BEGIN_CASE(BC_OP_SUB_RR)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            simd8f_sub(regs+d, regs[a], regs[b]);
        END_CASE
        BEGIN_CASE(BC_OP_MUL_R_F)
            uint8_t d = *bc++;
            uint8_t r = *bc++;
            simd8f_t f;
            simd8f_init1(&f, *(const float*)bc);
            bc += 4;
            simd8f_mul(regs+d, regs[r], f);
        END_CASE
        BEGIN_CASE(BC_OP_MUL_RR)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            simd8f_mul(regs+d, regs[a], regs[b]);
        END_CASE
        BEGIN_CASE(BC_OP_DIV_R_F)
            uint8_t d = *bc++;
            uint8_t r = *bc++;
            simd8f_t f;
            simd8f_init1(&f, *(const float*)bc);
            bc += 4;
            simd8f_div(regs+d, regs[r], f);
        END_CASE
        BEGIN_CASE(BC_OP_DIV_F_R)
            uint8_t d = *bc++;
            simd8f_t f;
            simd8f_init1(&f, *(const float*)bc);
            bc += 4;
            uint8_t r = *bc++;
            simd8f_div(regs+d, f, regs[r]);
        END_CASE
        BEGIN_CASE(BC_OP_DIV_RR)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            simd8f_div(regs+d, regs[a], regs[b]);
        END_CASE
        BEGIN_CASE(BC_OP_POW_R_F)
            uint8_t d = *bc++;
            uint8_t r = *bc++;
            simd8f_t f;
            simd8f_init1(&f, *(const float*)bc);
            bc += 4;
            simd8f_pow(regs+d, regs[r], f);
        END_CASE
        BEGIN_CASE(BC_OP_POW_F_R)
            uint8_t d = *bc++;
            simd8f_t f;
            simd8f_init1(&f, *(const float*)bc);
            bc += 4;
            uint8_t r = *bc++;
            simd8f_pow(regs+d, f, regs[r]);
        END_CASE
        BEGIN_CASE(BC_OP_POW_RR)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            simd8f_pow(regs+d, regs[a], regs[b]);
        END_CASE
        BEGIN_CASE(BC_OP_MOV_F)
            uint8_t d = *bc++;
            float f = *(const float*)bc;
            bc += 4;
            simd8f_init1(regs+d, f);
        END_CASE
        BEGIN_CASE(BC_OP_MIN_R_F)
            uint8_t d = *bc++;
            uint8_t r = *bc++;
            simd8f_t f;
            simd8f_init1(&f, *(const float*)bc);
            bc += 4;
            simd8f_min(regs+d, regs[r], f);
        END_CASE
        BEGIN_CASE(BC_OP_MIN_RR)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            simd8f_min(regs+d, regs[a], regs[b]);
        END_CASE
        BEGIN_CASE(BC_OP_MAX_R_F)
            uint8_t d = *bc++;
            uint8_t r = *bc++;
            simd8f_t f;
            simd8f_init1(&f, *(const float*)bc);
            bc += 4;
            simd8f_max(regs+d, regs[r], f);
        END_CASE
        BEGIN_CASE(BC_OP_MAX_RR)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            simd8f_max(regs+d, regs[a], regs[b]);
        END_CASE
        BEGIN_CASE(BC_OP_SQRT)
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            simd8f_sqrt(regs+d, regs[a]);
        END_CASE
        BEGIN_CASE(BC_OP_LOAD_PROP)
            uint8_t d = *bc++;
            uint8_t s = *bc++;
            simd8f_init(regs+d, properties[s]+offset);
        END_CASE
        BEGIN_CASE(BC_OP_STORE_PROP)
            uint8_t d = *bc++;
            uint8_t s = *bc++;
            simd8f_get(regs[s], properties[d]+offset);
        END_CASE
        BEGIN_CASE(BC_OP_DELETE)
            for (uint_fast8_t i = 0; i < 8; i++)
                deleted_flags[offset+i] = 1;
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
        BEGIN_CASE(BC_OP_COND_BEGIN)
            uint8_t c = *bc++;
            uint32_t count = *(uint32_t*)bc;
            bc += 4;
            unsigned int rmin = *bc++;
            unsigned int rmax = *bc++;
            
            float v[8];
            simd8f_get(regs[c], v);
            for (uint_fast8_t i = 0; i < 8; i++) {
                if (v[i] < 0.5f) continue;
                float fregs[rmax+1];
                for (uint_fast16_t j = rmin; j < rmax+1; j++)
                    fregs[j] = ((float*)(regs+j))[i];
                
                vm_execute1(bc, offset+i, deleted_flags, properties, fregs, true);
                for (uint_fast16_t j = rmin; j < rmax+1; j++)
                    ((float*)(regs+j))[i] = fregs[j];
            }
            bc += le32toh(count);
        END_CASE
        BEGIN_CASE(BC_OP_COND_END)
            //Nothing
        END_CASE
        BEGIN_CASE(BC_OP_END)
            return true;
        END_CASE
    #ifndef VM_COMPUTED_GOTO
        default: {break;}
        }
    }
    #endif
    
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

static size_t vm_get_property_padding(const runtime_t* runtime) {
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
    uint8_t* del_flags = system->deleted_flags;
    float** properties = system->properties;
    
    for (size_t i = 0; i < system->pool_size; i += 8) {
        if (!vm_execute8(p, i, del_flags, properties))
            return false;
    }
    
    return true;
}

bool vm_backend(backend_t* backend) {
    backend->create = &vm_create;
    backend->destroy = &vm_destroy;
    backend->get_property_padding = &vm_get_property_padding;
    backend->create_program = &vm_create_program;
    backend->destroy_program = &vm_destroy_program;
    backend->simulate_system = &vm_simulate_system;
    return true;
}

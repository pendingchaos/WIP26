#include "runtime.h"

#include <string.h>
#include <endian.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>

typedef struct llvm_prog_t {
    LLVMModuleRef module;
    LLVMModuleRef opt_module;
    LLVMBuilderRef builder;
    LLVMExecutionEngineRef exec_engine;
    LLVMValueRef main_func;
    LLVMValueRef floor_func;
    LLVMValueRef sqrt_func;
    LLVMValueRef pow_func;
    LLVMValueRef randf_func;
    LLVMValueRef inv_index;
    LLVMValueRef del_flags;
    LLVMValueRef particles;
    LLVMValueRef attr_data;
    LLVMValueRef attr_dtypes;
    LLVMValueRef uniforms;
    LLVMValueRef del_particle_func;
    LLVMValueRef spawn_particle_func;
} llvm_prog_t;

typedef struct llvm_backend_t {
    size_t next_name;
} llvm_backend_t;

typedef int (*sim_func_t)(unsigned int, unsigned int, float*, void**,
                          int*, uint8_t*, particles_t*);

typedef int (*emit_func_t)(float*, particles_t*, void**, int*);

static float randf() {
    return rand() / (float)RAND_MAX;
}

static char* get_reg_name(unsigned int i) {
    static char name[64];
    snprintf(name, sizeof(name), "r%u", i);
    return name;
}

static char* get_name(runtime_t* runtime) {
    llvm_backend_t* backend = runtime->backend.internal;
    static char name[64];
    snprintf(name, sizeof(name), "n%zu", backend->next_name++);
    return name;
}

static LLVMValueRef get_intrinsic1(LLVMModuleRef module, const char* name) {
    LLVMTypeRef param[] = {LLVMFloatType()};
    LLVMTypeRef ret = LLVMFunctionType(LLVMFloatType(), param, 1, 0);
    return LLVMAddFunction(module, name, ret);
}

static LLVMValueRef get_intrinsic2(LLVMModuleRef module, const char* name) {
    LLVMTypeRef params[] = {LLVMFloatType(), LLVMFloatType()};
    LLVMTypeRef ret = LLVMFunctionType(LLVMFloatType(), params, 2, 0);
    return LLVMAddFunction(module, name, ret);
}

static LLVMValueRef get_del_particle_func(LLVMModuleRef module) {
    LLVMTypeRef params[] = {LLVMPointerType(LLVMInt32Type(), 0), LLVMInt32Type()};
    LLVMTypeRef ret = LLVMFunctionType(LLVMIntType(1), params, 2, 0);
    return LLVMAddFunction(module, "delete_particle", ret);
}

static LLVMValueRef get_spawn_particle_func(LLVMModuleRef module) {
    LLVMTypeRef params[] = {LLVMPointerType(LLVMInt32Type(), 0)};
    LLVMTypeRef ret = LLVMFunctionType(LLVMInt32Type(), params, 1, 0);
    return LLVMAddFunction(module, "spawn_particle", ret);
}

static LLVMValueRef get_randf_func(LLVMModuleRef module) {
    LLVMTypeRef ret = LLVMFunctionType(LLVMFloatType(), NULL, 0, 0);
    return LLVMAddFunction(module, "randf", ret);
}

static LLVMValueRef load_reg_f(program_t* program, LLVMValueRef* regs, uint8_t i) {
    runtime_t* runtime = program->runtime;
    llvm_prog_t* llvm = program->backend_internal;
    return LLVMBuildLoad(llvm->builder, regs[i], get_name(runtime));
}

static LLVMValueRef load_reg_b(program_t* program, LLVMValueRef* regs, uint8_t i) {
    runtime_t* runtime = program->runtime;
    llvm_prog_t* llvm = program->backend_internal;
    LLVMValueRef res = LLVMBuildLoad(llvm->builder, regs[i], get_name(runtime));
    res = LLVMBuildBitCast(llvm->builder, res, LLVMInt32Type(), get_name(runtime));
    LLVMValueRef zero = LLVMConstInt(LLVMInt32Type(), 0, false);
    return LLVMBuildICmp(llvm->builder, LLVMIntNE, res, zero, get_name(runtime));
}

static void store_reg_b(program_t* program, LLVMValueRef* regs, uint8_t i, LLVMValueRef val) {
    runtime_t* runtime = program->runtime;
    llvm_prog_t* llvm = program->backend_internal;
    val = LLVMBuildZExt(llvm->builder, val, LLVMInt32Type(), get_name(runtime));
    val = LLVMBuildBitCast(llvm->builder, val, LLVMFloatType(), get_name(runtime));
    LLVMBuildStore(llvm->builder, val, regs[i]);
}

static LLVMBasicBlockRef load_attr(LLVMValueRef dest, size_t i, llvm_prog_t* llvm,
                                   runtime_t* runtime, LLVMValueRef inv_index) {
    LLVMValueRef index = LLVMConstInt(LLVMInt32Type(), i, false);
    
    LLVMValueRef dtype = LLVMBuildInBoundsGEP(llvm->builder, llvm->attr_dtypes,
                                              &index, 1, get_name(runtime));
    dtype = LLVMBuildLoad(llvm->builder, dtype, get_name(runtime));
    
    LLVMBasicBlockRef u8_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
    LLVMBasicBlockRef i8_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
    LLVMBasicBlockRef u16_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
    LLVMBasicBlockRef i16_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
    LLVMBasicBlockRef u32_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
    LLVMBasicBlockRef i32_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
    LLVMBasicBlockRef f32_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
    LLVMBasicBlockRef f64_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
    LLVMBasicBlockRef end_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
    
    LLVMValueRef switch_ = LLVMBuildSwitch(llvm->builder, dtype, end_block, 8);
    LLVMAddCase(switch_, LLVMConstInt(LLVMInt32Type(), ATTR_UINT8, false), u8_block);
    LLVMAddCase(switch_, LLVMConstInt(LLVMInt32Type(), ATTR_INT8, false), i8_block);
    LLVMAddCase(switch_, LLVMConstInt(LLVMInt32Type(), ATTR_UINT16, false), u16_block);
    LLVMAddCase(switch_, LLVMConstInt(LLVMInt32Type(), ATTR_INT16, false), i16_block);
    LLVMAddCase(switch_, LLVMConstInt(LLVMInt32Type(), ATTR_UINT32, false), u32_block);
    LLVMAddCase(switch_, LLVMConstInt(LLVMInt32Type(), ATTR_INT32, false), i32_block);
    LLVMAddCase(switch_, LLVMConstInt(LLVMInt32Type(), ATTR_FLOAT32, false), f32_block);
    LLVMAddCase(switch_, LLVMConstInt(LLVMInt32Type(), ATTR_FLOAT64, false), f64_block);
    
    #define LOAD_INT(block, type, signed_, max) {\
        LLVMPositionBuilderAtEnd(llvm->builder, block);\
        LLVMValueRef vals = LLVMBuildInBoundsGEP(llvm->builder, llvm->attr_data,\
                                                 &index, 1, get_name(runtime));\
        vals = LLVMBuildLoad(llvm->builder, vals, get_name(runtime));\
        vals = LLVMBuildBitCast(llvm->builder, vals,\
                                LLVMPointerType(type, 0),\
                                get_name(runtime));\
        LLVMValueRef val_ptr = LLVMBuildGEP(llvm->builder, vals,\
                                            &inv_index, 1, get_name(runtime));\
        LLVMValueRef val = LLVMBuildLoad(llvm->builder, val_ptr, get_name(runtime));\
        if (signed_) {\
            val = LLVMBuildSIToFP(llvm->builder, val, LLVMDoubleType(), get_name(runtime));\
            val = LLVMBuildFDiv(llvm->builder, val, LLVMConstReal(LLVMDoubleType(), max), get_name(runtime));\
        } else {\
            val = LLVMBuildUIToFP(llvm->builder, val, LLVMDoubleType(), get_name(runtime));\
            val = LLVMBuildFDiv(llvm->builder, val, LLVMConstReal(LLVMDoubleType(), max), get_name(runtime));\
        }\
        val = LLVMBuildFPTrunc(llvm->builder, val, LLVMFloatType(), get_name(runtime));\
        LLVMBuildStore(llvm->builder, val, dest);\
        LLVMBuildBr(llvm->builder, end_block);\
    }
    
    LOAD_INT(u8_block, LLVMInt8Type(), false, 255);
    LOAD_INT(i8_block, LLVMInt8Type(), true, 127);
    LOAD_INT(u16_block, LLVMInt16Type(), false, 65535);
    LOAD_INT(i16_block, LLVMInt16Type(), true, 32767);
    LOAD_INT(u32_block, LLVMInt32Type(), false, 4294967295);
    LOAD_INT(i32_block, LLVMInt32Type(), true, 2147483647);
    
    #undef LOAD_INT
    
    //ATTR_FLOAT32
    {
        LLVMPositionBuilderAtEnd(llvm->builder, f32_block);
        LLVMValueRef vals = LLVMBuildInBoundsGEP(llvm->builder, llvm->attr_data,
                                                 &index, 1, get_name(runtime));
        vals = LLVMBuildLoad(llvm->builder, vals, get_name(runtime));
        vals = LLVMBuildBitCast(llvm->builder, vals,
                                LLVMPointerType(LLVMFloatType(), 0),
                                get_name(runtime));
        LLVMValueRef val_ptr = LLVMBuildGEP(llvm->builder, vals,
                                            &inv_index, 1, get_name(runtime));
        LLVMValueRef val = LLVMBuildLoad(llvm->builder, val_ptr, get_name(runtime));
        LLVMBuildStore(llvm->builder, val, dest);
        LLVMBuildBr(llvm->builder, end_block);
    }
    
    //ATTR_FLOAT64
    {
        LLVMPositionBuilderAtEnd(llvm->builder, f64_block);
        LLVMValueRef vals = LLVMBuildInBoundsGEP(llvm->builder, llvm->attr_data,
                                                 &index, 1, get_name(runtime));
        vals = LLVMBuildLoad(llvm->builder, vals, get_name(runtime));
        vals = LLVMBuildBitCast(llvm->builder, vals,
                                LLVMPointerType(LLVMDoubleType(), 0),
                                get_name(runtime));
        LLVMValueRef val_ptr = LLVMBuildGEP(llvm->builder, vals,
                                            &inv_index, 1, get_name(runtime));
        LLVMValueRef val = LLVMBuildLoad(llvm->builder, val_ptr, get_name(runtime));
        val = LLVMBuildFPTrunc(llvm->builder, val, LLVMFloatType(), get_name(runtime));
        LLVMBuildStore(llvm->builder, val, dest);
        LLVMBuildBr(llvm->builder, end_block);
    }
    
    LLVMPositionBuilderAtEnd(llvm->builder, end_block);
    return end_block;
}

static LLVMBasicBlockRef store_attr(LLVMValueRef val, size_t i, llvm_prog_t* llvm,
                                    runtime_t* runtime, LLVMValueRef inv_index) {
    LLVMValueRef index = LLVMConstInt(LLVMInt32Type(), i, false);
    
    LLVMValueRef dtype = LLVMBuildInBoundsGEP(llvm->builder, llvm->attr_dtypes,
                                              &index, 1, get_name(runtime));
    dtype = LLVMBuildLoad(llvm->builder, dtype, get_name(runtime));
    
    LLVMBasicBlockRef u8_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
    LLVMBasicBlockRef i8_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
    LLVMBasicBlockRef u16_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
    LLVMBasicBlockRef i16_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
    LLVMBasicBlockRef u32_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
    LLVMBasicBlockRef i32_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
    LLVMBasicBlockRef f32_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
    LLVMBasicBlockRef f64_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
    LLVMBasicBlockRef end_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
    
    LLVMValueRef switch_ = LLVMBuildSwitch(llvm->builder, dtype, end_block, 8);
    LLVMAddCase(switch_, LLVMConstInt(LLVMInt32Type(), ATTR_UINT8, false), u8_block);
    LLVMAddCase(switch_, LLVMConstInt(LLVMInt32Type(), ATTR_INT8, false), i8_block);
    LLVMAddCase(switch_, LLVMConstInt(LLVMInt32Type(), ATTR_UINT16, false), u16_block);
    LLVMAddCase(switch_, LLVMConstInt(LLVMInt32Type(), ATTR_INT16, false), i16_block);
    LLVMAddCase(switch_, LLVMConstInt(LLVMInt32Type(), ATTR_UINT32, false), u32_block);
    LLVMAddCase(switch_, LLVMConstInt(LLVMInt32Type(), ATTR_INT32, false), i32_block);
    LLVMAddCase(switch_, LLVMConstInt(LLVMInt32Type(), ATTR_FLOAT32, false), f32_block);
    LLVMAddCase(switch_, LLVMConstInt(LLVMInt32Type(), ATTR_FLOAT64, false), f64_block);
    
    #define STORE_INT(block, type, signed_, max) {\
        LLVMPositionBuilderAtEnd(llvm->builder, block);\
        LLVMValueRef vals = LLVMBuildInBoundsGEP(llvm->builder, llvm->attr_data,\
                                                 &index, 1, get_name(runtime));\
        vals = LLVMBuildLoad(llvm->builder, vals, get_name(runtime));\
        vals = LLVMBuildBitCast(llvm->builder, vals,\
                                LLVMPointerType(type, 0),\
                                get_name(runtime));\
        LLVMValueRef dest_ptr = LLVMBuildGEP(llvm->builder, vals,\
                                             &inv_index, 1, get_name(runtime));\
        LLVMValueRef new_val = LLVMBuildFPExt(llvm->builder, val, LLVMDoubleType(), get_name(runtime));\
        new_val = LLVMBuildFMul(llvm->builder, new_val, LLVMConstReal(LLVMDoubleType(), max), get_name(runtime));\
        if (signed_) new_val = LLVMBuildFPToSI(llvm->builder, new_val, type, get_name(runtime));\
        else new_val = LLVMBuildFPToUI(llvm->builder, new_val, type, get_name(runtime));\
        LLVMBuildStore(llvm->builder, new_val, dest_ptr);\
        LLVMBuildBr(llvm->builder, end_block);\
    }
    
    STORE_INT(u8_block, LLVMInt8Type(), false, 255);
    STORE_INT(i8_block, LLVMInt8Type(), true, 127);
    STORE_INT(u16_block, LLVMInt16Type(), false, 65535);
    STORE_INT(i16_block, LLVMInt16Type(), true, 32767);
    STORE_INT(u32_block, LLVMInt32Type(), false, 4294967295);
    STORE_INT(i32_block, LLVMInt32Type(), true, 2147483647);
    
    #undef STORE_INT
    
    //ATTR_FLOAT32
    {
        LLVMPositionBuilderAtEnd(llvm->builder, f32_block);
        LLVMValueRef vals = LLVMBuildInBoundsGEP(llvm->builder, llvm->attr_data,
                                                 &index, 1, get_name(runtime));
        vals = LLVMBuildLoad(llvm->builder, vals, get_name(runtime));
        vals = LLVMBuildBitCast(llvm->builder, vals,
                                LLVMPointerType(LLVMFloatType(), 0),
                                get_name(runtime));
        LLVMValueRef dest_ptr = LLVMBuildGEP(llvm->builder, vals,
                                             &inv_index, 1, get_name(runtime));
        LLVMBuildStore(llvm->builder, val, dest_ptr);
        LLVMBuildBr(llvm->builder, end_block);
    }
    
    //ATTR_FLOAT64
    {
        LLVMPositionBuilderAtEnd(llvm->builder, f64_block);
        LLVMValueRef vals = LLVMBuildInBoundsGEP(llvm->builder, llvm->attr_data,
                                                 &index, 1, get_name(runtime));
        vals = LLVMBuildLoad(llvm->builder, vals, get_name(runtime));
        vals = LLVMBuildBitCast(llvm->builder, vals,
                                LLVMPointerType(LLVMDoubleType(), 0),
                                get_name(runtime));
        LLVMValueRef dest_ptr = LLVMBuildGEP(llvm->builder, vals,
                                            &inv_index, 1, get_name(runtime));
        LLVMValueRef new_val = LLVMBuildFPExt(llvm->builder, val, LLVMDoubleType(), get_name(runtime));
        LLVMBuildStore(llvm->builder, new_val, dest_ptr);
        LLVMBuildBr(llvm->builder, end_block);
    }
    
    LLVMPositionBuilderAtEnd(llvm->builder, end_block);
    return end_block;
}

static LLVMBasicBlockRef to_ir(LLVMBasicBlockRef block, program_t* program,
                               LLVMValueRef* regs, uint8_t* bc, uint8_t* end,
                               LLVMBasicBlockRef end_block) {
    runtime_t* runtime = program->runtime;
    llvm_prog_t* llvm = program->backend_internal;
    
    while (bc != end) {
        LLVMPositionBuilderAtEnd(llvm->builder, block);
        bc_op_t op = *bc++;
        switch (op) {
        case BC_OP_ADD: {
            LLVMValueRef av = load_reg_f(program, regs, bc[1]);
            LLVMValueRef bv = load_reg_f(program, regs, bc[2]);
            LLVMValueRef res = LLVMBuildFAdd(llvm->builder, av, bv, get_name(runtime));
            LLVMBuildStore(llvm->builder, res, regs[bc[0]]);
            bc += 3;
            break;
        }
        case BC_OP_SUB: {
            LLVMValueRef av = load_reg_f(program, regs, bc[1]);
            LLVMValueRef bv = load_reg_f(program, regs, bc[2]);
            LLVMValueRef res = LLVMBuildFSub(llvm->builder, av, bv, get_name(runtime));
            LLVMBuildStore(llvm->builder, res, regs[bc[0]]);
            bc += 3;
            break;
        }
        case BC_OP_MUL: {
            LLVMValueRef av = load_reg_f(program, regs, bc[1]);
            LLVMValueRef bv = load_reg_f(program, regs, bc[2]);
            LLVMValueRef res = LLVMBuildFMul(llvm->builder, av, bv, get_name(runtime));
            LLVMBuildStore(llvm->builder, res, regs[bc[0]]);
            bc += 3;
            break;
        }
        case BC_OP_DIV: {
            LLVMValueRef av = load_reg_f(program, regs, bc[1]);
            LLVMValueRef bv = load_reg_f(program, regs, bc[2]);
            LLVMValueRef res = LLVMBuildFDiv(llvm->builder, av, bv, get_name(runtime));
            LLVMBuildStore(llvm->builder, res, regs[bc[0]]);
            bc += 3;
            break;
        }
        case BC_OP_POW: {
            LLVMValueRef av = load_reg_f(program, regs, bc[1]);
            LLVMValueRef bv = load_reg_f(program, regs, bc[2]);
            LLVMValueRef args[] = {av, bv};
            LLVMValueRef res = LLVMBuildCall(llvm->builder, llvm->pow_func, args, 2, get_name(runtime));
            LLVMBuildStore(llvm->builder, res, regs[bc[0]]);
            bc += 3;
            break;
        }
        case BC_OP_MOVF: {
            uint8_t d = *bc++;
            float f = *(float*)bc;
            bc += 4;
            LLVMBuildStore(llvm->builder, LLVMConstReal(LLVMFloatType(), f), regs[d]);
            break;
        }
        case BC_OP_SQRT: {
            LLVMValueRef v = load_reg_f(program, regs, bc[1]);
            LLVMValueRef args[] = {v};
            LLVMValueRef res = LLVMBuildCall(llvm->builder, llvm->sqrt_func, args, 1, get_name(runtime));
            LLVMBuildStore(llvm->builder, res, regs[bc[0]]);
            bc += 2;
            break;
        }
        case BC_OP_DELETE: {
            LLVMValueRef index = LLVMBuildLoad(llvm->builder, llvm->inv_index, get_name(runtime));
            LLVMValueRef args[] = {llvm->particles, index};
            LLVMBuildCall(llvm->builder, llvm->del_particle_func, args, 2, get_name(runtime));
            
            LLVMBuildBr(llvm->builder, end_block);
            block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
            break;
        }
        case BC_OP_LESS: {
            LLVMValueRef av = load_reg_f(program, regs, bc[1]);
            LLVMValueRef bv = load_reg_f(program, regs, bc[2]);
            LLVMValueRef res = LLVMBuildFCmp(llvm->builder, LLVMRealOLT, av, bv, get_name(runtime));
            store_reg_b(program, regs, bc[0], res);
            bc += 3;
            break;
        }
        case BC_OP_GREATER: {
            LLVMValueRef av = load_reg_f(program, regs, bc[1]);
            LLVMValueRef bv = load_reg_f(program, regs, bc[2]);
            LLVMValueRef res = LLVMBuildFCmp(llvm->builder, LLVMRealOGT, av, bv, get_name(runtime));
            store_reg_b(program, regs, bc[0], res);
            bc += 3;
            break;
        }
        case BC_OP_EQUAL: {
            LLVMValueRef av = load_reg_f(program, regs, bc[1]);
            LLVMValueRef bv = load_reg_f(program, regs, bc[2]);
            LLVMValueRef res = LLVMBuildFCmp(llvm->builder, LLVMRealOEQ, av, bv, get_name(runtime));
            store_reg_b(program, regs, bc[0], res);
            bc += 3;
            break;
        }
        case BC_OP_BOOL_AND: {
            LLVMValueRef av = load_reg_b(program, regs, bc[1]);
            LLVMValueRef bv = load_reg_b(program, regs, bc[2]);
            LLVMValueRef res = LLVMBuildAnd(llvm->builder, av, bv, get_name(runtime));
            store_reg_b(program, regs, bc[0], res);
            bc += 3;
            break;
        }
        case BC_OP_BOOL_OR: {
            LLVMValueRef av = load_reg_b(program, regs, bc[1]);
            LLVMValueRef bv = load_reg_b(program, regs, bc[2]);
            LLVMValueRef res = LLVMBuildOr(llvm->builder, av, bv, get_name(runtime));
            store_reg_b(program, regs, bc[0], res);
            bc += 3;
            break;
        }
        case BC_OP_BOOL_NOT: {
            LLVMValueRef av = load_reg_b(program, regs, bc[1]);
            LLVMValueRef bv = LLVMConstInt(LLVMIntType(1), 1, false);
            LLVMValueRef res = LLVMBuildXor(llvm->builder, av, bv, get_name(runtime));
            store_reg_b(program, regs, bc[0], res);
            bc += 2;
            break;
        }
        case BC_OP_SEL: {
            LLVMValueRef av = load_reg_f(program, regs, bc[1]);
            LLVMValueRef bv = load_reg_f(program, regs, bc[2]);
            LLVMValueRef cv = load_reg_b(program, regs, bc[3]);
            LLVMValueRef res = LLVMBuildSelect(llvm->builder, cv, av, bv, get_name(runtime));
            LLVMBuildStore(llvm->builder, res, regs[bc[0]]);
            bc += 4;
            break;
        }
        case BC_OP_COND_BEGIN: {
            LLVMValueRef cond = load_reg_b(program, regs, *bc++);
            uint32_t count = *(uint32_t*)bc;
            bc += 6;
            
            LLVMBasicBlockRef then_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
            LLVMBasicBlockRef then_block_end = to_ir(then_block, program, regs, bc, bc+count, end_block);
            
            LLVMBasicBlockRef else_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
            
            LLVMPositionBuilderAtEnd(llvm->builder, then_block_end);
            LLVMBuildBr(llvm->builder, else_block);
            
            LLVMPositionBuilderAtEnd(llvm->builder, block);
            LLVMBuildCondBr(llvm->builder, cond, then_block, else_block);
            
            block = else_block;
            
            bc += count;
            break;
        }
        case BC_OP_COND_END: {
            break;
        }
        case BC_OP_WHILE_BEGIN: {
            uint8_t c = *bc++;
            uint32_t cond_count = *(uint32_t*)bc;
            bc += 6;
            uint32_t body_count = *(uint32_t*)bc;
            bc += 6;
            
            LLVMBasicBlockRef cond_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
            LLVMBasicBlockRef cond_block_end = to_ir(cond_block, program, regs, bc, bc+cond_count, end_block);
            LLVMValueRef cond = load_reg_b(program, regs, c);
            
            LLVMBasicBlockRef body_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
            LLVMBasicBlockRef body_block_end = to_ir(body_block, program, regs, bc+cond_count, bc+cond_count+body_count, end_block);
            
            LLVMBasicBlockRef new_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
            
            LLVMPositionBuilderAtEnd(llvm->builder, body_block_end);
            LLVMBuildBr(llvm->builder, cond_block);
            
            LLVMPositionBuilderAtEnd(llvm->builder, block);
            LLVMBuildBr(llvm->builder, cond_block);
            
            LLVMPositionBuilderAtEnd(llvm->builder, cond_block_end);
            LLVMBuildCondBr(llvm->builder, cond, body_block, new_block);
            
            block = new_block;
            
            bc += cond_count + body_count;
            break;
        }
        case BC_OP_WHILE_END_COND: {
            break;
        }
        case BC_OP_WHILE_END: {
            break;
        }
        case BC_OP_END: {
            LLVMBuildBr(llvm->builder, end_block);
            block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
            break;
        }
        case BC_OP_EMIT: {
            LLVMValueRef particle_index = LLVMBuildCall(llvm->builder, llvm->spawn_particle_func,
                                                        &llvm->particles, 1, get_name(runtime));
            //TODO: Check if index < 0
            
            uint8_t count = *bc++;
            for (size_t i = 0; i < count; i++) {
                LLVMValueRef val = LLVMBuildLoad(llvm->builder, regs[*bc++], get_name(runtime));
                block = store_attr(val, i, llvm, runtime, particle_index);
            }
            break;
        }
        case BC_OP_RAND: {
            LLVMValueRef v = LLVMBuildCall(llvm->builder, llvm->randf_func, NULL, 0, get_name(runtime));
            LLVMBuildStore(llvm->builder, v, regs[bc[0]]);
            bc += 1;
            break;
        }
        case BC_OP_FLOOR: {
            LLVMValueRef v = LLVMBuildLoad(llvm->builder, regs[bc[1]], get_name(runtime));
            LLVMValueRef args[] = {v};
            LLVMValueRef res = LLVMBuildCall(llvm->builder, llvm->floor_func, args, 1, get_name(runtime));
            LLVMBuildStore(llvm->builder, res, regs[bc[0]]);
            bc += 2;
            break;
        }
        case BC_OP_MOV: {
            LLVMValueRef v = LLVMBuildLoad(llvm->builder, regs[bc[1]], get_name(runtime));
            LLVMBuildStore(llvm->builder, v, regs[bc[0]]);
            bc += 2;
            break;
        }
        }
    }
    
    return block;
}

static void create_body_block(program_t* program, LLVMBasicBlockRef body_block,
                              LLVMValueRef* regs, LLVMBasicBlockRef end_block) {
    runtime_t* runtime = program->runtime;
    llvm_prog_t* llvm = program->backend_internal;
    
    LLVMBasicBlockRef store_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
    
    LLVMPositionBuilderAtEnd(llvm->builder, body_block);
    
    LLVMValueRef inv_index;
    if (program->type == PROGRAM_TYPE_SIMULATION) {
        inv_index = LLVMBuildLoad(llvm->builder, llvm->inv_index, get_name(runtime));
        
        LLVMValueRef del_flag = LLVMBuildGEP(llvm->builder, llvm->del_flags,
                                             &inv_index, 1, get_name(runtime));
        LLVMValueRef cmp_res = LLVMBuildICmp(llvm->builder, LLVMIntNE,
                                             LLVMBuildLoad(llvm->builder, del_flag, get_name(runtime)),
                                             LLVMConstInt(LLVMInt8Type(), 0, false),
                                             get_name(runtime));
        
        LLVMBasicBlockRef new_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
        LLVMBuildCondBr(llvm->builder, cmp_res, end_block, new_block);
        body_block = new_block;
    }
    
    //Load attributes
    LLVMPositionBuilderAtEnd(llvm->builder, body_block);
    
    if (program->type == PROGRAM_TYPE_SIMULATION)
        for (size_t i = 0; i < program->attribute_count; i++)
            body_block = load_attr(regs[program->attribute_load_regs[i]], i, llvm,
                                   runtime, inv_index);
    
    //Load uniforms
    for (size_t i = 0; i < program->uniform_count; i++) {
        LLVMValueRef index = LLVMConstInt(LLVMInt32Type(), i, false);
        
        LLVMValueRef val_ptr = LLVMBuildGEP(llvm->builder, llvm->uniforms,
                                            &index, 1, get_name(runtime));
        LLVMValueRef val = LLVMBuildLoad(llvm->builder, val_ptr, get_name(runtime));
        LLVMBuildStore(llvm->builder, val, regs[program->uniform_regs[i]]);
    }
    
    uint8_t* bc = program->bc;
    uint8_t* end = bc + program->bc_size;
    body_block = to_ir(body_block, program, regs, bc, end, store_block);
    
    LLVMPositionBuilderAtEnd(llvm->builder, body_block);
    LLVMBuildBr(llvm->builder, store_block);
    
    //Store attributes
    LLVMPositionBuilderAtEnd(llvm->builder, store_block);
    
    if (program->type == PROGRAM_TYPE_SIMULATION) {
        for (size_t i = 0; i < program->attribute_count; i++) {
            LLVMValueRef val = LLVMBuildLoad(llvm->builder, regs[program->attribute_store_regs[i]], get_name(runtime));
            body_block = store_attr(val, i, llvm, runtime, inv_index);
        }
    }
    
    LLVMBuildBr(llvm->builder, end_block);
}

static bool create_module(program_t* program) {
    runtime_t* runtime = program->runtime;
    llvm_prog_t* llvm = program->backend_internal;
    
    llvm->module = LLVMModuleCreateWithName(get_name(program->runtime));
    llvm->builder = LLVMCreateBuilder();
    
    llvm->floor_func = get_intrinsic1(llvm->module, "llvm.floor.f32");
    llvm->sqrt_func = get_intrinsic1(llvm->module, "llvm.sqrt.f32");
    llvm->pow_func = get_intrinsic2(llvm->module, "llvm.pow.f32");
    llvm->del_particle_func = get_del_particle_func(llvm->module);
    llvm->spawn_particle_func = get_spawn_particle_func(llvm->module);
    llvm->randf_func = get_randf_func(llvm->module);
    
    if (program->type == PROGRAM_TYPE_SIMULATION) {
        LLVMTypeRef param_types[7] = {LLVMInt32Type(), //int begin
                                      LLVMInt32Type(), //int end
                                      LLVMPointerType(LLVMFloatType(), 0), //float* uniforms
                                      LLVMPointerType(LLVMPointerType(LLVMInt32Type(), 0), 0), //int**, attr_data //presorted
                                      LLVMPointerType(LLVMInt32Type(), 0), //int* attr_dtypes //presorted
                                      LLVMPointerType(LLVMIntType(8), 0), //int8* deleted_flags
                                      LLVMPointerType(LLVMInt32Type(), 0)}; //particles_t* particles
        LLVMTypeRef ret_type = LLVMFunctionType(LLVMInt32Type(), param_types, 7, 0);
        llvm->main_func = LLVMAddFunction(llvm->module, get_name(runtime), ret_type);
        
        LLVMValueRef begin = LLVMGetParam(llvm->main_func, 0);
        LLVMValueRef end = LLVMGetParam(llvm->main_func, 1);
        llvm->uniforms = LLVMGetParam(llvm->main_func, 2);
        llvm->attr_data = LLVMGetParam(llvm->main_func, 3);
        llvm->attr_dtypes = LLVMGetParam(llvm->main_func, 4);
        llvm->del_flags = LLVMGetParam(llvm->main_func, 5);
        llvm->particles = LLVMGetParam(llvm->main_func, 6);
        
        LLVMBasicBlockRef init_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
        LLVMBasicBlockRef cond_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
        LLVMBasicBlockRef body_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
        LLVMBasicBlockRef end_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
        LLVMBasicBlockRef end_body_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
        
        //Initialization block
        LLVMPositionBuilderAtEnd(llvm->builder, init_block);
        
        LLVMValueRef regs[256];
        for (size_t i = 0; i < 256; i++)
            regs[i] = LLVMBuildAlloca(llvm->builder, LLVMFloatType(), get_reg_name(i));
        
        LLVMValueRef i = LLVMBuildAlloca(llvm->builder, LLVMInt32Type(), "i");
        LLVMBuildStore(llvm->builder, begin, i);
        llvm->inv_index = i;
        
        LLVMBuildBr(llvm->builder, cond_block);
        
        //Condition block
        LLVMPositionBuilderAtEnd(llvm->builder, cond_block);
        
        LLVMValueRef cmp_res = LLVMBuildICmp(llvm->builder, LLVMIntULT,
                                             LLVMBuildLoad(llvm->builder, i, get_name(runtime)),
                                             end,
                                             get_name(runtime));
        
        LLVMBuildCondBr(llvm->builder, cmp_res, body_block, end_block);
        
        //Body block
        create_body_block(program, body_block, regs, end_body_block);
        
        //End body block
        LLVMPositionBuilderAtEnd(llvm->builder, end_body_block);
        LLVMValueRef a = LLVMBuildLoad(llvm->builder, i, get_name(runtime));
        LLVMValueRef b = LLVMConstInt(LLVMInt32Type(), 1, false);
        LLVMBuildStore(llvm->builder, LLVMBuildAdd(llvm->builder, a, b, get_name(runtime)), i);
        LLVMBuildBr(llvm->builder, cond_block);
        
        //End block
        LLVMPositionBuilderAtEnd(llvm->builder, end_block);
        LLVMBuildRet(llvm->builder, LLVMConstInt(LLVMInt32Type(), 0, false));
    } else {
        LLVMTypeRef param_types[4] = {LLVMPointerType(LLVMFloatType(), 0), //float* uniforms
                                      LLVMPointerType(LLVMInt32Type(), 0), //particles_t* particles
                                      LLVMPointerType(LLVMPointerType(LLVMInt32Type(), 0), 0), //int**, attr_data //presorted
                                      LLVMPointerType(LLVMInt32Type(), 0)}; //int* attr_dtypes //presorted
        LLVMTypeRef ret_type = LLVMFunctionType(LLVMInt32Type(), param_types, 4, 0);
        llvm->main_func = LLVMAddFunction(llvm->module, get_name(runtime), ret_type);
        
        llvm->uniforms = LLVMGetParam(llvm->main_func, 0);
        llvm->particles = LLVMGetParam(llvm->main_func, 1);
        llvm->attr_data = LLVMGetParam(llvm->main_func, 2);
        llvm->attr_dtypes = LLVMGetParam(llvm->main_func, 3);
        
        LLVMBasicBlockRef block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
        LLVMBasicBlockRef end_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
        
        LLVMPositionBuilderAtEnd(llvm->builder, block);
        LLVMValueRef regs[256];
        for (size_t i = 0; i < 256; i++)
            regs[i] = LLVMBuildAlloca(llvm->builder, LLVMFloatType(), get_reg_name(i));
        create_body_block(program, block, regs, end_block);
        
        LLVMPositionBuilderAtEnd(llvm->builder, end_block);
        LLVMBuildRet(llvm->builder, LLVMConstInt(LLVMInt32Type(), 0, false));
    }
    
    char* error = NULL;
    LLVMVerifyModule(llvm->module, LLVMAbortProcessAction, &error);
    LLVMDisposeMessage(error);
    
    //Optimize
    if (LLVMWriteBitcodeToFile(llvm->module, "test.bc"))
        printf("Error writing bitcode\n");
    LLVMDisposeBuilder(llvm->builder);
    llvm->opt_module = LLVMModuleCreateWithName(get_name(program->runtime));
    system("opt test.bc -O3 > test_opt.bc");
    LLVMMemoryBufferRef buf;
    LLVMCreateMemoryBufferWithContentsOfFile("test_opt.bc", &buf, NULL);
    LLVMParseBitcode(buf, &llvm->opt_module, NULL);
    LLVMDisposeMemoryBuffer(buf);
    system("rm test_opt.bc");
    system("rm test.bc");
    
    if (LLVMWriteBitcodeToFile(llvm->opt_module, "test.bc"))
        printf("Error writing bitcode\n");
    
    error = NULL;
    if (LLVMCreateJITCompilerForModule(&llvm->exec_engine, llvm->opt_module, 3, &error)) {
        char new_error[1024];
        strncpy(new_error, error, sizeof(new_error));
        LLVMDisposeMessage(error);
        return set_error(program->runtime, new_error);
    }
    
    LLVMAddGlobalMapping(llvm->exec_engine, llvm->del_particle_func, &delete_particle);
    LLVMAddGlobalMapping(llvm->exec_engine, llvm->spawn_particle_func, &spawn_particle);
    LLVMAddGlobalMapping(llvm->exec_engine, llvm->randf_func, &randf);
    
    //LLVMTargetMachineEmitToFile(LLVMGetExecutionEngineTargetMachine(llvm->exec_engine), llvm->module, "something.s", LLVMAssemblyFile, NULL);
    
    return true;
}

static bool llvm_create(runtime_t* runtime) {
    llvm_backend_t* backend = malloc(sizeof(llvm_backend_t));
    if (!backend)
        return set_error(runtime, "Failed to allocate internal LLVM backend data");
    backend->next_name = 0;
    runtime->backend.internal = backend;
    
    //TODO: It's likely that not all of these are needed
    /*LLVMInitializeCore(LLVMGetGlobalPassRegistry());    
    LLVMInitializeScalarOpts(LLVMGetGlobalPassRegistry());    
    LLVMInitializeObjCARCOpts(LLVMGetGlobalPassRegistry());    
    LLVMInitializeVectorization(LLVMGetGlobalPassRegistry());
    LLVMInitializeIPO(LLVMGetGlobalPassRegistry());
    LLVMInitializeAnalysis(LLVMGetGlobalPassRegistry());
    LLVMInitializeTransformUtils(LLVMGetGlobalPassRegistry());
    LLVMInitializeInstCombine(LLVMGetGlobalPassRegistry());
    LLVMInitializeInstrumentation(LLVMGetGlobalPassRegistry());
    LLVMInitializeTarget(LLVMGetGlobalPassRegistry());*/
    //LLVMInitializeCodeGenPreparePass(LLVMGetGlobalPassRegistry());
    //LLVMInitializeAtomicExpandPass(LLVMGetGlobalPassRegistry());
    //LLVMInitializeRewriteSymbolsPass(LLVMGetGlobalPassRegistry());
    //LLVMInitializeWinEHPreparePass(LLVMGetGlobalPassRegistry());
    //LLVMInitializeDwarfEHPreparePass(LLVMGetGlobalPassRegistry());
    //LLVMInitializeSafeStackPass(LLVMGetGlobalPassRegistry());
    //LLVMInitializeSjLjEHPreparePass(LLVMGetGlobalPassRegistry());
    
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();
    
    return true;
}

static bool llvm_destroy(runtime_t* runtime) {
    free(runtime->backend.internal);
    return true;
}

static bool llvm_create_program(program_t* program) {
    llvm_prog_t* prog = malloc(sizeof(llvm_prog_t));
    if (!prog)
        return set_error(program->runtime, "Failed to allocate internal LLVM program data");
    program->backend_internal = prog;
    
    return create_module(program);
}

static bool llvm_destroy_program(program_t* program) {
    llvm_prog_t* prog = program->backend_internal;
    LLVMDisposeModule(prog->module);
    LLVMDisposeExecutionEngine(prog->exec_engine);
    free(prog);
    return true;
}

typedef struct thread_data_t {
    system_t* system;
    sim_func_t func;
} thread_data_t;

static void* thread_func(size_t begin, size_t count, void* userdata) {
    thread_data_t* data = userdata;
    system_t* system = data->system;
    program_t* prog = system->sim_program;
    
    particles_t* particles = system->particles;
    float* uniforms = system->sim_uniforms;
    void* attr_data[256];
    int attr_dtypes[256];
    for (size_t i = 0; i < prog->attribute_count; i++) {
        uint8_t index = system->sim_attribute_indices[i];
        attr_data[i] = system->particles->attributes[index];
        attr_dtypes[i] = (int)system->particles->attribute_dtypes[index];
    }
    uint8_t* deleted_flags = particles->deleted_flags;
    
    data->func(begin, begin+count, uniforms, attr_data, attr_dtypes, deleted_flags, particles);
    
    return (void*)true;
}

static bool llvm_simulate_system(system_t* system) {
    if (system->emit_program) {
        program_t* prog = system->emit_program;
        llvm_prog_t* llvm = prog->backend_internal;
        
        void* attr_data[256];
        int attr_dtypes[256];
        for (size_t i = 0; i < prog->attribute_count; i++) {
            uint8_t index = system->emit_attribute_indices[i];
            attr_data[i] = system->particles->attributes[index];
            attr_dtypes[i] = (int)system->particles->attribute_dtypes[index];
        }
        
        uint64_t func_ptr = LLVMGetFunctionAddress(llvm->exec_engine, LLVMGetValueName(llvm->main_func));
        assert(func_ptr);
        ((emit_func_t)func_ptr)(system->emit_uniforms, system->particles, attr_data, attr_dtypes);
    }
    
    if (system->sim_program) {
        llvm_prog_t* llvm = system->sim_program->backend_internal;
        
        thread_data_t data;
        data.system = system;
        data.func = (sim_func_t)LLVMGetFunctionAddress(llvm->exec_engine, LLVMGetValueName(llvm->main_func));
        assert(data.func);
        
        threading_t* threading = &system->runtime->threading;
        thread_run_t run = (thread_run_t){.func = &thread_func,
                                          .count=system->particles->pool_size,
                                          .data=&data};
        thread_res_t res = threading_run(threading, run);
        if (!res.success) {
            strncpy(system->runtime->error, threading->error, sizeof(system->runtime->error)-1);
            return false;
        }
        
        for (size_t i = 0; i < res.count; i++)
            if (!res.res[i]) return false;
        /*program_t* prog = system->sim_program;
        llvm_prog_t* llvm = prog->backend_internal;
        
        particles_t* particles = system->particles;
        float* uniforms = system->sim_uniforms;
        void* attr_data[256];
        int attr_dtypes[256];
        for (size_t i = 0; i < prog->attribute_count; i++) {
            uint8_t index = system->sim_attribute_indices[i];
            attr_data[i] = system->particles->attributes[index];
            attr_dtypes[i] = (int)system->particles->attribute_dtypes[index];
        }
        uint8_t* deleted_flags = particles->deleted_flags;
        
        uint64_t func_ptr = LLVMGetFunctionAddress(llvm->exec_engine, LLVMGetValueName(llvm->main_func));
        assert(func_ptr);
        
        ((sim_func_t)func_ptr)(0, particles->pool_size, uniforms, attr_data, attr_dtypes, deleted_flags, particles);*/
    }
    
    return true;
}

bool llvm_backend(backend_t* backend) {
#ifdef LLVM_NATIVE_ARCH
    backend->create = &llvm_create;
    backend->destroy = &llvm_destroy;
    backend->create_program = &llvm_create_program;
    backend->destroy_program = &llvm_destroy_program;
    backend->simulate_system = &llvm_simulate_system;
    return false;
#else
    return false;
#endif
}

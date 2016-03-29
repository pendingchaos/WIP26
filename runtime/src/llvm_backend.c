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
    LLVMBuilderRef builder;
    LLVMExecutionEngineRef exec_engine;
    LLVMValueRef main_func;
    LLVMValueRef floor_func;
    LLVMValueRef sqrt_func;
    LLVMValueRef pow_func;
    LLVMValueRef inv_index;
    LLVMValueRef particles;
    LLVMValueRef attr_data;
    LLVMValueRef attr_dtypes;
    LLVMValueRef uniforms;
    LLVMValueRef del_particle_func;
} llvm_prog_t;

typedef struct llvm_backend_t {
    size_t next_name;
} llvm_backend_t;

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
            uint8_t count = *bc++;
            bc += count;
            break;
        }
        case BC_OP_RAND: {
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
    
    LLVMPositionBuilderAtEnd(llvm->builder, body_block);
    
    LLVMBasicBlockRef store_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
    
    //Load attributes
    for (size_t i = 0; i < program->attribute_count; i++) {
        LLVMValueRef index = LLVMConstInt(LLVMInt32Type(), i, false);
        
        //TODO: Data types
        LLVMValueRef floats = LLVMBuildInBoundsGEP(llvm->builder, llvm->attr_data,
                                                   &index, 1, get_name(runtime));
        floats = LLVMBuildLoad(llvm->builder, floats, get_name(runtime));
        floats = LLVMBuildBitCast(llvm->builder, floats,
                                  LLVMPointerType(LLVMFloatType(), 0),
                                  get_name(runtime));
        LLVMValueRef val_ptr = LLVMBuildGEP(llvm->builder, floats,
                                            &index, 1, get_name(runtime));
        LLVMValueRef val = LLVMBuildLoad(llvm->builder, val_ptr, get_name(runtime));
        
        LLVMBuildStore(llvm->builder, val, regs[program->attribute_load_regs[i]]);
    }
    
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
    
    for (size_t i = 0; i < program->attribute_count; i++) {
        LLVMValueRef index = LLVMConstInt(LLVMInt32Type(), i, false);
        
        //TODO: Data types
        LLVMValueRef floats = LLVMBuildInBoundsGEP(llvm->builder, llvm->attr_data,
                                                   &index, 1, get_name(runtime));
        floats = LLVMBuildLoad(llvm->builder, floats, get_name(runtime));
        floats = LLVMBuildBitCast(llvm->builder, floats,
                                  LLVMPointerType(LLVMFloatType(), 0),
                                  get_name(runtime));
        LLVMValueRef dest_ptr = LLVMBuildGEP(llvm->builder, floats,
                                             &index, 1, get_name(runtime));
        
        LLVMValueRef val = LLVMBuildLoad(llvm->builder, regs[program->attribute_store_regs[i]], get_name(runtime));
        
        LLVMBuildStore(llvm->builder, val, dest_ptr);
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
    
    LLVMTypeRef param_types[8] = {LLVMInt32Type(), //int begin
                                  LLVMInt32Type(), //int count
                                  LLVMPointerType(LLVMFloatType(), 0), //int* uniforms
                                  LLVMPointerType(LLVMPointerType(LLVMInt32Type(), 0), 0), //int**, attr_data //presorted
                                  LLVMPointerType(LLVMInt32Type(), 0), //int* attr_dtypes //presorted
                                  LLVMPointerType(LLVMIntType(8), 0), //int8* deleted_flags
                                  LLVMPointerType(LLVMInt32Type(), 0)}; //particles_t* particles
    LLVMTypeRef ret_type = LLVMFunctionType(LLVMPointerType(LLVMFloatType(), 0), param_types, 8, 0);
    llvm->main_func = LLVMAddFunction(llvm->module, get_name(runtime), ret_type);
    
    LLVMValueRef begin = LLVMGetParam(llvm->main_func, 0);
    LLVMValueRef count = LLVMGetParam(llvm->main_func, 1);
    llvm->uniforms = LLVMGetParam(llvm->main_func, 2);
    llvm->attr_data = LLVMGetParam(llvm->main_func, 3);
    llvm->attr_dtypes = LLVMGetParam(llvm->main_func, 4);
    llvm->particles = LLVMGetParam(llvm->main_func, 5);
    
    LLVMBasicBlockRef init_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
    LLVMBasicBlockRef cond_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
    LLVMBasicBlockRef body_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
    LLVMBasicBlockRef end_block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
    
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
    
    LLVMValueRef cmp_res = LLVMBuildICmp(llvm->builder, LLVMIntULE, 
                                         LLVMBuildLoad(llvm->builder, i, get_name(runtime)),
                                         count,
                                         get_name(runtime));
    
    LLVMBuildCondBr(llvm->builder, cmp_res, body_block, end_block);
    
    //Body block
    create_body_block(program, body_block, regs, cond_block);
    
    //End block
    LLVMPositionBuilderAtEnd(llvm->builder, end_block);
    //LLVMBuildRet(llvm->builder, LLVMConstReal(LLVMFloatType(), 0.0));
    LLVMBuildRet(llvm->builder, regs[0]);
    
    if (LLVMWriteBitcodeToFile(llvm->module, "test.bc"))
        printf("Error writing bitcode\n");
    
    char* error = NULL;
    LLVMVerifyModule(llvm->module, LLVMAbortProcessAction, &error);
    LLVMDisposeMessage(error);
    
    error = NULL;
    if (LLVMCreateExecutionEngineForModule(&llvm->exec_engine, llvm->module, &error)) {
        char new_error[1024];
        strncpy(new_error, error, sizeof(new_error));
        LLVMDisposeMessage(error);
        return set_error(program->runtime, new_error);
    }
    
    return true;
}

static bool llvm_create(runtime_t* runtime) {
    llvm_backend_t* backend = malloc(sizeof(llvm_backend_t));
    if (!backend)
        return set_error(runtime, "Failed to allocate internal LLVM backend data");
    backend->next_name = 0;
    runtime->backend.internal = backend;
    
    LLVMInitializeNativeTarget();
    
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
    LLVMDisposeExecutionEngine(prog->exec_engine);
    LLVMDisposeBuilder(prog->builder);
    free(prog);
    return true;
}

static bool llvm_simulate_system(system_t* system) {
    return true;
}

bool llvm_backend(backend_t* backend) {
    backend->create = &llvm_create;
    backend->destroy = &llvm_destroy;
    backend->create_program = &llvm_create_program;
    backend->destroy_program = &llvm_destroy_program;
    backend->simulate_system = &llvm_simulate_system;
    return true;
}

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
} llvm_prog_t;

typedef struct llvm_backend_t {
    size_t next_name;
} llvm_backend_t;

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

static bool create_module(program_t* program) {
    runtime_t* runtime = program->runtime;
    
    llvm_prog_t* llvm = program->backend_internal;
    llvm->module = LLVMModuleCreateWithName(get_name(program->runtime));
    llvm->builder = LLVMCreateBuilder();
    
    LLVMTypeRef param_types[] = {};
    LLVMTypeRef ret_type = LLVMFunctionType(LLVMPointerType(LLVMFloatType(), 0), param_types,
                                            sizeof(param_types)/sizeof(LLVMTypeRef), 0);
    llvm->main_func = LLVMAddFunction(llvm->module, get_name(runtime), ret_type);
    
    LLVMValueRef floor_func = get_intrinsic1(llvm->module, "llvm.floor.f32");
    LLVMValueRef sqrt_func = get_intrinsic1(llvm->module, "llvm.sqrt.f32");
    LLVMValueRef pow_func = get_intrinsic2(llvm->module, "llvm.pow.f32");
    
    LLVMBasicBlockRef block = LLVMAppendBasicBlock(llvm->main_func, get_name(runtime));
    LLVMPositionBuilderAtEnd(llvm->builder, block);
    
    LLVMValueRef regs[256];
    for (size_t i = 0; i < 256; i++)
        regs[i] = LLVMBuildAlloca(llvm->builder, LLVMFloatType(), get_name(runtime));
    
    uint8_t* bc = program->bc;
    uint8_t* end = bc + program->bc_size;
    while (bc != end) {
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
            LLVMValueRef res = LLVMBuildCall(llvm->builder, pow_func, args, 2, get_name(runtime));
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
            LLVMValueRef res = LLVMBuildCall(llvm->builder, sqrt_func, args, 1, get_name(runtime));
            LLVMBuildStore(llvm->builder, res, regs[bc[0]]);
            bc += 2;
            break;
        }
        case BC_OP_DELETE: {
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
            break;
        }
        case BC_OP_COND_END: {
            break;
        }
        case BC_OP_WHILE_BEGIN: {
            break;
        }
        case BC_OP_WHILE_END_COND: {
            break;
        }
        case BC_OP_WHILE_END: {
            break;
        }
        case BC_OP_END: {
            break;
        }
        case BC_OP_EMIT: {
            break;
        }
        case BC_OP_RAND: {
            break;
        }
        case BC_OP_FLOOR: {
            LLVMValueRef v = LLVMBuildLoad(llvm->builder, regs[bc[1]], get_name(runtime));
            LLVMValueRef args[] = {v};
            LLVMValueRef res = LLVMBuildCall(llvm->builder, floor_func, args, 1, get_name(runtime));
            LLVMBuildStore(llvm->builder, res, regs[bc[0]]);
            bc += 2;
            break;
        }
        case BC_OP_MOV: {
            LLVMBuildStore(llvm->builder, regs[bc[1]], regs[bc[0]]);
            bc += 2;
            break;
        }
        }
    }
    
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

#ifdef VM_COMPUTED_GOTO
#undef DISPATCH
#endif
#undef BEGIN_CASE
#undef END_CASE

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

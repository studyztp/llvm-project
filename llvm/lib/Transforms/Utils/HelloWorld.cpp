//===-- HelloWorld.cpp - Example Transformations --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//


/*
1. each basic block has a const global variable that stores its instruction
    length
2. every time a basic block is executed, it adds its instruction count to 
    the global instruction counter atomically
3. every function has a const global variable that stores the name of the 
    function
4. at the entry of every targeted function, it atomically add its name to a
    global set
5. when the global instruction counter reaches a certain threshold, it prints
    the name of the functions in the global set and the total instruction count
    then resets the global instruction counter.
6. the global set is reset after the print
*/

#include "llvm/Transforms/Utils/HelloWorld.h"

namespace llvm {

bool HelloWorldPass::emptyFunction(Function &F) {
  int count = 0;
  for (auto &block : F) {
    for (auto &inst : block) {
      count++;
    }
  }
  return count == 0;
}

GlobalVariable* HelloWorldPass::createGlobalUint64Array(
  Module& M,
  std::string variableName,
  uint64_t size) 
{
  ArrayType* arrayTy = ArrayType::get(IntegerType::get(M.getContext(), 64), 
                                                                        size);
  PointerType* pointerTy = PointerType::get(arrayTy, 0);

  GlobalVariable* gvar_array_ptr = new GlobalVariable(
    M,
    arrayTy,
    false,
    GlobalValue::ExternalLinkage,
    0,
    variableName
  );
  ConstantAggregateZero* const_array = ConstantAggregateZero::get(arrayTy);
  gvar_array_ptr->setInitializer(const_array);

  return gvar_array_ptr;
}

Function* HelloWorldPass::createInstrumentationFunction(Module &M) {
  Type* VoidTy = Type::getVoidTy(M.getContext());
  Type* Int64Ty = Type::getInt64Ty(M.getContext());
  Type* Int32Ty = Type::getInt32Ty(M.getContext());
  FunctionType* FTy = FunctionType::get(VoidTy, {Int32Ty, Int32Ty, Int64Ty}, false);
  Function* F = Function::Create(
    FTy,
    GlobalValue::ExternalLinkage,
    "instrumentationFunction",
    M
  );

  BasicBlock*BB = BasicBlock::Create(M.getContext(), "entry", F);
  IRBuilder<> builder(M.getContext());
  builder.SetInsertPoint(BB);
  Function::arg_iterator args = F->arg_begin();
  Value* basicBlockId = &*args++;
  Value* functionId = &*args++;
  Value* basicBlockInstCount = &*args++;

  Value* counter = M.getGlobalVariable("instructionCounter");
  if (!counter) {
    errs() << "Global variable instructionCounter not found\n";
  }

  Function* IncreaseCounterFunction = M.getFunction("increase_counter");
  if (!IncreaseCounterFunction) {
    errs() << "Function increase_counter not found\n";
  }

  Function* PrintIntFunction = M.getFunction("print_int");
  if (!PrintIntFunction) {
    errs() << "Function print_int not found\n";
  }

  Value* variableName = 
          builder.CreateGlobalStringPtr("global counter");

  builder.CreateCall(IncreaseCounterFunction, {counter, basicBlockInstCount});
  Value* loadCounter = builder.CreateLoad(Int64Ty, counter);
  builder.CreateCall(PrintIntFunction, {variableName, loadCounter});
  builder.CreateRetVoid();

  return F;
}


PreservedAnalyses HelloWorldPass::run(Module &M, ModuleAnalysisManager &AM) 
{
  IRBuilder<> builder(M.getContext());
  uint32_t functionId = 0;
  uint32_t basicBlockId = 0;

  // Create a global variable to store the instruction count
  Type *Int64Ty = Type::getInt64Ty(M.getContext());
  GlobalVariable* instructionCounter = new GlobalVariable(
    M,
    Int64Ty,
    false,
    GlobalValue::ExternalLinkage,
    ConstantInt::get(Int64Ty, 0),
    "instructionCounter"
  );
  if (!instructionCounter) {
    errs() << "Global variable instructionCounter not found\n";
  }

  // Get helper function
  Function* instrumentationFunction = createInstrumentationFunction(M);

  for (auto& function : M.getFunctionList()) {
    if (emptyFunction(function) || function.isDeclaration()) 
    {
      continue;
    }
    if (std::find(exclude_functions.begin(), exclude_functions.end(), function.getName()) != exclude_functions.end()) {
      errs() << "Skipping function: " << function.getName() << "\n";
      continue;
    }
    errs() << functionId << ": Function: " << function.getName() << "\n";
    functionId++;
    for (auto& block : function) {
      // errs() << "\t" << basicBlockId << ": BasicBlock: " << block.getName() << "\n";
      basicBlockId++;

      // Create a function call to add the instruction count to the global
      builder.SetInsertPoint(block.getFirstInsertionPt());
      Function* PrintHiFunction = M.getFunction("print_hi");
      if (!PrintHiFunction) {
        errs() << "Function print_hi not found\n";
      }
      // Value *counter = M.getGlobalVariable("instructionCounter");

      // Constant *BBinst = ConstantInt::get(Int64Ty, block.size());

      // builder.CreateCall(IncreaseCounterFunction, {counter, BBinst});

      // Value* format = builder.CreateGlobalStringPtr("%d \n");
      // Value* counterValue = builder.CreateLoad(Int64Ty,counter);
      // builder.CreateCall(PrintFunction, {format, counterValue});
      CallInst * addedCall = builder.CreateCall(instrumentationFunction,
          {ConstantInt::get(Type::getInt32Ty(M.getContext()), basicBlockId),
          ConstantInt::get(Type::getInt32Ty(M.getContext()), functionId),
          ConstantInt::get(Type::getInt64Ty(M.getContext()), block.size())});
      InlineFunctionInfo ifi;
      auto res = InlineFunction(*addedCall, ifi);
      if (!res.isSuccess()) {
        errs() << "Failed to inline print_hi\n";
      }
    }
  }

  return PreservedAnalyses::all();
}

} // end llvm namespace

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

void createResetArray(Module &M) {
  // Create the resetArray function
  std::vector<Type*> resetArgs{PointerType::getUnqual(Type::getInt32Ty(M.getContext())), Type::getInt64Ty(M.getContext())};
  FunctionType *resetType = FunctionType::get(Type::getVoidTy(M.getContext()), resetArgs, false);
  Function *resetArray = Function::Create(resetType, Function::ExternalLinkage, "resetArray", &M);

  // Create the entry block
  BasicBlock *entry = BasicBlock::Create(M.getContext(), "entry", resetArray);
  IRBuilder<> builder(entry);

  // Get the arguments
  Value *array = resetArray->getArg(0);
  Value *size = resetArray->getArg(1);

  // Create a loop to iterate over the elements of the array
  BasicBlock *loopBB = BasicBlock::Create(M.getContext(), "loop", resetArray);
  BasicBlock *exitBB = BasicBlock::Create(M.getContext(), "exit", resetArray);
  Value *i = builder.CreateAlloca(Type::getInt64Ty(M.getContext()));
  builder.CreateStore(ConstantInt::get(Type::getInt64Ty(M.getContext()), 0), i);
  builder.CreateBr(loopBB);

  // Loop block
  builder.SetInsertPoint(loopBB);
  Value *index = builder.CreateLoad(i);
  Value *element = builder.CreateLoad(builder.CreateGEP(array, index));
  // Reset the element
  builder.CreateStore(ConstantInt::get(Type::getInt32Ty(M.getContext()), 0), element);
  // Increment the index
  Value *nextIndex = builder.CreateAdd(index, ConstantInt::get(Type::getInt64Ty(M.getContext()), 1));
  builder.CreateStore(nextIndex, i);
  // Check if we've reached the end of the array
  Value *endCondition = builder.CreateICmpEQ(nextIndex, size);
  builder.CreateCondBr(endCondition, exitBB, loopBB);

  // Exit block
  builder.SetInsertPoint(exitBB);
  builder.CreateRetVoid();

}

void createPrintArray(Module &M) {
  // Get or create the printf function
  std::vector<Type*> printfArgs{PointerType::getUnqual(Type::getInt8Ty(M.getContext()))};
  FunctionType *printfType = FunctionType::get(Type::getInt32Ty(M.getContext()), printfArgs, true);
  FunctionCallee printfFunc = M.getOrInsertFunction("printf", printfType);

  // Create the printArray function
  std::vector<Type*> printArgs{PointerType::getUnqual(Type::getInt32Ty(M.getContext())), Type::getInt64Ty(M.getContext())};
  FunctionType *printType = FunctionType::get(Type::getVoidTy(M.getContext()), printArgs, false);
  Function *printArray = Function::Create(printType, Function::ExternalLinkage, "printArray", &M);

  // Create the entry block
  BasicBlock *entry = BasicBlock::Create(M.getContext(), "entry", printArray);
  IRBuilder<> builder(entry);

  // Get the arguments
  Value *array = printArray->getArg(0);
  Value *size = printArray->getArg(1);

  // Create a loop to iterate over the elements of the array
  BasicBlock *loopBB = BasicBlock::Create(M.getContext(), "loop", printArray);
  BasicBlock *exitBB = BasicBlock::Create(M.getContext(), "exit", printArray);
  Value *i = builder.CreateAlloca(Type::getInt64Ty(M.getContext()));
  builder.CreateStore(ConstantInt::get(Type::getInt64Ty(M.getContext()), 0), i);
  builder.CreateBr(loopBB);

  // Loop block
  builder.SetInsertPoint(loopBB);
  Value *index = builder.CreateLoad(i);
  Value *element = builder.CreateLoad(builder.CreateGEP(array, index));
  // Print the element
  Value *formatStr = builder.CreateGlobalStringPtr("%d : %d\n");
  builder.CreateCall(printfFunc, {formatStr, index, element});

  // Increment the index
  Value *nextIndex = builder.CreateAdd(index, ConstantInt::get(Type::getInt64Ty(M.getContext()), 1));
  builder.CreateStore(nextIndex, i);
  // Check if we've reached the end of the array
  Value *endCondition = builder.CreateICmpEQ(nextIndex, size);
  builder.CreateCondBr(endCondition, exitBB, loopBB);

  // Exit block
  builder.SetInsertPoint(exitBB);
  builder.CreateRetVoid();

}

void creatCheckInstCount(Module &M) {
  Function *checkInstCount = Function::Create(
    FunctionType::get(Type::getVoidTy(M.getContext()), false),
    GlobalValue::ExternalLinkage,
    "checkInstCount",
    &M
  );

  BasicBlock *entry = BasicBlock::Create(M.getContext(), "entry", checkInstCount);

  IRBuilder<> BBuilder(entry);

  Value *instCount = M.getGlobalVariable("global_inst_count");
  Value *numFuncs = M.getGlobalVariable("num_functions");
  Value *calledFuncs = M.getGlobalVariable("called_functions");

  Value *instCountVal = BBuilder.CreateLoad(instCount);
  Value *cmp = BBuilder.CreateICmpUGE(instCountVal, ConstantInt::get(Type::getInt32Ty(M.getContext()), threshold));

  // Create blocks for the then and else cases
  BasicBlock *thenBB = BasicBlock::Create(M.getContext(), "then", checkInstCount);
  BasicBlock *elseBB = BasicBlock::Create(M.getContext(), "else", checkInstCount);

  // Conditional branch
  BBuilder.CreateCondBr(cmp, thenBB, elseBB);

  BBuilder.SetInsertPoint(thenBB);

  Function *printFunc = M.getFunction("printArray");
  Function *resetFunc = M.getFunction("resetArray");

  if (!printFunc) {
    errs() << "printBitmap function not found\n";
  }

  BBuilder.CreateCall(printFunc, {calledFuncs, numFuncs});
  BBuilder.CreateCall(resetFunc, {calledFuncs, numFuncs});
  // Reset the global instruction count
  BBuilder.CreateStore(ConstantInt::get(Type::getInt32Ty(M.getContext()), 0), instCount);
  BBuilder.CreateBr(elseBB);

  // Else block
  BBuilder.SetInsertPoint(elseBB);
  // Return
  BBuilder.CreateRetVoid();
}

PreservedAnalyses HelloWorldPass::run(Module &M, ModuleAnalysisManager &AM) 
{

  // Initialize the IRBuilder
  IRBuilder<> MBuilder(M.getContext());

  // Assign each function a unique index
  int functionIndex = 0;
  for (auto &function : M.getFunctionList()) {
      function.setMetadata("functionIndex", MDNode::get(M.getContext(), ConstantAsMetadata::get(ConstantInt::get(Type::getInt32Ty(M.getContext()), functionIndex++))));
      errs() << std::to_string(functionIndex) + " " + function.getName() + "\n";
  }

  // Create a global variable to store total number of functions
  GlobalVariable *numFuncs = new GlobalVariable(
      M,
      Type::getInt32Ty(M.getContext()),
      true,
      GlobalValue::PrivateLinkage,
      ConstantInt::get(Type::getInt32Ty(M.getContext()), functionIndex),
      "num_functions"
  );

  // Create a global variable to store the bitmap
  GlobalVariable *calledFuncs = new GlobalVariable(
      M,
      ArrayType::get(Type::getInt32Ty(M.getContext()), functionIndex),
      false,
      GlobalValue::PrivateLinkage,
      Constant::getNullValue(ArrayType::get(Type::getInt32Ty(M.getContext()), functionIndex)),
      "called_functions"
  );

  // Create a global variable to store the atomic counter for the instruction count
  Constant *zero = ConstantInt::get(M.getContext(), APInt(32, 0, false));
  GlobalVariable *instCount = new GlobalVariable(
    M,
    zero->getType(),
    true,
    GlobalValue::PrivateLinkage,
    zero,
    "global_inst_count"
  );

  createPrintArray(M);
  creatCheckInstCount(M);

  Function *checkInstCount = M.getFunction("checkInstCount");

  for (auto &function : M.getFunctionList()) {
    if (function.isDeclaration() 
    || function.empty()) {
      continue;
    }

    if (function.getName() == "checkInstCount" 
    || function.getName() == "printArray" 
    || function.getName() == "resetArray")
    {
      continue;
    }


    // Get the function's index
    int index = cast<ConstantInt>(function.getMetadata("functionIndex")->getOperand(0))->getZExtValue();
    
    // Insert the atomic increment at the beginning of the function
    MBuilder.SetInsertPoint(&function.getEntryBlock().getFirstInsertionPt());
    MBuilder.CreateAtomicRMW(AtomicRMWInst::Add, MBuilder.CreateConstGEP1_32(calledFuncs, index), ConstantInt::get(Type::getInt32Ty(M.getContext()), 1), AtomicOrdering::Monotonic);

    bool insertCheck = false;

    for (auto &block : function) {
      uint32_t blockInstCount = 0;
      for (auto &inst : block) {
        if ((std::string)inst.getOpcodeName() == "ret") {
          insertCheck = true;
        }
        blockInstCount++;
      }
      // Create a global variable to store the instruction count of the block
      Constant *blockInstCountVal = ConstantInt::get(M.getContext(), APInt(32, blockInstCount, false));
      GlobalVariable *blockInstCountVar = new GlobalVariable(
        M,
        blockInstCountVal->getType(),
        true,
        GlobalValue::PrivateLinkage,
        blockInstCountVal,
        funcName + "_" + block.getName().str() + "_inst_count"
      );
      // Insert the atomic instruction count increment at the beginning of the block
      MBuilder.SetInsertPoint(&block.getFirstInsertionPt());
      MBuilder.CreateAtomicRMW(AtomicRMWInst::Add, instCount, blockInstCountVal, AtomicOrdering::Monotonic);
      if (insertCheck) {
        MBuilder.CreateCall(checkInstCount);
        insertCheck = false;
      }
    }
  }
  return PreservedAnalyses::all();
}

} // end llvm namespace

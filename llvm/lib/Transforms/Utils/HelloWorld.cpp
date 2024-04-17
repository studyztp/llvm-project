//===-- HelloWorld.cpp - Example Transformations --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Utils/HelloWorld.h"

using namespace llvm;

PreservedAnalyses HelloWorldPass::run(Function &F,
                                      FunctionAnalysisManager &AM) {
  LLVMContext &context = F.getContext();
  auto module = F.getParent();
  Type* int32Type = Type::getInt32Ty(context);
  PointerType* int32PtrType = PointerType::get(int32Type, 0);
  FunctionType *printfType = FunctionType::get(int32Type,
                                               {int32PtrType}, true);
  FunctionCallee printfFunction = module->getOrInsertFunction("printf", printfType);

  std::string functionName = F.getName().str();
  std::string functionCallVarName = functionName + "_callCount";

  GlobalVariable *functionCallCount = module->getGlobalVariable(functionName + functionCallVarName);
  if (!functionCallCount) {
      functionCallCount = new GlobalVariable(*module, int32Type, false, GlobalValue::CommonLinkage, 0, functionCallVarName);
      functionCallCount->setInitializer(0);
  }
  
  Instruction *firstInstruction = &F.front().front();
  IRBuilder<> builder(firstInstruction);

  Value *loadedCallCount = builder.CreateLoad(int32Type,functionCallCount);
  Value *addedCallCount = builder.CreateAdd(loadedCallCount, builder.getInt32(1));
  builder.CreateStore(addedCallCount, functionCallCount);

  std::string printLog = functionName + " called %d times\n";
  Value *formatStr = builder.CreateGlobalStringPtr(printLog);
  builder.CreateCall(printfFunction, {formatStr, addedCallCount});

  // errs() << F.getName() << "\n";
  return PreservedAnalyses::all();
}

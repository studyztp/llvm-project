//===-- HelloWorld.cpp - Example Transformations --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Utils/HelloWorld.h"

namespace llvm {

PreservedAnalyses HelloWorldPass::run(Module &M, ModuleAnalysisManager &AM) 
{

  // Initialize the IRBuilder
  IRBuilder<> MBuilder(M.getContext());
  Function *mainFunction = M.getFunction("main");
  if (!mainFunction) {
    errs() << "No main function found in the module\n";
  }
  Type *Int64Ty = Type::getInt64Ty(M.getContext());

  std::vector<std::string> counterNames;

  // Create a global variable to store the atomic counter for each function
  for (auto &function : M.getFunctionList()) {
    if (function.isDeclaration() 
    || !function.hasFnAttribute(Attribute::MustProgress)) {
      continue;
    }
    Value *atomic_counter = new GlobalVariable(M, Int64Ty, false, 
    GlobalValue::CommonLinkage, ConstantInt::get(Int64Ty, 0), 
    function.getName().str()+"_counter");
    counterNames.push_back(function.getName().str()+"_counter");
  }

  // iterate over the functions
  for (auto mit = M.begin(); mit != M.end(); ++mit) {
    if (mit->isDeclaration() || !mit->hasFnAttribute(Attribute::MustProgress)) 
    {
      continue;
    }
    // iterate over the basic blocks
    auto entryBlock = &mit->getEntryBlock();
    Value *atomic_counter = M.getOrInsertGlobal(
      mit->getName().str()+"_counter", Int64Ty
    );
    Value *one = ConstantInt::get(Int64Ty, 1);

    MBuilder.SetInsertPoint(entryBlock->getFirstInsertionPt());
    MBuilder.CreateAtomicRMW(
      AtomicRMWInst::Add,
      atomic_counter,
      one,
      MaybeAlign(),
      AtomicOrdering::SequentiallyConsistent,
      SyncScope::System
    );
  }

  Function* printfFn = M.getFunction("printf");
  if (!printfFn) {
    errs() << "No printf function found in the module\n";
  }

  for (auto bbit = mainFunction->begin(); bbit != mainFunction->end(); bbit++) 
  {
    for (auto iit = bbit->begin(); iit != bbit->end(); iit ++) {
      if ((std::string)iit->getOpcodeName()=="ret") {
        MBuilder.SetInsertPoint(&*iit);
        for (auto &counterName : counterNames) {
          errs() << "counterName: " << counterName << "\n";
          Value *counter = M.getGlobalVariable(counterName);
          Value *formatStr = 
          MBuilder.CreateGlobalStringPtr("Function %s was called %ld times\n");
          Value *name = MBuilder.CreateGlobalStringPtr(counterName.c_str());
          Value *loadCounter = MBuilder.CreateLoad(Int64Ty, counter);
          Value *args[] = {formatStr, name, loadCounter};
          MBuilder.CreateCall(printfFn, args);
        }
      }
    }
  }
  return PreservedAnalyses::all();
}

} // end llvm namespace

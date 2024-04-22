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
    || function.empty()) {
      continue;
    }
    if (function.getName().str() == roi_start_function_name) {
      errs() << "Found the start function\n";
      // Iterate over the basic blocks in the function
      int counter = 0;
      for (auto &block : function) {
        // Iterate over the instructions in the block
        for (auto &instr : block) {
          // Increment the count for this block
          errs() << "Instr: " << instr << "\n";
          counter ++;
          if (counter == roi_start_bb_offset) {
            errs() << "Found the start basic block\n";
            // setup atomic counter for roi begin
            Value *atomic_counter = new GlobalVariable(M, Int64Ty, false, 
            GlobalValue::CommonLinkage, ConstantInt::get(Int64Ty, 0),
              "roi_begin_counter");
            counterNames.push_back("roi_begin_counter");

            Value *one = ConstantInt::get(Int64Ty, 1);

            MBuilder.SetInsertPoint(block.getFirstInsertionPt());
            MBuilder.CreateAtomicRMW(
              AtomicRMWInst::Add,
              atomic_counter,
              one,
              MaybeAlign(),
              AtomicOrdering::SequentiallyConsistent,
              SyncScope::System
            );
            break;
          }

        }
      }
    }
    if (function.getName().str() == roi_end_function_name) {
      errs() << "Found the end function\n";
      int counter = 0;
      for (auto &block : function) {
        for (auto &instr : block) {
          errs() << "Instr: " << instr << "\n";
          counter ++;
          if (counter == roi_end_bb_offset) {
            errs() << "Found the end basic block\n";
            // setup atomic counter
            Value *atomic_counter = new GlobalVariable(M, Int64Ty, false, 
            GlobalValue::CommonLinkage, ConstantInt::get(Int64Ty, 0),
              "roi_end_counter");
            counterNames.push_back("roi_end_counter");

            Value *one = ConstantInt::get(Int64Ty, 1);

            MBuilder.SetInsertPoint(block.getFirstInsertionPt());
            MBuilder.CreateAtomicRMW(
              AtomicRMWInst::Add,
              atomic_counter,
              one,
              MaybeAlign(),
              AtomicOrdering::SequentiallyConsistent,
              SyncScope::System
            );
            break;
          }
        }
      }
    }
  }

  if (counterNames.empty()) {
    errs() << "No counters were created for the functions\n";
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

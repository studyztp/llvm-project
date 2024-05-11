//===-- HelloWorld.h - Example Transformations ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_UTILS_HELLOWORLD_H
#define LLVM_TRANSFORMS_UTILS_HELLOWORLD_H

#include "llvm/IR/PassManager.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/Transforms/Instrumentation.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Support/FileSystem.h"
#include <string>
#include <vector>


namespace llvm {

class Module;

struct basicBlockInfo {
  // function related
  std::string functionName;
  uint32_t functionId;
  bool ifStartOfFunction;

  // basic block related
  std::string basicBlockName;
  uint64_t basicBlockCount;
  uint32_t basicBlockId;
  Instruction* lastNotBranchInstruction;

  // pointers to the basic block and function
  BasicBlock* basicBlock;
  Function* function;
};

class HelloWorldPass : public PassInfoMixin<HelloWorldPass> {
private:
  uint64_t totalFunctionCount;
  uint64_t totalBasicBlockCount;
  uint64_t threshold = 100000000;

  std::vector<basicBlockInfo> basicBlockList;

  bool emptyFunction(Function &F);
  GlobalVariable* createGlobalUint64Array(
    Module& M,
    std::string variableName,
    uint64_t size
  );

  // temp excluding list
  std::vector<std::string> exclude_functions = {
    "print_array",
    "print_int",
    "print_hi",
    "if_reach_threshold",
    "reset_counter",
    "increase_counter",
    "increment_array_element_at",
    "reset_array_element_at",
    "increase_array_by",
    "reset_array",
    "instrumentationFunction"
  };

  Function* createInstrumentationFunction(Module &M);
  
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};

} // namespace llvm

#endif // LLVM_TRANSFORMS_UTILS_HELLOWORLD_H

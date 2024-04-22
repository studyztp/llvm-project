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
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Instrumentation.h"
#include <string>
#include <vector>

namespace llvm {

class Module;
class HelloWorldPass : public PassInfoMixin<HelloWorldPass> {
private:
  std::string roi_start_function_name = "rank.omp_outlined";
  int roi_start_bb_offset = 261;
  int roi_start_function_count = 698538;
  std::string roi_end_function_name = "rank.omp_outlined";
  int roi_end_bb_offset = 118;
  int roi_end_function_count = 28593311;
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};

} // namespace llvm

#endif // LLVM_TRANSFORMS_UTILS_HELLOWORLD_H

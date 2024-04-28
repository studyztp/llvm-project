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
#include <string>
#include <vector>

namespace llvm {

class Module;
class HelloWorldPass : public PassInfoMixin<HelloWorldPass> {
private:
  uint32_t threshold = 100000000;
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};

} // namespace llvm

#endif // LLVM_TRANSFORMS_UTILS_HELLOWORLD_H

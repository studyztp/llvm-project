#ifndef LLVM_TRANSFORMS_UTILS_COMMENTINASSEMBLY_H
#define LLVM_TRANSFORMS_UTILS_COMMENTINASSEMBLY_H

#include "llvm/IR/PassManager.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/Transforms/Instrumentation.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <regex>

namespace llvm {
class CommentInAssemblyPass : public PassInfoMixin<CommentInAssemblyPass> {

public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);

};
} // namespace llvm

#endif // LLVM_TRANSFORMS_UTILS_COMMENTINASSEMBLY_H

#ifndef LLVM_TRANSFORMS_UTILS_PHASEBOUND_H
#define LLVM_TRANSFORMS_UTILS_PHASEBOUND_H

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

class PhaseBoundPass : public PassInfoMixin<PhaseBoundPass> {

public:
  struct basicBlockInfo {
    // function related
    std::string functionName;
    uint64_t functionId;
    bool ifStartMark;
    bool ifEndMark;
    bool ifWarmupMark;

    // basic block related
    std::string basicBlockName;
    uint64_t basicBlockCount;
    uint64_t basicBlockId;

    // pointers to the basic block and function
    BasicBlock* basicBlock;
    Function* function;
  };

  uint64_t totalFunctionCount;
  uint64_t totalBasicBlockCount;

  std::vector<basicBlockInfo> basicBlockList;

  uint64_t startMarkerFunctionId;
  uint64_t startMarkerBBId;
  uint64_t startMarkerCount;
  uint64_t endMarkerFunctionId;
  uint64_t endMarkerBBId;
  uint64_t endMarkerCount;
  uint64_t warmupMarkerFunctionId;
  uint64_t warmupMarkerBBId;
  uint64_t warmupMarkerCount;
  bool hasWarmupMarker = true;
  bool foundStartMarker = false;
  bool labelOnly = false;

  void getInformation(Module &M);
  void formBasicBlockList(Module& M);
  bool emptyFunction(Function &F);

public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};

} // namespace llvm

#endif // LLVM_TRANSFORMS_UTILS_PHASEBOUND_H
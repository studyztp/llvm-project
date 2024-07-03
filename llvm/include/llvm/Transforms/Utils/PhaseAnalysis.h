#ifndef LLVM_TRANSFORMS_UTILS_PHASEANALYSIS_H
#define LLVM_TRANSFORMS_UTILS_PHASEANALYSIS_H

#include "llvm/IR/PassManager.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/Transforms/Instrumentation.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include <bits/stdc++.h>
#include <string>
#include <vector>

namespace llvm {

class Module;

class PhaseAnalysisPass : public PassInfoMixin<PhaseAnalysisPass> {
public:
  struct basicBlockInfo {
    // function related
    std::string functionName;
    uint64_t functionId;
    bool ifStartOfFunction;

    // basic block related
    std::string basicBlockName;
    uint64_t basicBlockCount;
    uint64_t basicBlockId;

    // pointers to the basic block and function
    BasicBlock* basicBlock;
    Function* function;
  };

private:
  uint64_t totalFunctionCount;
  uint64_t totalBasicBlockCount;
  uint64_t threshold;
  bool usingPapiToAnalyze = false;

  std::vector<basicBlockInfo> basicBlockList;

  // check if the function is empty by checking the number of 
  bool emptyFunction(Function &F);


  GlobalVariable* createGlobalUint64Array(
    Module& M,
    std::string variableName,
    uint64_t size
  );

  void modifyROIFunctionsForBBV(Module &M);

  Function* createBBVAnalysisFunction(Module &M);
  Function* createPapiAnalysisFunction(Module &M);

  void instrumentBBVAnalysis(Module &M);
  void instrumentPapiAnalysis(Module &M);
   
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};

} // namespace llvm

#endif // LLVM_TRANSFORMS_UTILS_PHASEANALYSIS_H
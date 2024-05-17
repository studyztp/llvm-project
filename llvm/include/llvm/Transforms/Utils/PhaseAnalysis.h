#ifndef LLVM_TRANSFORMS_UTILS_PHASEANALYSIS_H
#define LLVM_TRANSFORMS_UTILS_PHASEANALYSIS_H

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

class PhaseAnalysisPass : public PassInfoMixin<PhaseAnalysisPass> {
public:
  struct basicBlockInfo {
    // function related
    std::string functionName;
    uint32_t functionId;
    bool ifStartOfFunction;

    // basic block related
    std::string basicBlockName;
    uint64_t basicBlockCount;
    uint32_t basicBlockId;

    // pointers to the basic block and function
    BasicBlock* basicBlock;
    Function* function;
  };

private:
  uint64_t totalFunctionCount;
  uint64_t totalBasicBlockCount;
  uint64_t threshold = 100000000;

  std::vector<basicBlockInfo> basicBlockList;

  // check if the function is empty by checking the number of 
  bool emptyFunction(Function &F);


  GlobalVariable* createGlobalUint64Array(
    Module& M,
    std::string variableName,
    uint64_t size
  );

  void modifyROIFunctions(Module &M);

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
    "instrumentationFunction",
    "write_single_data",
    "write_array_data",
    "roi_begin_",
    "roi_end_",
    "start_marker",
    "end_marker",
    "start_function",
    "end_function",
    "warmup_marker",
    "warmup_function",
    "set_array_element_at"
  };

  Function* createInstrumentationFunction(Module &M);
  
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};

} // namespace llvm

#endif // LLVM_TRANSFORMS_UTILS_PHASEANALYSIS_H
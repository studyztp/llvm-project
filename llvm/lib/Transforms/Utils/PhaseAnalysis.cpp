#include "llvm/Transforms/Utils/PhaseAnalysis.h"

namespace llvm {

bool PhaseAnalysisPass::emptyFunction(Function &F) {
  int count = 0;
  for (auto &block : F) {
    for (auto &inst : block) {
      count++;
    }
  }
  return count == 0;
}

void PhaseAnalysisPass::modifyROIFunctionsForBBV(Module &M) {
  Function* roiBegin = M.getFunction("roi_begin_");
  if (!roiBegin) {
    errs() << "Function roi_begin_ not found\n";
  }
  Function* roiEnd = M.getFunction("roi_end_");
  if (!roiEnd) {
    errs() << "Function roi_end_ not found\n";
  }

  Function* initArraysFunction = M.getFunction("init_arrays");
  if (!initArraysFunction) {
    errs() << "Function init_arrays not found\n";
  }


  IRBuilder<> builder(M.getContext());

  builder.SetInsertPoint(roiBegin->back().getTerminator());
  builder.CreateCall(initArraysFunction, {
    ConstantInt::get(Type::getInt64Ty(M.getContext()), totalBasicBlockCount)
  });

}

void PhaseAnalysisPass::instrumentBBVAnalysis(Module &M) {
  IRBuilder<> builder(M.getContext());

  std::string bbHookFunctionName = "";
  for (auto& function : M.getFunctionList()) {
    if (function.getName().str().find("bb_hook") != std::string::npos) {
      bbHookFunctionName = function.getName().str();
    }
  }

  Function* BBHookFunction = M.getFunction(bbHookFunctionName);
  if (!BBHookFunction) {
    errs() << "Function " << bbHookFunctionName<< " not found\n";
  }

  for (auto item : basicBlockList) {
    if (item.basicBlock->getTerminator()) {
      builder.SetInsertPoint(item.basicBlock->getTerminator());
    } else {
      errs() << "Could not find terminator point for fucntion " << item.functionName << " bbid " << item.basicBlockId << "\n";
      builder.SetInsertPoint(item.basicBlock->getFirstInsertionPt());
    }
    CallInst* main_instrument = builder.CreateCall(BBHookFunction, {
      ConstantInt::get(Type::getInt64Ty(M.getContext()), item.basicBlockCount),
      ConstantInt::get(Type::getInt64Ty(M.getContext()), item.basicBlockId),
      ConstantInt::get(Type::getInt64Ty(M.getContext()), threshold),
    });

  }

  modifyROIFunctionsForBBV(M);
}

void PhaseAnalysisPass::instrumentPapiAnalysis(Module &M) {
  IRBuilder<> builder(M.getContext());

  std::string bbHookFunctionName = "";
  for (auto& function : M.getFunctionList()) {
    if (function.getName().str().find("bb_hook") != std::string::npos) {
      bbHookFunctionName = function.getName().str();
    }
  }

  Function* BBHookFunction = M.getFunction(bbHookFunctionName);
  if (!BBHookFunction) {
    errs() << "Function " << bbHookFunctionName<< " not found\n";
  }

  for (auto item : basicBlockList) {
    if (item.basicBlock->getTerminator()) {
      builder.SetInsertPoint(item.basicBlock->getTerminator());
    } else {
      errs() << "Could not find terminator point for fucntion " << item.functionName << " bbid " << item.basicBlockId << "\n";
      builder.SetInsertPoint(item.basicBlock->getFirstInsertionPt());
    }
        // Call the instrumentation function with the basic block count as the argument
        // (this is the number of instructions in the basic block
    CallInst* main_instrument = builder.CreateCall(BBHookFunction, {
      ConstantInt::get(Type::getInt64Ty(M.getContext()), item.basicBlockCount),
      ConstantInt::get(Type::getInt64Ty(M.getContext()), threshold),
    });
  }
  
}

cl::opt<std::string> PhaseAnalysisOutputFilename(
  "phase-analysis-output-file", 
  cl::init("basicBlockList.txt"),
  cl::desc("<output file>"),
  cl::ValueRequired
);

cl::opt<uint64_t> PhaseAnalysisRegionLength(
  "phase-analysis-region-length",
  cl::init(100000000),
  cl::desc("<region length>"),
  cl::ValueRequired
);

cl::opt<bool> PhaseAnalysisUsingPapi(
  "phase-analysis-using-papi",
  cl::init(false),
  cl::desc("<using papi>"),
  cl::ValueRequired
);

PreservedAnalyses PhaseAnalysisPass::run(Module &M, ModuleAnalysisManager &AM) 
{
  std::error_code EC;
  raw_fd_ostream out(PhaseAnalysisOutputFilename.c_str(), EC, sys::fs::OF_Text);
  if (EC) {
    errs() << "Could not open file: " << EC.message() << "\n";
  }
  threshold = PhaseAnalysisRegionLength;
  errs() << "Threshold: " << threshold << "\n";
  usingPapiToAnalyze = PhaseAnalysisUsingPapi;

  IRBuilder<> builder(M.getContext());

  totalFunctionCount = 0;
  totalBasicBlockCount = 0;

  // find all basic blocks that will be instrumented
  for (auto& function : M.getFunctionList()) {
    std::string functionName = function.getName().str();
    if (emptyFunction(function) || function.isDeclaration()) 
    {
      continue;
    }

    if (function.hasFnAttribute(Attribute::NoProfile)) {
      errs() << "Skipping function: " << function.getName() << "\n";
      continue;
    }

    if (functionName.find("_GLOBAL__sub_I_") != std::string::npos ||
      functionName.find("__cxx_global_var_init") != std::string::npos ||
      functionName.find("_ZNSt13__atomic_") != std::string::npos ||
      functionName.find("_ZStanSt12memory_orderSt23__memory_order_modifier") != std::string::npos ||
      functionName.find("__clang_call_terminate") != std::string::npos ||
      functionName.find("_ZNSt13__atomic_baseImEaSEm") != std::string::npos ||
      functionName.find("cxx119to_string") != std::string::npos ||
      functionName.find("cxx1112basic_string") != std::string::npos) {
      // This is an auto-generated function, skip it
      continue;
    }

    basicBlockInfo basicBlock;
    basicBlock.functionName = function.getName();
    basicBlock.functionId = totalFunctionCount;
    
    totalFunctionCount++;
    bool ifStartOfFunction = true;
    for (auto& block : function) {
      if (ifStartOfFunction) {
        basicBlock.ifStartOfFunction = true;
        ifStartOfFunction = false;
      }

      basicBlock.basicBlockName = block.getName();
      basicBlock.basicBlockCount = block.size();
      basicBlock.basicBlockId = totalBasicBlockCount;
      basicBlock.function = &function;
      basicBlock.basicBlock = &block;
      totalBasicBlockCount++;
      basicBlockList.push_back(basicBlock);
    }
  }

  if (usingPapiToAnalyze) {
    instrumentPapiAnalysis(M);
  } else {
    instrumentBBVAnalysis(M);
  }

  out << "[functionID:functionName] [basicBlockID:basicBlockName:basicBlockIRInstCount] \n";

  std::string workingFunctionName = "";

  for (auto item : basicBlockList) {
    if (workingFunctionName != item.functionName) {
      if (workingFunctionName != ""){
        out << "\n";
      }
      workingFunctionName = item.functionName;
      out << "[" << item.functionId << ":" << item.functionName << "]";
    }
    out << " [" << item.basicBlockId << ":" << item.basicBlockName << ":" << item.basicBlockCount << "] ";
  }

  out.close();
  
  return PreservedAnalyses::all();
}

} // end llvm namespace

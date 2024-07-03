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

GlobalVariable* PhaseAnalysisPass::createGlobalUint64Array(
  Module& M,
  std::string variableName,
  uint64_t size) 
{
  ArrayType* arrayTy = ArrayType::get(IntegerType::get(M.getContext(), 64), 
                                                                        size);

  GlobalVariable* gvar_array = new GlobalVariable(
    M,
    arrayTy,
    false,
    GlobalValue::ExternalLinkage,
    0,
    variableName
  );
  ConstantAggregateZero* const_array = ConstantAggregateZero::get(arrayTy);
  gvar_array->setInitializer(const_array);

  return gvar_array;
}

Function* PhaseAnalysisPass::createBBVAnalysisFunction(Module &M) {
  Type* VoidTy = Type::getVoidTy(M.getContext());
  Type* Int64Ty = Type::getInt64Ty(M.getContext());
  FunctionType* FTy = FunctionType::get(VoidTy, {Int64Ty, Int64Ty}, false);
  Function* F = Function::Create(
    FTy,
    GlobalValue::ExternalLinkage,
    "instrumentationFunction",
    M
  );
  F->addFnAttr(Attribute::NoInline);
  F->addFnAttr(Attribute::NoProfile);

  BasicBlock* mainBB = BasicBlock::Create(M.getContext(), "instrumentation_entry", F);
  IRBuilder<> builder(M.getContext());
  builder.SetInsertPoint(mainBB);
  Function::arg_iterator args = F->arg_begin();
  Value* basicBlockId = &*args++;
  Value* basicBlockInstCount = &*args++;

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

  InlineFunctionInfo ifi;

  builder.CreateCall(BBHookFunction, {
    basicBlockInstCount,
    basicBlockId,
    ConstantInt::get(Int64Ty, threshold)
  });

  builder.CreateRetVoid();

  std::vector<CallInst*> calls;

  for (auto& BB : *F) {
    for (auto& I : BB) {
      if (isa<CallInst>(&I)) {
        std::string name = cast<CallInst>(&I)->getCalledFunction()->getName().str();
        if (M.getFunction(name) && !M.getFunction(name)->hasFnAttribute(Attribute::NoInline)){
          calls.push_back(cast<CallInst>(&I));
        }
      }
    }
  }

  for (auto* call : calls) {
    if(InlineFunction(*call, ifi).isSuccess()) {
      errs() << "Successfully inlined function for" << call->getCalledFunction()->getName().str() << "\n";
    } else {
      errs() << "Failed to inline function for" << call->getCalledFunction()->getName().str() << "\n";
    }
  }

  return F;
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

Function* PhaseAnalysisPass::createPapiAnalysisFunction(Module &M) {
  Type* VoidTy = Type::getVoidTy(M.getContext());
  Type* Int64Ty = Type::getInt64Ty(M.getContext());
  FunctionType* FTy = FunctionType::get(VoidTy, {Int64Ty}, false);
  Function* F = Function::Create(
    FTy,
    GlobalValue::ExternalLinkage,
    "instrumentationFunction",
    M
  );
  F->addFnAttr(Attribute::NoInline);
  F->addFnAttr(Attribute::NoProfile);

  BasicBlock* mainBB = BasicBlock::Create(M.getContext(), "instrumentation_entry", F);
  IRBuilder<> builder(M.getContext());

  Function::arg_iterator args = F->arg_begin();
  Value* basicBlockInstCount = &*args;

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

  builder.SetInsertPoint(mainBB);
  builder.CreateCall(BBHookFunction, {
    basicBlockInstCount,
    ConstantInt::get(Int64Ty, threshold)
  });

  builder.CreateRetVoid();

  return F;
}

void PhaseAnalysisPass::instrumentBBVAnalysis(Module &M) {
  IRBuilder<> builder(M.getContext());

  // Create the instrumentation function
  Function* instrumentationFunction = createBBVAnalysisFunction(M);

  for (auto item : basicBlockList) {
    if (item.basicBlock->getTerminator()) {
      builder.SetInsertPoint(item.basicBlock->getTerminator());
    } else {
      errs() << "Could not find terminator point for fucntion " << item.functionName << " bbid " << item.basicBlockId << "\n";
      builder.SetInsertPoint(item.basicBlock->getFirstInsertionPt());
    }
    CallInst* main_instrument = builder.CreateCall(instrumentationFunction, {
      ConstantInt::get(Type::getInt64Ty(M.getContext()), item.basicBlockId),
      ConstantInt::get(Type::getInt64Ty(M.getContext()), item.basicBlockCount)
    });

  }

  modifyROIFunctionsForBBV(M);
}

void PhaseAnalysisPass::instrumentPapiAnalysis(Module &M) {
  IRBuilder<> builder(M.getContext());

  Function* instrumentationFunction = createPapiAnalysisFunction(M);

  for (auto item : basicBlockList) {
    if (item.basicBlock->getTerminator()) {
      builder.SetInsertPoint(item.basicBlock->getTerminator());
    } else {
      errs() << "Could not find terminator point for fucntion " << item.functionName << " bbid " << item.basicBlockId << "\n";
      builder.SetInsertPoint(item.basicBlock->getFirstInsertionPt());
    }
        // Call the instrumentation function with the basic block count as the argument
        // (this is the number of instructions in the basic block
    CallInst* main_instrument = builder.CreateCall(instrumentationFunction, {
      ConstantInt::get(Type::getInt64Ty(M.getContext()), item.basicBlockCount)
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

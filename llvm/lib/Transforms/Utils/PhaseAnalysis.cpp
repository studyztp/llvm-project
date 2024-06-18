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
  Type* Int32Ty = Type::getInt32Ty(M.getContext());
  FunctionType* FTy = FunctionType::get(VoidTy, {Int32Ty, Int64Ty}, false);
  Function* F = Function::Create(
    FTy,
    GlobalValue::ExternalLinkage,
    "instrumentationFunction",
    M
  );
  F->addFnAttr(Attribute::NoInline);
  F->addFnAttr(Attribute::NoProfile);

  BasicBlock* mainBB = BasicBlock::Create(M.getContext(), "instrumentation_entry", F);
  BasicBlock* ifMeet = BasicBlock::Create(M.getContext(), "instrumentation_ifMeet", F);
  BasicBlock* ifNotMeet = BasicBlock::Create(M.getContext(), "instrumentation_ifNotMeet", F);
  IRBuilder<> builder(M.getContext());
  builder.SetInsertPoint(mainBB);
  Function::arg_iterator args = F->arg_begin();
  Value* basicBlockId = &*args++;
  Value* basicBlockInstCount = &*args++;

  Value* counter = M.getGlobalVariable("instructionCounter");
  if (!counter) {
    errs() << "Global variable instructionCounter not found\n";
  }

  Function* checkUpFunction = M.getFunction("atomic_increase");
  if (!checkUpFunction) {
    errs() << "Function atomic_increase not found\n";
  }

  Function* printThreadIdFunction = M.getFunction("print_thread_num");

  InlineFunctionInfo ifi;

  CallInst* returnValue = builder.CreateCall(checkUpFunction, 
        {counter, basicBlockInstCount, ConstantInt::get(Int64Ty, threshold)});
  builder.CreateCondBr(returnValue, ifMeet, ifNotMeet);

  builder.SetInsertPoint(ifMeet);
  builder.CreateCall(printThreadIdFunction);
  builder.CreateRetVoid(); 

  builder.SetInsertPoint(ifNotMeet);
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
  Function* resetArrayFunction = M.getFunction("reset_array");
  if (!resetArrayFunction) {
    errs() << "Function reset_array not found\n";
  }
  Function* writeSingleDataFunction = M.getFunction("write_single_data");
  if (!writeSingleDataFunction) {
    errs() << "Function write_single_data not found\n";
  }
  Function* writeArrayDataFunction = M.getFunction("write_array_data");
  if (!writeArrayDataFunction) {
    errs() << "Function write_array_data not found\n";
  }

  IRBuilder<> builder(M.getContext());

  builder.SetInsertPoint(roiBegin->back().getTerminator());
  builder.CreateCall(writeArrayDataFunction, {
    builder.CreateGlobalStringPtr("basic block vector"),
    M.getGlobalVariable("basicBlockVector"),
    ConstantInt::get(Type::getInt32Ty(M.getContext()), totalBasicBlockCount)
  });
  // reset all global instruction counter and basic block distance counter
  builder.CreateStore(
    ConstantInt::get(Type::getInt64Ty(M.getContext()), 0), 
    M.getGlobalVariable("instructionCounter"));
  builder.CreateCall(resetArrayFunction, {
    M.getGlobalVariable("basicBlockDist"),
    ConstantInt::get(Type::getInt32Ty(M.getContext()), totalBasicBlockCount)
  });

  builder.SetInsertPoint(roiEnd->front().getFirstInsertionPt());
  // write all stats
  builder.CreateCall(writeSingleDataFunction, {
    builder.CreateGlobalStringPtr("instructionCounter"),
    builder.CreateLoad(Type::getInt64Ty(M.getContext()), M.getGlobalVariable("instructionCounter"))
  });
  builder.CreateCall(writeArrayDataFunction, {
    builder.CreateGlobalStringPtr("basic block vector"),
    M.getGlobalVariable("basicBlockVector"),
    ConstantInt::get(Type::getInt32Ty(M.getContext()), totalBasicBlockCount)
  });
  builder.CreateCall(writeArrayDataFunction, {
    builder.CreateGlobalStringPtr("basic block distance"),
    M.getGlobalVariable("basicBlockDist"),
    ConstantInt::get(Type::getInt32Ty(M.getContext()), totalBasicBlockCount)
  });

}

void PhaseAnalysisPass::modifyROIFunctionsForPapi(Module &M) {
  Function* roiBegin = M.getFunction("roi_begin_");
  if (!roiBegin) {
    errs() << "Function roi_begin_ not found\n";
  }

  IRBuilder<> builder(M.getContext());

  builder.SetInsertPoint(roiBegin->back().getTerminator());
  // reset all global instruction counter and basic block distance counter
  builder.CreateStore(
    ConstantInt::get(Type::getInt64Ty(M.getContext()), 0), 
    M.getGlobalVariable("instructionCounter"));

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
  BasicBlock* ifMeet = BasicBlock::Create(M.getContext(), "instrumentation_ifMeet", F);
  BasicBlock* ifNotMeet = BasicBlock::Create(M.getContext(), "instrumentation_ifNotMeet", F);
  IRBuilder<> builder(M.getContext());

  Function::arg_iterator args = F->arg_begin();
  Value* basicBlockInstCount = &*args;

  Function* papiRegionBegin = M.getFunction("start_papi_region");
  if (!papiRegionBegin) {
    errs() << "Function start_region not found\n";
  }

  Function* papiRegionEnd = M.getFunction("end_papi_region");
  if (!papiRegionEnd) {
    errs() << "Function end_region not found\n";
  }

  Value* counter = M.getGlobalVariable("instructionCounter");
  if (!counter) {
    errs() << "Global variable instructionCounter not found\n";
  }

  InlineFunctionInfo ifi;

  builder.SetInsertPoint(mainBB);

  Value* loadOldCounter = builder.CreateLoad(Int64Ty, counter);
  Value* addResult = builder.CreateAdd(loadOldCounter, basicBlockInstCount);
  builder.CreateStore(addResult, counter);

  Value* ifReachThreshold = 
      builder.CreateICmpSGE(addResult, ConstantInt::get(Int64Ty, threshold));
  builder.CreateCondBr(ifReachThreshold, ifMeet, ifNotMeet);

  builder.SetInsertPoint(ifMeet);
  builder.CreateCall(papiRegionEnd);
  builder.CreateCall(papiRegionBegin);
  builder.CreateStore(ConstantInt::get(Int64Ty, 0), counter);

  builder.CreateRetVoid(); 

  builder.SetInsertPoint(ifNotMeet);
  builder.CreateRetVoid();

  return F;
}

void PhaseAnalysisPass::instrumentBBVAnalysis(Module &M) {
  IRBuilder<> builder(M.getContext());
  // create arrays
  GlobalVariable* basicBlockVector = createGlobalUint64Array(M, "basicBlockVector", totalBasicBlockCount);
  if (!basicBlockVector) {
    errs() << "Global variable basicBlockVector not found\n";
  }
  GlobalVariable* basicBlockDist = createGlobalUint64Array(M, "basicBlockDist", totalBasicBlockCount);
  if (!basicBlockDist) {
    errs() << "Global variable basicBlockDist not found\n";
  }

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
      ConstantInt::get(Type::getInt32Ty(M.getContext()), item.basicBlockId),
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

  modifyROIFunctionsForPapi(M);

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

  // Create a global variable to store the instruction count
  Type *Int64Ty = Type::getInt64Ty(M.getContext());
  GlobalVariable* instructionCounter = new GlobalVariable(
    M,
    Int64Ty,
    false,
    GlobalValue::ExternalLinkage,
    ConstantInt::get(Int64Ty, 0),
    "instructionCounter"
  );
  if (!instructionCounter) {
    errs() << "Global variable instructionCounter not found\n";
  }

  totalFunctionCount = 0;
  totalBasicBlockCount = 0;

  // find all basic blocks that will be instrumented
  for (auto& function : M.getFunctionList()) {
    if (emptyFunction(function) || function.isDeclaration()) 
    {
      continue;
    }
    if (function.hasFnAttribute(Attribute::NoProfile)) {
      errs() << "Skipping function: " << function.getName() << "\n";
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


/*
1. each basic block has a const global variable that stores its instruction
    length
2. every time a basic block is executed, it adds its instruction count to 
    the global instruction counter atomically
3. every function has a const global variable that stores the name of the 
    function
4. at the entry of every targeted function, it atomically add its name to a
    global set
5. when the global instruction counter reaches a certain threshold, it prints
    the name of the functions in the global set and the total instruction count
    then resets the global instruction counter.
6. the global set is reset after the print
*/

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


Function* PhaseAnalysisPass::createInstrumentationFunction(Module &M) {
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

  Value* basicBlockVector = M.getGlobalVariable("basicBlockVector");

  Value* basicBlockDist = M.getGlobalVariable("basicBlockDist");

  Function* writeSingleDataFunction = M.getFunction("write_single_data");
  if (!writeSingleDataFunction) {
    errs() << "Function write_single_data not found\n";
  }
  Function* writeArrayDataFunction = M.getFunction("write_array_data");
  if (!writeArrayDataFunction) {
    errs() << "Function write_array_data not found\n";
  }

  Function* incrementArrayElementAtFunction = M.getFunction("increment_array_element_at");
  if (!incrementArrayElementAtFunction) {
    errs() << "Function increment_array_element_at not found\n";
  }
  Function* resetArrayElementAtFunction = M.getFunction("reset_array_element_at");
  if (!resetArrayElementAtFunction) {
    errs() << "Function reset_array_element_at not found\n";
  }
  Function* increaseArrayByFunction = M.getFunction("increase_array_by");
  if (!increaseArrayByFunction) {
    errs() << "Function increase_array_by not found\n";
  }
  Function* resetArrayFunction = M.getFunction("reset_array");
  if (!resetArrayFunction) {
    errs() << "Function reset_array not found\n";
  }

  InlineFunctionInfo ifi;

  // can be atomic add
  // increase global counter by bb IR inst count
  Value* loadOldCounter = builder.CreateLoad(Int64Ty, counter);
  Value* addResult = builder.CreateAdd(loadOldCounter, basicBlockInstCount);
  builder.CreateStore(addResult, counter);

  // increase basic block vector counter by 1
  CallInst* incrementBasicBlockVector = 
  builder.CreateCall(incrementArrayElementAtFunction, {basicBlockVector, basicBlockId});

  // increase basic block distance vector by bb IR inst count
  CallInst* increaseBasicBlockDistance = 
  builder.CreateCall(increaseArrayByFunction, {basicBlockDist,
  ConstantInt::get(Int32Ty, totalBasicBlockCount)
  ,basicBlockInstCount});

  // reset the current basic block distance to 0
  CallInst* resetBasicBlockDistanceOfCurrentBlock = 
  builder.CreateCall(resetArrayElementAtFunction, {basicBlockDist, basicBlockId});

  // can be atomic load
  Value* loadNewCounter = builder.CreateLoad(Int64Ty, counter);

  // can be atomic comparison
  Value* ifReachThreshold = 
      builder.CreateICmpSGE(loadNewCounter, ConstantInt::get(Int64Ty, threshold));
  builder.CreateCondBr(ifReachThreshold, ifMeet, ifNotMeet);

  builder.SetInsertPoint(ifMeet);
  // the helper function side should have mutex lock or we can wrap this 
  // whole function with mutex calling from the profiler_helper.c
  // print the global counter
  builder.CreateCall(writeSingleDataFunction,
  {builder.CreateGlobalStringPtr("instructionCounter"), loadNewCounter});
  // print the basic block vector
  builder.CreateCall(writeArrayDataFunction,
  {builder.CreateGlobalStringPtr("basic block vector"),
  basicBlockVector, ConstantInt::get(Int32Ty, totalBasicBlockCount)});
  // print the basic block distance
  builder.CreateCall(writeArrayDataFunction,
  {builder.CreateGlobalStringPtr("basic block distance"),
  basicBlockDist, ConstantInt::get(Int32Ty, totalBasicBlockCount)});

  // can set to atomic store later
  // reset the global counter
  builder.CreateStore(ConstantInt::get(Int64Ty, 0), counter);
  // reset the basic block distance
  CallInst* resetBasicBlockDistance =
  builder.CreateCall(resetArrayFunction, 
  {basicBlockDist, 
  ConstantInt::get(Int32Ty, totalBasicBlockCount)});
  builder.CreateRetVoid(); 

  builder.SetInsertPoint(ifNotMeet);
  builder.CreateRetVoid();

  std::vector<CallInst*> calls;

  for (auto& BB : *F) {
    for (auto& I : BB) {
      if (isa<CallInst>(&I)) {
        std::string name = cast<CallInst>(&I)->getCalledFunction()->getName().str();
        if (name != "write_single_data" && name != "write_array_data") {
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

void PhaseAnalysisPass::modifyROIFunctions(Module &M) {
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

  builder.SetInsertPoint(roiBegin->front().getFirstInsertionPt());
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


PreservedAnalyses PhaseAnalysisPass::run(Module &M, ModuleAnalysisManager &AM) 
{
  std::error_code EC;
  raw_fd_ostream out("basicBlockInfo.txt", EC, sys::fs::OF_Text);
  if (EC) {
    errs() << "Could not open file: " << EC.message() << "\n";
  }
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
    if (std::find(exclude_functions.begin(), exclude_functions.end(), function.getName()) != exclude_functions.end()) {
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
  Function* instrumentationFunction = createInstrumentationFunction(M);

  for (auto item : basicBlockList) {
    builder.SetInsertPoint(item.basicBlock->getFirstInsertionPt());
    CallInst* main_instrument = builder.CreateCall(instrumentationFunction, {
      ConstantInt::get(Type::getInt32Ty(M.getContext()), item.basicBlockId),
      ConstantInt::get(Type::getInt64Ty(M.getContext()), item.basicBlockCount)
    });
  }

  modifyROIFunctions(M);

  out << "functionID:functionName basicBlockID:basicBlockName basicBlockCount \n";

  for (auto item : basicBlockList) {
    out << item.functionId << ":" << item.functionName << " "  
    << item.basicBlockId <<":"<< item.basicBlockName << " " 
    << item.basicBlockCount << "\n";
  }

  out.close();
  
  return PreservedAnalyses::all();
}

} // end llvm namespace

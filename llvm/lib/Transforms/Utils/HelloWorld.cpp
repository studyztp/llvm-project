//===-- HelloWorld.cpp - Example Transformations --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//


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

#include "llvm/Transforms/Utils/HelloWorld.h"

namespace llvm {

bool HelloWorldPass::emptyFunction(Function &F) {
  int count = 0;
  for (auto &block : F) {
    for (auto &inst : block) {
      count++;
    }
  }
  return count == 0;
}

GlobalVariable* HelloWorldPass::createGlobalUint64Array(
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


Function* HelloWorldPass::createInstrumentationFunction(Module &M) {
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

  // can be atomic add
  // increase global counter by bb IR inst count
  Value* loadOldCounter = builder.CreateLoad(Int64Ty, counter);
  Value* addResult = builder.CreateAdd(loadOldCounter, basicBlockInstCount);
  builder.CreateStore(addResult, counter);

  // increase basic block vector counter by 1
  builder.CreateCall(incrementArrayElementAtFunction, {basicBlockVector, basicBlockId});

  // increase basic block distance vector by bb IR inst count
  builder.CreateCall(increaseArrayByFunction, {basicBlockDist,
  ConstantInt::get(Int32Ty, totalBasicBlockCount)
  ,basicBlockInstCount});

  // reset the current basic block distance to 0
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
  builder.CreateCall(resetArrayFunction, 
  {basicBlockDist, 
  ConstantInt::get(Int32Ty, totalBasicBlockCount)});
  builder.CreateRetVoid(); 

  builder.SetInsertPoint(ifNotMeet);
  builder.CreateRetVoid();

  return F;
}


PreservedAnalyses HelloWorldPass::run(Module &M, ModuleAnalysisManager &AM) 
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
  InlineFunctionInfo ifi;

  for (auto item : basicBlockList) {
    builder.SetInsertPoint(item.basicBlock->getFirstNonPHI());
    CallInst* main_instrument = builder.CreateCall(instrumentationFunction, {
      ConstantInt::get(Type::getInt32Ty(M.getContext()), item.basicBlockId),
      ConstantInt::get(Type::getInt64Ty(M.getContext()), item.basicBlockCount)
    });
    auto res = InlineFunction(*main_instrument, ifi);
    if (!res.isSuccess()) {
      errs() << "Failed to inline function\n";
    }
  }

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

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
  FunctionType* FTy = FunctionType::get(VoidTy, {Int32Ty, Int32Ty, Int64Ty}, false);
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
  Value* functionId = &*args++;
  Value* basicBlockInstCount = &*args++;

  Value* counter = M.getGlobalVariable("instructionCounter");
  if (!counter) {
    errs() << "Global variable instructionCounter not found\n";
  }

  Value* basicBlockVector = M.getGlobalVariable("basicBlockVector");

  Value* functionVector = M.getGlobalVariable("functionVector");

  Value* basicBlockDist = M.getGlobalVariable("basicBlockDist");

  Value* functionDist = M.getGlobalVariable("functionDist");

  Function* IncreaseCounterFunction = M.getFunction("increase_counter");
  if (!IncreaseCounterFunction) {
    errs() << "Function increase_counter not found\n";
  }

  Function* PrintIntFunction = M.getFunction("print_int");
  if (!PrintIntFunction) {
    errs() << "Function print_int not found\n";
  }

  Function* ResetCounterFunction = M.getFunction("reset_counter");
  if (!ResetCounterFunction) {
    errs() << "Function reset_counter not found\n";
  }

  Function* IncreaseArrayElementAtFunction = M.getFunction("increment_array_element_at");
  if (!IncreaseArrayElementAtFunction) {
    errs() << "Function increment_array_element_at not found\n";
  }

  Function* PrintArrayFunction = M.getFunction("print_array");
  if (!PrintArrayFunction) {
    errs() << "Function print_array not found\n";
  }

  Function* ResetArrayElementAtFunction = M.getFunction("reset_array_element_at");
  if (!ResetArrayElementAtFunction) {
    errs() << "Function reset_array_element_at not found\n";
  }

  Function* IncreaseArrayByFunction = M.getFunction("increase_array_by");
  if (!IncreaseArrayByFunction) {
    errs() << "Function increase_array_by not found\n";
  }

  Function* ResetArrayFunction = M.getFunction("reset_array");
  if (!ResetArrayFunction) {
    errs() << "Function reset_array not found\n";
  }

  // can be atomic add
  // increase global counter by bb IR inst count
  builder.CreateCall(IncreaseCounterFunction, {counter, basicBlockInstCount});
  // increase basic block vector counter by 1
  builder.CreateCall(IncreaseArrayElementAtFunction, {basicBlockVector, basicBlockId});
  // increase basic block distance vector by bb IR inst count
  builder.CreateCall(IncreaseArrayByFunction, 
  {basicBlockDist, 
  ConstantInt::get(Int32Ty, totalBasicBlockCount),
  basicBlockInstCount});
  // reset the current basic block distance to 0
  builder.CreateCall(ResetArrayElementAtFunction, {basicBlockDist, basicBlockId});
  // increase function distance vector by bb IR inst count
  builder.CreateCall(IncreaseArrayByFunction,
  {functionDist,
  ConstantInt::get(Int32Ty, totalFunctionCount), 
  basicBlockInstCount});
  // can be atomic load
  Value* loadCounter = builder.CreateLoad(Int64Ty, counter);
  // can be atomic comparison
  Value* ifReachThreshold = 
      builder.CreateICmpSGE(loadCounter, ConstantInt::get(Int64Ty, threshold));
  builder.CreateCondBr(ifReachThreshold, ifMeet, ifNotMeet);

  builder.SetInsertPoint(ifMeet);
  // the helper function side should have mutex lock or we can wrap this 
  // whole function with mutex calling from the profiler_helper.c
  // print the global counter
  builder.CreateCall(PrintIntFunction, {
  builder.CreateGlobalStringPtr("global counter"),loadCounter});
  // print the basic block vector
  builder.CreateCall(PrintArrayFunction, 
  {builder.CreateGlobalStringPtr("basic block vector"),
  basicBlockVector, 
  ConstantInt::get(Int32Ty, totalBasicBlockCount)});
  // print the function vector
  builder.CreateCall(PrintArrayFunction,
  {builder.CreateGlobalStringPtr("function vector"),
  functionVector,
  ConstantInt::get(Int32Ty, totalFunctionCount)});
  // print the basic block distance
  builder.CreateCall(PrintArrayFunction,
  {
  builder.CreateGlobalStringPtr("basic block distance"),
  basicBlockDist,
  ConstantInt::get(Int32Ty, totalBasicBlockCount)});
  // print the function distance
  builder.CreateCall(PrintArrayFunction,
  {
  builder.CreateGlobalStringPtr("function distance"),
  functionDist,
  ConstantInt::get(Int32Ty, totalFunctionCount)});
  // can set to atomic store later
  // reset the global counter
  builder.CreateCall(ResetCounterFunction, {counter});
  // reset the basic block distance
  builder.CreateCall(ResetArrayFunction, {basicBlockDist,
  ConstantInt::get(Int32Ty, totalBasicBlockCount)});
  // reset the function distance
  builder.CreateCall(ResetArrayFunction, {functionDist,
  ConstantInt::get(Int32Ty, totalFunctionCount)});
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

  Function* ResetArrayAtElementFunction = M.getFunction("reset_array_element_at");
  if (!ResetArrayAtElementFunction) {
    errs() << "Function reset_array_element_at not found\n";
  }
  Function* IncrementArrayElementAtFunction = M.getFunction("increment_array_element_at");
  if (!IncrementArrayElementAtFunction) {
    errs() << "Function increment_array_element_at not found\n";
  }


  totalFunctionCount = 0;
  totalBasicBlockCount = 0;
  uint32_t instLength = 0;
  uint32_t counter = 0;

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

      instLength = block.size();
      counter = 1;

      for (Instruction& inst : block) {
        if (counter >= instLength - 1) {
          basicBlock.lastNotBranchInstruction = &inst;
        }
        counter++;
      }
      basicBlock.basicBlockName = block.getName();
      basicBlock.basicBlockCount = instLength;
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
  GlobalVariable* functionVector = createGlobalUint64Array(M, "functionVector", totalFunctionCount);
  if (!functionVector) {
    errs() << "Global variable functionVector not found\n";
  }
  GlobalVariable* basicBlockDist = createGlobalUint64Array(M, "basicBlockDist", totalBasicBlockCount);
  if (!basicBlockDist) {
    errs() << "Global variable basicBlockDist not found\n";
  }
  GlobalVariable* functionDist = createGlobalUint64Array(M, "functionDist", totalFunctionCount);
  if (!functionDist) {
    errs() << "Global variable functionDist not found\n";
  }

  // Create the instrumentation function
  Function* instrumentationFunction = createInstrumentationFunction(M);
  InlineFunctionInfo ifi;

  for (auto item : basicBlockList) {
    builder.SetInsertPoint(item.lastNotBranchInstruction->getNextNode());

    if (!InlineFunction(*(builder.CreateCall(instrumentationFunction, {
      ConstantInt::get(Type::getInt32Ty(M.getContext()), item.basicBlockId),
      ConstantInt::get(Type::getInt32Ty(M.getContext()), item.functionId),
      ConstantInt::get(Type::getInt64Ty(M.getContext()), item.basicBlockCount)
    })), ifi).isSuccess()) {
      errs() << "Failed to inline function\n";
    }
    if (item.ifStartOfFunction) {
      if (!InlineFunction(*(builder.CreateCall(ResetArrayAtElementFunction,
      {
        functionDist,
        ConstantInt::get(Type::getInt32Ty(M.getContext()), item.functionId)
      })), ifi).isSuccess()) {
        errs() << "Failed to inline function\n";
      }
      if (!InlineFunction(*(builder.CreateCall(
        IncrementArrayElementAtFunction,
        {
          functionVector,
          ConstantInt::get(Type::getInt32Ty(M.getContext()), item.functionId)
        }
      )), ifi).isSuccess()) {
        errs() << "Failed to inline function\n";
      }
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

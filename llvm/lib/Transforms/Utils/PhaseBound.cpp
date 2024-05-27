#include "llvm/Transforms/Utils/PhaseBound.h"

namespace llvm {

bool PhaseBoundPass::emptyFunction(Function &F) {
    int count = 0;
    for (auto &block : F) {
    for (auto &inst : block) {
        count++;
    }
    }
    return count == 0;
}

void PhaseBoundPass::getInformation(Module &M) {
    // I will use a super hacky way to get the information I need
    std::string filename = "/home/studyztp/stuffs/info.txt";
    std::ifstream readThisFile(filename);
    if (!readThisFile.is_open()) {
        errs() << "Could not open file: " << filename << "\n";
    }
    std::string line;
    getline (readThisFile, line);
    warmupMarkerFunctionId = static_cast<uint64_t>(std::stoi(line));
    getline (readThisFile, line);
    warmupMarkerBBId = static_cast<uint64_t>(std::stoi(line));
    getline (readThisFile, line);
    warmupMarkerCount = static_cast<uint64_t>(std::stoi(line));
    getline (readThisFile, line);
    startMarkerFunctionId = static_cast<uint64_t>(std::stoi(line));
    getline (readThisFile, line);
    startMarkerBBId = static_cast<uint64_t>(std::stoi(line));
    getline (readThisFile, line);
    startMarkerCount = static_cast<uint64_t>(std::stoi(line));
    getline (readThisFile, line);
    endMarkerFunctionId = static_cast<uint64_t>(std::stoi(line));
    getline (readThisFile, line);
    endMarkerBBId = static_cast<uint64_t>(std::stoi(line));
    getline (readThisFile, line);
    endMarkerCount = static_cast<uint64_t>(std::stoi(line));
    readThisFile.close();

    if (warmupMarkerFunctionId == 0 && warmupMarkerBBId == 0 && warmupMarkerCount == 0) {
        errs() << "No start marker found\n";
    } else {
        errs() << "warmupMarkerFunctionId: " << warmupMarkerFunctionId << "\n";
        errs() << "warmupMarkerBBId: " << warmupMarkerBBId << "\n";
        errs() << "warmupMarkerCount: " << warmupMarkerCount << "\n";
    }

    errs() << "startMarkerFunctionId: " << startMarkerFunctionId << "\n";
    errs() << "startMarkerBBId: " << startMarkerBBId << "\n";
    errs() << "startMarkerCount: " << startMarkerCount << "\n";
    errs() << "endMarkerFunctionId: " << endMarkerFunctionId << "\n";
    errs() << "endMarkerBBId: " << endMarkerBBId << "\n";
    errs() << "endMarkerCount: " << endMarkerCount << "\n";
    
}

void PhaseBoundPass::formBasicBlockList(Module& M) {
    totalFunctionCount = 0;
    totalBasicBlockCount = 0;

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
    for (auto& block : function) {
        basicBlock.basicBlockName = block.getName();
        basicBlock.basicBlockCount = block.size();
        basicBlock.basicBlockId = totalBasicBlockCount;
        basicBlock.function = &function;
        basicBlock.basicBlock = &block;
        totalBasicBlockCount++;

        if (basicBlock.functionId == startMarkerFunctionId && basicBlock.basicBlockId == startMarkerBBId) {
            basicBlock.ifStartMark = true;
        } else {
            basicBlock.ifStartMark = false;
        }
        if (basicBlock.functionId == endMarkerFunctionId && basicBlock.basicBlockId == endMarkerBBId) {
            basicBlock.ifEndMark = true;
        } else {
            basicBlock.ifEndMark = false;
        }
        if (basicBlock.functionId == warmupMarkerFunctionId && basicBlock.basicBlockId == warmupMarkerBBId) {
            basicBlock.ifWarmupMark = true;
        } else {
            basicBlock.ifWarmupMark = false;
        }

        basicBlockList.push_back(basicBlock);
    }
  }

}

Function* PhaseBoundPass::createMarkerFunction(Module& M, std::string functionName,
                        uint64_t threshold, std::string raiseFunction) {

    Function* printfFunction = M.getFunction("printf");
    if(!printfFunction) {
        errs() << "printf not found\n";
    }
    IRBuilder<> builder(M.getContext());
    Type* Int64Ty = Type::getInt64Ty(M.getContext());
    Type* Int1Ty = Type::getInt1Ty(M.getContext());
    FunctionType* functionType = FunctionType::get(builder.getVoidTy(), {}, false);
    Function* function = Function::Create(functionType, GlobalValue::ExternalLinkage, functionName, M);
    BasicBlock* entry = BasicBlock::Create(M.getContext(), "entry", function);
    BasicBlock* ifRasingBB = BasicBlock::Create(M.getContext(), "ifRasing", function);
    BasicBlock* ifNotRasingBB = BasicBlock::Create(M.getContext(), "ifNotRasing", function);
    BasicBlock* ifNotMeetBB = BasicBlock::Create(M.getContext(), "ifNotMeet", function);
    BasicBlock* ifMeetBB = BasicBlock::Create(M.getContext(), "ifMeet", function);
    builder.SetInsertPoint(entry);
    GlobalVariable* counter = new GlobalVariable(
        M,
        Int64Ty,
        false,
        GlobalValue::ExternalLinkage,
        ConstantInt::get(Int64Ty, 0),
        functionName + "_instructionCounter"
    );
    GlobalVariable* ifRasing = new GlobalVariable(
        M,
        Int1Ty,
        false,
        GlobalValue::ExternalLinkage,
        ConstantInt::get(Int1Ty, 1),
        functionName + "ifRasingBool"
    );
    Function* raiseFunctionObject = M.getFunction(raiseFunction);
    if(!raiseFunctionObject) {
        errs() << raiseFunction <<" not found\n";
    }
    if(!counter) {
        errs() << "counter not found\n";
    }
    Value *ifRasingValue = builder.CreateLoad(Int1Ty, ifRasing);
    builder.CreateCondBr(ifRasingValue, ifRasingBB, ifNotRasingBB);
    builder.SetInsertPoint(ifRasingBB);
    Value* counterValue = builder.CreateLoad(Int64Ty, counter);
    Value* newCounter = builder.CreateAdd(counterValue, ConstantInt::get(Int64Ty, 1));
    builder.CreateStore(newCounter, counter);
    newCounter = builder.CreateLoad(Int64Ty, counter);

    // debug printf here
    Value* formatString = builder.CreateGlobalStringPtr("%s Counter: %ld\n");
    Value* functionNameValue = builder.CreateGlobalStringPtr(functionName);
    Value* args[] = {formatString, functionNameValue, newCounter};
    builder.CreateCall(printfFunction, args);

    builder.CreateCondBr(builder.CreateICmpSGE(newCounter, ConstantInt::get(Int64Ty, threshold)), ifMeetBB, ifNotMeetBB);
    builder.SetInsertPoint(ifNotMeetBB);
    builder.CreateRetVoid();
    builder.SetInsertPoint(ifMeetBB);
    builder.CreateCall(raiseFunctionObject);
    builder.CreateStore(ConstantInt::get(Int1Ty, 0), ifRasing);
    builder.CreateRetVoid();
    builder.SetInsertPoint(ifNotRasingBB);
    builder.CreateRetVoid();
    return function;
}

PreservedAnalyses PhaseBoundPass::run(Module &M, ModuleAnalysisManager &AM) 
{

    std::error_code EC;
    raw_fd_ostream out("VerifyingCheck.txt", EC, sys::fs::OF_Text);
    if (EC) {
        errs() << "Could not open file: " << EC.message() << "\n";
    }

    getInformation(M);

    IRBuilder<> builder(M.getContext());
    Type *Int64Ty = Type::getInt64Ty(M.getContext());

    formBasicBlockList(M);


    for (auto item : basicBlockList) {
        if(item.ifStartMark) {
            errs() << "Start marker found\n";
            Function* startFunction = createMarkerFunction(M, "start_function", startMarkerCount, "start_marker");
            if (item.basicBlock->getTerminator()) {
                builder.SetInsertPoint(item.basicBlock->getTerminator());
            } else {
                errs() << "Could not find terminator point for fucntion " << item.functionName << " bbid " << item.basicBlockId << "\n";
                builder.SetInsertPoint(item.basicBlock->getFirstInsertionPt());
            }
            builder.CreateCall(startFunction);
        }
        if(item.ifEndMark) {
            errs() << "End marker found\n";
            Function* endFunction = createMarkerFunction(M, "end_function", endMarkerCount, "end_marker");
            if (item.basicBlock->getTerminator()) {
                builder.SetInsertPoint(item.basicBlock->getTerminator());
            } else {
                errs() << "Could not find terminator point for fucntion " << item.functionName << " bbid " << item.basicBlockId << "\n";
                builder.SetInsertPoint(item.basicBlock->getFirstInsertionPt());
            }
            builder.CreateCall(endFunction);
        }
        if(item.ifWarmupMark) {
            errs() << "Warmup marker found\n";
            Function* warmupFunction = createMarkerFunction(M, "warmup_function", warmupMarkerCount, "warmup_marker");
            if (item.basicBlock->getTerminator()) {
                builder.SetInsertPoint(item.basicBlock->getTerminator());
            } else {
                errs() << "Could not find terminator point for fucntion " << item.functionName << " bbid " << item.basicBlockId << "\n";
                builder.SetInsertPoint(item.basicBlock->getFirstInsertionPt());
            }
            builder.CreateCall(warmupFunction);
        }
    }

    out << "[functionID:functionName] [basicBlockID:basicBlockName] [basicBlockCount] \n";

    for (auto item : basicBlockList) {
        if (item.ifStartMark) {
            out << "Start marker found\n";
        }
        if (item.ifEndMark) {
            out << "End marker found\n";
        }
        if (item.ifWarmupMark) {
            out << "Warmup marker found\n";
        }
        out << "[" << item.functionId << ":" << item.functionName << "] ["  
        << item.basicBlockId <<":"<< item.basicBlockName << "] [" 
        << item.basicBlockCount << "]\n";
    }

    out.close();

    return PreservedAnalyses::all();
}

}

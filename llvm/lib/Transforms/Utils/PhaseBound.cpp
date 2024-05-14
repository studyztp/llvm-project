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
    std::string filename = "/home/studyztp/test_ground/experiments/hardware-profiling/ori_NPB/NPB3.4.2/NPB3.4-OMP/info.txt";
    std::ifstream readThisFile(filename);
    if (!readThisFile.is_open()) {
        errs() << "Could not open file: " << filename << "\n";
    }
    std::string line;
    getline (readThisFile, line);
    startMarkerFunctionId = static_cast<uint64_t>(std::stoi(line));
    getline (readThisFile, line);
    startMarkerBBId = static_cast<uint64_t>(std::stoi(line));
    getline (readThisFile, line);
    endMarkerFunctionId = static_cast<uint64_t>(std::stoi(line));
    getline (readThisFile, line);
    endMarkerBBId = static_cast<uint64_t>(std::stoi(line));
    getline (readThisFile, line);
    startMarkerCount = static_cast<uint64_t>(std::stoi(line));
    getline (readThisFile, line);
    endMarkerCount = static_cast<uint64_t>(std::stoi(line));
    readThisFile.close();

    errs() << "startMarkerFunctionId: " << startMarkerFunctionId << "\n";
    errs() << "startMarkerBBId: " << startMarkerBBId << "\n";
    errs() << "endMarkerFunctionId: " << endMarkerFunctionId << "\n";
    errs() << "endMarkerBBId: " << endMarkerBBId << "\n";
    errs() << "startMarkerCount: " << startMarkerCount << "\n";
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

        basicBlockList.push_back(basicBlock);
    }
  }

}

Function* createMarkerFunction(Module& M, std::string functionName,
                        uint64_t threshold, Function* raiseFunction) {
    IRBuilder<> builder(M.getContext());
    Type* Int64Ty = Type::getInt64Ty(M.getContext());
    FunctionType* functionType = FunctionType::get(builder.getVoidTy(), {}, false);
    Function* function = Function::Create(functionType, GlobalValue::ExternalLinkage, functionName, M);
    BasicBlock* entry = BasicBlock::Create(M.getContext(), "entry", function);
    BasicBlock* ifNotMeet = BasicBlock::Create(M.getContext(), "ifNotMeet", function);
    BasicBlock* ifMeet = BasicBlock::Create(M.getContext(), "ifMeet", function);
    builder.SetInsertPoint(entry);
    GlobalVariable* counter = new GlobalVariable(
        M,
        Int64Ty,
        false,
        GlobalValue::ExternalLinkage,
        ConstantInt::get(Int64Ty, 0),
        "instructionCounter"
    );
    if(!counter) {
        errs() << "counter not found\n";
    }
    Value* counterValue = builder.CreateLoad(Int64Ty, counter);
    Value* newCounter = builder.CreateAdd(counterValue, ConstantInt::get(Int64Ty, 1));
    builder.CreateStore(newCounter, counter);
    newCounter = builder.CreateLoad(Int64Ty, counter);
    builder.CreateCondBr(builder.CreateICmpSGE(newCounter, ConstantInt::get(Int64Ty, threshold)), ifMeet, ifNotMeet);
    builder.SetInsertPoint(ifNotMeet);
    builder.CreateRetVoid();
    builder.SetInsertPoint(ifMeet);
    builder.CreateCall(raiseFunction);
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
    GlobalVariable* startMarkerCounter = new GlobalVariable(
        M,
        Int64Ty,
        false,
        GlobalValue::ExternalLinkage,
        ConstantInt::get(Int64Ty, 0),
        "instructionCounter"
    );
    if(!startMarkerCounter) {
        errs() << "startMarkerCounter not found\n";
    }
    GlobalVariable* endMarkerCounter = new GlobalVariable(
        M,
        Int64Ty,
        false,
        GlobalValue::ExternalLinkage,
        ConstantInt::get(Int64Ty, 0),
        "instructionCounter"
    );
    if(!endMarkerCounter) {
        errs() << "endMarkerCounter not found\n";
    }

    formBasicBlockList(M);


    for (auto item : basicBlockList) {
        if(item.ifStartMark) {
            errs() << "Start marker found\n";
            Function* start_marker = M.getFunction("start_marker");
            if(!start_marker) {
                errs() << "start_marker not found\n";
            }
            Function* startFunction = createMarkerFunction(M, "start_function", startMarkerCount, start_marker);
            builder.SetInsertPoint(item.basicBlock->getFirstInsertionPt());
            builder.CreateCall(startFunction);
        }
        if(item.ifEndMark) {
            errs() << "End marker found\n";
            Function* end_marker = M.getFunction("end_marker");
            if(!end_marker) {
                errs() << "end_marker not found\n";
            }
            Function* endFunction = createMarkerFunction(M, "end_function", endMarkerCount, end_marker);
            builder.SetInsertPoint(item.basicBlock->getFirstInsertionPt());
            builder.CreateCall(endFunction);
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
        out << "[" << item.functionId << ":" << item.functionName << "] ["  
        << item.basicBlockId <<":"<< item.basicBlockName << "] [" 
        << item.basicBlockCount << "]\n";
    }

    out.close();

    return PreservedAnalyses::all();
}

}

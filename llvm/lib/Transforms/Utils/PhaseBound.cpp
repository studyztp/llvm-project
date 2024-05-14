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
    startMarkerFunctionId = std::stoi(line);
    getline (readThisFile, line);
    startMarkerBBId = std::stoi(line);
    getline (readThisFile, line);
    endMarkerFunctionId = std::stoi(line);
    getline (readThisFile, line);
    endMarkerBBId = std::stoi(line);
    getline (readThisFile, line);
    startMarkerCount = std::stoi(line);
    getline (readThisFile, line);
    endMarkerCount = std::stoi(line);
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

    for (Function &F : M) {
        for (BasicBlock &BB : F) {
            if (emptyFunction(F) || F.isDeclaration()) 
            {
                continue;
            }
            if (std::find(exclude_functions.begin(), exclude_functions.end(), F.getName()) != exclude_functions.end()) {
                errs() << "Skipping function: " << F.getName() << "\n";
                continue;
            }
            basicBlockInfo temp;
            temp.functionName = F.getName();
            temp.functionId = totalFunctionCount;
            temp.basicBlockName = BB.getName();
            temp.basicBlockCount = 0;
            temp.basicBlockId = totalBasicBlockCount;
            temp.basicBlock = &BB;
            temp.function = &F;
            if(startMarkerBBId == totalBasicBlockCount && startMarkerFunctionId == totalFunctionCount) {
                temp.ifStartMark = true;
            } else {
                temp.ifStartMark = false;
            }
            
            if(endMarkerBBId == totalBasicBlockCount && endMarkerFunctionId == totalFunctionCount) {
                temp.ifEndMark = true;
            } else {
                temp.ifEndMark = false;
            }

            basicBlockList.push_back(temp);
            totalBasicBlockCount++;
        }
        totalFunctionCount++;
    }

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
            builder.SetInsertPoint(item.basicBlock->getFirstInsertionPt());
            Value* counter = builder.CreateLoad(Int64Ty, startMarkerCounter);
            Value* newCounter = builder.CreateAdd(counter, ConstantInt::get(Int64Ty, 1));
            builder.CreateStore(newCounter, startMarkerCounter);
            BasicBlock* StartIfNotMeet = BasicBlock::Create(M.getContext(), "StartIfNotMeet", item.function);
            BasicBlock* StartIfMeet = BasicBlock::Create(M.getContext(), "StartIfMeet", item.function);
            newCounter = builder.CreateLoad(Int64Ty, startMarkerCounter);
            builder.CreateCondBr(builder.CreateICmpEQ(newCounter, ConstantInt::get(Int64Ty, startMarkerCount)), StartIfMeet, StartIfNotMeet);
            builder.SetInsertPoint(StartIfNotMeet);
            builder.CreateRetVoid();
            builder.SetInsertPoint(StartIfMeet);
            Function* roi_begin = M.getFunction("roi_begin_");
            if(!roi_begin) {
                errs() << "roi_begin_ not found\n";
            }
            builder.CreateCall(roi_begin);
            builder.CreateRetVoid();
        }
        if(item.ifEndMark) {
            errs() << "End marker found\n";
            builder.SetInsertPoint(item.basicBlock->getFirstInsertionPt());
            Value* counter = builder.CreateLoad(Int64Ty, endMarkerCounter);
            Value* newCounter = builder.CreateAdd(counter, ConstantInt::get(Int64Ty, 1));
            builder.CreateStore(newCounter, endMarkerCounter);
            BasicBlock* EndIfNotMeet = BasicBlock::Create(M.getContext(), "EndIfNotMeet", item.function);
            BasicBlock* EndIfMeet = BasicBlock::Create(M.getContext(), "EndIfMeet", item.function);
            newCounter = builder.CreateLoad(Int64Ty, endMarkerCounter);
            builder.CreateCondBr(builder.CreateICmpEQ(newCounter, ConstantInt::get(Int64Ty, endMarkerCount)), EndIfMeet, EndIfNotMeet);
            builder.SetInsertPoint(EndIfNotMeet);
            builder.CreateRetVoid();
            builder.SetInsertPoint(EndIfMeet);
            Function* roi_end = M.getFunction("roi_end_");
            if(!roi_end) {
                errs() << "roi_end_ not found\n";
            }
            builder.CreateCall(roi_end);
            builder.CreateRetVoid();
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

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

cl::opt<std::string> PhaseBoundInputFilename(
    "phase-bound-input-file",
    cl::desc("<input file>"), 
    cl::ValueRequired
);

cl::opt<std::string> PhaseBoundBBOrderFilename(
    "phase-bound-bb-order-file",
    cl::desc("<bb order file>"), 
    cl::ValueRequired
);
                                
cl::opt<std::string> PhaseBoundOutputFilename(
    "phase-bound-output-file", 
    cl::init("basicBlockList.txt"),
    cl::desc("<output file>"),
    cl::ValueRequired
);

cl::opt<std::string> PhaseBoundLabelOnly(
    "phase-bound-label-only", 
    cl::init("false"),
    cl::desc("<true/false>")
);

uint64_t readLineAsUInt64(std::ifstream& file) {
    std::string line;
    if (!std::getline(file, line)) {
        errs() << "Could not read line\n";
        return 0;
    }
    return static_cast<uint64_t>(std::stoull(line));
}

void PhaseBoundPass::getInformation(Module &M) {

    std::string filename = PhaseBoundInputFilename.c_str();
    std::ifstream readThisFile(filename);
    if (!readThisFile.is_open()) {
        errs() << "Could not open file: " << filename << "\n";
        return;
    }

    warmupMarkerFunctionId = readLineAsUInt64(readThisFile);
    warmupMarkerBBId = readLineAsUInt64(readThisFile);
    warmupMarkerCount = readLineAsUInt64(readThisFile);
    startMarkerFunctionId = readLineAsUInt64(readThisFile);
    startMarkerBBId = readLineAsUInt64(readThisFile);
    startMarkerCount = readLineAsUInt64(readThisFile);
    endMarkerFunctionId = readLineAsUInt64(readThisFile);
    endMarkerBBId = readLineAsUInt64(readThisFile);
    endMarkerCount = readLineAsUInt64(readThisFile);

    readThisFile.close();

    if (warmupMarkerFunctionId == 0 
            && warmupMarkerBBId == 0 
            && warmupMarkerCount == 0) {
        hasWarmupMarker = false;
        errs() << "No warmup marker found so warmup marker at roi begin\n";
        Function* roiBegin = M.getFunction("roi_begin_");
        if (!roiBegin) {
            errs() << "Function roi_begin_ not found\n";
        }
        Function* warmUpMarkerHookFunction = M.getFunction("warmup_hook");
        IRBuilder<> builder(M.getContext());
        if (roiBegin->back().getTerminator()) {
            builder.SetInsertPoint(roiBegin->back().getTerminator());
        } else {
            errs() << "Could not find terminator point for roiBegin\n";
            builder.SetInsertPoint(roiBegin->back().getFirstInsertionPt());
        }
        builder.CreateCall(warmUpMarkerHookFunction);
        if (startMarkerFunctionId == 0 
                && startMarkerBBId == 0 
                && startMarkerCount == 0) {
            foundStartMarker = true;
            Function* startMarkerHookFunction = M.getFunction("start_hook");
            if (!startMarkerHookFunction) {
                errs() << "Function startHook not found\n";
            }
            builder.CreateCall(startMarkerHookFunction);
        }
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

    std::string filename = PhaseBoundBBOrderFilename.c_str(); 
    std::ifstream readThisFile(filename);
    if (!readThisFile.is_open()) {
        errs() << "Could not open file: " << filename << "\n";
        return;
    }

    std::string line;
    std::string token;

    char start = '[';
    char middle = ':';
    char end = ']';

    if(std::getline(readThisFile, line)) {
        errs() << "reading basic block order file\n";
    } else {
        errs() << "Could not read line\n";
        return;
    }

    while(std::getline(readThisFile, line)) {
        std::istringstream current_line(line);
        std::getline(current_line, token, start);
        std::getline(current_line, token, middle);
        // errs() << "Function ID: " << token << "\n";
        uint32_t function_ref = std::stoi(token);
        std::getline(current_line, token, end);
        // errs() << "Function Name: " << token << "\n";
        std::string function_name = token;
        Function* function = M.getFunction(function_name);
        if (!function) {
            errs() << "Could not find function: " << function_name << "\n";
            continue;
        }
        for (auto& block: *function) {
            if(!std::getline(current_line, token, start)) {
                break;
            }
            basicBlockInfo basicBlock;
            std::getline(current_line, token, middle);
            // errs() << "Basic Block ID: " << token << "\n";
            basicBlock.basicBlockId = std::stoi(token);
            std::getline(current_line, token, middle);
            // errs() << "Basic Block Name: " << token << "\n";
            basicBlock.basicBlockName = token;
            std::getline(current_line, token, end);
            // errs() << "Basic Block Count: " << token << "\n";
            basicBlock.basicBlockCount = std::stoull(token);
            basicBlock.functionName = function_name;
            basicBlock.functionId = function_ref;
            basicBlock.basicBlock = &block;
            basicBlock.function = &(*function);

            basicBlock.ifStartMark = (
                !foundStartMarker &&
                basicBlock.functionId == startMarkerFunctionId && 
                basicBlock.basicBlockId == startMarkerBBId);
            
            basicBlock.ifEndMark = (
                basicBlock.functionId == endMarkerFunctionId && 
                basicBlock.basicBlockId == endMarkerBBId);

            basicBlock.ifWarmupMark = (
                hasWarmupMarker && 
                basicBlock.functionId == warmupMarkerFunctionId && 
                basicBlock.basicBlockId == warmupMarkerBBId);

            basicBlockList.push_back(basicBlock);

            // extra check
            if (basicBlock.basicBlockName != block.getName()) {
                errs() << "For basic block: " << basicBlock.basicBlockId
                    << "Basic block name mismatch: " << 
                    basicBlock.basicBlockName << " " 
                    << block.getName().str() << "\n";
            }
            if (basicBlock.basicBlockCount != block.size()) {
                errs() << "For basic block: " << basicBlock.basicBlockId
                    << "Basic block count mismatch: " << 
                    basicBlock.basicBlockCount << " " 
                    << block.size() << "\n";
            }
        }
    }

    readThisFile.close(); 
}

PreservedAnalyses PhaseBoundPass::run(Module &M, ModuleAnalysisManager &AM) 
{
    std::error_code EC;

    raw_fd_ostream out(PhaseBoundOutputFilename.c_str(), EC, sys::fs::OF_Text);
    if (EC) {
        errs() << "Could not open file: " << EC.message() << "\n";
    }

    if (strcmp(PhaseBoundLabelOnly.c_str(), "true") == 0) {
        labelOnly = true;
    }

    getInformation(M);

    IRBuilder<> builder(M.getContext());
    Type *Int64Ty = Type::getInt64Ty(M.getContext());

    formBasicBlockList(M);

    if (!labelOnly) {
        Function *roiBegin = M.getFunction("roi_begin_");
        if (!roiBegin) {
            errs() << "Function roi_begin_ not found\n";
        }
        Function* setupThresholdsFunction = M.getFunction("setup_threshold");
        if (!setupThresholdsFunction) {
            errs() << "Function setupThresholds not found\n";
        }
        builder.SetInsertPoint(roiBegin->front().getFirstInsertionPt());
        builder.CreateCall(setupThresholdsFunction, {
            ConstantInt::get(Int64Ty, warmupMarkerCount),
            ConstantInt::get(Int64Ty, startMarkerCount),
            ConstantInt::get(Int64Ty, endMarkerCount)
        });
    }

    if (labelOnly) {
        Function* startMarkerHookFunction = FunctionType::get(builder.getVoidTy(), false);
        InlineAsm *IA = InlineAsm::get(Ty, 
            "Start Marker:\n\t",            // Label the current location
            "",                             
            /*hasSideEffects*/ true,
            /*isAlignStack*/ false,
            InlineAsm::AD_ATT);   
        Function* endMarkerHookFunction = FunctionType::get(builder.getVoidTy(), false);
        InlineAsm *IA = InlineAsm::get(Ty, 
            "End Marker:\n\t",            // Label the current location
            "",                             
            /*hasSideEffects*/ true,
            /*isAlignStack*/ false,
            InlineAsm::AD_ATT);
        Function* warmupMarkerHookFunction = FunctionType::get(builder.getVoidTy(), false);
        InlineAsm *IA = InlineAsm::get(Ty, 
            "Warmup Marker:\n\t",            // Label the current location
            "",                             
            /*hasSideEffects*/ true,
            /*isAlignStack*/ false,
            InlineAsm::AD_ATT);
    } else {
        Function* startMarkerHookFunction = M.getFunction("start_hook");
        if (!startMarkerHookFunction) {
            errs() << "Function startHook not found\n";
        }
        Function* endMarkerHookFunction = M.getFunction("end_hook");
        if (!endMarkerHookFunction) {
            errs() << "Function endHook not found\n";
        }
        Function* warmupMarkerHookFunction = M.getFunction("warmup_hook");
        if (!warmupMarkerHookFunction) {
            errs() << "Function warmUpHook not found\n";
        }
    }

    for (auto item : basicBlockList) {
        if(item.ifStartMark) {
            errs() << "Start marker found\n";
            if (item.basicBlock->getTerminator()) {
                builder.SetInsertPoint(item.basicBlock->getTerminator());
            } else {
                errs() << "Could not find terminator point for fucntion " << item.functionName << " bbid " << item.basicBlockId << "\n";
                builder.SetInsertPoint(item.basicBlock->getFirstInsertionPt());
            }
            builder.CreateCall(startMarkerHookFunction);
        }
        if(item.ifEndMark) {
            errs() << "End marker found\n";
            if (item.basicBlock->getTerminator()) {
                builder.SetInsertPoint(item.basicBlock->getTerminator());
            } else {
                errs() << "Could not find terminator point for fucntion " << item.functionName << " bbid " << item.basicBlockId << "\n";
                builder.SetInsertPoint(item.basicBlock->getFirstInsertionPt());
            }
            builder.CreateCall(endMarkerHookFunction);
        }
        if(item.ifWarmupMark) {
            errs() << "Warmup marker found\n";
            if (item.basicBlock->getTerminator()) {
                builder.SetInsertPoint(item.basicBlock->getTerminator());
            } else {
                errs() << "Could not find terminator point for fucntion " << item.functionName << " bbid " << item.basicBlockId << "\n";
                builder.SetInsertPoint(item.basicBlock->getFirstInsertionPt());
            }
            builder.CreateCall(warmUpMarkerHookFunction);
        }
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

}

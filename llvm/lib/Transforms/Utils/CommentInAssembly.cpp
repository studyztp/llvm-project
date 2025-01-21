#include "llvm/Transforms/Utils/CommentInAssembly.h"

namespace llvm {
PreservedAnalyses CommentInAssemblyPass::run(Module &M, ModuleAnalysisManager &AM) 
{
    IRBuilder<> builder(M.getContext());
    Function *testing = M.getFunction("testing");
    if (!testing) {
        errs() << "Function testing not found\n";
        return PreservedAnalyses::all();
    }
    for (auto &BB : *testing) {
        if (BB.getTerminator()) {
            builder.SetInsertPoint(BB.getTerminator());
        } else {
            errs() << "Could not find terminator point for function " 
                  << testing->getName() << " BB: " << BB.getName() << "\n";
            builder.SetInsertPoint(&BB);
        }
        
        FunctionType *Ty = FunctionType::get(builder.getVoidTy(), false);
        InlineAsm *IA = InlineAsm::get(Ty, 
            "BB_marker:\n\t",            // Label the current location
            "",                             
            /*hasSideEffects*/ true,
            /*isAlignStack*/ false,
            InlineAsm::AD_ATT);   
        builder.CreateCall(IA);
        break;
    }
    
    return PreservedAnalyses::all();
}
}

cmake -S llvm -B build -G "Unix Makefiles" \
-DLLVM_ENABLE_PROJECTS="clang;flang;openmp;lld" \
-DCMAKE_INSTALL_PREFIX=/scr/studyztp/compiler/llvm-dir \
-DCMAKE_BUILD_TYPE=Release -DLLVM_TARGETS_TO_BUILD=all


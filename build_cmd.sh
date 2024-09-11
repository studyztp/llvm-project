cmake -S llvm -B build -G "Unix Makefiles" \
-DLLVM_ENABLE_PROJECTS="clang;flang;openmp;lld" \
-DCMAKE_INSTALL_PREFIX=$1 \
-DCMAKE_BUILD_TYPE=Release -DLLVM_TARGETS_TO_BUILD=all


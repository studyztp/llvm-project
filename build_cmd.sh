if [ "$#" -eq 0 ]; then
	echo "Error: missing install prefix argument." >&2
	echo "Usage: $0 <install_prefix>" >&2
	exit 1
fi

if [ "$#" -gt 1 ]; then
	echo "Error: too many arguments; expected only install prefix." >&2
	echo "Usage: $0 <install_prefix>" >&2
	exit 1
fi

install_prefix="$1"

cmake -S llvm -B build -G "Unix Makefiles" \
-DLLVM_ENABLE_PROJECTS="clang;flang;openmp;lld;compiler-rt" \
-DCMAKE_INSTALL_PREFIX="${install_prefix}" \
-DCMAKE_BUILD_TYPE=Release -DLLVM_TARGETS_TO_BUILD=all

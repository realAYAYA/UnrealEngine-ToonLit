@echo off
setlocal

set LLVM_VERSION=12.0.1

Set TREE_ROOT=%~dp0
Set LLVM_ROOT=%TREE_ROOT%\llvm-%LLVM_VERSION%
rmdir /s/q %LLVM_ROOT%
git clone https://github.com/llvm/llvm-project.git --branch llvmorg-%LLVM_VERSION% llvm-%LLVM_VERSION%

Set LLVM_BUILD=%LLVM_ROOT%\build
Set LLVM_INSTALL=%TREE_ROOT%\llvm
mkdir %LLVM_BUILD%
rmdir /s/q %LLVM_INSTALL%
mkdir %LLVM_INSTALL%
cd %LLVM_BUILD%
cmake -Thost=x64 -G "Visual Studio 16" -DCMAKE_INSTALL_PREFIX=%LLVM_INSTALL% -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_PROJECTS="clang" -DLLVM_ENABLE_DUMP=ON -DLLVM_ENABLE_ASSERTIONS=ON -DLLVM_INSTALL_UTILS=ON -DLLVM_TARGETS_TO_BUILD=AArch64;ARM;X86 -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=WebAssembly -DLLVM_LIT_TOOLS_DIR="C:\cygwin64\bin" ..\llvm
msbuild INSTALL.vcxproj /V:m /p:Platform=x64 /p:Configuration=Release /t:rebuild

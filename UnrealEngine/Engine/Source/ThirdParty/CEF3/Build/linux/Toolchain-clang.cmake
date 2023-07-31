set(CMAKE_SYSTEM_NAME Linux)

set(CMAKE_C_COMPILER /code/chromium/src/third_party/llvm-build/Release+Asserts/bin/clang)
set(CMAKE_CXX_COMPILER /code/chromium/src/third_party/llvm-build/Release+Asserts/bin/clang++)
set(CMAKE_LINKER  "/code/chromium/src/third_party/llvm-build/Release+Asserts/bin/ld.lld")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CEF_CXX_COMPILER_FLAGS ${CEF_CXX_COMPILER_FLAGS} -nostdinc++ -isystem /code/chromium/src/buildtools/third_party/libc++/trunk/include)
include_directories(/code/chromium/src/buildtools/third_party/libc++abi/trunk/include)
include_directories(/code/chromium/src/buildtools/third_party/libc++)

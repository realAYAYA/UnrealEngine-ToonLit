include($ENV{UE4_PHYSX_ROOT_DIR}/Externals/CMakeModules/Android/android.toolchain.cmake)

set(SOCKET_IMPL "../src/sock.c" CACHE TYPE INTERNAL FORCE)
set(DISABLE_TLS 0 CACHE TYPE INTERNAL FORCE)

# OpenSSL folder may or may not have different includes for each arch, so check which this version supports
if (NOT EXISTS "$ENV{UE4_OPENSSL_ROOT_DIR}/include/Android/$ENV{UE4_OPENSSL_ARCH}")
set(OPENSSL_PATH "$ENV{UE4_OPENSSL_ROOT_DIR}/include/Android" CACHE TYPE INTERNAL FORCE)
else()
set(OPENSSL_PATH "$ENV{UE4_OPENSSL_ROOT_DIR}/include/Android/$ENV{UE4_OPENSSL_ARCH}" CACHE TYPE INTERNAL FORCE)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-O0" CACHE TYPE INTERNAL FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "-O3" CACHE TYPE INTERNAL FORCE)

# add_definitions(-DXML_STATIC -DUSE_WEBSOCKETS)
add_definitions(-DXML_STATIC)

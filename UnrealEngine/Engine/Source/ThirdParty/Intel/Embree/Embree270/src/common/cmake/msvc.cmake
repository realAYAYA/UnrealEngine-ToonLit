## ======================================================================== ##
## Copyright 2009-2015 Intel Corporation                                    ##
##                                                                          ##
## Licensed under the Apache License, Version 2.0 (the "License");          ##
## you may not use this file except in compliance with the License.         ##
## You may obtain a copy of the License at                                  ##
##                                                                          ##
##     http://www.apache.org/licenses/LICENSE-2.0                           ##
##                                                                          ##
## Unless required by applicable law or agreed to in writing, software      ##
## distributed under the License is distributed on an "AS IS" BASIS,        ##
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. ##
## See the License for the specific language governing permissions and      ##
## limitations under the License.                                           ##
## ======================================================================== ##

# the /QxXXX flags are meant for the Intel Compiler, MSVC ignores them
SET(FLAGS_SSE2  "/QxSSE2")
SET(FLAGS_SSE3  "/QxSSE3")
SET(FLAGS_SSSE3 "/QxSSSE3")
SET(FLAGS_SSE41 "/DCONFIG_SSE41 /QxSSE4.1")
SET(FLAGS_SSE42 "/DCONFIG_SSE42 /QxSSE4.2")
SET(FLAGS_AVX   "/arch:AVX /DCONFIG_AVX")
# Intel Compiler 15, Update 1 unfortunately cannot handle /arch:AVX2
IF (COMPILER STREQUAL "ICC") # for scripts/regression.py to work with ICC
  SET(FLAGS_AVX2  "/DCONFIG_AVX2 /QxCORE-AVX2")
ELSE()
  SET(FLAGS_AVX2  "/arch:AVX2 /DCONFIG_AVX2 /QxCORE-AVX2")
ENDIF()
SET(FLAGS_AVX512 "")

SET(COMMON_CXX_FLAGS "/EHsc /MP /GR ")

SET(CMAKE_CXX_FLAGS_DEBUG          "${CMAKE_CXX_FLAGS_DEBUG} ${COMMON_CXX_FLAGS}")
SET(CMAKE_CXX_FLAGS_RELEASE        "${CMAKE_CXX_FLAGS_RELEASE}        ${COMMON_CXX_FLAGS} /Ox /fp:fast /Oi /Gy ")
SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${COMMON_CXX_FLAGS} /Ox /fp:fast /Oi /Gy ")

SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:4000000")
SET(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /DEBUG")
SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /STACK:4000000")
SET(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /DEBUG")

# optionally use static runtime library
OPTION(USE_STATIC_RUNTIME "Use the static version of the C/C++ runtime library." ON)
IF (USE_STATIC_RUNTIME)
  STRING(REPLACE "/MDd" "/MTd" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
  STRING(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELWITHDEBINFO})
  STRING(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})
ENDIF()

# remove define NDEBUG and instead set define DEBUG for config RelWithDebInfo
STRING(REPLACE "NDEBUG" "DEBUG" CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELWITHDEBINFO})

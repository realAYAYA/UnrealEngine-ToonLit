## ======================================================================== ##
## Copyright 2009-2017 Intel Corporation                                    ##
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

SET(FLAGS_SSE2   "-xsse2")
SET(FLAGS_SSE3   "-xsse3")
SET(FLAGS_SSSE3  "-xssse3")
SET(FLAGS_SSE41  "-xsse4.1")
SET(FLAGS_SSE42  "-xsse4.2")
SET(FLAGS_AVX    "-xAVX")
SET(FLAGS_AVX2   "-xCORE-AVX2")
SET(FLAGS_AVX512KNL "-xMIC-AVX512")
SET(FLAGS_AVX512SKX "-xCORE-AVX512")

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fPIC -std=c++11 -fvisibility-inlines-hidden -fvisibility=hidden -no-ansi-alias -fasm-blocks")

SET(CMAKE_CXX_FLAGS_DEBUG          "-DDEBUG  -DTBB_USE_DEBUG -g -O0")
SET(CMAKE_CXX_FLAGS_RELEASE        "-DNDEBUG                    -O3 -restrict -no-inline-max-total-size -inline-factor=200")
SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-DDEBUG  -DTBB_USE_DEBUG -g -O3 -restrict -no-inline-max-total-size -inline-factor=200")

# enable -static-intel and avoid to export ICC specific symbols from Embree
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-intel -no-intel-extensions")

#SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g -debug inline-debug-info")
#SET(CMAKE_EXE_LINKER_FLAGS "-g") 

IF (APPLE)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmacosx-version-min=10.7 ") # we only use MacOSX 10.7 features
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++"            ) # link against C++11 stdlib
ENDIF (APPLE)


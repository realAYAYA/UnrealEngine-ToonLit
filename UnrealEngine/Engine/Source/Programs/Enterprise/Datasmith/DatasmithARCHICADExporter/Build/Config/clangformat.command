#!/bin/sh

projectPath=`dirname "$0"`

cd "$projectPath/../"
clang-format -i ../Private/*.h
clang-format -i ../Private/*.cpp
clang-format -i ../Private/Utils/*.h
clang-format -i ../Private/Utils/*.cpp

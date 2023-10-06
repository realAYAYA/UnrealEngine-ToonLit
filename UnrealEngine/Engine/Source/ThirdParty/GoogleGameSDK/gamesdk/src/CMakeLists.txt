cmake_minimum_required(VERSION 3.10.1)
project(android_game_sdk C CXX)

project(android_game_sdk C CXX)
set(IgnoreOldToolchainWarning "${ANDROID_UNIFIED_HEADERS}")

if ("memory_advice" IN_LIST GAMESDK_LIBRARIES)
  add_subdirectory( ./memory_advice )
endif()

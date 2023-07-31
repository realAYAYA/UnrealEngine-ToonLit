cmake_minimum_required(VERSION 3.4.1)

add_subdirectory( ./swappy )
if (GAMESDK_BUILD_TUNINGFORK)
  add_subdirectory( ./tuningfork )
endif()

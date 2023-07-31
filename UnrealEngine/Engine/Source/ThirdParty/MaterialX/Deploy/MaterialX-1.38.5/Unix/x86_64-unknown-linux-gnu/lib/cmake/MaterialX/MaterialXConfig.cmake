#
# TM & (c) 2021 Lucasfilm Entertainment Company Ltd. and Lucasfilm Ltd.
# All rights reserved.  See LICENSE.txt for license.
#
# MaterialX CMake configuration file. Mostly auto-generated.
#

# Auto-generated content:

####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was MaterialXConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

# Gather MaterialX targets:
include("${CMAKE_CURRENT_LIST_DIR}/MaterialXTargets.cmake")

# Resource paths:
# MATERIALX_BASE_DIR         MaterialX root installation directory
# MATERIALX_STDLIB_DIR       Path to the MaterialX standard library directory
# MATERIALX_PYTHON_DIR       Path to MaterialX Python library
# MATERIALX_RESOURCES_DIR    Path to MaterialX Resources (sample data, mtlx etc)

set_and_check(MATERIALX_BASE_DIR "${PACKAGE_PREFIX_DIR}")
set_and_check(MATERIALX_STDLIB_DIR "${PACKAGE_PREFIX_DIR}/libraries")
if(OFF AND ON)
    set_and_check(MATERIALX_PYTHON_DIR "${PACKAGE_PREFIX_DIR}/python")
endif()
set_and_check(MATERIALX_RESOURCES_DIR "${PACKAGE_PREFIX_DIR}/resources")

check_required_components(MaterialX)

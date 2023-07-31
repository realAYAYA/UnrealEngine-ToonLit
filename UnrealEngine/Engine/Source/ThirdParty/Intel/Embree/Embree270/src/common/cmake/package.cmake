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

OPTION(ENABLE_INSTALLER "Switches between installer or ZIP file creation for 'make package'" ON)
MARK_AS_ADVANCED(ENABLE_INSTALLER)

IF (ENABLE_INSTALLER AND APPLE)
  #SET(CMAKE_MACOSX_RPATH ON)
  #SET(CMAKE_SKIP_INSTALL_RPATH OFF)
  #SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
  SET(CMAKE_INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib")
  SET(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
  #SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
  #SET(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
ELSE()
  SET(CMAKE_SKIP_INSTALL_RPATH ON)
ENDIF()

IF (NOT ENABLE_INSTALLER)
  SET(DOC_INSTALL_DIR doc)
  SET(TUTORIALS_INSTALL_DIR bin)
  SET(UTILITIES_INSTALL_DIR bin)
ELSEIF (WIN32)
  SET(DOC_INSTALL_DIR doc)
  SET(TUTORIALS_INSTALL_DIR bin)
  SET(UTILITIES_INSTALL_DIR bin)
ELSEIF (APPLE)
  SET(DOC_INSTALL_DIR ../../Applications/Embree2/documentation)
  SET(APPLICATION_INSTALL_DIR ../../Applications/Embree2)
  SET(TUTORIALS_INSTALL_DIR ../../Applications/Embree2/tutorials)
  SET(UTILITIES_INSTALL_DIR ../../Applications/Embree2/tutorials)
ELSE()
  SET(DOC_INSTALL_DIR share/doc/embree-${EMBREE_VERSION})
  SET(TUTORIALS_INSTALL_DIR bin/embree-${EMBREE_VERSION})
  SET(UTILITIES_INSTALL_DIR bin/embree-${EMBREE_VERSION})
ENDIF()

##############################################################
# Install Headers
##############################################################
INSTALL(DIRECTORY include/embree2 DESTINATION include COMPONENT devel)
CONFIGURE_FILE(include/embree2/rtcore.h rtcore.h @ONLY)
CONFIGURE_FILE(include/embree2/rtcore.isph rtcore.isph @ONLY)
INSTALL(FILES ${PROJECT_BINARY_DIR}/rtcore.h DESTINATION include/embree2 COMPONENT devel)
INSTALL(FILES ${PROJECT_BINARY_DIR}/rtcore.isph DESTINATION include/embree2 COMPONENT devel)

##############################################################
# Install Models
##############################################################
INSTALL(DIRECTORY tutorials/models DESTINATION "${TUTORIALS_INSTALL_DIR}" COMPONENT examples)

##############################################################
# Install Documentation
##############################################################

#FILE(GLOB DOC_FILES ${PROJECT_SOURCE_DIR}/embree-doc/docbin/*)
#INSTALL(FILES ${DOC_FILES} OPTIONAL DESTINATION ${DOC_INSTALL_DIR} COMPONENT lib)
INSTALL(FILES ${PROJECT_SOURCE_DIR}/LICENSE.txt DESTINATION ${DOC_INSTALL_DIR} COMPONENT lib)
INSTALL(FILES ${PROJECT_SOURCE_DIR}/CHANGELOG.md DESTINATION ${DOC_INSTALL_DIR} COMPONENT lib)
INSTALL(FILES ${PROJECT_SOURCE_DIR}/README.md DESTINATION ${DOC_INSTALL_DIR} COMPONENT lib)
INSTALL(FILES ${PROJECT_SOURCE_DIR}/readme.pdf DESTINATION ${DOC_INSTALL_DIR} COMPONENT lib)

SET(CPACK_NSIS_MENU_LINKS ${CPACK_NSIS_MENU_LINKS} "${DOC_INSTALL_DIR}/LICENSE.txt" "LICENSE")
SET(CPACK_NSIS_MENU_LINKS ${CPACK_NSIS_MENU_LINKS} "${DOC_INSTALL_DIR}/CHANGELOG.txt" "CHANGELOG")
SET(CPACK_NSIS_MENU_LINKS ${CPACK_NSIS_MENU_LINKS} "${DOC_INSTALL_DIR}/README.md" "README.md")
SET(CPACK_NSIS_MENU_LINKS ${CPACK_NSIS_MENU_LINKS} "${DOC_INSTALL_DIR}/readme.pdf" "readme.pdf")

# currently CMake does not support solution folders without projects
# SOURCE_GROUP("Documentation" FILES README.md CHANGELOG.md LICENSE.txt readme.pdf)

##############################################################
# Install scripts to set embree paths
##############################################################

IF (NOT ENABLE_INSTALLER)
  IF (WIN32)
  ELSEIF(APPLE)
    INSTALL(FILES ${PROJECT_SOURCE_DIR}/scripts/install_macosx/embree-vars.sh DESTINATION "." COMPONENT lib)
    INSTALL(FILES ${PROJECT_SOURCE_DIR}/scripts/install_macosx/embree-vars.csh DESTINATION "." COMPONENT lib)
  ELSE()
    INSTALL(FILES ${PROJECT_SOURCE_DIR}/scripts/install_linux/embree-vars.sh  DESTINATION "." COMPONENT lib)
    INSTALL(FILES ${PROJECT_SOURCE_DIR}/scripts/install_linux/embree-vars.csh DESTINATION "." COMPONENT lib)
  ENDIF()
ENDIF()

##############################################################
# Install Embree CMake Configuration
##############################################################
IF (ENABLE_INSTALLER)
  SET(EMBREE_CONFIG_VERSION ${EMBREE_VERSION})
ELSE()
  SET(EMBREE_CONFIG_VERSION ${EMBREE_VERSION_MAJOR})
ENDIF()

IF (WIN32)
  CONFIGURE_FILE(common/cmake/embree-config-windows.cmake embree-config.cmake @ONLY)
ELSEIF (APPLE)
  CONFIGURE_FILE(common/cmake/embree-config-macosx.cmake embree-config.cmake @ONLY)
  IF (ENABLE_INSTALLER)
    CONFIGURE_FILE(scripts/install_macosx/uninstall.command uninstall.command @ONLY)
    INSTALL(PROGRAMS "${PROJECT_BINARY_DIR}/uninstall.command" DESTINATION ${APPLICATION_INSTALL_DIR} COMPONENT lib)
  ENDIF()
ELSE()
  CONFIGURE_FILE(common/cmake/embree-config-linux.cmake embree-config.cmake @ONLY)
ENDIF()

CONFIGURE_FILE(common/cmake/embree-config-version.cmake embree-config-version.cmake @ONLY)

INSTALL(FILES "${PROJECT_BINARY_DIR}/embree-config.cmake" DESTINATION "lib/cmake/embree-${EMBREE_VERSION}" COMPONENT devel)
INSTALL(FILES "${PROJECT_BINARY_DIR}/embree-config-version.cmake" DESTINATION "lib/cmake/embree-${EMBREE_VERSION}" COMPONENT devel)

##############################################################
# CPack specific stuff
##############################################################

SET(CPACK_PACKAGE_NAME "Embree")
SET(CPACK_PACKAGE_FILE_NAME "embree-${EMBREE_VERSION}")
#SET(CPACK_PACKAGE_ICON ${PROJECT_SOURCE_DIR}/embree-doc/images/icon.png)
#SET(CPACK_PACKAGE_RELOCATABLE TRUE)

SET(CPACK_PACKAGE_VERSION_MAJOR ${EMBREE_VERSION_MAJOR})
SET(CPACK_PACKAGE_VERSION_MINOR ${EMBREE_VERSION_MINOR})
SET(CPACK_PACKAGE_VERSION_PATCH ${EMBREE_VERSION_PATCH})
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Embree: High Performance Ray Tracing Kernels")
SET(CPACK_PACKAGE_VENDOR "Intel Corporation")
SET(CPACK_PACKAGE_CONTACT embree_support@intel.com)

SET(CPACK_COMPONENT_LIB_DISPLAY_NAME "Library")
SET(CPACK_COMPONENT_LIB_DESCRIPTION "The Embree library including documentation.")

SET(XEONPHI_DESCRIPTION "For The Intel Xeon Phi coprocessor." )
SET(CPACK_COMPONENT_LIB_XEONPHI_NAME "Xeon Phi Library")
SET(CPACK_COMPONENT_LIB_XEONPHI_DESCRIPTION "${CPACK_COMPONENT_LIB_DESCRIPTION}\n${XEONPHI_DESCRIPTION}")

SET(CPACK_COMPONENT_DEVEL_DISPLAY_NAME "Development")
SET(CPACK_COMPONENT_DEVEL_DESCRIPTION "Header Files for C and ISPC required to develop applications with Embree.")

SET(CPACK_COMPONENT_EXAMPLES_DISPLAY_NAME "Examples")
SET(CPACK_COMPONENT_EXAMPLES_DESCRIPTION "Tutorials demonstrating how to use Embree.")

SET(CPACK_COMPONENT_EXAMPLES_XEONPHI_NAME "Xeon Phi Examples")
SET(CPACK_COMPONENT_EXAMPLES_XEONPHI_DESCRIPTION "${CPACK_COMPONENT_EXAMPLES_DESCRIPTION}\n${XEONPHI_DESCRIPTION}")

# dependencies between components
SET(CPACK_COMPONENT_LIB_XEONPHI_DEPENDS lib)
SET(CPACK_COMPONENT_EXAMPLES_DEPENDS lib)
SET(CPACK_COMPONENT_EXAMPLES_XEONPHI_DEPENDS lib_xeonphi)

# point to readme and license files
SET(CPACK_RESOURCE_FILE_README ${PROJECT_SOURCE_DIR}/README.md)
SET(CPACK_RESOURCE_FILE_LICENSE ${PROJECT_SOURCE_DIR}/LICENSE.txt)

# Windows specific settings
IF(WIN32)

  IF (CMAKE_SIZEOF_VOID_P EQUAL 8)
    SET(ARCH x64)
    SET(PROGRAMFILES "\$PROGRAMFILES64")
	SET(CPACK_PACKAGE_NAME "${CPACK_PACKAGE_NAME} x64")
  ELSE()
    SET(ARCH win32)
    SET(PROGRAMFILES "\$PROGRAMFILES")
	SET(CPACK_PACKAGE_NAME "${CPACK_PACKAGE_NAME} Win32")
  ENDIF()

  # NSIS specific settings
  IF (ENABLE_INSTALLER)
    SET(CPACK_GENERATOR NSIS)
    SET(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}.${ARCH}")
    SET(CPACK_COMPONENTS_ALL lib devel examples)
    SET(CPACK_NSIS_INSTALL_ROOT "${PROGRAMFILES}\\\\Intel")
    SET(CPACK_PACKAGE_INSTALL_DIRECTORY "Embree v${EMBREE_VERSION} ${ARCH}")
    SET(CPACK_NSIS_DISPLAY_NAME "Embree v${EMBREE_VERSION} ${ARCH}")
    SET(CPACK_NSIS_PACKAGE_NAME "Embree v${EMBREE_VERSION} ${ARCH}")
    SET(CPACK_NSIS_URL_INFO_ABOUT http://embree.github.io/)
    #SET(CPACK_NSIS_HELP_LINK http://embree.github.io/downloads.html#windows)
    SET(CPACK_NSIS_MUI_ICON ${PROJECT_SOURCE_DIR}/scripts/install_windows/icon32.ico)
    SET(CPACK_NSIS_CONTACT ${CPACK_PACKAGE_CONTACT})
	#SET(CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS ${CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS} "\n CreateDirectory \\\"$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\"")
	#SET(CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS ${CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS} "\n CreateDirectory \\\"$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\documentation\\\"")
	#SET(CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS ${CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS} "\n CreateDirectory \\\"$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\tutorials\\\" \n")
	#SET(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS  ${CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS} "\n ${UNINSTALL_LIST}\n RMDir \\\"$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\documentation\\\"")
	#SET(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS  ${CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS} "\n ${UNINSTALL_LIST}\n RMDir \\\"$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\tutorials\\\"")
	#SET(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS  ${CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS} "\n ${UNINSTALL_LIST}\n RMDir \\\"$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\"\n")
  ELSE()
    SET(CPACK_GENERATOR ZIP)
    SET(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}.${ARCH}.windows")
    SET(CPACK_MONOLITHIC_INSTALL 1)
  ENDIF()

# MacOSX specific settings
ELSEIF(APPLE)

  CONFIGURE_FILE(README.md README.txt)
  SET(CPACK_RESOURCE_FILE_README ${PROJECT_BINARY_DIR}/README.txt)

  IF (ENABLE_INSTALLER)
    SET(CPACK_GENERATOR PackageMaker)
    SET(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}.x86_64")
    #SET(CPACK_COMPONENTS_ALL lib devel examples)
    SET(CPACK_MONOLITHIC_INSTALL 1)
    SET(CPACK_PACKAGE_NAME embree-${EMBREE_VERSION})
    SET(CPACK_PACKAGE_VENDOR "intel") # creates short name com.intel.embree2.xxx in pkgutil
    SET(CPACK_OSX_PACKAGE_VERSION 10.7)
  ELSE()
    SET(CPACK_GENERATOR TGZ)
    SET(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}.x86_64.macosx")
    SET(CPACK_MONOLITHIC_INSTALL 1)
  ENDIF()

# Linux specific settings
ELSE()


  IF (ENABLE_INSTALLER)

    SET(CPACK_GENERATOR RPM)
    SET(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}-${CPACK_RPM_PACKAGE_RELEASE}.x86_64")
    SET(CPACK_COMPONENTS_ALL devel lib examples lib_xeonphi examples_xeonphi)
    SET(CPACK_RPM_COMPONENT_INSTALL ON)
    SET(CPACK_RPM_PACKAGE_LICENSE "ASL 2.0") # Apache Software License, Version 2.0
    SET(CPACK_RPM_PACKAGE_GROUP "Development/Libraries")
    #SET(CPACK_RPM_CHANGELOG_FILE "") # ChangeLog of the RPM; also CHANGELOG.md is not in the required format
    SET(CPACK_RPM_PACKAGE_ARCHITECTURE x86_64)
    SET(CPACK_RPM_PACKAGE_URL http://embree.github.io/)

    # post install and uninstall scripts
    SET(CPACK_RPM_lib_POST_INSTALL_SCRIPT_FILE ${PROJECT_SOURCE_DIR}/common/cmake/rpm_ldconfig.sh)
    SET(CPACK_RPM_lib_POST_UNINSTALL_SCRIPT_FILE ${PROJECT_SOURCE_DIR}/common/cmake/rpm_ldconfig.sh)
    SET(CPACK_RPM_lib_xeonphi_POST_INSTALL_SCRIPT_FILE ${PROJECT_SOURCE_DIR}/common/cmake/rpm_ldconfig.sh)
    SET(CPACK_RPM_lib_xeonphi_POST_UNINSTALL_SCRIPT_FILE ${PROJECT_SOURCE_DIR}/common/cmake/rpm_ldconfig.sh)
  ELSE()
  
    SET(CPACK_GENERATOR TGZ)
    SET(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}.x86_64.linux")
    SET(CPACK_MONOLITHIC_INSTALL 1)
  ENDIF()
  
ENDIF()


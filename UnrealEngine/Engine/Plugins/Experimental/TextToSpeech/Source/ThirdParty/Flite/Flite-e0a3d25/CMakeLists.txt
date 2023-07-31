# Copyright Epic Games, Inc. All Rights Reserved.

cmake_minimum_required(VERSION 3.6)
project(flite C)

set(FLITE_BIN_ROOT ${CMAKE_BINARY_DIR}/../)

if (NOT DEFINED CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${FLITE_BIN_ROOT})
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${FLITE_BIN_ROOT})
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${FLITE_BIN_ROOT})
endif()

#we suppress all warnings associated with flite 
if(MSVC)
	add_compile_options("/W0" "/FS" "$<$<CONFIG:RELEASE>:/O2>")
else()
	add_compile_options("-w" "-g" "$<$<CONFIG:RELEASE>:-O3>")
endif()

	# We're building all of the individual subdirectories as Object libraries 
# This creates a pdb per object library. We set these variables to have them output to a single .pdb file located where teh .lib files will be generated
set(FLITE_COMPILE_PDB_NAME  "libFlite")
    set(CMAKE_COMPILE_PDB_OUTPUT_DIRECTORY ${FLITE_BIN_ROOT})

# we don't need the networking functionality of Flite so we'll disable sockets
add_definitions(-DCST_NO_SOCKETS)

#We want to use Unreal's memory allocation instead of regular malloc so we define this macro# to override the malloc functions found in cst_alloc.c 
add_definitions(-DCST_USER_MALLOC)

# we prevent cmake from apending lib to the satart of static libraries to ensure naming parity across platforms
set(CMAKE_STATIC_LIBRARY_PREFIX "")

add_subdirectory(src)
add_subdirectory(lang)

add_library(lib_main OBJECT
	# we don't compile flite_main.c to exclude unix headers for dependencies 
	#If the below 2 files are missing 
	# run setup_voicelist_and_langlist.sh in WSL (Windows) or command line (unix)
	main/flite_lang_list.c
	main/flite_voice_list.c
)

target_include_directories(lib_main
    PUBLIC include
)

set_target_properties(lib_main PROPERTIES COMPILE_PDB_NAME "${FLITE_COMPILE_PDB_NAME}")
add_library(libFlite STATIC
	$<TARGET_OBJECTS:lib_audio>
	$<TARGET_OBJECTS:lib_hrg>
	$<TARGET_OBJECTS:lib_regex>
	$<TARGET_OBJECTS:lib_stats>
    $<TARGET_OBJECTS:lib_utils>
	$<TARGET_OBJECTS:lib_cg>
	$<TARGET_OBJECTS:lib_lexicon>
	$<TARGET_OBJECTS:lib_speech>
	$<TARGET_OBJECTS:lib_synth>
	$<TARGET_OBJECTS:lib_wavesynth>
	$<TARGET_OBJECTS:lib_cmu_grapheme_lang>
	$<TARGET_OBJECTS:lib_cmu_grapheme_lex>
	$<TARGET_OBJECTS:lib_cmu_indic_lang>
	$<TARGET_OBJECTS:lib_cmu_indic_lex>
	$<TARGET_OBJECTS:lib_cmulex>
	$<TARGET_OBJECTS:lib_cmu_us_kal>
	$<TARGET_OBJECTS:lib_cmu_us_kal16>
	$<TARGET_OBJECTS:lib_cmu_us_slt>
	$<TARGET_OBJECTS:lib_cmu_us_rms>
	$<TARGET_OBJECTS:lib_cmu_us_awb>
	$<TARGET_OBJECTS:lib_cmu_time_awb>
	$<TARGET_OBJECTS:lib_usenglish>
		$<TARGET_OBJECTS:lib_main>
)

target_include_directories(libFlite
    PUBLIC include
)


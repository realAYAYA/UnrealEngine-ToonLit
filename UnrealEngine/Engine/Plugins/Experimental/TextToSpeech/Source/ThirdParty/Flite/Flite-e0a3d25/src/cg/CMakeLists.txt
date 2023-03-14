# Copyright Epic Games, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.6)
project(lib_cg)
add_library(lib_cg OBJECT
	cst_cg.c
	cst_mlsa.c
	cst_mlpg.c
	cst_vc.c
	cst_cg_load_voice.c
	cst_cg_dump_voice.c
	cst_cg_map.c
	cst_spamf0.c
)

target_include_directories(lib_cg
	PUBLIC .
	PUBLIC ../../include
)

target_link_libraries(lib_cg
	PUBLIC lib_stats
)

set_target_properties(lib_cg PROPERTIES COMPILE_PDB_NAME "${FLITE_COMPILE_PDB_NAME}")
#@TODOAccessibility: See if this is necessary 
if(${UNIX})
target_link_libraries(lib_cg
	PUBLIC m
)
endif()
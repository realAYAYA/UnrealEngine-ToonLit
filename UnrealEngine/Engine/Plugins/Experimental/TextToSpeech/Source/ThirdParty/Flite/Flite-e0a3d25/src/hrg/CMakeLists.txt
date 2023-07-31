# Copyright Epic Games, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.6)
project(lib_hrg)
add_library(lib_hrg OBJECT
	cst_utterance.c
	cst_relation.c
	cst_item.c
	cst_ffeature.c
	cst_rel_io.c
)

target_include_directories(lib_hrg
	PUBLIC . 
	PUBLIC ../../include
)

set_target_properties(lib_hrg PROPERTIES COMPILE_PDB_NAME "${FLITE_COMPILE_PDB_NAME}")
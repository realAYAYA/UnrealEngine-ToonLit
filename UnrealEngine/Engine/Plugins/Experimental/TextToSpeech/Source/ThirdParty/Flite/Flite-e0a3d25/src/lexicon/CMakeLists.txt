# Copyright Epic Games, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.6)
project(lib_lexicon C)
add_library(lib_lexicon OBJECT
	cst_lexicon.c
	cst_lts.c
	cst_lts_rewrites.c
)

target_include_directories(lib_lexicon 
	PUBLIC ../../include
)

set_target_properties(lib_lexicon PROPERTIES COMPILE_PDB_NAME "${FLITE_COMPILE_PDB_NAME}")
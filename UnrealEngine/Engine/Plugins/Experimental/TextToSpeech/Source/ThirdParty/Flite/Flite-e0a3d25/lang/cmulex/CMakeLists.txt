# Copyright Epic Games, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.6)
project(lib_cmulex C)
add_library(lib_cmulex OBJECT
	cmu_lex.c
	cmu_lex_data.c
	cmu_lex_entries.c
	cmu_lts_model.c
	cmu_lts_rules.c
	cmu_postlex.c
)


target_include_directories(lib_cmulex
	PUBLIC .
	PUBLIC ../../include
)

set_target_properties(lib_cmulex PROPERTIES COMPILE_PDB_NAME "${FLITE_COMPILE_PDB_NAME}")
# Copyright Epic Games, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.6)
project(lib_cmu_indic_lex)
add_library(lib_cmu_indic_lex OBJECT
	cmu_indic_lex.c
)

target_include_directories(lib_cmu_indic_lex
	PUBLIC .
	PUBLIC ../../include
)

set_target_properties(lib_cmu_indic_lex PROPERTIES COMPILE_PDB_NAME "${FLITE_COMPILE_PDB_NAME}")
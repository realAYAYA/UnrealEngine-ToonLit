# Copyright Epic Games, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.6)
project(lib_cmu_grapheme_lex C)
add_library(lib_cmu_grapheme_lex OBJECT
	cmu_grapheme_lex.c
	grapheme_unitran_tables.c
)

target_include_directories(lib_cmu_grapheme_lex
    PUBLIC .
    PUBLIC ../../include
)

set_target_properties(lib_cmu_grapheme_lex PROPERTIES COMPILE_PDB_NAME "${FLITE_COMPILE_PDB_NAME}")
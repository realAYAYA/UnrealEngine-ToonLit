# Copyright Epic Games, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.6)
project(lib_stats C)

add_library(lib_stats OBJECT
	cst_cart.c
	cst_viterbi.c
	cst_ss.c
)

target_include_directories(lib_stats 
	PUBLIC ../../include
)

set_target_properties(lib_stats PROPERTIES COMPILE_PDB_NAME "${FLITE_COMPILE_PDB_NAME}")
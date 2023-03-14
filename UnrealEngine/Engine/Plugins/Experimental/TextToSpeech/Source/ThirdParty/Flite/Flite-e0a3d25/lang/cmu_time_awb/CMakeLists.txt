# Copyright Epic Games, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.6)
project(lib_cmu_time_awb C)
add_library(lib_cmu_time_awb OBJECT
	cmu_time_awb.c
	cmu_time_awb_cart.c
	cmu_time_awb_clunits.c
	cmu_time_awb_lex_entry.c
	cmu_time_awb_lpc.c
	cmu_time_awb_mcep.c
)

target_include_directories(lib_cmu_time_awb
	PUBLIC .
	PUBLIC ../usenglish
	PUBLIC ../cmulex
PUBLIC ../../include
)

set_target_properties(lib_cmu_time_awb PROPERTIES COMPILE_PDB_NAME "${FLITE_COMPILE_PDB_NAME}")
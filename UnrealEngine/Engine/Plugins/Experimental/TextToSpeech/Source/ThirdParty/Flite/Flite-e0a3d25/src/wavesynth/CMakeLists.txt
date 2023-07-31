# Copyright Epic Games, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.6)
project(lib_wavesynth C)
add_library(lib_wavesynth OBJECT
	cst_units.c
	cst_clunits.c
	cst_diphone.c
	cst_sigpr.c
	cst_sts.c
	cst_reflpc.c
)

target_include_directories(lib_wavesynth 
	PUBLIC ../../include
)

set_target_properties(lib_wavesynth PROPERTIES COMPILE_PDB_NAME "${FLITE_COMPILE_PDB_NAME}")
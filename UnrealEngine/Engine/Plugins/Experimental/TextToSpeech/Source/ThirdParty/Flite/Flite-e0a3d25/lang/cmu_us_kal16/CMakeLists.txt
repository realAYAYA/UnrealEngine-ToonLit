# Copyright Epic Games, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.6)
project(lib_cmu_us_kal16 C)
add_library(lib_cmu_us_kal16 OBJECT
	cmu_us_kal16.c
	cmu_us_kal16_diphone.c
	cmu_us_kal16_lpc.c
	cmu_us_kal16_res.c
	cmu_us_kal16_residx.c
)

target_include_directories(lib_cmu_us_kal16
    PUBLIC .
    PUBLIC ../usenglish
    PUBLIC ../cmulex
    PUBLIC ../../include
)

set_target_properties(lib_cmu_us_kal16 PROPERTIES COMPILE_PDB_NAME "${FLITE_COMPILE_PDB_NAME}")
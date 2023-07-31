# Copyright Epic Games, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.6)
project(lib_cmu_us_kal)
add_library(lib_cmu_us_kal OBJECT
	cmu_us_kal.c
	cmu_us_kal_diphone.c
	cmu_us_kal_lpc.c
	cmu_us_kal_res.c
	cmu_us_kal_residx.c
	cmu_us_kal_ressize.c
)

target_include_directories(lib_cmu_us_kal
	PUBLIC .
	PUBLIC ../usenglish
	PUBLIC ../cmulex
	PUBLIC ../../include
)

set_target_properties(lib_cmu_us_kal PROPERTIES COMPILE_PDB_NAME "${FLITE_COMPILE_PDB_NAME}")
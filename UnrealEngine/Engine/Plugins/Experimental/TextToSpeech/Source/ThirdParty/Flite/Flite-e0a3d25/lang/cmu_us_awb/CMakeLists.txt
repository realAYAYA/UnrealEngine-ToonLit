# Copyright Epic Games, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.6)
project(lib_cmu_us_awb C)
add_library(lib_cmu_us_awb OBJECT
	cmu_us_awb_cg_durmodel.c
	cmu_us_awb_cg_f0_trees.c
	cmu_us_awb_cg_phonestate.c
	cmu_us_awb_cg_single_mcep_trees.c
	cmu_us_awb_cg_single_params.c
	cmu_us_awb_cg.c
	cmu_us_awb_spamf0_accent_params.c
	cmu_us_awb_spamf0_accent.c
	cmu_us_awb_spamf0_phrase.c
	cmu_us_awb.c
)

target_include_directories(lib_cmu_us_awb
	PUBLIC ../cmu_us_awb
	PUBLIC ../usenglish
	PUBLIC ../cmulex
	PUBLIC ../../include
)

set_target_properties(lib_cmu_us_awb PROPERTIES COMPILE_PDB_NAME "${FLITE_COMPILE_PDB_NAME}")
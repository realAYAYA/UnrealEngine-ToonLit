# Copyright Epic Games, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.6)
project(lib_cmu_us_rms C)
add_library(lib_cmu_us_rms OBJECT
	cmu_us_rms.c
	cmu_us_rms_cg.c
	cmu_us_rms_cg_durmodel.c
	cmu_us_rms_cg_f0_trees.c
	cmu_us_rms_cg_phonestate.c
	cmu_us_rms_cg_single_mcep_trees.c
	cmu_us_rms_cg_single_params.c
	cmu_us_rms_spamf0_accent.c
	cmu_us_rms_spamf0_accent_params.c
	cmu_us_rms_spamf0_phrase.c
)

target_include_directories(lib_cmu_us_rms
	PUBLIC .
    PUBLIC ../usenglish
	PUBLIC ..//cmulex
	PUBLIC ../../include
)

set_target_properties(lib_cmu_us_rms PROPERTIES COMPILE_PDB_NAME "${FLITE_COMPILE_PDB_NAME}")
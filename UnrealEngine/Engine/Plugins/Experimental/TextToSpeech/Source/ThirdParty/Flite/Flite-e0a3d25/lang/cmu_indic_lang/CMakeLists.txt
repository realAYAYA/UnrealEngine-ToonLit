# Copyright Epic Games, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.6)
project(lib_cmu_indic_lang C)
add_library(lib_cmu_indic_lang OBJECT
	cmu_indic_lang.c
	cmu_indic_phoneset.c
	cmu_indic_phrasing_cart.c
)

target_include_directories(lib_cmu_indic_lang
	PUBLIC .
	PUBLIC ../../include
)

target_link_libraries(lib_cmu_indic_lang
    PUBLIC lib_synth
)

set_target_properties(lib_cmu_indic_lang PROPERTIES COMPILE_PDB_NAME "${FLITE_COMPILE_PDB_NAME}")
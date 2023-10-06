# Copyright Epic Games, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.6)
project(lib_cmu_graphene_lang C)

add_library(lib_cmu_grapheme_lang OBJECT
	cmu_grapheme_lang.c
	cmu_grapheme_phrasing_cart.c
	cmu_grapheme_phoneset.c
)

target_include_directories(lib_cmu_grapheme_lang
	PUBLIC .
    PUBLIC ../../include
)

target_link_libraries(lib_cmu_grapheme_lang
	PUBLIC lib_synth
)

set_target_properties(lib_cmu_grapheme_lang PROPERTIES COMPILE_PDB_NAME "${FLITE_COMPILE_PDB_NAME}")
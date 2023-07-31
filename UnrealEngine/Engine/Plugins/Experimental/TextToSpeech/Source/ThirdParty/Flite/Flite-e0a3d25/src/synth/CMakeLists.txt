# Copyright Epic Games, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.6)
project(lib_synth C)
add_library(lib_synth OBJECT
	cst_synth.c
	cst_utt_utils.c
	cst_voice.c
	cst_phoneset.c
	cst_ffeatures.c
	cst_ssml.c
	flite.c
)

target_include_directories(lib_synth 
	PUBLIC ../../include
)

target_link_libraries(lib_synth
	PUBLIC lib_cg
	PUBLIC lib_lexicon
	PUBLIC lib_speech
)


set_target_properties(lib_synth PROPERTIES COMPILE_PDB_NAME "${FLITE_COMPILE_PDB_NAME}")
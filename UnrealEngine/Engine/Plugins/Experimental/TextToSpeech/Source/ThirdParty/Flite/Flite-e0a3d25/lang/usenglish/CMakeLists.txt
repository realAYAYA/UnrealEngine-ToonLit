# Copyright Epic Games, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.6)
project(lib_usenglish C)
add_library(lib_usenglish OBJECT
	us_aswd.c
	us_dur_stats.c
	us_durz_cart.c
	usenglish.c
	us_expand.c
	us_f0lr.c
	us_f0_model.c
	us_ffeatures.c
	us_gpos.c
	us_int_accent_cart.c
	us_int_tone_cart.c
	us_nums_cart.c
	us_phoneset.c
	us_phrasing_cart.c
	us_pos_cart.c
	us_text.c
)

target_include_directories(lib_usenglish
	PUBLIC .
	PUBLIC ../../include
)

target_link_libraries(lib_usenglish
	PUBLIC lib_utils
)

set_target_properties(lib_usenglish PROPERTIES COMPILE_PDB_NAME "${FLITE_COMPILE_PDB_NAME}")
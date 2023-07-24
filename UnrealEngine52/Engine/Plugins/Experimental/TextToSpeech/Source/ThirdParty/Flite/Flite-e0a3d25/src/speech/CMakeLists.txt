# Copyright Epic Games, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.6)
project(lib_speech C)
add_library(lib_speech OBJECT
	cst_wave.c
	cst_wave_io.c
	cst_track.c
	cst_track_io.c
	cst_wave_utils.c
	cst_lpcres.c
	rateconv.c
	g721.c
	g72x.c
	g723_24.c
	g723_40.c
)

target_include_directories(lib_speech 
	PUBLIC .
	PUBLIC ../../include
)

set_target_properties(lib_speech PROPERTIES COMPILE_PDB_NAME "${FLITE_COMPILE_PDB_NAME}")
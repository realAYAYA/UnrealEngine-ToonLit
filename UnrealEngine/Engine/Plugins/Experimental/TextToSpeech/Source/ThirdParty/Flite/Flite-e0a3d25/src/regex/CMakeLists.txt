# Copyright Epic Games, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.6)
project(lib_regex C)

add_library(lib_regex OBJECT
	cst_regex.c
	regexp.c
	regsub.c
)

target_include_directories(lib_regex 
	PUBLIC . 
	PUBLIC ../../include
)

set_target_properties(lib_regex PROPERTIES COMPILE_PDB_NAME "${FLITE_COMPILE_PDB_NAME}")
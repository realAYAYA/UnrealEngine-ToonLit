# Copyright Epic Games, Inc. All Rights Reserved.
cmake_minimum_required(VERSION 3.6)
project(lib_utils C)

# We use cst_mmap_none.c to ignore platform dependence 
# as we most likely won't need to use this much   
add_library(lib_utils OBJECT
	cst_alloc.c
	cst_error.c
	cst_string.c
	cst_wchar.c
	cst_tokenstream.c
	cst_val.c
	cst_features.c
	cst_endian.c
	cst_socket.c
	cst_val_const.c
	cst_val_user.c
	cst_args.c
	cst_url.c
	cst_file_stdio.c
	cst_mmap_none.c
)

target_include_directories(lib_utils 
	PUBLIC ../../include
)

set_target_properties(lib_utils PROPERTIES COMPILE_PDB_NAME "${FLITE_COMPILE_PDB_NAME}")
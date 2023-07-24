// Copyright Epic Games, Inc. All Rights Reserved.

/*================================================================================================
	StrataDefinitions.ush: used in ray tracing shaders and C++ code to define common constants
	!!! Changing this file requires recompilation of the engine !!!
=================================================================================================*/

#pragma once

// Change this to force recompilation of all strata dependent shaders (use https://www.random.org/cgi-bin/randbyte?nbytes=4&format=h)
#define STRATA_SHADER_VERSION 0x084bf26a2 



#define STRATA_MAX_BSDF_COUNT				15
#define STRATA_MAX_OPERATOR_COUNT			15

// It this is changed, STATE_BIT_COUNT_SHAREDLOCALBASESID and HEADER_BIT_COUNT_SHAREDLOCALBASES_COUNT also needs to be updated
#define STRATA_MAX_SHAREDLOCALBASES_REGISTERS		4

#define STRATA_PACKED_SHAREDLOCALBASIS_STRIDE_BYTES	4


#define STRATA_BSDF_TYPE_SLAB				0
#define STRATA_BSDF_TYPE_VOLUMETRICFOGCLOUD	1
#define STRATA_BSDF_TYPE_UNLIT				2
#define STRATA_BSDF_TYPE_HAIR				3
#define STRATA_BSDF_TYPE_SINGLELAYERWATER	4
#define STRATA_BSDF_TYPE_EYE				5
// When more than 7 BSDF must exists, please update STATE_BIT_COUNT_BSDF and FStrataClassification.ShadingModels packing in Strata.ush

// The size of strata material classification tiles on screen
#define STRATA_TILE_SIZE					8
#define STRATA_TILE_SIZE_DIV_AS_SHIFT	    3

// The default thickness of a layer is considered to be 0.01 centimeter = 0.1 millimeter
#define STRATA_LAYER_DEFAULT_THICKNESS_CM	0.01f

// Min Fuzz Roughness to avoid numerical issue
#define STRATA_MIN_FUZZ_ROUGHNESS 0.02f

#define STRATA_BASE_PASS_MRT_OUTPUT_COUNT	3

#define STRATA_SSS_DATA_UINT_COUNT		2

#define STRATA_OPERATOR_WEIGHT			0
#define STRATA_OPERATOR_VERTICAL		1
#define STRATA_OPERATOR_HORIZONTAL		2
#define STRATA_OPERATOR_ADD				3
#define STRATA_OPERATOR_BSDF			4
#define STRATA_OPERATOR_BSDF_LEGACY		5

// This must map directly to EStrataTileMaterialType
#define STRATA_TILE_TYPE_SIMPLE						0
#define STRATA_TILE_TYPE_SINGLE						1
#define STRATA_TILE_TYPE_COMPLEX					2
#define STRATA_TILE_TYPE_ROUGH_REFRACT				3
#define STRATA_TILE_TYPE_ROUGH_REFRACT_SSS_WITHOUT	4
#define STRATA_TILE_TYPE_DECAL_SIMPLE				5
#define STRATA_TILE_TYPE_DECAL_SINGLE				6
#define STRATA_TILE_TYPE_DECAL_COMPLEX				7
#define STRATA_TILE_TYPE_COUNT						8

// sizeof(FRHIDrawIndirectParameters) = 4 uints = 16 bytes
#define GetStrataTileTypeDrawIndirectArgOffset_Byte(x)  (x * 16)
#define GetStrataTileTypeDrawIndirectArgOffset_DWord(x) (x * 4)

// sizeof(FRHIDispatchIndirectParameters) can vary per-platform
#ifdef __cplusplus
	#define GetStrataTileTypeDispatchIndirectArgOffset_Byte(x)  (x * sizeof(FRHIDispatchIndirectParameters))
	#define GetStrataTileTypeDispatchIndirectArgOffset_DWord(x) (x * sizeof(FRHIDispatchIndirectParameters) / sizeof(uint32))
#else
	#define GetStrataTileTypeDispatchIndirectArgOffset_Byte(x)  (x * DISPATCH_INDIRECT_UINT_COUNT * 4)
	#define GetStrataTileTypeDispatchIndirectArgOffset_DWord(x) (x * DISPATCH_INDIRECT_UINT_COUNT)
#endif
// Copyright Epic Games, Inc. All Rights Reserved.

/*================================================================================================
	HairStrandsDefinitions.ush: used in ray tracing shaders and C++ code to define common constants
	!!! Changing this file requires recompilation of the engine !!!
=================================================================================================*/

#pragma once

// Change this to force recompilation of all strata dependent shaders (use https://www.random.org/cgi-bin/randbyte?nbytes=4&format=h)
#define HAIRSTRANDS_SHADER_VERSION 0x3ba5af0e

// Attribute index
#define HAIR_ATTRIBUTE_ROOTUV 0
#define HAIR_ATTRIBUTE_SEED 1
#define HAIR_ATTRIBUTE_LENGTH 2
#define HAIR_ATTRIBUTE_CLUMPID 3
#define HAIR_ATTRIBUTE_BASECOLOR 4
#define HAIR_ATTRIBUTE_ROUGHNESS 5
#define HAIR_ATTRIBUTE_AO 6
#define HAIR_ATTRIBUTE_COUNT 7

// HAIR_ATTRIBUTE_MAX rounded to 4
#define HAIR_ATTRIBUTE_OFFSET_COUNT ((HAIR_ATTRIBUTE_COUNT + 3) / 4)

// Pack all offset into uint4
#define PACK_HAIR_ATTRIBUTE_OFFSETS(Out, In) \
	for (uint32 AttributeIt4 = 0; AttributeIt4 < HAIR_ATTRIBUTE_OFFSET_COUNT; ++AttributeIt4) \
	{ \
		Out[AttributeIt4] = FUintVector4(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF); \
	} \
	for (uint32 AttributeIt = 0; AttributeIt < HAIR_ATTRIBUTE_COUNT; ++AttributeIt) \
	{ \
		const uint32 Index4 = AttributeIt & (~0x3); \
		const uint32 SubIndex = AttributeIt - Index4; \
		const uint32 IndexDiv4 = Index4 >> 2u; \
		Out[IndexDiv4][SubIndex] = In[AttributeIt]; \
	}
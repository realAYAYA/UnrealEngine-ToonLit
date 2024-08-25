// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// Constants for 'SamplerType'
#define PATHTRACER_SAMPLER_DEFAULT			0
#define PATHTRACER_SAMPLER_ERROR_DIFFUSION	1

// Constants for the 'Flags' field of FPathTracingLight
#define PATHTRACER_FLAG_LIGHTING_CHANNEL_MASK				(7 << 0)	// Which lighting channel is this light assigned to?
#define PATHTRACER_FLAG_TRANSMISSION_MASK					(1 << 3)	// Does the light affect the transmission side?
#define PATHTRACER_FLAG_NON_INVERSE_SQUARE_FALLOFF_MASK		(1 << 4)	// Does the light have a non-inverse square decay?
#define PATHTRACER_FLAG_STATIONARY_MASK						(1 << 5)	// Only used by GPULightmass
#define PATHTRACER_FLAG_TYPE_MASK							(7 << 6)
#define PATHTRACING_LIGHT_SKY								(0 << 6)
#define PATHTRACING_LIGHT_DIRECTIONAL						(1 << 6)
#define PATHTRACING_LIGHT_POINT								(2 << 6)
#define PATHTRACING_LIGHT_SPOT								(3 << 6)
#define PATHTRACING_LIGHT_RECT								(4 << 6)
#define PATHTRACER_FLAG_CAST_SHADOW_MASK 					(1 << 9)
#define PATHTRACER_FLAG_CAST_VOL_SHADOW_MASK 				(1 << 10)
#define PATHTRACER_FLAG_HAS_RECT_TEXTURE_MASK				(1 << 11)

#define PATHTRACER_MASK_CAMERA								0x01			// opaque and alpha tested meshes and particles as a whole (primary ray) excluding hairs
#define PATHTRACER_MASK_HAIR_CAMERA							0x02			// For primary ray tracing against hair
#define PATHTRACER_MASK_SHADOW								0x04			// Whether the geometry is visible for shadow rays
#define PATHTRACER_MASK_HAIR_SHADOW							0x08			// Whether hair is visible for shadow rays
#define PATHTRACER_MASK_INDIRECT							0x10			// opaque and alpha tested meshes and particles as a whole (indirect ray) excluding hairs
#define PATHTRACER_MASK_HAIR_INDIRECT						0x20			// For indirect ray tracing against hair
#define PATHTRACER_MASK_EMPTY_SLOT1							0x40			
#define PATHTRACER_MASK_EMPTY_SLOT2							0x80			

#define PATHTRACER_MASK_IGNORE								0x00			// used when mapping general tracing mask to path tracing mask
#define PATHTRACER_MASK_UNUSED	(PATHTRACER_MASK_EMPTY_SLOT1|PATHTRACER_MASK_EMPTY_SLOT2)	
#define PATHTRACER_MASK_ALL									0xFF

// Constants for light contribution types (AOV decomposition of the image)
// Leaving all constants enabled creates the beauty image, but turning off some bits allows
// the path tracer to create an image with only certain components enabled
#define PATHTRACER_CONTRIBUTION_EMISSIVE                     1
#define PATHTRACER_CONTRIBUTION_DIFFUSE                      2
#define PATHTRACER_CONTRIBUTION_SPECULAR                     4
#define PATHTRACER_CONTRIBUTION_VOLUME                       8

// Constants for the path tracer light grid
#define PATHTRACER_LIGHT_GRID_SINGULAR_MASK					0x80000000u
#define PATHTRACER_LIGHT_GRID_LIGHT_COUNT_MASK				0x7FFFFFFFu

// Constants for the energy conservation texture sizes
#define PATHTRACER_ENERGY_TABLE_RESOLUTION				32

// Constants related to volumetric support
#define VOLUMEID_ATMOSPHERE				0
#define VOLUMEID_FOG					1
#define VOLUMEID_HETEROGENEOUS_VOLUMES	2
#define PATH_TRACER_MAX_VOLUMES			3

#define PATH_TRACER_VOLUME_ENABLE_BIT						(1u)
#define PATH_TRACER_VOLUME_ENABLE_ATMOSPHERE  				(PATH_TRACER_VOLUME_ENABLE_BIT << VOLUMEID_ATMOSPHERE)
#define PATH_TRACER_VOLUME_ENABLE_FOG               		(PATH_TRACER_VOLUME_ENABLE_BIT << VOLUMEID_FOG)
#define PATH_TRACER_VOLUME_ENABLE_HETEROGENEOUS_VOLUMES 	(PATH_TRACER_VOLUME_ENABLE_BIT << VOLUMEID_HETEROGENEOUS_VOLUMES)

#define PATH_TRACER_VOLUME_HOLDOUT_BIT						(PATH_TRACER_VOLUME_ENABLE_BIT << PATH_TRACER_MAX_VOLUMES)
#define PATH_TRACER_VOLUME_HOLDOUT_ATMOSPHERE  				(PATH_TRACER_VOLUME_HOLDOUT_BIT << VOLUMEID_ATMOSPHERE)
#define PATH_TRACER_VOLUME_HOLDOUT_FOG               		(PATH_TRACER_VOLUME_HOLDOUT_BIT << VOLUMEID_FOG)
#define PATH_TRACER_VOLUME_HOLDOUT_HETEROGENEOUS_VOLUMES	(PATH_TRACER_VOLUME_HOLDOUT_BIT << VOLUMEID_HETEROGENEOUS_VOLUMES)

#define PATH_TRACER_VOLUME_USE_ANALYTIC_TRANSMITTANCE       (PATH_TRACER_VOLUME_HOLDOUT_BIT << PATH_TRACER_MAX_VOLUMES)

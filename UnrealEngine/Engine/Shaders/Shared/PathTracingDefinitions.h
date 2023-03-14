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

// Constants for the path tracer light grid
#define PATHTRACER_LIGHT_GRID_SINGULAR_MASK					0x80000000u
#define PATHTRACER_LIGHT_GRID_LIGHT_COUNT_MASK				0x7FFFFFFFu

// Constants for the energy conservation texture sizes
#define PATHTRACER_ENERGY_TABLE_RESOLUTION				32

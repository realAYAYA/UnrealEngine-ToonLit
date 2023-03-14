// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneTextures.h"

/*
* Stencil layout during basepass / deferred decals:
*		BIT ID    | USE
*		[0]       | sandbox bit (bit to be use by any rendering passes, but must be properly reset to 0 after using)
*		[1]       | unallocated
*		[2]       | Distance Field Representation
*		[3]       | Temporal AA mask for translucent object.
*		[4]       | Lighting channels
*		[5]       | Lighting channels
*		[6]       | Lighting channels
*		[7]       | primitive receive decal bit
*
* After deferred decals, stencil is cleared to 0 and no longer packed in this way, to ensure use of fast hardware clears and HiStencil.
*/
#define STENCIL_SANDBOX_BIT_ID				0
// Must match usf
#define STENCIL_DISTANCE_FIELD_REPRESENTATION_BIT_ID 2
#define STENCIL_TEMPORAL_RESPONSIVE_AA_BIT_ID 3
#define STENCIL_LIGHTING_CHANNELS_BIT_ID	4
#define STENCIL_RECEIVE_DECAL_BIT_ID		7
// Used only during the lighting pass - alias/reuse light channels (which copied from stencil to a texture prior to lighting pass)
#define STENCIL_STRATA_FASTPATH				4 
#define STENCIL_STRATA_SINGLEPATH			5
#define STENCIL_STRATA_COMPLEX				6

// Outputs a compile-time constant stencil's bit mask ready to be used
// in TStaticDepthStencilState<> template parameter. It also takes care
// of masking the Value macro parameter to only keep the low significant
// bit to ensure to not overflow on other bits.
#define GET_STENCIL_BIT_MASK(BIT_NAME,Value) uint8((uint8(Value) & uint8(0x01)) << (STENCIL_##BIT_NAME##_BIT_ID))

#define STENCIL_SANDBOX_MASK GET_STENCIL_BIT_MASK(SANDBOX,1)

#define STENCIL_TEMPORAL_RESPONSIVE_AA_MASK GET_STENCIL_BIT_MASK(TEMPORAL_RESPONSIVE_AA,1)

#define STENCIL_LIGHTING_CHANNELS_MASK(Value) uint8(((Value) & 0x7) << STENCIL_LIGHTING_CHANNELS_BIT_ID)

// Mobile specific
// Store shading model into stencil [1-2] bits
#define GET_STENCIL_MOBILE_SM_MASK(Value) uint8(((Value) & 0x3) << 1)
// Sky material mask - bit 3
#define STENCIL_MOBILE_SKY_MASK uint8(1 << 3)

class FSceneRenderTargets
{
public:
	UE_DEPRECATED(5.0, "FSceneRenderTargets is now deprecated from the RDG refactor. FSceneTextures should be used instead.")
	static FSceneRenderTargets& Get()
	{
		static FSceneRenderTargets Instance;
		return Instance;
	}

	UE_DEPRECATED(5.0, "FSceneRenderTargets is now deprecated from the RDG refactor. FSceneTextures should be used instead.")
	static FSceneRenderTargets& Get(FRHICommandListImmediate&)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return Get();
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	UE_DEPRECATED(5.0, "FSceneRenderTargets is now deprecated from the RDG refactor. FSceneTextures should be used instead.")
	static FClearValueBinding GetDefaultColorClear()
	{
		return FSceneTexturesConfig::Get().ColorClearValue;
	}

	UE_DEPRECATED(5.0, "FSceneRenderTargets is now deprecated from the RDG refactor. FSceneTextures should be used instead.")
	static FClearValueBinding GetDefaultDepthClear()
	{
		return FSceneTexturesConfig::Get().DepthClearValue;
	}

	UE_DEPRECATED(5.0, "FSceneRenderTargets is now deprecated from the RDG refactor. FSceneTextures should be used instead.")
	static FIntPoint GetBufferSizeXY()
	{
		return FSceneTexturesConfig::Get().Extent;
	}

	UE_DEPRECATED(5.0, "FSceneRenderTargets is now deprecated from the RDG refactor. FSceneTextures should be used instead.")
	static int32 GetMSAACount()
	{
		return FSceneTexturesConfig::Get().NumSamples;
	}

	UE_DEPRECATED(5.0, "FSceneRenderTargets is now deprecated from the RDG refactor. FSceneTextures should be used instead.")
	static ERHIFeatureLevel::Type GetCurrentFeatureLevel()
	{
		return FSceneTexturesConfig::Get().FeatureLevel;
	}

	UE_DEPRECATED(5.0, "FSceneRenderTargets is now deprecated from the RDG refactor. FSceneTextures should be used instead.")
	static TRefCountPtr<IPooledRenderTarget> GetSceneColor()
	{
		checkNoEntry();
		return nullptr;
	}
};

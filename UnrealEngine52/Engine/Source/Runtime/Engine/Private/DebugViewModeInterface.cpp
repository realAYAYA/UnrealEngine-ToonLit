// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
DebugViewModeInterface.cpp: Contains definitions for rendering debug viewmodes.
=============================================================================*/

#include "DebugViewModeInterface.h"

#if ENABLE_DRAW_DEBUG

#include "RHIStaticStates.h"
#include "MaterialShared.h"

FDebugViewModeInterface* FDebugViewModeInterface::Singleton = nullptr;

void FDebugViewModeInterface::SetDrawRenderState(EDebugViewShaderMode DebugViewMode, EBlendMode InBlendMode, FRenderState& DrawRenderState, bool bHasDepthPrepassForMaskedMaterial) const
{
	if (DebugViewMode == DVSM_QuadComplexity || DebugViewMode == DVSM_ShaderComplexityBleedingQuadOverhead || DebugViewMode == DVSM_ShaderComplexityContainedQuadOverhead || DebugViewMode == DVSM_ShaderComplexity)
	{
		if (IsOpaqueBlendMode(InBlendMode))
		{
			DrawRenderState.DepthStencilState = TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI();
		}
		else if (IsMaskedBlendMode(InBlendMode))
		{
			if (bHasDepthPrepassForMaskedMaterial)
			{
				DrawRenderState.DepthStencilState = TStaticDepthStencilState<false, CF_Equal>::GetRHI();
			}
			else
			{
				DrawRenderState.DepthStencilState = TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI();
			}
		}
		else // Translucent
		{
			DrawRenderState.DepthStencilState = TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI();
		}
		DrawRenderState.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_One>::GetRHI();
	}
	else if (DebugViewMode == DVSM_OutputMaterialTextureScales)
	{
		DrawRenderState.BlendState = TStaticBlendState<>::GetRHI();
		DrawRenderState.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	}
	else
	{
		if (IsTranslucentBlendMode(InBlendMode))
		{
			// Otherwise, force translucent blend mode (shaders will use an hardcoded alpha).
			DrawRenderState.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI();
			DrawRenderState.DepthStencilState = TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI();
		}
		else
		{
			DrawRenderState.BlendState = TStaticBlendState<>::GetRHI();

			// If not selected, use depth equal to make alpha test stand out (goes with EarlyZPassMode = DDM_AllOpaque) 
			if (IsMaskedBlendMode(InBlendMode) && bHasDepthPrepassForMaskedMaterial)
			{
				DrawRenderState.DepthStencilState = TStaticDepthStencilState<false, CF_Equal>::GetRHI();
			}
			else
			{
				DrawRenderState.DepthStencilState = TStaticDepthStencilState<>::GetRHI();
			}
		}
	}
}

void FDebugViewModeInterface::SetInterface(FDebugViewModeInterface* Interface)
{
	ensure(!Singleton);
	Singleton = Interface;
}

bool FDebugViewModeInterface::AllowFallbackToDefaultMaterial(bool bHasVertexPositionOffsetConnected, bool bHasPixelDepthOffsetConnected)
{
	// Check for anything that could change the shape from the default material.
	return !bHasVertexPositionOffsetConnected &&
		!bHasPixelDepthOffsetConnected;
}

bool FDebugViewModeInterface::AllowFallbackToDefaultMaterial(const FMaterial* InMaterial)
{
	check(InMaterial);
	return FDebugViewModeInterface::AllowFallbackToDefaultMaterial(InMaterial->HasVertexPositionOffsetConnected(), InMaterial->HasPixelDepthOffsetConnected());
}


#endif // ENABLE_DRAW_DEBUG


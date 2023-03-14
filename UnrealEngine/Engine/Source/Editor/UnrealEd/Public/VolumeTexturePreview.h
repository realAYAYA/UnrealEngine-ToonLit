// Copyright Epic Games, Inc. All Rights Reserved.

/*==============================================================================
	VolumeTexturePreview.h: Definitions for previewing 2d textures.
==============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "BatchedElements.h"

/**
 * Batched element parameters for previewing 2d textures.
 */
class UNREALED_API FBatchedElementVolumeTexturePreviewParameters : public FBatchedElementParameters
{
public:
	FBatchedElementVolumeTexturePreviewParameters(bool InViewModeAsDepthSlices, int32 InSizeZ, float InMipLevel, float InOpacity, bool InShowSlices, const FRotator& InTraceOrientation)
		: bViewModeAsDepthSlices(InViewModeAsDepthSlices)
		, SizeZ(InSizeZ)
		, MipLevel(InMipLevel)
		, Opacity(InOpacity)
		, bShowSlices(InShowSlices)
		, TraceOrientation(InTraceOrientation)
	{
	}

	/** Binds vertex and pixel shaders for this element */
	virtual void BindShaders(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, ERHIFeatureLevel::Type InFeatureLevel, const FMatrix& InTransform, const float InGamma, const FMatrix& ColorWeights, const FTexture* Texture) override;

private:

	/** Whether to render depth slices or trace into the volume */
	bool bViewModeAsDepthSlices;
	
	/** The size Z of the texture */
	int32 SizeZ;

	/** The mip level to visualize */
	float MipLevel;

	float Opacity;

	/** Whether to show each depth slize of the volume */
	 bool bShowSlices;

	/** The orientation when tracing */
	FRotator TraceOrientation;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BatchedElements.h"

class FGLTFCombinedTexturePreview : public FBatchedElementParameters
{
public:

	FGLTFCombinedTexturePreview(const FTexture* TextureA, const FTexture* TextureB, const FMatrix& ColorTransformA, const FMatrix& ColorTransformB, const FLinearColor& BackgroundColor)
		: TextureA(TextureA)
		, TextureB(TextureB)
		, ColorTransformA(ColorTransformA)
		, ColorTransformB(ColorTransformB)
		, BackgroundColor(BackgroundColor)
	{
	}

private:

	const FTexture* TextureA;
	const FTexture* TextureB;
	FMatrix44f ColorTransformA;
	FMatrix44f ColorTransformB;
	FLinearColor BackgroundColor;

	/** Binds vertex and pixel shaders for this element */
	virtual void BindShaders(
		FRHICommandList& RHICmdList,
		FGraphicsPipelineStateInitializer& GraphicsPSOInit,
		ERHIFeatureLevel::Type InFeatureLevel,
		const FMatrix& InTransform,
		const float InGamma,
		const FMatrix& ColorWeights,
		const FTexture* Texture) override;
};

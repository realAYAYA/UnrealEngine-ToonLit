// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeightfieldMinMaxTextureThumbnailRenderer.h"

#include "Engine/Texture2D.h"
#include "HeightfieldMinMaxTexture.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HeightfieldMinMaxTextureThumbnailRenderer)

UHeightfieldMinMaxTextureThumbnailRenderer::UHeightfieldMinMaxTextureThumbnailRenderer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UHeightfieldMinMaxTextureThumbnailRenderer::CanVisualizeAsset(UObject* Object)
{
	UHeightfieldMinMaxTexture* MinMaxTextureBuilder = Cast<UHeightfieldMinMaxTexture>(Object);
	UTexture2D* Texture = MinMaxTextureBuilder != nullptr ? MinMaxTextureBuilder->Texture : nullptr;
	return Texture != nullptr ? UTextureThumbnailRenderer::CanVisualizeAsset(Texture) : false;
}

void UHeightfieldMinMaxTextureThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily)
{
	UHeightfieldMinMaxTexture* MinMaxTextureBuilder = Cast<UHeightfieldMinMaxTexture>(Object);
	UTexture2D* Texture = MinMaxTextureBuilder != nullptr ? MinMaxTextureBuilder->Texture : nullptr;
	if (Texture != nullptr)
	{
		UTextureThumbnailRenderer::Draw(Texture, X, Y, Width, Height, RenderTarget, Canvas, bAdditionalViewFamily);
	}
}


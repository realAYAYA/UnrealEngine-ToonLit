// Copyright Epic Games, Inc. All Rights Reserved.

#include "Converters/GLTFTextureConverters.h"
#include "Tasks/GLTFDelayedTextureTasks.h"

FGLTFJsonTexture* FGLTFTexture2DConverter::Convert(const UTexture2D* Texture2D, bool bToSRGB, TextureAddress WrapS, TextureAddress WrapT)
{
	if (Builder.ExportOptions->TextureImageFormat == EGLTFTextureImageFormat::None)
	{
		return nullptr;
	}

	FGLTFJsonTexture* JsonTexture = Builder.AddTexture();
	Builder.ScheduleSlowTask<FGLTFDelayedTexture2DTask>(Builder, Texture2D, bToSRGB, JsonTexture, WrapS, WrapT);
	return JsonTexture;
}

FGLTFJsonTexture* FGLTFTextureRenderTarget2DConverter::Convert(const UTextureRenderTarget2D* RenderTarget2D, bool bToSRGB)
{
	if (Builder.ExportOptions->TextureImageFormat == EGLTFTextureImageFormat::None)
	{
		return nullptr;
	}

	FGLTFJsonTexture* JsonTexture = Builder.AddTexture();
	Builder.ScheduleSlowTask<FGLTFDelayedTextureRenderTarget2DTask>(Builder, RenderTarget2D, bToSRGB, JsonTexture);
	return JsonTexture;
}

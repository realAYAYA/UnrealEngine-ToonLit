// Copyright Epic Games, Inc. All Rights Reserved.

#include "Converters/GLTFTextureConverters.h"
#include "Converters/GLTFTextureUtility.h"
#include "Tasks/GLTFDelayedTextureTasks.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureCube.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/TextureRenderTargetCube.h"

void FGLTFTexture2DConverter::Sanitize(const UTexture2D*& Texture2D, bool& bToSRGB)
{
	if (FGLTFTextureUtility::IsHDR(Texture2D) && Builder.GetTextureHDREncoding() != EGLTFJsonHDREncoding::None)
	{
		bToSRGB = false;
	}
}

FGLTFJsonTexture* FGLTFTexture2DConverter::Convert(const UTexture2D* Texture2D, bool bToSRGB)
{
	if (Builder.ExportOptions->TextureImageFormat != EGLTFTextureImageFormat::None)
	{
		FGLTFJsonTexture* JsonTexture = Builder.AddTexture();
		Builder.ScheduleSlowTask<FGLTFDelayedTexture2DTask>(Builder, Texture2D, bToSRGB, JsonTexture);
		return JsonTexture;
	}

	return nullptr;
}

void FGLTFTextureCubeConverter::Sanitize(const UTextureCube*& TextureCube, ECubeFace& CubeFace, bool& bToSRGB)
{
	if (FGLTFTextureUtility::IsHDR(TextureCube) && Builder.GetTextureHDREncoding() != EGLTFJsonHDREncoding::None)
	{
		bToSRGB = false;
	}
}

FGLTFJsonTexture* FGLTFTextureCubeConverter::Convert(const UTextureCube* TextureCube, ECubeFace CubeFace, bool bToSRGB)
{
	if (Builder.ExportOptions->TextureImageFormat != EGLTFTextureImageFormat::None)
	{
		FGLTFJsonTexture* JsonTexture = Builder.AddTexture();
		Builder.ScheduleSlowTask<FGLTFDelayedTextureCubeTask>(Builder, TextureCube, CubeFace, bToSRGB, JsonTexture);
		return JsonTexture;
	}

	return nullptr;
}

void FGLTFTextureRenderTarget2DConverter::Sanitize(const UTextureRenderTarget2D*& RenderTarget2D, bool& bToSRGB)
{
	if (FGLTFTextureUtility::IsHDR(RenderTarget2D) && Builder.GetTextureHDREncoding() != EGLTFJsonHDREncoding::None)
	{
		bToSRGB = false;
	}
}

FGLTFJsonTexture* FGLTFTextureRenderTarget2DConverter::Convert(const UTextureRenderTarget2D* RenderTarget2D, bool bToSRGB)
{
	if (Builder.ExportOptions->TextureImageFormat != EGLTFTextureImageFormat::None)
	{
		FGLTFJsonTexture* JsonTexture = Builder.AddTexture();
		Builder.ScheduleSlowTask<FGLTFDelayedTextureRenderTarget2DTask>(Builder, RenderTarget2D, bToSRGB, JsonTexture);
		return JsonTexture;
	}

	return nullptr;
}

void FGLTFTextureRenderTargetCubeConverter::Sanitize(const UTextureRenderTargetCube*& RenderTargetCube, ECubeFace& CubeFace, bool& bToSRGB)
{
	if (FGLTFTextureUtility::IsHDR(RenderTargetCube) && Builder.GetTextureHDREncoding() != EGLTFJsonHDREncoding::None)
	{
		bToSRGB = false;
	}
}

FGLTFJsonTexture* FGLTFTextureRenderTargetCubeConverter::Convert(const UTextureRenderTargetCube* RenderTargetCube, ECubeFace CubeFace, bool bToSRGB)
{
	if (Builder.ExportOptions->TextureImageFormat != EGLTFTextureImageFormat::None)
	{
		FGLTFJsonTexture* JsonTexture = Builder.AddTexture();
		Builder.ScheduleSlowTask<FGLTFDelayedTextureRenderTargetCubeTask>(Builder, RenderTargetCube, CubeFace, bToSRGB, JsonTexture);
		return JsonTexture;
	}

	return nullptr;
}

FGLTFJsonTexture* FGLTFTextureLightMapConverter::Convert(const ULightMapTexture2D* LightMap)
{
#if WITH_EDITOR
	if (Builder.ExportOptions->TextureImageFormat != EGLTFTextureImageFormat::None)
	{
		FGLTFJsonTexture* JsonTexture = Builder.AddTexture();
		Builder.ScheduleSlowTask<FGLTFDelayedTextureLightMapTask>(Builder, LightMap, JsonTexture);
		return JsonTexture;
	}
#endif

	return nullptr;
}

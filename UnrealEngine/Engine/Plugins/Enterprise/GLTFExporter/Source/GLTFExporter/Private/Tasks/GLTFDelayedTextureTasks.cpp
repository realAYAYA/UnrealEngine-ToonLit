// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tasks/GLTFDelayedTextureTasks.h"
#include "Builders/GLTFContainerBuilder.h"
#include "Utilities/GLTFCoreUtilities.h"
#include "Converters/GLTFTextureUtility.h"
#include "Converters/GLTFNameUtility.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureCube.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/TextureRenderTargetCube.h"
#include "Engine/LightMapTexture2D.h"

FString FGLTFDelayedTexture2DTask::GetName()
{
	return Texture2D->GetName();
}

void FGLTFDelayedTexture2DTask::Process()
{
	FGLTFTextureUtility::FullyLoad(Texture2D);
	Texture2D->GetName(JsonTexture->Name);

	const bool bFromSRGB = Texture2D->SRGB;
	if (bFromSRGB != bToSRGB)
	{
		JsonTexture->Name += bToSRGB ? TEXT("_sRGB") : TEXT("_Linear");
	}

	const bool bIsHDR = FGLTFTextureUtility::IsHDR(Texture2D);
	const FIntPoint Size = FGLTFTextureUtility::GetInGameSize(Texture2D);
	UTextureRenderTarget2D* RenderTarget = FGLTFTextureUtility::CreateRenderTarget(Size, bIsHDR);

	// TODO: preserve maximum image quality (avoid compression artifacts) by copying source data (and adjustments) to a temp texture
	FGLTFTextureUtility::DrawTexture(RenderTarget, Texture2D, FVector2D::ZeroVector, Size);

	if (bIsHDR)
	{
		JsonTexture->Encoding = Builder.GetTextureHDREncoding();
	}

	TGLTFSharedArray<FColor> Pixels;
	if (!FGLTFTextureUtility::ReadPixels(RenderTarget, *Pixels, JsonTexture->Encoding))
	{
		Builder.LogWarning(FString::Printf(TEXT("Failed to read pixels for 2D texture %s"), *JsonTexture->Name));
		return;
	}

	if (Builder.ExportOptions->bAdjustNormalmaps && Texture2D->IsNormalMap())
	{
		// TODO: add support for adjusting normals in GLTFNormalMapPreview instead
		FGLTFTextureUtility::FlipGreenChannel(*Pixels);
	}

	FGLTFTextureUtility::TransformColorSpace(*Pixels, bFromSRGB, bToSRGB);

	const bool bIgnoreAlpha = FGLTFTextureUtility::IsAlphaless(Texture2D->GetPixelFormat());
	const EGLTFTextureType Type =
		Texture2D->IsNormalMap() ? EGLTFTextureType::Normalmaps :
		bIsHDR ? EGLTFTextureType::HDR : EGLTFTextureType::None;

	JsonTexture->Source = Builder.AddUniqueImage(Pixels, Size, bIgnoreAlpha, Type, JsonTexture->Name);
	JsonTexture->Sampler = Builder.AddUniqueSampler(Texture2D);
}

FString FGLTFDelayedTextureCubeTask::GetName()
{
	return TextureCube->GetName();
}

void FGLTFDelayedTextureCubeTask::Process()
{
	FGLTFTextureUtility::FullyLoad(TextureCube);
	JsonTexture->Name = TextureCube->GetName() + TEXT("_") + FGLTFJsonUtilities::GetValue(FGLTFCoreUtilities::ConvertCubeFace(CubeFace));

	const bool bFromSRGB = TextureCube->SRGB;
	if (bFromSRGB != bToSRGB)
	{
		JsonTexture->Name += bToSRGB ? TEXT("_sRGB") : TEXT("_Linear");
	}

	// TODO: add optimized "happy path" if cube face doesn't need rotation and has suitable pixel format

	// TODO: preserve maximum image quality (avoid compression artifacts) by copying source data (and adjustments) to a temp texture
	const UTexture2D* FaceTexture = FGLTFTextureUtility::CreateTextureFromCubeFace(TextureCube, CubeFace);
	if (FaceTexture == nullptr)
	{
		Builder.LogWarning(FString::Printf(TEXT("Failed to extract cube face %d for cubemap texture %s"), CubeFace, *TextureCube->GetName()));
		return;
	}

	const bool bIsHDR = FGLTFTextureUtility::IsHDR(TextureCube);
	const FIntPoint Size = { TextureCube->GetSizeX(), TextureCube->GetSizeY() };
	UTextureRenderTarget2D* RenderTarget = FGLTFTextureUtility::CreateRenderTarget(Size, bIsHDR);

	const float FaceRotation = FGLTFTextureUtility::GetCubeFaceRotation(CubeFace);
	FGLTFTextureUtility::RotateTexture(RenderTarget, FaceTexture, FVector2D::ZeroVector, Size, FaceRotation);

	if (bIsHDR)
	{
		JsonTexture->Encoding = Builder.GetTextureHDREncoding();
		if (JsonTexture->Encoding == EGLTFJsonHDREncoding::None)
		{
			bToSRGB = true;
		}
	}

	TGLTFSharedArray<FColor> Pixels;
	if (!FGLTFTextureUtility::ReadPixels(RenderTarget, *Pixels, JsonTexture->Encoding))
	{
		Builder.LogWarning(FString::Printf(TEXT("Failed to read pixels (cube face %d) for cubemap texture %s"), CubeFace, *TextureCube->GetName()));
		return;
	}

	FGLTFTextureUtility::TransformColorSpace(*Pixels, bFromSRGB, bToSRGB);

	const bool bIgnoreAlpha = FGLTFTextureUtility::IsAlphaless(TextureCube->GetPixelFormat());
	const EGLTFTextureType Type = bIsHDR ? EGLTFTextureType::HDR : EGLTFTextureType::None;

	JsonTexture->Source = Builder.AddUniqueImage(Pixels, Size, bIgnoreAlpha, Type, JsonTexture->Name);
	JsonTexture->Sampler = Builder.AddUniqueSampler(TextureCube);
}

FString FGLTFDelayedTextureRenderTarget2DTask::GetName()
{
	return RenderTarget2D->GetName();
}

void FGLTFDelayedTextureRenderTarget2DTask::Process()
{
	FGLTFTextureUtility::FullyLoad(RenderTarget2D);
	RenderTarget2D->GetName(JsonTexture->Name);

	const bool bFromSRGB = RenderTarget2D->SRGB;
	if (bFromSRGB != bToSRGB)
	{
		JsonTexture->Name += bToSRGB ? TEXT("_sRGB") : TEXT("_Linear");
	}

	const bool bIsHDR = FGLTFTextureUtility::IsHDR(RenderTarget2D);
	const FIntPoint Size = { RenderTarget2D->SizeX, RenderTarget2D->SizeY };

	if (bIsHDR)
	{
		JsonTexture->Encoding = Builder.GetTextureHDREncoding();
	}

	TGLTFSharedArray<FColor> Pixels;
	if (!FGLTFTextureUtility::ReadPixels(RenderTarget2D, *Pixels, JsonTexture->Encoding))
	{
		Builder.LogWarning(FString::Printf(TEXT("Failed to read pixels for 2D render target %s"), *JsonTexture->Name));
		return;
	}

	FGLTFTextureUtility::TransformColorSpace(*Pixels, bFromSRGB, bToSRGB);

	const bool bIgnoreAlpha = FGLTFTextureUtility::IsAlphaless(RenderTarget2D->GetFormat());
	const EGLTFTextureType Type = bIsHDR ? EGLTFTextureType::HDR : EGLTFTextureType::None;

	JsonTexture->Source = Builder.AddUniqueImage(Pixels, Size, bIgnoreAlpha, Type, JsonTexture->Name);
	JsonTexture->Sampler = Builder.AddUniqueSampler(RenderTarget2D);
}

FString FGLTFDelayedTextureRenderTargetCubeTask::GetName()
{
	return RenderTargetCube->GetName();
}

void FGLTFDelayedTextureRenderTargetCubeTask::Process()
{
	FGLTFTextureUtility::FullyLoad(RenderTargetCube);
	JsonTexture->Name = RenderTargetCube->GetName() + TEXT("_") + FGLTFJsonUtilities::GetValue(FGLTFCoreUtilities::ConvertCubeFace(CubeFace));

	const bool bFromSRGB = RenderTargetCube->SRGB;
	if (bFromSRGB != bToSRGB)
	{
		JsonTexture->Name += bToSRGB ? TEXT("_sRGB") : TEXT("_Linear");
	}

	// TODO: add optimized "happy path" if cube face doesn't need rotation

	const UTexture2D* FaceTexture = FGLTFTextureUtility::CreateTextureFromCubeFace(RenderTargetCube, CubeFace);
	if (FaceTexture == nullptr)
	{
		Builder.LogWarning(FString::Printf(TEXT("Failed to extract cube face %d for cubemap render target %s"), CubeFace, *RenderTargetCube->GetName()));
		return;
	}

	const bool bIsHDR = FGLTFTextureUtility::IsHDR(RenderTargetCube);
	const FIntPoint Size = { RenderTargetCube->SizeX, RenderTargetCube->SizeX };
	UTextureRenderTarget2D* RenderTarget = FGLTFTextureUtility::CreateRenderTarget(Size, bIsHDR);

	const float FaceRotation = FGLTFTextureUtility::GetCubeFaceRotation(CubeFace);
	FGLTFTextureUtility::RotateTexture(RenderTarget, FaceTexture, FVector2D::ZeroVector, Size, FaceRotation);

	if (bIsHDR)
	{
		JsonTexture->Encoding = Builder.GetTextureHDREncoding();
	}

	TGLTFSharedArray<FColor> Pixels;
	if (!FGLTFTextureUtility::ReadPixels(RenderTarget, *Pixels, JsonTexture->Encoding))
	{
		Builder.LogWarning(FString::Printf(TEXT("Failed to read pixels (cube face %d) for cubemap render target %s"), CubeFace, *RenderTargetCube->GetName()));
		return;
	}

	FGLTFTextureUtility::TransformColorSpace(*Pixels, bFromSRGB, bToSRGB);

	const bool bIgnoreAlpha = FGLTFTextureUtility::IsAlphaless(RenderTargetCube->GetFormat());
	const EGLTFTextureType Type = bIsHDR ? EGLTFTextureType::HDR : EGLTFTextureType::None;

	JsonTexture->Source = Builder.AddUniqueImage(Pixels, Size, bIgnoreAlpha, Type, JsonTexture->Name);
	JsonTexture->Sampler = Builder.AddUniqueSampler(RenderTargetCube);
}

#if WITH_EDITOR

FString FGLTFDelayedTextureLightMapTask::GetName()
{
	return LightMap->GetName();
}

void FGLTFDelayedTextureLightMapTask::Process()
{
	FGLTFTextureUtility::FullyLoad(LightMap);
	LightMap->GetName(JsonTexture->Name);

	// NOTE: export of lightmaps via source data is used to work around issues with
	// quality-loss due to incorrect gamma transformation when rendering to a canvas.

	FTextureSource& Source = const_cast<FTextureSource&>(LightMap->Source);
	if (!Source.IsValid())
	{
		Builder.LogWarning(FString::Printf(TEXT("Failed to export lightmap texture %s because of missing source data"), *JsonTexture->Name));
		return;
	}

	const ETextureSourceFormat SourceFormat = Source.GetFormat();
	if (SourceFormat != TSF_BGRA8)
	{
		Builder.LogWarning(FString::Printf(TEXT("Failed to export lightmap texture %s because of unsupported source format %s"), *JsonTexture->Name, *FGLTFNameUtility::GetName(SourceFormat)));
		return;
	}

	const FIntPoint Size = { Source.GetSizeX(), Source.GetSizeY() };
	const int64 ByteLength = Source.CalcMipSize(0);

	const bool bIgnoreAlpha = false;
	const EGLTFTextureType Type = EGLTFTextureType::Lightmaps;

	const void* RawData = Source.LockMip(0);
	TGLTFSharedArray<FColor> Pixels = MakeShared<TArray<FColor>>(static_cast<const FColor*>(RawData), ByteLength / sizeof(FColor));
	Source.UnlockMip(0);

	JsonTexture->Source = Builder.AddUniqueImage(Pixels, Size, bIgnoreAlpha, Type, JsonTexture->Name);
	JsonTexture->Sampler = Builder.AddUniqueSampler(LightMap);
}

#endif

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonEnums.h"
#include "Engine/Texture.h"
#include "PixelFormat.h"
#include "RHIDefinitions.h"

class UTextureCube;
class UTextureRenderTarget2D;
class UTextureRenderTargetCube;

struct FGLTFTextureUtility
{
	static bool IsAlphaless(EPixelFormat PixelFormat);

	static void FullyLoad(const UTexture* InTexture);

	static bool IsHDR(const UTexture* Texture);
	static bool IsCubemap(const UTexture* Texture);

	static float GetCubeFaceRotation(ECubeFace CubeFace);

	static TextureFilter GetDefaultFilter(TextureGroup Group);

	static int32 GetMipBias(const UTexture* Texture);

	static FIntPoint GetInGameSize(const UTexture* Texture);

	static TTuple<TextureAddress, TextureAddress> GetAddressXY(const UTexture* Texture);

	static UTexture2D* CreateTransientTexture(const void* RawData, int64 ByteLength, const FIntPoint& Size, EPixelFormat Format, bool bSRGB = false);

	static UTextureRenderTarget2D* CreateRenderTarget(const FIntPoint& Size, bool bIsHDR);

	static bool DrawTexture(UTextureRenderTarget2D* OutTarget, const UTexture2D* InSource, const FVector2D& InPosition, const FVector2D& InSize, const FMatrix& InTransform = FMatrix::Identity);
	static bool RotateTexture(UTextureRenderTarget2D* OutTarget, const UTexture2D* InSource, const FVector2D& InPosition, const FVector2D& InSize, float InDegrees);

	static UTexture2D* CreateTextureFromCubeFace(const UTextureCube* TextureCube, ECubeFace CubeFace);
	static UTexture2D* CreateTextureFromCubeFace(const UTextureRenderTargetCube* RenderTargetCube, ECubeFace CubeFace);

	static bool ReadPixels(const UTextureRenderTarget2D* InRenderTarget, TArray<FColor>& OutPixels, EGLTFJsonHDREncoding Encoding);

	static void EncodeRGBM(const TArray<FFloat16Color>& InPixels, TArray<FColor>& OutPixels, float MaxRange = 8);
	static void EncodeRGBE(const TArray<FFloat16Color>& InPixels, TArray<FColor>& OutPixels);

	// TODO: maybe use template specialization to avoid the need for duplicated functions
	static bool LoadPlatformData(UTexture2D* Texture);
	static bool LoadPlatformData(UTextureCube* TextureCube);

	static void FlipGreenChannel(TArray<FColor>& Pixels);
	static void TransformColorSpace(TArray<FColor>& Pixels, bool bFromSRGB, bool bToSRGB);
};

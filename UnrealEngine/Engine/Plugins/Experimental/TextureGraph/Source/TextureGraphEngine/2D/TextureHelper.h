// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "2D/Mask/MaskEnums.h"
#include "CoreMinimal.h"
#include "Data/RawBuffer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Helper/Promise.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "PixelFormat.h" 
#include "TextureContent.h"
#include "TextureType.h"

class Tex;
class UTextureRenderTarget2D;
class UTexture2D;
struct BufferDescriptor;
enum class ETG_TextureFormat : uint8;
enum class ETSBufferFormat;
class TiledBlob;
typedef std::shared_ptr<TiledBlob>		TiledBlobPtr;

typedef cti::continuable<bool>			AsyncBool;
typedef cti::continuable<FLinearColor>	AsyncLinearColor;

struct TEXTUREGRAPHENGINE_API TextureHelper
{
	static TiledBlobPtr					GBlack;					/// Black RGBA texture
	static TiledBlobPtr					GWhite;					/// White RGBA texture
	static TiledBlobPtr					GGray;					/// Gray RGBA texture
	static TiledBlobPtr					GRed;					/// Red RGBA texture
	static TiledBlobPtr					GGreen;					/// Green RGBA texture
	static TiledBlobPtr					GBlue;					/// Blue RGBA texture
	static TiledBlobPtr					GYellow;				/// Yellow RGBA texture
	static TiledBlobPtr					GMagenta;				/// Magenta RGBA texture
	static TiledBlobPtr					GDefaultNormal;			/// Default Normal RGBA texture
	static TiledBlobPtr					GWhiteMask;				/// Default white mask
	static TiledBlobPtr					GBlackMask;				/// Default black mask

	static cti::continuable<bool>		InitSolidTexture(TiledBlobPtr* BlobObj, FLinearColor Color, FString Name, struct TexDescriptor Desc);
	static void							InitStockTextures();
	static void							FreeStockTextures();

	static FORCEINLINE TiledBlobPtr		GetBlack() { check(GBlack); return GBlack; }
	static FORCEINLINE TiledBlobPtr		GetWhite() { check(GWhite); return GWhite; }
	static FORCEINLINE TiledBlobPtr		GetGray() { check(GGray);  return GGray; }
	static FORCEINLINE TiledBlobPtr		GetRed() { check(GRed);  return GRed; }
	static FORCEINLINE TiledBlobPtr		GetGreen() { check(GGreen);  return GGreen; }
	static FORCEINLINE TiledBlobPtr		GetBlue() { check(GBlue);  return GBlue; }
	static FORCEINLINE TiledBlobPtr		GetYellow() { check(GYellow);  return GYellow; }
	static FORCEINLINE TiledBlobPtr		GetMagenta() { check(GMagenta);  return GMagenta; }
	static FORCEINLINE TiledBlobPtr		GetErrorBlob() { return GetMagenta(); }
	static FORCEINLINE TiledBlobPtr		GetDefaultNormal() { check(GDefaultNormal); return GDefaultNormal; }
	static FORCEINLINE TiledBlobPtr		GetWhiteMask() { check(GWhiteMask); return GWhiteMask; }
	static FORCEINLINE TiledBlobPtr		GetBlackMask() { check(GBlackMask); return GBlackMask; }

	//////////////////////////////////////////////////////////////////////////
	static uint32						GetChannelsFromPixelFormat(EPixelFormat InPixelFormat);
	static uint32						GetBppFromPixelFormat(EPixelFormat InPixelFormat);
	static ETextureRenderTargetFormat	GetRenderTargetFormatFromPixelFormat(EPixelFormat InPixelFormat);
	static ETextureSourceFormat			GetSourceFormat(ETG_TextureFormat TGTextureFormat);
	static bool							GetBufferFormatAndChannelsFromTGTextureFormat(ETG_TextureFormat Format, BufferFormat& OutBufferFormat, uint32& OutBufferChannels);
	static EPixelFormat					GetPixelFormatFromRenderTargetFormat(ETextureRenderTargetFormat RTFormat);
	static bool							GetPixelFormatFromTextureSourceFormat(ETextureSourceFormat SourceFormat, EPixelFormat& OutPixelFormat, uint32& OutNumChannels);
	static ETextureSourceFormat			GetTextureSourceFormat(BufferFormat Format, uint32 ItemsPerPoint);
	static ETG_TextureFormat			GetTGTextureFormatFromChannelsAndFormat(uint32 ItemsPerPoint, BufferFormat Format);
	static uint32						GetNumChannelsFromTGTextureFormat(ETG_TextureFormat TextureFormat);
	static FString						GetChannelsTextFromItemsPerPoint(const int32 InItemsPerPoint);	
	static void							ClearRT(UTextureRenderTarget2D* RenderTarget, FLinearColor Color = FLinearColor::Transparent);
	static void							ClearRT(FRHICommandList& RHI, UTextureRenderTarget2D* RenderTarget, FLinearColor Color = FLinearColor::Transparent);

	static const char*					TextureTypeToString(TextureType Type);
	static TextureType					TextureStringToType(const FString& TypeString);
	static const char*					TextureTypeToMegascansType(TextureType Type);

	static TextureType					TextureContentToTextureType(TextureContent Content);
	static TextureContent				TextureTypeToTextureContent(TextureType Type);
	static MaskType						TextureContentToMaskType(TextureContent Content);
	static TextureContent				MaskTypeToTextureContent(MaskType Type);
	static MaskModifierType				TextureContentToMaskModifier(TextureContent Content);
	static TextureContent				MaskModifierToTextureContent(MaskModifierType Type);

	static RawBufferPtr					RawFromRT(UTextureRenderTarget2D* RenderTarget, const BufferDescriptor& Desc);
	static RawBufferPtr					RawFromTexture(UTexture2D* Texture, const BufferDescriptor& Desc);
	static RawBufferPtr					RawFromResource(const FTexture2DRHIRef& ResourceRHI, const BufferDescriptor& Desc);
	static BufferFormat					FindOptimalSupportedFormat(BufferFormat SrcFormat);

	static void							RawFromRT_Tiled(UTextureRenderTarget2D* RenderTarget, const BufferDescriptor& Desc, size_t TileSizeX, size_t TileSizeY, RawBufferPtrTiles& Tiles);
	static void							RawFromTexture_Tiled(UTexture2D* Texture, const BufferDescriptor& Desc, size_t TileSizeX, size_t TileSizeY, RawBufferPtrTiles& Tiles);
	static void							RawFromResource_Tiled(FTexture2DRHIRef ResourceRHI, const BufferDescriptor& Desc, size_t TileSizeX, size_t TileSizeY, RawBufferPtrTiles& Tiles);
	static void							RawFromMem_Tiled(const uint8* SrcData, size_t SrcDataLength, const BufferDescriptor& SrcDesc, size_t TileSizeX, size_t TileSizeY, RawBufferPtrTiles& Tiles);
	static RawBufferPtr					CombineRaw_Tiles(const RawBufferPtrTiles& Tiles, CHashPtr HashValue = nullptr, bool bIsTransient = false);

	static bool							IsFloatRT(UTextureRenderTarget2D* RenderTarget);
	static FLinearColor					GetPixelValueFromRaw(RawBufferPtr RawObj, int32 Width, int32 Height, int32 X, int32 Y);
	static AsyncBool					ExportRaw(RawBufferPtr RawObj, const FString& CompletePath);
	static bool							CanSupportTexture(UTexture* Tex);
	static bool							CanSplitToTiles(UTexture* Texture, int TilesX, int TilesY);
	static size_t						RoundUpTo(size_t Size, size_t DesiredRounding);

	//////////////////////////////////////////////////////////////////////////
	/// Inline functions
	//////////////////////////////////////////////////////////////////////////
	static FORCEINLINE FString			CreateTileName(const FString& BaseName, int32 TileX, int32 TileY)
	{
		return FString::Printf(TEXT("%s-%d,%d"), *BaseName, TileX, TileY);
	}

	template <class T>
	static FORCEINLINE bool				IsPowerOf2(T Value)
	{
		return Value > 0 && (Value & (Value - 1)) == 0;
	}

	template <class T>
	static FORCEINLINE T				FloorToPowerOf2(T Value)
	{
		Value = Value | (Value >> 1);
		Value = Value | (Value >> 2);
		Value = Value | (Value >> 4);
		Value = Value | (Value >> 8);
		Value = Value | (Value >> 16);
		return Value - (Value >> 1);;
	}

	template <class T>
	static FORCEINLINE T				CeilToPowerOf2(T Value)
	{
		Value--;

		Value |= Value >> 1;
		Value |= Value >> 2;
		Value |= Value >> 4;
		Value |= Value >> 8;
		Value |= Value >> 16;
		Value++;

		return Value;
	}


	// Calculate the total number of mip levels for a given base size.
	// This is including the level 0 of said base size 
	static FORCEINLINE uint32			CalcNumMipLevels(uint32 BaseSize)
	{
		return (uint32)(ceil(log2(double(BaseSize))) + 1.0);
	}
};

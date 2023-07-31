// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "Engine/Texture.h"
#include "Memory/SharedBuffer.h"

namespace UE
{
	namespace Interchange
	{
		struct INTERCHANGEIMPORT_API FImportImage
		{
			virtual ~FImportImage() = default;
			FImportImage() = default;
			FImportImage(FImportImage&&) = default;
			FImportImage& operator=(FImportImage&&) = default;

			FImportImage(const FImportImage&) = delete;
			FImportImage& operator=(const FImportImage&) = delete;

			FUniqueBuffer RawData;

			/** Which compression format (if any) that is applied to RawData */
			ETextureSourceCompressionFormat RawDataCompressionFormat = TSCF_None;

			ETextureSourceFormat Format = TSF_Invalid;
			TextureCompressionSettings CompressionSettings = TC_Default;
			int32 NumMips = 0;
			int32 SizeX = 0;
			int32 SizeY = 0;
			bool bSRGB = true;
			TOptional<TextureMipGenSettings> MipGenSettings;

			void Init2DWithParams(int32 InSizeX, int32 InSizeY, ETextureSourceFormat InFormat, bool bInSRGB, bool bShouldAllocateRawData = true);
			void Init2DWithParams(int32 InSizeX, int32 InSizeY, int32 InNumMips, ETextureSourceFormat InFormat, bool bInSRGB, bool bShouldAllocateRawData = true);
			void Init2DWithOneMip(int32 InSizeX, int32 InSizeY, ETextureSourceFormat InFormat, const void* InData = nullptr);
			void Init2DWithMips(int32 InSizeX, int32 InSizeY, int32 InNumMips, ETextureSourceFormat InFormat, const void* InData = nullptr);

			virtual int64 GetMipSize(int32 InMipIndex) const;
			virtual int64 ComputeBufferSize() const;

			TArrayView64<uint8> GetArrayViewOfRawData();
			virtual bool IsValid() const;
		};

		struct INTERCHANGEIMPORT_API FImportImageHelper
		{
			/**
			 * Tests if the given height and width specify a supported texture resolution to import; Can optionally check if the height/width are powers of two
			 *
			 * @param Width The width of an imported texture whose validity should be checked
			 * @param Height The height of an imported texture whose validity should be checked
			 * @param bAllowNonPowerOfTwo Whether or not non-power-of-two textures are allowed
			 * @param OutErrorMessage Optional output for an error message
			 *
			 * @return bool true if the given height/width represent a supported texture resolution, false if not
			 */
			static bool IsImportResolutionValid(int32 Width, int32 Height, bool bAllowNonPowerOfTwo, FText* OutErrorMessage = nullptr);
		};
	}//ns Interchange
}//ns UE



// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Texture/InterchangeTexturePayloadData.h"

#include "CoreMinimal.h"
#include "Engine/Texture.h"
#include "Memory/SharedBuffer.h"

namespace UE
{
	namespace Interchange
	{
		// Also known as a UDIMs texture
		struct FImportBlockedImage
		{
			FUniqueBuffer RawData;
			TArray<FTextureSourceBlock> BlocksData;

			ETextureSourceFormat Format = TSF_Invalid;
			TextureCompressionSettings CompressionSettings = TC_Default;
			bool bSRGB = true;
			TOptional<TextureMipGenSettings> MipGenSettings;


			int64 ComputeBufferSize() const;
			bool IsValid() const;


			bool InitDataSharedAmongBlocks(const FImportImage& Image);

			/**
			 * Everything except the RawData and BlocksData must be initialized before calling this function
			 */
			bool InitBlockFromImage(int32 BlockX, int32 BlockY, const FImportImage& Image);

			/**
			 * Everything except the RawData must be initialized before calling this function
			 */
			bool MigrateDataFromImagesToRawData(TArray<FImportImage>& Images);
		};
	}
}
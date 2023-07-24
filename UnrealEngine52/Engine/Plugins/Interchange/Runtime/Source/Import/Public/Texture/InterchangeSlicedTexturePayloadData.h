// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Texture/InterchangeTexturePayloadData.h"

namespace UE
{
	namespace Interchange
	{
		struct INTERCHANGEIMPORT_API FImportSlicedImage : public FImportImage
		{
			int32 NumSlice = 0;
			bool bIsVolume = false;

			void Init(int32 InSizeX, int32 InSizeY, int32 InNumSlice, int32 InNumMips, ETextureSourceFormat InFormat, bool InSRGB);
			void InitVolume(int32 InSizeX, int32 InSizeY, int32 InNumSlice, int32 InNumMips, ETextureSourceFormat InFormat, bool InSRGB);

			const uint8* GetMipData(int32 InMipIndex, int32 InSliceIndex = INDEX_NONE) const;
			uint8* GetMipData(int32 InMipIndex, int32 InSliceIndex = INDEX_NONE);

			virtual int64 GetMipSize(int32 InMipIndex) const override;
			virtual int64 ComputeBufferSize() const override;

			virtual bool IsValid() const override;
		};
	}
}

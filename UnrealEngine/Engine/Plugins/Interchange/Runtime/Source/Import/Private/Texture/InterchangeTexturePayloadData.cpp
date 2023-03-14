// Copyright Epic Games, Inc. All Rights Reserved.

#include "Texture/InterchangeTexturePayloadData.h"

#include "CoreMinimal.h"
#include "Engine/Texture.h"
#include "HAL/IConsoleManager.h"
#include "Misc/MessageDialog.h"
#include "RHI.h"

bool UE::Interchange::FImportImageHelper::IsImportResolutionValid(int32 Width, int32 Height, bool bAllowNonPowerOfTwo, FText* OutErrorMessage)
{
	// VT res is currently limited by pixel count fitting in int32
	const int64 MaximumSupportedVirtualTextureResolution = 32768;

	// limit on current rendering RHI : == GetMax2DTextureDimension()
	const int64 CurrentRHIMaxResolution = int64(1) << (GMaxTextureMipCount - 1);

	// MaximumSupportedResolutionNonVT is only a popup/warning , not a hard limit
#if WITH_EDITOR
	// Get the non-VT size limit :
	int64 MaximumSupportedResolutionNonVT = (int64)UTexture::GetMaximumDimensionOfNonVT();
	MaximumSupportedResolutionNonVT = FMath::Min(MaximumSupportedResolutionNonVT, CurrentRHIMaxResolution);
#else
	int64 MaximumSupportedResolutionNonVT = CurrentRHIMaxResolution;
#endif

	// No zero-size textures :
	if (Width == 0 || Height == 0)
	{
		if (OutErrorMessage)
		{
			*OutErrorMessage = NSLOCTEXT("Interchange", "Warning_TextureSizeZero", "Texture has zero width or height");
		}

		return false;
	}

	bool bTextureTooLargeOrInvalidResolution = false;

	// Dimensions must fit in signed int32
	//  could be negative here if it was over 2G and int32 was used earlier
	if (Width < 0 || Height < 0 || Width > MAX_int32 || Height > MAX_int32)
	{
		bTextureTooLargeOrInvalidResolution = true;
	}

	// pixel count must fit in int32 :
	//  mip surface could still be larger than 2 GB, that's allowed
	//	eg. 16k RGBA float = 4 GB
	if (Width * Height > MAX_int32)
	{
		bTextureTooLargeOrInvalidResolution = true;
	}

	if ((Width * Height) > FMath::Square(MaximumSupportedVirtualTextureResolution))
	{
		bTextureTooLargeOrInvalidResolution = true;
	}

	if (bTextureTooLargeOrInvalidResolution)
	{
		if (OutErrorMessage)
		{
			*OutErrorMessage = FText::Format(
				NSLOCTEXT("Interchange", "Warning_TextureSizeTooLargeOrInvalid", "Texture is too large to import or it has an invalid resolution. The current maximum is {0} pixels"),
				FText::AsNumber(FMath::Square(MaximumSupportedVirtualTextureResolution)));
		}

		return false;
	}

	if (Width > MaximumSupportedResolutionNonVT || Height > MaximumSupportedResolutionNonVT)
	{
		const TConsoleVariableData<int32>* CVarVirtualTexturesEnabled = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VirtualTextures")); check(CVarVirtualTexturesEnabled);
		check(CVarVirtualTexturesEnabled != nullptr);

		if (!CVarVirtualTexturesEnabled->GetValueOnAnyThread())
		{
			const FText VTMessage = NSLOCTEXT("Interchange", "Warning_LargeTextureVTDisabled", "\nWarning: Virtual Textures are disabled in this project.");

			if (EAppReturnType::Yes != FMessageDialog::Open(EAppMsgType::YesNo, EAppReturnType::Yes, FText::Format(
				NSLOCTEXT("Interchange", "Warning_LargeTextureImport", "Attempting to import {0} x {1} texture, proceed?\nLargest supported non-VT texture size: {2} x {3}{4}"),
				FText::AsNumber(Width), FText::AsNumber(Height), FText::AsNumber(MaximumSupportedResolutionNonVT), FText::AsNumber(MaximumSupportedResolutionNonVT), VTMessage)))
			{
				return false;
			}
		}
	}

	// Check if the texture dimensions are powers of two
	if (!bAllowNonPowerOfTwo)
	{
		const bool bIsPowerOfTwo = FMath::IsPowerOfTwo(Width) && FMath::IsPowerOfTwo(Height);
		if (!bIsPowerOfTwo)
		{
			*OutErrorMessage = NSLOCTEXT("Interchange", "Warning_TextureNotAPowerOfTwo", "Cannot import texture with non-power of two dimensions");
			return false;
		}
	}

	return true;
}

void UE::Interchange::FImportImage::Init2DWithParams(int32 InSizeX, int32 InSizeY, ETextureSourceFormat InFormat, bool bInSRGB, bool bShouldAllocateRawData)
{
	Init2DWithParams(InSizeX, InSizeY, 1, InFormat, bInSRGB, bShouldAllocateRawData);
}

void UE::Interchange::FImportImage::Init2DWithParams(int32 InSizeX, int32 InSizeY, int32 InNumMips, ETextureSourceFormat InFormat, bool bInSRGB, bool bShouldAllocateRawData)
{
	SizeX = InSizeX;
	SizeY = InSizeY;
	NumMips = InNumMips;
	Format = InFormat;
	bSRGB = bInSRGB;
	if (bShouldAllocateRawData)
	{
		RawData = FUniqueBuffer::Alloc(ComputeBufferSize());
	}
}

void UE::Interchange::FImportImage::Init2DWithOneMip(int32 InSizeX, int32 InSizeY, ETextureSourceFormat InFormat, const void* InData)
{
	Init2DWithParams(InSizeX, InSizeY, 1, InFormat, bSRGB);

	if (InData)
	{
		FMemory::Memcpy(RawData.GetData(), InData, RawData.GetSize());
	}
}

void UE::Interchange::FImportImage::Init2DWithMips(int32 InSizeX, int32 InSizeY, int32 InNumMips, ETextureSourceFormat InFormat, const void* InData)
{
	Init2DWithParams(InSizeX, InSizeY, 1, InFormat, bSRGB);

	if (InData)
	{
		FMemory::Memcpy(RawData.GetData(), InData, RawData.GetSize());
	}
}

int64 UE::Interchange::FImportImage::GetMipSize(int32 InMipIndex) const
{
	check(InMipIndex >= 0);
	check(InMipIndex < NumMips);
	const int32 MipSizeX = FMath::Max(SizeX >> InMipIndex, 1);
	const int32 MipSizeY = FMath::Max(SizeY >> InMipIndex, 1);
	return (int64)MipSizeX * MipSizeY * FTextureSource::GetBytesPerPixel(Format);
}

int64 UE::Interchange::FImportImage::ComputeBufferSize() const
{
	int64 TotalSize = 0;
	for (int32 MipIndex = 0; MipIndex < NumMips; ++MipIndex)
	{
		TotalSize += GetMipSize(MipIndex);
	}

	return TotalSize;
}

TArrayView64<uint8> UE::Interchange::FImportImage::GetArrayViewOfRawData()
{
	return TArrayView64<uint8>(static_cast<uint8*>(RawData.GetData()), RawData.GetSize());
}

bool UE::Interchange::FImportImage::IsValid() const
{
	bool bIsRawDataBufferValid = false;

	if (RawDataCompressionFormat == TSCF_None)
	{
		bIsRawDataBufferValid = ComputeBufferSize() == RawData.GetSize();
	}
	else
	{
		bIsRawDataBufferValid = !RawData.IsNull();
	}

	return SizeX > 0
		&& SizeY > 0
		&& NumMips > 0
		&& Format != TSF_Invalid
		&& bIsRawDataBufferValid;
}

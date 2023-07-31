// Copyright Epic Games, Inc. All Rights Reserved.

#include "GenericImgMediaReader.h"
#include "ImgMediaPrivate.h"

#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImgMediaLoader.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"


TSharedPtr<IImageWrapper> FGenericImgMediaReader::LoadFrameImage(const FString& ImagePath, TArray64<uint8>& OutBuffer, FImgMediaFrameInfo& OutInfo, bool bUseLoaderFirstFrame)
{
	// load image into buffer
	if (!FFileHelper::LoadFileToArray(OutBuffer, *ImagePath))
	{
		UE_LOG(LogImgMedia, Warning, TEXT("FGenericImgMediaReader: Failed to load %s"), *ImagePath);
		return nullptr;
	}

	// determine image format
	EImageFormat ImageFormat;

	const FString Extension = FPaths::GetExtension(ImagePath).ToLower();

	if (Extension == TEXT("bmp"))
	{
		ImageFormat = EImageFormat::BMP;
		OutInfo.FormatName = TEXT("BMP");
	}
	else if ((Extension == TEXT("jpg")) || (Extension == TEXT("jpeg")))
	{
		ImageFormat = EImageFormat::JPEG;
		OutInfo.FormatName = TEXT("JPEG");
	}
	else if (Extension == TEXT("png"))
	{
		ImageFormat = EImageFormat::PNG;
		OutInfo.FormatName = TEXT("PNG");
	}
	else
	{
		UE_LOG(LogImgMedia, Warning, TEXT("FGenericImgMediaReader: Unsupported file format in %s"), *ImagePath);
		return nullptr;
	}

	// create image wrapper
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);

	if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(OutBuffer.GetData(), OutBuffer.Num()))
	{
		UE_LOG(LogImgMedia, Warning, TEXT("FGenericImgMediaReader: Failed to create image wrapper for %s"), *ImagePath);
		return nullptr;
	}

	if (bUseLoaderFirstFrame && LoaderPtr.IsValid())
	{
		const TSharedPtr<FImgMediaLoader, ESPMode::ThreadSafe> Loader = LoaderPtr.Pin();

		OutInfo.Dim = Loader->GetSequenceDim();
		OutInfo.NumMipLevels = Loader->GetNumMipLevels();

		const SIZE_T SizeMip0 = (SIZE_T)OutInfo.Dim.X * OutInfo.Dim.Y * 4;
		OutInfo.UncompressedSize = SizeMip0;
		for (int32 Level = 1; Level < OutInfo.NumMipLevels; Level++)
		{
			OutInfo.UncompressedSize += SizeMip0 >> (2 * Level);
		}
	}
	else
	{
		OutInfo.Dim.X = ImageWrapper->GetWidth();
		OutInfo.Dim.Y = ImageWrapper->GetHeight();
		OutInfo.UncompressedSize = (SIZE_T)OutInfo.Dim.X * OutInfo.Dim.Y * 4;
		OutInfo.NumMipLevels = 1;
	}

	// get file info
	const UImgMediaSettings* Settings = GetDefault<UImgMediaSettings>();
	OutInfo.CompressionName = TEXT("");
	OutInfo.FrameRate = Settings->DefaultFrameRate;
	OutInfo.Srgb = true;
	OutInfo.NumChannels = 4;
	OutInfo.bHasTiles = false;
	OutInfo.TileDimensions = OutInfo.Dim;
	OutInfo.NumTiles = FIntPoint(1, 1);
	OutInfo.TileBorder = 0;

	return ImageWrapper;
}

/* FGenericImgMediaReader structors
 *****************************************************************************/

FGenericImgMediaReader::FGenericImgMediaReader(IImageWrapperModule& InImageWrapperModule, const TSharedRef<FImgMediaLoader, ESPMode::ThreadSafe>& InLoader)
	: ImageWrapperModule(InImageWrapperModule)
	, LoaderPtr(InLoader)
{ }


/* FGenericImgMediaReader interface
 *****************************************************************************/

bool FGenericImgMediaReader::GetFrameInfo(const FString& ImagePath, FImgMediaFrameInfo& OutInfo)
{
	TArray64<uint8> InputBuffer;
	TSharedPtr<IImageWrapper> ImageWrapper = LoadFrameImage(ImagePath, InputBuffer, OutInfo, false);

	return ImageWrapper.IsValid();
}


bool FGenericImgMediaReader::ReadFrame(int32 FrameId, const TMap<int32, FImgMediaTileSelection>& InMipTiles, TSharedPtr<FImgMediaFrame, ESPMode::ThreadSafe> OutFrame)
{
	const TSharedPtr<FImgMediaLoader, ESPMode::ThreadSafe> Loader = LoaderPtr.Pin();
	if (Loader.IsValid() == false)
	{
		return false;
	}

	if (InMipTiles.IsEmpty())
	{
		return false;
	}

	SIZE_T BufferDataOffset = 0;
	const int32 NumMipLevels = Loader->GetNumMipLevels();
	const FIntPoint BaseLevelDim = Loader->GetSequenceDim();
	FIntPoint CurrenDim = BaseLevelDim;

	// Loop over all mips.
	for (int32 CurrentMipLevel = 0; CurrentMipLevel < NumMipLevels; ++CurrentMipLevel)
	{
		if (InMipTiles.Contains(CurrentMipLevel))
		{
			const FImgMediaTileSelection& CurrentSelection = InMipTiles[CurrentMipLevel];

			// Do we want to read in this mip?
			bool ReadThisMip = !OutFrame->MipTilesPresent.Contains(CurrentMipLevel);
			if (ReadThisMip)
			{
				// Load image.
				const FString& ImagePath = Loader->GetImagePath(FrameId, CurrentMipLevel);

				TArray64<uint8> InputBuffer;
				FImgMediaFrameInfo Info;
				TSharedPtr<IImageWrapper> ImageWrapper = LoadFrameImage(ImagePath, InputBuffer, Info, true);

				if (!ImageWrapper.IsValid())
				{
					UE_LOG(LogImgMedia, Warning, TEXT("FGenericImgMediaReader: Failed to load image %s"), *ImagePath);
					return false;
				}

				TArray64<uint8> RawData;
				if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
				{
					UE_LOG(LogImgMedia, Warning, TEXT("FGenericImgMediaReader: Failed to get image data for %s"), *ImagePath);
					return false;
				}

				// Create the full buffer for data.
				if (!OutFrame->Data.IsValid())
				{
					int64 AllocSize = (SIZE_T)BaseLevelDim.X * BaseLevelDim.Y * 4;
					// Need more space for mips.
					if (NumMipLevels > 1)
					{
						AllocSize = (AllocSize * 4) / 3;
					}
					void* Buffer = FMemory::Malloc(AllocSize, PLATFORM_CACHE_LINE_SIZE);
					OutFrame->Info = Info;
					OutFrame->Data = MakeShareable(Buffer, [](void* ObjectToDelete) { FMemory::Free(ObjectToDelete); });
					OutFrame->MipTilesPresent.Reset();
					OutFrame->Format = EMediaTextureSampleFormat::CharBGRA;
					OutFrame->Stride = OutFrame->Info.Dim.X * 4;
				}

				// Copy data to our buffer with the right mip level offset
				FMemory::Memcpy((void*)((uint8*)OutFrame->Data.Get() + BufferDataOffset), RawData.GetData(), RawData.Num());
				OutFrame->MipTilesPresent.Emplace(CurrentMipLevel, CurrentSelection);
				OutFrame->NumTilesRead++;
			}
		}

		// Next level.
		BufferDataOffset += (CurrenDim.X * CurrenDim.Y * 4);
		CurrenDim /= 2;
	}

	return true;
}

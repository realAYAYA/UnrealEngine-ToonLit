// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuR/ImagePrivate.h"
#include "MuR/MutableMath.h"
#include "MuR/Platform.h"


namespace mu
{

	bool FImageOperator::ImageCrop( Image* InCropped, int32 CompressionQuality, const Image* InBase, const box< FIntVector2 >& Rect)
	{
		Ptr<const Image> Base = InBase;
		Ptr<Image> BaseReformat;
		Ptr<Image> Cropped = InCropped;

		EImageFormat BaseFormat = Base->GetFormat();
		EImageFormat UncompressedFormat = GetUncompressedFormat(BaseFormat);

		if (BaseFormat != UncompressedFormat)
		{
			// Compressed formats need decompression + compression after crop			
			// \TODO: This may use some additional untracked memory locally in this function.
			BaseReformat = ImagePixelFormat(CompressionQuality, Base.get(), UncompressedFormat);
			Base = BaseReformat;
			Cropped = CreateImage(InCropped->GetSizeX(), InCropped->GetSizeY(), InCropped->GetLODCount(), UncompressedFormat, EInitializationType::NotInitialized);
        }

		const FImageFormatData& finfo = GetImageFormatData(UncompressedFormat);

		check(Base && Cropped);
		check(Cropped->GetSizeX() == Rect.size[0]);
		check(Cropped->GetSizeY() == Rect.size[1]);

		// TODO: better error control. This happens if some layouts are corrupt.
		bool bCorrect =
				(Rect.min[0] >= 0 && Rect.min[1] >= 0) &&
				(Rect.size[0] >= 0 && Rect.size[1] >= 0) &&
				(Rect.min[0] + Rect.size[0] <= Base->GetSizeX()) &&
				(Rect.min[1] + Rect.size[1] <= Base->GetSizeY());

		if (!bCorrect)
		{
			return false;
		}

		// Block images are not supported for now
		check(finfo.PixelsPerBlockX == 1);
		check(finfo.PixelsPerBlockY == 1);

		checkf(Rect.min[0] % finfo.PixelsPerBlockX == 0, TEXT("Rect must snap to blocks."));
		checkf(Rect.min[1] % finfo.PixelsPerBlockY == 0, TEXT("Rect must snap to blocks."));
		checkf(Rect.size[0] % finfo.PixelsPerBlockX == 0, TEXT("Rect must snap to blocks."));
		checkf(Rect.size[1] % finfo.PixelsPerBlockY == 0, TEXT("Rect must snap to blocks."));

		int32 baseRowSize = finfo.BytesPerBlock * FMath::DivideAndRoundUp(Base->GetSizeX(), uint16(finfo.PixelsPerBlockX));
		int32 cropRowSize = finfo.BytesPerBlock * FMath::DivideAndRoundUp(Rect.size[0], int32(finfo.PixelsPerBlockX));

        const uint8* pBaseBuf = Base->GetLODData(0);
        uint8* pCropBuf = Cropped->GetLODData(0);

		int32 SkipPixels = Base->GetSizeX() * Rect.min[1] + Rect.min[0];
		pBaseBuf += finfo.BytesPerBlock * SkipPixels / finfo.PixelsPerBlockX;
		for (int32 Y = 0; Y < Rect.size[1]; ++Y)
		{
			FMemory::Memcpy(pCropBuf, pBaseBuf, cropRowSize);
			pCropBuf += cropRowSize;
			pBaseBuf += baseRowSize;
		}

		if (BaseFormat != UncompressedFormat)
		{
			ReleaseImage(BaseReformat);

			bool bSuccess = false;
			ImagePixelFormat(bSuccess, CompressionQuality, InCropped, Cropped.get());
			check(bSuccess);

			ReleaseImage(Cropped);
		}

		return true;
	}

}

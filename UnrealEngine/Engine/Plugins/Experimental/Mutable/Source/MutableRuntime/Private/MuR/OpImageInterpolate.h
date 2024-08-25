// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/Platform.h"

#include "Async/ParallelFor.h"

namespace mu
{

    inline void ImageInterpolate(Image* DestImage, const Image* BImage, float Factor)
	{
		MUTABLE_CPUPROFILER_SCOPE(ImageInterpolate)

		check(DestImage && BImage);
        check(DestImage->GetSizeX() == BImage->GetSizeX());
        check(DestImage->GetSizeY() == BImage->GetSizeY());
        check(DestImage->GetFormat() == BImage->GetFormat());

		// Clamp the factor
		Factor = FMath::Clamp(Factor, 0.0f, 1.0f);
		const uint16 FactorUNorm = static_cast<uint16>(Factor * 255.0f);

		const int32 BytesPerElem = GetImageFormatData(DestImage->GetFormat()).BytesPerBlock;

		constexpr int32 NumBatchElems = 1 << 14;

		const int32 NumBatches = DestImage->DataStorage.GetNumBatches(NumBatchElems, BytesPerElem);
		check(BImage->DataStorage.GetNumBatches(NumBatchElems, BytesPerElem) == NumBatches);

		// Generic implementation	
		auto ProcessBatch = [DestImage, BImage, NumBatchElems, BytesPerElem, FactorUNorm](int32 BatchId)
		{
			TArrayView<uint8> DestView = DestImage->DataStorage.GetBatch(BatchId, NumBatchElems, BytesPerElem);
			TArrayView<const uint8> BView = BImage->DataStorage.GetBatch(BatchId, NumBatchElems, BytesPerElem);

			const uint8* BBuf = BView.GetData();
			uint8* DestBuf = DestView.GetData();

			const int32 NumBytes = DestView.Num();
			check(BView.Num() == NumBytes);

			switch (DestImage->GetFormat())
			{
			case EImageFormat::IF_L_UBYTE:
			case EImageFormat::IF_RGB_UBYTE:
			case EImageFormat::IF_BGRA_UBYTE:
			case EImageFormat::IF_RGBA_UBYTE:
			{
				for (int32 I = 0; I < NumBytes; ++I)
				{
					uint16 AValue = DestBuf[I];
					uint16 BValue = BBuf[I];
					DestBuf[I] = static_cast<uint8>((AValue * (255 - FactorUNorm) + BValue*FactorUNorm) / 255);
				}
				break;
			}
			default:
				check(false);
			}
		};

		if (NumBatches == 1)
		{
			ProcessBatch(0);
		}
		else if (NumBatches > 1)
		{
			ParallelFor(NumBatches, ProcessBatch);
		}

	}

}

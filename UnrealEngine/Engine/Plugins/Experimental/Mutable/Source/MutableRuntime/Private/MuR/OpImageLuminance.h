// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/Platform.h"

#include "Async/ParallelFor.h"

namespace mu
{

	inline void ImageLuminance(Image* DestImage, const Image* AImage)
	{
		check(DestImage && AImage && DestImage->GetFormat() == EImageFormat::IF_L_UBYTE);

		const int32 BytesPerElem = GetImageFormatData(AImage->GetFormat()).BytesPerBlock;
		constexpr int32 NumBatchElems = 1 << 14;

		const int32 NumBatches = DestImage->DataStorage.GetNumBatches(NumBatchElems, 1); 
		check(NumBatches == AImage->DataStorage.GetNumBatches(NumBatchElems, BytesPerElem)); 

        auto ProcessBatch = [DestImage, AImage, NumBatchElems, BytesPerElem](int32 BatchId)
        {
			TArrayView<uint8> DestView = DestImage->DataStorage.GetBatch(BatchId, NumBatchElems, 1);
			TArrayView<const uint8> AView = AImage->DataStorage.GetBatch(BatchId, NumBatchElems, BytesPerElem);

			uint8* DestBuf = DestView.GetData(); 
			const uint8* ABuf = AView.GetData(); 
			
            const int32 NumElems = DestView.Num() / 1;

            switch (AImage->GetFormat())
            {
            case EImageFormat::IF_RGB_UBYTE:
            {
                for (int32 I = 0; I < NumElems; ++I)
                {
                    const uint16 L = (76*ABuf[3*I + 0] + 150*ABuf[3*I + 1] + 29*ABuf[3*I + 2]);
                    DestBuf[I] = uint8(L / 255);
                }
                break;
            }

            case EImageFormat::IF_RGBA_UBYTE:
            {
                for (int32 I = 0; I < NumElems; ++I)
                {
                    const uint16 L = (76*ABuf[4*I + 0] + 150*ABuf[4*I + 1] + 29*ABuf[4*I + 2]);
                    DestBuf[I] = uint8(L / 255);
                }
                break;
            }

            case EImageFormat::IF_BGRA_UBYTE:
            {
                for (int32 I = 0; I < NumElems; ++I)
                {
                    const uint16 L = (76*ABuf[4*I + 2] + 150*ABuf[4*I + 1] + 29*ABuf[4*I + 0]);
                    DestBuf[I] = uint8(L / 255);
                }
                break;
            }

            default:
                check(false);
                break;
            }
        };

        if (NumBatches == 1)
        {
            ProcessBatch(0);
        }
        else
        {
            ParallelFor(NumBatches, ProcessBatch);
        }
	}

}

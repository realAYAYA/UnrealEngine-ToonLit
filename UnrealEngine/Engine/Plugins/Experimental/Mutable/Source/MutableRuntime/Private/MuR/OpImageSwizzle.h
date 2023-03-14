// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "Async/ParallelFor.h"
#include "MuR/Platform.h"

namespace mu
{

	inline ImagePtr ImageSwizzle
		(
			EImageFormat format,
			const ImagePtrConst pSources[],
            const uint8_t channels[]
		)
	{
		MUTABLE_CPUPROFILER_SCOPE(ImageSwizzle);

        ImagePtr pDest = new Image( pSources[0]->GetSizeX(), pSources[0]->GetSizeY(),
                                    pSources[0]->GetLODCount(),
                                    format );

		// Very slow generic implementations
        size_t pixelCount = pDest->CalculatePixelCount();

        // Pixelcount should already match, but due to bugs it may not be the case. Try to detect it,
        // but avoid crashing below:
        uint16 numChannels = GetImageFormatData(format).m_channels;
        for (uint16 c=0;c<numChannels; ++c)
        {
            if (pSources[c])
            {
                size_t sourcePixelCount = pSources[c]->CalculatePixelCount();
                if (pixelCount>sourcePixelCount)
                {
                    check(false);

                    // Something went wrong
                    pixelCount = sourcePixelCount;
                }
            }
        }

		int NumDestChannels = 0;

		switch ( format )
		{
		case EImageFormat::IF_L_UBYTE:
			NumDestChannels = 1;
			break;

		case EImageFormat::IF_RGB_UBYTE:
			NumDestChannels = 3;
			break;

        case EImageFormat::IF_RGBA_UBYTE:
        case EImageFormat::IF_BGRA_UBYTE:
			NumDestChannels = 4;
			break;

        default:
			check(false);
		}

		constexpr int PixelCountConcurrencyThreshold = 0xff;

		for (int i = 0; i < NumDestChannels; ++i)
		{
			uint8_t* pDestBuf = pDest->GetData() + i;

			if (format == EImageFormat::IF_BGRA_UBYTE)
			{
				if (i == 0)
				{
					pDestBuf = pDest->GetData() + 2;
				}
				else if (i == 2)
				{
					pDestBuf = pDest->GetData() + 0;
				}
			}

			bool filled = false;

			if (pSources[i])
			{
				const uint8_t* pSourceBuf = pSources[i]->GetData() + channels[i];

				switch (pSources[i]->GetFormat())
				{
				case EImageFormat::IF_L_UBYTE:
					if (channels[i] < 1)
					{
						//for (size_t p = 0; p < pixelCount; ++p)
						ParallelFor(pixelCount,[pDestBuf, pSourceBuf, NumDestChannels](int p)
							{
								pDestBuf[p * NumDestChannels] = pSourceBuf[p];
							});

						filled = true;
					}
					break;

				case EImageFormat::IF_RGB_UBYTE:
					if (channels[i] < 3)
					{
						ParallelFor(pixelCount, [pDestBuf, pSourceBuf, NumDestChannels](int p)
							{
								pDestBuf[p * NumDestChannels] = pSourceBuf[p * 3];
							});
						filled = true;
					}
					break;

				case EImageFormat::IF_RGBA_UBYTE:
					if (channels[i] < 4)
					{
						ParallelFor(pixelCount, [pDestBuf, pSourceBuf, NumDestChannels](int p)
							{
								pDestBuf[p * NumDestChannels] = pSourceBuf[p * 4];
							});
						filled = true;
					}
					break;

				case EImageFormat::IF_BGRA_UBYTE:
					if (channels[i] == 0)
					{
						pSourceBuf = pSources[i]->GetData() + 2;
					}
					else if (channels[i] == 2)
					{
						pSourceBuf = pSources[i]->GetData() + 0;
					}
					if (channels[i] < 4)
					{
						ParallelFor(pixelCount, [pDestBuf, pSourceBuf, NumDestChannels](int p)
							{
								pDestBuf[p * NumDestChannels] = pSourceBuf[p * 4];
							});
						filled = true;
					}
					break;

				default:
					check(false);
				}
			}

			if (!filled)
			{
				// Source not set. Clear to 0
				ParallelFor(pixelCount, [pDestBuf, NumDestChannels](int p)
					{
						pDestBuf[p * NumDestChannels] = 0;
					});
			}
		}

		return pDest;
	}

}

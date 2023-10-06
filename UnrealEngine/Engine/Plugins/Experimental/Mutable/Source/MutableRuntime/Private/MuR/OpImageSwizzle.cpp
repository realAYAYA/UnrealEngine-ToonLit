// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuR/ImagePrivate.h"
#include "Async/ParallelFor.h"

namespace mu
{

	void ImageSwizzle( Image* Result, const Ptr<const Image> Sources[], const uint8 Channels[] )
	{
		MUTABLE_CPUPROFILER_SCOPE(ImageSwizzle);

		if (!Sources[0])
		{
			return;
		}

		// Very slow generic implementations
        int32 PixelCount = Result->CalculatePixelCount();

		EImageFormat Format = Result->GetFormat();

        // Pixelcount should already match, but due to bugs it may not be the case. Try to detect it,
        // but avoid crashing below:
        uint16 numChannels = GetImageFormatData(Format).Channels;
        for (uint16 c=0;c<numChannels; ++c)
        {
            if (Sources[c])
            {
                int32 SourcePixelCount = Sources[c]->CalculatePixelCount();
                if (PixelCount> SourcePixelCount)
                {
                    // Something went wrong
					PixelCount = SourcePixelCount;
                }
            }
        }

		int NumDestChannels = 0;

		switch (Format)
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

		for (int i = 0; i < NumDestChannels; ++i)
		{
			uint8* pDestBuf = Result->GetData() + i;

			if (Format == EImageFormat::IF_BGRA_UBYTE)
			{
				if (i == 0)
				{
					pDestBuf = Result->GetData() + 2;
				}
				else if (i == 2)
				{
					pDestBuf = Result->GetData() + 0;
				}
			}

			bool filled = false;

			constexpr int32 NumBatchElems = 4096*2;
			const int32 NumBatches = FMath::DivideAndRoundUp(PixelCount, NumBatchElems);

			if (Sources[i])
			{
				const uint8* pSourceBuf = Sources[i]->GetData() + Channels[i];

				switch (Sources[i]->GetFormat())
				{
				case EImageFormat::IF_L_UBYTE:
					if (Channels[i] < 1)
					{
						auto ProcessBatch = [pDestBuf, pSourceBuf, NumDestChannels, PixelCount, NumBatchElems ](int32 BatchId)
						{
							const int32 BatchBegin = BatchId * NumBatchElems;
							const int32 BatchEnd = FMath::Min(BatchBegin + NumBatchElems, PixelCount);

							for (int32 p = BatchBegin; p < BatchEnd; ++p)
							{
								pDestBuf[p * NumDestChannels] = pSourceBuf[p];
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

						filled = true;
					}
					break;

				case EImageFormat::IF_RGB_UBYTE:
					if (Channels[i] < 3)
					{
						auto ProcessBatch = [pDestBuf, pSourceBuf, NumDestChannels, PixelCount, NumBatchElems](int32 BatchId)
						{
							const int32 BatchBegin = BatchId * NumBatchElems;
							const int32 BatchEnd = FMath::Min(BatchBegin + NumBatchElems, PixelCount);

							for (int32 p = BatchBegin; p < BatchEnd; ++p)
							{
								pDestBuf[p * NumDestChannels] = pSourceBuf[p * 3];
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

						filled = true;
					}
					break;

				case EImageFormat::IF_RGBA_UBYTE:
					if (Channels[i] < 4)
					{	
						auto ProcessBatch = [pDestBuf, pSourceBuf, NumDestChannels, PixelCount, NumBatchElems](int32 BatchId)
						{
							const int32 BatchBegin = BatchId * NumBatchElems;
							const int32 BatchEnd = FMath::Min(BatchBegin + NumBatchElems, PixelCount);

							for (int32 p = BatchBegin; p < BatchEnd; ++p)
							{
								pDestBuf[p * NumDestChannels] = pSourceBuf[p * 4];
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

						filled = true;
					}
					break;

				case EImageFormat::IF_BGRA_UBYTE:
					if (Channels[i] == 0)
					{
						pSourceBuf = Sources[i]->GetData() + 2;
					}
					else if (Channels[i] == 2)
					{
						pSourceBuf = Sources[i]->GetData() + 0;
					}
					if (Channels[i] < 4)
					{
						auto ProcessBatch = [pDestBuf, pSourceBuf, NumDestChannels, PixelCount, NumBatchElems](int32 BatchId)
						{
							const int32 BatchBegin = BatchId * NumBatchElems;
							const int32 BatchEnd = FMath::Min(BatchBegin + NumBatchElems, PixelCount);
							for (int32 p = BatchBegin; p < BatchEnd; ++p)
							{
								pDestBuf[p * NumDestChannels] = pSourceBuf[p * 4];
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
				auto ProcessBatch = [pDestBuf, NumDestChannels, PixelCount, NumBatchElems](int32 BatchId)
				{
					const int32 BatchBegin = BatchId * NumBatchElems;
					const int32 BatchEnd = FMath::Min(BatchBegin + NumBatchElems, PixelCount);
					for (int32 p = BatchBegin; p < BatchEnd; ++p)
					{
						pDestBuf[p * NumDestChannels] = 0;
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
	}


	Ptr<Image> FImageOperator::ImageSwizzle( EImageFormat Format, const Ptr<const Image> Sources[], const uint8 Channels[] )
	{
		MUTABLE_CPUPROFILER_SCOPE(ImageSwizzle);

		if (!Sources[0])
		{
			return nullptr;
		}

		Ptr<Image> Dest = CreateImage(Sources[0]->GetSizeX(), Sources[0]->GetSizeY(), Sources[0]->GetLODCount(), Format, EInitializationType::Black);

		mu::ImageSwizzle(Dest.get(), Sources, Channels);

		return Dest;
	}


}

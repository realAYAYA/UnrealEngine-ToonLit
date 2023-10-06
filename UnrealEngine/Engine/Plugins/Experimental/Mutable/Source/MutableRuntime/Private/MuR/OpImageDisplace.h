// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/ModelPrivate.h"
#include "MuR/Platform.h"
#include "MuR/ConvertData.h"
#include "Async/ParallelFor.h"


namespace mu
{

    inline uint8 MutableEncodeOffset( int x, int y )
	{
        uint8 c = uint8( ((7)<<4) | 7 );
		if ( x<8 && x>-8 && y<8 && y>-8 )
		{
            c = uint8( ((x+7)<<4) | (y+7) );
		}
		return c;
	}

    inline void MutableDecodeOffset( uint8 c, int& x, int& y )
	{
		x = int(c>>4) - 7;
		y = int(c&0xf) - 7;
	}

	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	inline void ImageMakeGrowMap( Image* pResult, const Image* pMask, int InBorder )
	{
		MUTABLE_CPUPROFILER_SCOPE(ImageMakeGrowMap);

		check(pMask->GetFormat() == EImageFormat::IF_L_UBYTE);
		check(pResult->GetFormat() == EImageFormat::IF_L_UBYTE);
		check(pResult->GetSizeX() == pMask->GetSizeX());
		check(pResult->GetSizeY() == pMask->GetSizeY());
		check(pResult->GetLODCount() == pMask->GetLODCount());

		int32 MipCount = pResult->GetLODCount();
		int32 SizeX = pResult->GetSizeX();
		int32 SizeY = pResult->GetSizeY();
		
		if (SizeX <= 0 || SizeY <= 0)
		{
			return;
		}

		FMemory::Memset( pResult->GetData(), MutableEncodeOffset( 0, 0 ), pResult->CalculatePixelCount() );

		Ptr<const Image> pThisMask = pMask;
		for ( int b=0; b<InBorder; ++b )
		{
			Ptr<Image> pNextMask = pThisMask->Clone();

			for( int CurrentMip=0; CurrentMip<MipCount; ++CurrentMip)
			{ 
				FIntVector2 MipSize = pResult->CalculateMipSize(CurrentMip);

				uint8* ResultData = pResult->GetMipData(CurrentMip);
				const uint8* ThisMaskData = pThisMask->GetMipData(CurrentMip);
				uint8* NextMaskData = pNextMask->GetMipData(CurrentMip);

				//for ( int y=0; y< MipSize.Y; ++y )
				const auto ProcessRow = [
					ThisMaskData, NextMaskData, MipSize, ResultData
				] (int32 y)
				{
					uint8* pNextData = NextMaskData + MipSize.X * y;

					const uint8* pS0 = y > 0 ? ThisMaskData + (y - 1) * MipSize.X : nullptr;
					const uint8* pS1 = ThisMaskData + y * MipSize.X;
					bool bNotLastRow = y < MipSize.Y - 1;
					const uint8* pS2 = bNotLastRow ? ThisMaskData + (y + 1) * MipSize.X : nullptr;

					const uint8* pR0 = y > 0 ? ResultData + (y - 1) * MipSize.X : 0;
					uint8* pR1 = ResultData + y * MipSize.X;

					const uint8* pR2 = bNotLastRow ? ResultData + (y + 1) * MipSize.X : nullptr;

					for (int x = 0; x < MipSize.X; ++x)
					{
						int dx, dy;
						if (pS1[x])
						{
							*pNextData = 255;
						}
						else if (y > 0 && pS0[x])
						{
							MutableDecodeOffset(pR0[x], dx, dy);
							pR1[x] = MutableEncodeOffset(dx, dy - 1);
							*pNextData = 255;
						}
						else if (bNotLastRow && pS2 && pR2 && pS2[x])
						{
							MutableDecodeOffset(pR2[x], dx, dy);
							pR1[x] = MutableEncodeOffset(dx, dy + 1);
							*pNextData = 255;
						}
						else if (x < MipSize.X - 1 && pS1[x + 1])
						{
							MutableDecodeOffset(pR1[x + 1], dx, dy);
							pR1[x] = MutableEncodeOffset(dx + 1, dy);
							*pNextData = 255;
						}
						else if (x > 0 && pS1[x - 1])
						{
							MutableDecodeOffset(pR1[x - 1], dx, dy);
							pR1[x] = MutableEncodeOffset(dx - 1, dy);
							*pNextData = 255;
						}

						++pNextData;
					}

				};

				ParallelFor(MipSize.Y, ProcessRow);
			}

			pThisMask = pNextMask;
		}

	}


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	inline void ImageDisplace( Image* pResult, const Image* pSource, const Image* pMap )
	{
		MUTABLE_CPUPROFILER_SCOPE(ImageDisplace)

		check( pResult->GetFormat() == pSource->GetFormat()  );
		check( pMap->GetFormat() == EImageFormat::IF_L_UBYTE
						||
						pMap->GetFormat() == EImageFormat::IF_L_UBYTE_RLE );
		check( pResult->GetSizeX() == pSource->GetSizeX() );
		check( pResult->GetSizeY() == pSource->GetSizeY() );
		check( pResult->GetSizeX() == pMap->GetSizeX() );
		check( pResult->GetSizeY() == pMap->GetSizeY() );

		int32 LODCount = pResult->GetLODCount();
		check(LODCount <= pMap->GetLODCount());
		check(LODCount <= pSource->GetLODCount());

		int32 sizeX = pResult->GetSizeX();
		int32 sizeY = pResult->GetSizeY();

        //if (sizeX<4 || sizeY<4)
        //{
        //    return;
        //}

        const uint8* pSourceData = pSource->GetData();

        auto mapFormat = pMap->GetFormat();
        bool isUncompressed = ( mapFormat == EImageFormat::IF_L_UBYTE );

        if ( isUncompressed )
		{
			switch (pResult->GetFormat())
			{

			case EImageFormat::IF_L_UBYTE:
			{
				//for ( int y=0; y<sizeY; ++y )
				const auto ProcessRow = [
					pSourceData, pMap, pResult, sizeX, sizeY
				] (int32 y)
				{
					const uint8* pMapData = pMap->GetData() + y * sizeX;
					uint8* pResultData = pResult->GetData() + y * sizeX;

					for (int x = 0; x < sizeX; ++x)
					{
						int dx, dy;
						MutableDecodeOffset(*pMapData, dx, dy);

						// This could actually happen since we enable the crop+displace optimization
						//check(x + dx >= 0 && x + dx < sizeX);
						//check(y + dy >= 0 && y + dy < sizeY);
						if ((x + dx >= 0 && x + dx < sizeX) && (y + dy >= 0 && y + dy < sizeY))
						{
							int offset = ((y + dy) * sizeX + (x + dx));
							FMemory::Memcpy(pResultData, &pSourceData[offset], 1);
						}

						++pResultData;
						++pMapData;
					}

				};

				ParallelFor(sizeY, ProcessRow);
				break;
			}

			case EImageFormat::IF_RGB_UBYTE:
			{
				//for (int y = 0; y < sizeY; ++y)
				const auto ProcessRow = [
					pSourceData, pMap, pResult, sizeX, sizeY
				] (int32 y)
				{
					const uint8* pMapData = pMap->GetData() + y * sizeX;
					uint8* pResultData = pResult->GetData() + y * sizeX*3;

					for (int x = 0; x < sizeX; ++x)
					{
						int dx, dy;
						MutableDecodeOffset(*pMapData, dx, dy);
						// This could actually happen since we enable the crop+displace optimization
						//check(x + dx >= 0 && x + dx < sizeX);
						//check(y + dy >= 0 && y + dy < sizeY);
						if ((x + dx >= 0 && x + dx < sizeX) && (y + dy >= 0 && y + dy < sizeY))
						{
							int offset = ((y + dy) * sizeX + (x + dx)) * 3;
							FMemory::Memcpy(pResultData, &pSourceData[offset], 3);
						}

						pResultData += 3;
						++pMapData;
					}

				};

				ParallelFor(sizeY, ProcessRow);
				break;
			}

			default:
			{
				// Generic
				int pixelSize = GetImageFormatData( pResult->GetFormat() ).BytesPerBlock;
				//for ( int y=0; y<sizeY; ++y )
				const auto ProcessRow = [
					pSourceData, pMap, pResult, sizeX, sizeY, pixelSize
				] (int32 y)
				{
					const uint8* pMapData = pMap->GetData() + y * sizeX;
					uint8* pResultData = pResult->GetData() + y * sizeX * pixelSize;

					for (int x = 0; x < sizeX; ++x)
					{
						int dx, dy;
						MutableDecodeOffset(*pMapData, dx, dy);
						// This could actually happen since we enable the crop+displace optimization
						//check(x + dx >= 0 && x + dx < sizeX);
						//check(y + dy >= 0 && y + dy < sizeY);
						if ((x + dx >= 0 && x + dx < sizeX) && (y + dy >= 0 && y + dy < sizeY))
						{
							int offset = ((y + dy) * sizeX + (x + dx)) * pixelSize;
							FMemory::Memcpy(pResultData, &pSourceData[offset], pixelSize);
						}

						pResultData += pixelSize;
						++pMapData;
					}

				};

				ParallelFor(sizeY, ProcessRow);
			}

			}
		}

        else if (mapFormat==EImageFormat::IF_L_UBYTE_RLE )
		{
            int pixelSize = GetImageFormatData( pResult->GetFormat() ).BytesPerBlock;

			const uint8* pMapData = pMap->GetData();
			uint8* pResultData = pResult->GetData();

			// TODO: ParallelFor this

            for (int lod=0;lod<LODCount; ++lod)
            {
                pMapData += 4+sizeY*sizeof(uint32);

                for ( int y=0; y<sizeY; ++y )
                {
                    int x=0;
                    while ( x<sizeX )
                    {
                        // Decode header
                        uint16 equal = *(const uint16*)pMapData;
                        pMapData += 2;

                        uint8 different = *pMapData;
                        ++pMapData;

                        uint8 equalPixel = *pMapData;
                        ++pMapData;

                        // Equal pixels
                        {
                            int dx,dy;
                            MutableDecodeOffset( equalPixel, dx, dy );
                            dx += x;
                            dy += y;

							if (dx >= 0 && dx < sizeX && dy >= 0 && dy < sizeY)
							{
								int offset = (dy * sizeX + dx) * pixelSize;
								FMemory::Memcpy(pResultData, &pSourceData[offset], equal * pixelSize);
							}

                            pResultData += equal*pixelSize;
                            x += equal;
                        }


                        // Different pixels
                        for ( int i=0; i<different; ++i )
                        {
                            int dx,dy;
                            MutableDecodeOffset( pMapData[i], dx, dy );
                            dx += x;
                            dy += y;

							if (dx>=0 && dx<sizeX && dy>=0 && dy<sizeY)
							{
								int offset = (dy * sizeX + dx) * pixelSize;
								FMemory::Memcpy(pResultData, &pSourceData[offset], pixelSize);
							}

                            pResultData += pixelSize;
                            ++x;
                        }

                        pMapData += different;
                    }
                }

                sizeX = FMath::DivideAndRoundUp(sizeX,2);
                sizeY = FMath::DivideAndRoundUp(sizeY,2);
            }
		}

		else
		{
			checkf( false, TEXT("Unsupported mask format.") );
		}


		// Update the relevancy data of the image for the worst case. 
		if (pResult->m_flags & Image::EImageFlags::IF_HAS_RELEVANCY_MAP)
		{
			// Displace can encode at max MUTABLE_GROW_BORDER_VALUE pixels away according to "border" in MakeGrowMap.
			pResult->RelevancyMinY = FMath::Max(int32(pResult->RelevancyMinY) - MUTABLE_GROW_BORDER_VALUE, 0);
			pResult->RelevancyMaxY = FMath::Min(pResult->RelevancyMaxY + MUTABLE_GROW_BORDER_VALUE, pResult->GetSizeY() - 1);
		}

	}


}



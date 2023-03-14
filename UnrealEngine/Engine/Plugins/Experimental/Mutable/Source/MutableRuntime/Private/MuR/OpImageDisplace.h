// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/Platform.h"
#include "MuR/ConvertData.h"
#include "Async/ParallelFor.h"


namespace mu
{

    inline uint8_t MutableEncodeOffset( int x, int y )
	{
        uint8_t c = uint8_t( ((7)<<4) | 7 );
		if ( x<8 && x>-8 && y<8 && y>-8 )
		{
            c = uint8_t( ((x+7)<<4) | (y+7) );
		}
		return c;
	}

    inline void MutableDecodeOffset( uint8_t c, int& x, int& y )
	{
		x = int(c>>4) - 7;
		y = int(c&0xf) - 7;
	}

	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	inline void ImageMakeGrowMap( Image* pResult, const Image* pMask, int border )
	{
		MUTABLE_CPUPROFILER_SCOPE(ImageMakeGrowMap)

		check( pMask->GetFormat() == EImageFormat::IF_L_UBYTE );
		check( pResult->GetFormat() == EImageFormat::IF_L_UBYTE );
		check( pResult->GetSizeX() == pMask->GetSizeX() );
		check( pResult->GetSizeY() == pMask->GetSizeY() );

		int sizeX = pResult->GetSizeX();
		int sizeY = pResult->GetSizeY();
		check( sizeX>0 && sizeY>0 );

        if (sizeX<4 || sizeY<4)
        {
            return;
        }

		memset( pResult->GetData(), MutableEncodeOffset( 0, 0 ), sizeX*sizeY );

		ImagePtrConst pThisMask = pMask;
		for ( int b=0; b<border; ++b )
		{
			ImagePtr pNextMask = pThisMask->Clone();            

			//for ( int y=0; y<sizeY; ++y )
			const auto& ProcessRow = [
				pThisMask, pNextMask, sizeX, sizeY, pResult
			] (int32 y)
			{
				uint8_t* pNextData = pNextMask->GetData() + sizeX*y;

				const uint8_t* pS0 = y > 0 ? pThisMask->GetData() + (y - 1) * sizeX : nullptr;
				const uint8_t* pS1 = pThisMask->GetData() + y * sizeX;
				bool bNotLastRow = y < sizeY - 1;
				const uint8_t* pS2 = bNotLastRow ? pThisMask->GetData() + (y + 1) * sizeX : nullptr;

				const uint8_t* pR0 = y > 0 ? pResult->GetData() + (y - 1) * sizeX : 0;
				uint8_t* pR1 = pResult->GetData() + y * sizeX;

				const uint8_t* pR2 = bNotLastRow ? pResult->GetData() + (y + 1) * sizeX : nullptr;

				for (int x = 0; x < sizeX; ++x)
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
					else if (x < sizeX - 1 && pS1[x + 1])
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

			ParallelFor(sizeY, ProcessRow);

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

		int sizeX = pResult->GetSizeX();
		int sizeY = pResult->GetSizeY();

        if (sizeX<4 || sizeY<4)
        {
            return;
        }

        const uint8_t* pSourceData = pSource->GetData();

        auto mapFormat = pMap->GetFormat();
        bool isUncompressed = ( mapFormat == EImageFormat::IF_L_UBYTE );

        if ( isUncompressed )
		{
			switch (pResult->GetFormat())
			{

			case EImageFormat::IF_L_UBYTE:
			{
				//for ( int y=0; y<sizeY; ++y )
				const auto& ProcessRow = [
					pSourceData, pMap, pResult, sizeX, sizeY
				] (int32 y)
				{
					const uint8_t* pMapData = pMap->GetData() + y * sizeX;
					uint8_t* pResultData = pResult->GetData() + y * sizeX;

					for (int x = 0; x < sizeX; ++x)
					{
						int dx, dy;
						MutableDecodeOffset(*pMapData, dx, dy);
						check(x + dx >= 0 && x + dx < sizeX);
						check(y + dy >= 0 && y + dy < sizeY);

						int offset = ((y + dy) * sizeX + (x + dx));
						memcpy(pResultData, &pSourceData[offset], 1);

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
				const auto& ProcessRow = [
					pSourceData, pMap, pResult, sizeX, sizeY
				] (int32 y)
				{
					const uint8_t* pMapData = pMap->GetData() + y * sizeX;
					uint8_t* pResultData = pResult->GetData() + y * sizeX*3;

					for (int x = 0; x < sizeX; ++x)
					{
						int dx, dy;
						MutableDecodeOffset(*pMapData, dx, dy);
						check(x + dx >= 0 && x + dx < sizeX);
						check(y + dy >= 0 && y + dy < sizeY);

						int offset = ((y + dy) * sizeX + (x + dx)) * 3;
						memcpy(pResultData, &pSourceData[offset], 3);

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
				int pixelSize = GetImageFormatData( pResult->GetFormat() ).m_bytesPerBlock;
				//for ( int y=0; y<sizeY; ++y )
				const auto& ProcessRow = [
					pSourceData, pMap, pResult, sizeX, sizeY, pixelSize
				] (int32 y)
				{
					const uint8_t* pMapData = pMap->GetData() + y * sizeX;
					uint8_t* pResultData = pResult->GetData() + y * sizeX * pixelSize;

					for (int x = 0; x < sizeX; ++x)
					{
						int dx, dy;
						MutableDecodeOffset(*pMapData, dx, dy);
						check(x + dx >= 0 && x + dx < sizeX);
						check(y + dy >= 0 && y + dy < sizeY);

						int offset = ((y + dy) * sizeX + (x + dx)) * pixelSize;
						memcpy(pResultData, &pSourceData[offset], pixelSize);

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
            int pixelSize = GetImageFormatData( pResult->GetFormat() ).m_bytesPerBlock;

			const uint8_t* pMapData = pMap->GetData();
			uint8_t* pResultData = pResult->GetData();

			// TODO: ParallelFor this

            for (int lod=0;lod<pMap->GetLODCount(); ++lod)
            {
                pMapData += 4+sizeY*sizeof(uint32_t);

                for ( int y=0; y<sizeY; ++y )
                {
                    int x=0;
                    while ( x<sizeX )
                    {
                        // Decode header
                        uint16 equal = *(const uint16*)pMapData;
                        pMapData += 2;

                        uint8_t different = *pMapData;
                        ++pMapData;

                        uint8_t equalPixel = *pMapData;
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
								memcpy(pResultData, &pSourceData[offset], equal * pixelSize);
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
								memcpy(pResultData, &pSourceData[offset], pixelSize);
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


	}


}

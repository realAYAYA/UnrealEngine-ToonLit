// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/Platform.h"


namespace mu
{

	inline ImagePtr ImageDifference( const Image* pA, const Image* pB )
	{
		check( pA->GetSizeX() == pB->GetSizeX() );
		check( pA->GetSizeY() == pB->GetSizeY() );
		check( pA->GetFormat() == pB->GetFormat() );

        ImagePtr pDest = new Image( pA->GetSizeX(), pA->GetSizeY(), pA->GetLODCount(), EImageFormat::IF_L_UBYTE );

        uint8_t* pDestBuf = pDest->GetData();
        const uint8_t* pABuf = pA->GetData();
        const uint8_t* pBBuf = pB->GetData();

		// Generic implementation
		int pixelCount = (int)pA->CalculatePixelCount();

		switch ( pA->GetFormat() )
		{
		case EImageFormat::IF_RGB_UBYTE:
			for ( int i=0; i<pixelCount; ++i )
			{
                uint8_t diff = 0;
				for ( int c=0; c<3; ++c )
				{
                    uint8_t a = pABuf[3*i+c];
                    uint8_t b = pBBuf[3*i+c];
					if ( abs(a-b)>3 )
					{
                        diff = FMath::Max( diff, (uint8_t)255 );
					}
				}
				pDestBuf[i] = diff;
			}
			break;

        case EImageFormat::IF_BGRA_UBYTE:
        case EImageFormat::IF_RGBA_UBYTE:
            for ( int i=0; i<pixelCount; ++i )
			{
                uint8_t diff = 0;
				for ( int c=0; c<3; ++c )
				{
                    uint8_t a = pABuf[4*i+c];
                    uint8_t b = pBBuf[4*i+c];
					if ( abs(a-b)>3 )
					{
                        diff = FMath::Max( diff, (uint8_t)255 );
					}
				}
				pDestBuf[i] = diff;
			}
			break;

		default:
			check(false);
		}

		return pDest;
	}

}

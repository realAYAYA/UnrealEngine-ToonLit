// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/Platform.h"


namespace mu
{

	inline ImagePtr ImageSaturate( const Image* pA, float factor )
	{
		// Clamp the factor
		// TODO: See what happens if we don't
		factor = FMath::Max( 0.0f, factor );
        int32_t f_8 = (int32_t)(factor*255);

        ImagePtr pDest = new Image( pA->GetSizeX(), pA->GetSizeY(),
                                    pA->GetLODCount(),
                                    pA->GetFormat() );

        uint8_t* pDestBuf = pDest->GetData();
        const uint8_t* pABuf = pA->GetData();

		// Generic implementation
		int pixelCount = (int)pA->CalculatePixelCount();

		switch ( pA->GetFormat() )
		{
		case EImageFormat::IF_RGB_UBYTE:
		{
			for ( int i=0; i<pixelCount; ++i )
			{
                int32_t l_16 = 76 * pABuf[3*i+0] + 150 * pABuf[3*i+1] + 29 * pABuf[3*i+2];

				for ( int c=0; c<3; ++c )
				{
                    int32_t d_16 = (pABuf[3*i+c]<<8) - l_16;
                    int32_t r_16 = l_16 + ( (d_16 * f_8) >> 8 );
                    pDestBuf[3*i+c] = (uint8_t)FMath::Min( (uint32_t)r_16>>8, 255u );
				}
			}
			break;
		}

        case EImageFormat::IF_RGBA_UBYTE:
        {
            for ( int i=0; i<pixelCount; ++i )
            {
                int32_t l_16 = 76 * pABuf[4*i+0] + 150 * pABuf[4*i+1] + 29 * pABuf[4*i+2];

                for ( int c=0; c<3; ++c )
                {
                    int32_t d_16 = (pABuf[4*i+c]<<8) - l_16;
                    int32_t r_16 = l_16 + ( (d_16 * f_8) >> 8 );
                    pDestBuf[4*i+c] = (uint8_t)FMath::Min( (uint32_t)r_16>>8, 255u );
                }

                pDestBuf[4*i+3] = pABuf[4*i+3];
            }
            break;
        }

        case EImageFormat::IF_BGRA_UBYTE:
        {
            for ( int i=0; i<pixelCount; ++i )
            {
                int32_t l_16 = 76 * pABuf[4*i+2] + 150 * pABuf[4*i+1] + 29 * pABuf[4*i+0];

                for ( int c=0; c<3; ++c )
                {
                    int32_t d_16 = (pABuf[4*i+c]<<8) - l_16;
                    int32_t r_16 = l_16 + ( (d_16 * f_8) >> 8 );
                    pDestBuf[4*i+c] = (uint8_t)FMath::Min( (uint32_t)r_16>>8, 255u );
                }

                pDestBuf[4*i+3] = pABuf[4*i+3];
            }
            break;
        }

		default:
			check(false);
		}

		return pDest;
	}

}

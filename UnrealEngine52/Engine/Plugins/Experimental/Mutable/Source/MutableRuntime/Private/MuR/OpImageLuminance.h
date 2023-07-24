// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/Platform.h"

namespace mu
{

	inline ImagePtr ImageLuminance( const Image* pA )
	{
        ImagePtr pDest = new Image( pA->GetSizeX(), pA->GetSizeY(), pA->GetLODCount(), EImageFormat::IF_L_UBYTE );

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
                uint32_t l_16 = 76 * pABuf[3*i+0] + 150 * pABuf[3*i+1] + 29 * pABuf[3*i+2];
                pDestBuf[i] = (uint8_t)FMath::Min( l_16>>8, 255u );
			}
			break;
		}

        case EImageFormat::IF_RGBA_UBYTE:
        {
            for ( int i=0; i<pixelCount; ++i )
            {
                uint32_t l_16 = 76 * pABuf[4*i+0] + 150 * pABuf[4*i+1] + 29 * pABuf[4*i+2];
                pDestBuf[i] = (uint8_t)FMath::Min( l_16>>8, 255u );
            }
            break;
        }

        case EImageFormat::IF_BGRA_UBYTE:
        {
            for ( int i=0; i<pixelCount; ++i )
            {
                uint32_t l_16 = 76 * pABuf[4*i+2] + 150 * pABuf[4*i+1] + 29 * pABuf[4*i+0];
                pDestBuf[i] = (uint8_t)FMath::Min( l_16>>8, 255u );
            }
            break;
        }

		default:
			check(false);
			break;
		}

		return pDest;
	}

}

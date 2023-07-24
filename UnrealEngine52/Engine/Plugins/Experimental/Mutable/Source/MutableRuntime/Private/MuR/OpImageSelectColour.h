// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/Platform.h"


namespace mu
{

	inline ImagePtr ImageSelectColour( const Image* pA, const vec3<float>& colour )
	{
		ImagePtr pDest = new Image( pA->GetSizeX(), pA->GetSizeY(), pA->GetLODCount(), EImageFormat::IF_L_UBYTE );

        uint8_t* pDestBuf = pDest->GetData();
        const uint8_t* pABuf = pA->GetData();

		// Generic implementation
		int pixelCount = pA->GetSizeX() * pA->GetSizeY();

		switch ( pA->GetFormat() )
		{
		case EImageFormat::IF_L_UBYTE:
		{
            uint8_t c;
            c = (uint8_t)(FMath::Max( 0.0f, FMath::Min( 255.0f, colour[0] * 255.0f ) )+0.5f);

			for ( int i=0; i<pixelCount; ++i )
			{
				if ( pABuf[i]==c )
				{
					pDestBuf[i] = 255;
				}
				else
				{
					pDestBuf[i] = 0;
				}
			}
			break;
		}

		case EImageFormat::IF_RGB_UBYTE:
		{
            uint8_t c[3];
            c[0] = (uint8_t)(FMath::Max( 0.0f, FMath::Min( 255.0f, colour[0] * 255.0f ) )+0.5f);
            c[1] = (uint8_t)(FMath::Max( 0.0f, FMath::Min( 255.0f, colour[1] * 255.0f ) )+0.5f);
            c[2] = (uint8_t)(FMath::Max( 0.0f, FMath::Min( 255.0f, colour[2] * 255.0f ) )+0.5f);

			for ( int i=0; i<pixelCount; ++i )
			{
				if ( pABuf[3*i+0]==c[0] && pABuf[3*i+1]==c[1] && pABuf[3*i+2]==c[2] )
				{
					pDestBuf[i] = 255;
				}
				else
				{
					pDestBuf[i] = 0;
				}
			}
			break;
		}

        case EImageFormat::IF_RGBA_UBYTE:
        {
            uint8_t c[3];
            c[0] = (uint8_t)(FMath::Max( 0.0f, FMath::Min( 255.0f, colour[0] * 255.0f ) )+0.5f);
            c[1] = (uint8_t)(FMath::Max( 0.0f, FMath::Min( 255.0f, colour[1] * 255.0f ) )+0.5f);
            c[2] = (uint8_t)(FMath::Max( 0.0f, FMath::Min( 255.0f, colour[2] * 255.0f ) )+0.5f);

            for ( int i=0; i<pixelCount; ++i )
            {
                if ( pABuf[4*i+0]==c[0] && pABuf[4*i+1]==c[1] && pABuf[4*i+2]==c[2] )
                {
                    pDestBuf[i] = 255;
                }
                else
                {
                    pDestBuf[i] = 0;
                }
            }
            break;
        }

        case EImageFormat::IF_BGRA_UBYTE:
        {
            uint8_t c[3];
            c[0] = (uint8_t)(FMath::Max( 0.0f, FMath::Min( 255.0f, colour[2] * 255.0f ) )+0.5f);
            c[1] = (uint8_t)(FMath::Max( 0.0f, FMath::Min( 255.0f, colour[1] * 255.0f ) )+0.5f);
            c[2] = (uint8_t)(FMath::Max( 0.0f, FMath::Min( 255.0f, colour[0] * 255.0f ) )+0.5f);

            for ( int i=0; i<pixelCount; ++i )
            {
                if ( pABuf[4*i+0]==c[0] && pABuf[4*i+1]==c[1] && pABuf[4*i+2]==c[2] )
                {
                    pDestBuf[i] = 255;
                }
                else
                {
                    pDestBuf[i] = 0;
                }
            }
            break;
        }

		default:
			check(false);
		}

		return pDest;
	}

}

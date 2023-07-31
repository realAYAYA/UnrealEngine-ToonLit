// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/Platform.h"

namespace mu
{

	inline ImagePtr ImageColourMap( const Image* pSource, const Image* pMask, const Image* pMap )
	{
		check( pSource->GetSizeX() == pMask->GetSizeX() );
		check( pSource->GetSizeY() == pMask->GetSizeY() );
        check( pMask->GetFormat() == EImageFormat::IF_L_UBYTE );

		ImagePtr pDest = new Image( pSource->GetSizeX(),
									pSource->GetSizeY(),
                                    pSource->GetLODCount(),
									pSource->GetFormat() );

        uint8_t* pDestBuf = pDest->GetData();
        const uint8_t* pSourceBuf = pSource->GetData();
        const uint8_t* pMaskBuf = pMask->GetData();

		// Generic implementation
		int pixelCount = (int)pSource->CalculatePixelCount();

		// Make a palette for faster conversion
        uint8_t palette[256][4];
		for ( int i=0; i<256; ++i )
		{
			vec4<float> c = pMap->Sample( vec2<float>( float(i)/256.0f, 0.0f ) );
            palette[i][0] = (uint8_t)FMath::Max( 0, FMath::Min( 255, int(c[0]*255.0f) ) );
            palette[i][1] = (uint8_t)FMath::Max( 0, FMath::Min( 255, int(c[1]*255.0f) ) );
            palette[i][2] = (uint8_t)FMath::Max( 0, FMath::Min( 255, int(c[2]*255.0f) ) );
            palette[i][3] = (uint8_t)FMath::Max( 0, FMath::Min( 255, int(c[3]*255.0f) ) );
		}

		switch ( pSource->GetFormat() )
		{
		case EImageFormat::IF_L_UBYTE:
		{
			for ( int i=0; i<pixelCount; ++i )
			{
                uint8_t m_8 = pMaskBuf[i];
				if (m_8>127)
				{
					pDestBuf[i] = palette[ pSourceBuf[i] ][0];
				}
				else
				{
					pDestBuf[i] = pSourceBuf[i];
				}
			}
			break;
		}

		case EImageFormat::IF_RGB_UBYTE:
		{
			for ( int i=0; i<pixelCount; ++i )
			{
                uint8_t m_8 = pMaskBuf[i];
				if (m_8>127)
				{
					pDestBuf[3*i+0] = palette[ pSourceBuf[3*i+0] ][0];
					pDestBuf[3*i+1] = palette[ pSourceBuf[3*i+1] ][1];
					pDestBuf[3*i+2] = palette[ pSourceBuf[3*i+2] ][2];
				}
				else
				{
					pDestBuf[3*i+0] = pSourceBuf[3*i+0];
					pDestBuf[3*i+1] = pSourceBuf[3*i+1];
					pDestBuf[3*i+2] = pSourceBuf[3*i+2];
				}
			}
			break;
		}

        case EImageFormat::IF_RGBA_UBYTE:
        {
            for ( int i=0; i<pixelCount; ++i )
            {
                uint8_t m_8 = pMaskBuf[i];
                if (m_8>127)
                {
                    pDestBuf[4*i+0] = palette[ pSourceBuf[4*i+0] ][0];
                    pDestBuf[4*i+1] = palette[ pSourceBuf[4*i+1] ][1];
                    pDestBuf[4*i+2] = palette[ pSourceBuf[4*i+2] ][2];
                    pDestBuf[4*i+3] = palette[ pSourceBuf[4*i+3] ][3];
                }
                else
                {
                    pDestBuf[4*i+0] = pSourceBuf[4*i+0];
                    pDestBuf[4*i+1] = pSourceBuf[4*i+1];
                    pDestBuf[4*i+2] = pSourceBuf[4*i+2];
                    pDestBuf[4*i+3] = pSourceBuf[4*i+3];
                }
            }
            break;
        }

        case EImageFormat::IF_BGRA_UBYTE:
        {
            for ( int i=0; i<pixelCount; ++i )
            {
                uint8_t m_8 = pMaskBuf[i];
                if (m_8>127)
                {
                    pDestBuf[4*i+0] = palette[ pSourceBuf[4*i+0] ][2];
                    pDestBuf[4*i+1] = palette[ pSourceBuf[4*i+1] ][1];
                    pDestBuf[4*i+2] = palette[ pSourceBuf[4*i+2] ][0];
                    pDestBuf[4*i+3] = palette[ pSourceBuf[4*i+3] ][3];
                }
                else
                {
                    pDestBuf[4*i+0] = pSourceBuf[4*i+0];
                    pDestBuf[4*i+1] = pSourceBuf[4*i+1];
                    pDestBuf[4*i+2] = pSourceBuf[4*i+2];
                    pDestBuf[4*i+3] = pSourceBuf[4*i+3];
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

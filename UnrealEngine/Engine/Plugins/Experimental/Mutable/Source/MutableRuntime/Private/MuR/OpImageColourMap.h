// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/Platform.h"

namespace mu
{

	inline void ImageColourMap( Image* pDest, const Image* pSource, const Image* pMask, const Image* pMap, bool bOnlyOneMip )
	{
		check( pSource->GetSizeX() == pMask->GetSizeX() );
		check( pSource->GetSizeY() == pMask->GetSizeY() );
		check( pSource->GetLODCount() == pMask->GetLODCount() || bOnlyOneMip );
		check( pMask->GetFormat() == EImageFormat::IF_L_UBYTE );

        uint8* pDestBuf = pDest->GetData();
        const uint8* pSourceBuf = pSource->GetData();
        const uint8* pMaskBuf = pMask->GetData();

		// Generic implementation
		int32 PixelCount = 0;
		if (bOnlyOneMip)
		{
			PixelCount = pSource->CalculatePixelCount(0);
		}
		else
		{
			PixelCount = pSource->CalculatePixelCount();
		}

		// Make a palette for faster conversion
        uint8 palette[256][4];
		for ( int i=0; i<256; ++i )
		{
			FVector4f c = pMap->Sample(FVector2f( float(i)/256.0f, 0.0f ) );
            palette[i][0] = (uint8)FMath::Max( 0, FMath::Min( 255, int(c[0]*255.0f) ) );
            palette[i][1] = (uint8)FMath::Max( 0, FMath::Min( 255, int(c[1]*255.0f) ) );
            palette[i][2] = (uint8)FMath::Max( 0, FMath::Min( 255, int(c[2]*255.0f) ) );
            palette[i][3] = (uint8)FMath::Max( 0, FMath::Min( 255, int(c[3]*255.0f) ) );
		}

		switch ( pSource->GetFormat() )
		{
		case EImageFormat::IF_L_UBYTE:
		{
			for ( int i=0; i< PixelCount; ++i )
			{
                uint8 m_8 = pMaskBuf[i];
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
			for ( int i=0; i< PixelCount; ++i )
			{
                uint8 m_8 = pMaskBuf[i];
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
            for ( int i=0; i< PixelCount; ++i )
            {
                uint8 m_8 = pMaskBuf[i];
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
            for ( int i=0; i< PixelCount; ++i )
            {
                uint8 m_8 = pMaskBuf[i];
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
	}

}

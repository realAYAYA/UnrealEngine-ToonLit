// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/Platform.h"



namespace mu
{

	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	inline ImagePtr ImageBinarise( const Image* pA, float threshold )
	{
        ImagePtr pDest = new Image( pA->GetSizeX(), pA->GetSizeY(), pA->GetLODCount(), EImageFormat::IF_L_UBYTE );

        uint8_t* pDestBuf = pDest->GetData();
        const uint8_t* pABuf = pA->GetData();

		// Generic implementation
		int pixelCount = (int)pA->CalculatePixelCount();

        uint32_t t_8 = uint32_t( threshold * 255.0f );

		switch ( pA->GetFormat() )
		{
		case EImageFormat::IF_L_UBYTE:
		{
			for ( int i=0; i<pixelCount; ++i )
			{
                uint32_t a_8 = pABuf[i];
				pDestBuf[i] = (a_8>=t_8) ? 255 : 0;
			}
			break;
		}

		case EImageFormat::IF_RGB_UBYTE:
		{
			for ( int i=0; i<pixelCount; ++i )
			{
                uint32_t a_8 = pABuf[3*i+0];
				a_8 += pABuf[3*i+1];
				a_8 += pABuf[3*i+2];
				a_8 /= 3;
				pDestBuf[i] = (a_8>=t_8) ? 255 : 0;
			}
			break;
		}

        case EImageFormat::IF_RGBA_UBYTE:
        case EImageFormat::IF_BGRA_UBYTE:
        {
			for ( int i=0; i<pixelCount; ++i )
			{
                uint32_t a_8 = pABuf[4*i+0];
				a_8 += pABuf[4*i+1];
				a_8 += pABuf[4*i+2];
				a_8 /= 3;
				pDestBuf[i] = (a_8>=t_8) ? 255 : 0;
			}
			break;
		}

		default:
			check(false);
		}

		return pDest;
	}

}

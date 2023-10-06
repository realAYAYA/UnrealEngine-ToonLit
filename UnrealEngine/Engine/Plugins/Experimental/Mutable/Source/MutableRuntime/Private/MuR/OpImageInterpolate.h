// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/Platform.h"

namespace mu
{

	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
    inline void ImageInterpolate( Image* pDest,
                                  const Image* pB,
                                  float factor )
	{
		MUTABLE_CPUPROFILER_SCOPE(ImageInterpolate)

		check(pDest && pB);
        check(pDest->GetSizeX() == pB->GetSizeX() );
        check(pDest->GetSizeY() == pB->GetSizeY() );
        check(pDest->GetFormat() == pB->GetFormat() );

		// Clamp the factor
		factor = FMath::Max( 0.0f, FMath::Min( 1.0f, factor ) );

        uint8* pDestBuf = pDest->GetData();
        const uint8* pBBuf = pB->GetData();

		// Generic implementation
		int32 PixelCount = (int32)pDest->CalculatePixelCount();

		switch ( pDest->GetFormat() )
		{
		case EImageFormat::IF_L_UBYTE:
		{
            uint32 w_8 = (uint32)(factor*255);
			for ( int i=0; i< PixelCount; ++i )
			{
                uint32 a_8 = *pDestBuf;
                uint32 b_8 = *pBBuf++;
                uint32 i_16 = a_8 * (255-w_8) + b_8 * w_8;
                *pDestBuf++ = (uint8) ( i_16>>8 );
			}
			break;
		}

		case EImageFormat::IF_RGB_UBYTE:
		{
            uint32 w_8 = (uint32)(factor*255);
			for ( int i=0; i< PixelCount *3; ++i )
			{
                uint32 a_8 = *pDestBuf;
                uint32 b_8 = *pBBuf++;
                uint32 i_16 = a_8 * (255-w_8) + b_8 * w_8;
                *pDestBuf++ = (uint8) ( i_16>>8 );
			}
			break;
		}

        case EImageFormat::IF_BGRA_UBYTE:
        case EImageFormat::IF_RGBA_UBYTE:
        {
            uint32 w_8 = (uint32)(factor*255);
			for ( int i=0; i< PixelCount *4; ++i )
			{
                uint32 a_8 = *pDestBuf;
                uint32 b_8 = *pBBuf++;
                uint32 i_16 = a_8 * (255-w_8) + b_8 * w_8;
                *pDestBuf++ = (uint8)(i_16>>8);
			}
			break;
		}

		default:
			check(false);
		}

	}

}

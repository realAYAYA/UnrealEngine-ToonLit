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
                                  const Image* pA,
                                  const Image* pB,
                                  float factor )
	{
		MUTABLE_CPUPROFILER_SCOPE(ImageInterpolate)

        check( pA->GetSizeX() == pDest->GetSizeX() );
        check( pA->GetSizeY() == pDest->GetSizeY() );
        check( pA->GetFormat() == pDest->GetFormat() );
        check( pA->GetSizeX() == pB->GetSizeX() );
        check( pA->GetSizeY() == pB->GetSizeY() );
        check( pA->GetFormat() == pB->GetFormat() );

		// Clamp the factor
		factor = FMath::Max( 0.0f, FMath::Min( 1.0f, factor ) );

        uint8_t* pDestBuf = pDest->GetData();
        const uint8_t* pABuf = pA->GetData();
        const uint8_t* pBBuf = pB->GetData();

		// Generic implementation
		int pixelCount = (int)pA->CalculatePixelCount();

		switch ( pA->GetFormat() )
		{
		case EImageFormat::IF_L_UBYTE:
		{
            uint32_t w_8 = (uint32_t)(factor*255);
			for ( int i=0; i<pixelCount; ++i )
			{
                uint32_t a_8 = *pABuf++;
                uint32_t b_8 = *pBBuf++;
                uint32_t i_16 = a_8 * (255-w_8) + b_8 * w_8;
                *pDestBuf++ = (uint8_t) ( i_16>>8 );
			}
			break;
		}

		case EImageFormat::IF_RGB_UBYTE:
		{
            uint32_t w_8 = (uint32_t)(factor*255);
			for ( int i=0; i<pixelCount*3; ++i )
			{
                uint32_t a_8 = *pABuf++;
                uint32_t b_8 = *pBBuf++;
                uint32_t i_16 = a_8 * (255-w_8) + b_8 * w_8;
                *pDestBuf++ = (uint8_t) ( i_16>>8 );
			}
			break;
		}

        case EImageFormat::IF_BGRA_UBYTE:
        case EImageFormat::IF_RGBA_UBYTE:
        {
            uint32_t w_8 = (uint32_t)(factor*255);
			for ( int i=0; i<pixelCount*4; ++i )
			{
                uint32_t a_8 = *pABuf++;
                uint32_t b_8 = *pBBuf++;
                uint32_t i_16 = a_8 * (255-w_8) + b_8 * w_8;
                *pDestBuf++ = (uint8_t)(i_16>>8);
			}
			break;
		}

		default:
			check(false);
		}

	}


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	inline ImagePtr ImageInterpolate( const Image* p0, const Image* p1, const Image* p2,
									  float factor1, float factor2 )
	{
		MUTABLE_CPUPROFILER_SCOPE(ImageInterpolate2)

		check( p0->GetSizeX() == p1->GetSizeX() );
		check( p0->GetSizeY() == p1->GetSizeY() );
		check( p0->GetFormat() == p1->GetFormat() );
		check( p0->GetSizeX() == p2->GetSizeX() );
		check( p0->GetSizeY() == p2->GetSizeY() );
		check( p0->GetFormat() == p2->GetFormat() );

		// Clamp the factor
		factor1 = FMath::Max( 0.0f, FMath::Min( 1.0f, factor1 ) );
		factor2 = FMath::Max( 0.0f, FMath::Min( 1.0f-factor1, factor2 ) );
		float factor0 = FMath::Max( 0.0f, 1.0f-factor1-factor2 );

        ImagePtr pDest = new Image( p0->GetSizeX(), p0->GetSizeY(),
                                    p0->GetLODCount(),
                                    p0->GetFormat() );

        uint8_t* pDestBuf = pDest->GetData();
        const uint8_t* p0Buf = p0->GetData();
        const uint8_t* p1Buf = p1->GetData();
        const uint8_t* p2Buf = p2->GetData();

		// Generic implementation
		int pixelCount = (int)p0->CalculatePixelCount();

        uint32_t w0_8 = (uint32_t)(factor0*255);
        uint32_t w1_8 = (uint32_t)(factor1*255);
        uint32_t w2_8 = (uint32_t)(factor2*255);

		switch ( p0->GetFormat() )
		{
		case EImageFormat::IF_L_UBYTE:
		{
			for ( int i=0; i<pixelCount; ++i )
			{
                uint32_t t0_8 = *p0Buf++;
                uint32_t t1_8 = *p1Buf++;
                uint32_t t2_8 = *p2Buf++;
                uint32_t i_16 = t0_8 * w0_8 + t1_8 * w1_8 + t2_8 * w2_8;
                *pDestBuf++ = (uint8_t) ( i_16>>8 );
			}
			break;
		}

		case EImageFormat::IF_RGB_UBYTE:
		{
			for ( int i=0; i<pixelCount*3; ++i )
			{
                uint32_t t0_8 = *p0Buf++;
                uint32_t t1_8 = *p1Buf++;
                uint32_t t2_8 = *p2Buf++;
                uint32_t i_16 = t0_8 * w0_8 + t1_8 * w1_8 + t2_8 * w2_8;
                *pDestBuf++ = (uint8_t) ( i_16>>8 );
			}
			break;
		}

        case EImageFormat::IF_BGRA_UBYTE:
        case EImageFormat::IF_RGBA_UBYTE:
        {
			for ( int i=0; i<pixelCount*4; ++i )
			{
                uint32_t t0_8 = *p0Buf++;
                uint32_t t1_8 = *p1Buf++;
                uint32_t t2_8 = *p2Buf++;
                uint32_t i_16 = t0_8 * w0_8 + t1_8 * w1_8 + t2_8 * w2_8;
                *pDestBuf++ = (uint8_t) (i_16>>8);
			}
			break;
		}

		default:
			check(false);
		}

		return pDest;
	}

}

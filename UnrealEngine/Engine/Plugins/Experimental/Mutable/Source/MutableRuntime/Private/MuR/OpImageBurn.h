// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"


namespace mu
{

	inline unsigned BurnChannel( unsigned base, unsigned blended )
	{
		// R = 1 - (1-Base) / Blend
		unsigned burn =
				FMath::Min( 255,
						  FMath::Max( 0,
									255 - ( ( ( 255 - (int)base  ) << 8 ) / ((int)blended+1) )
									)
						  );
		return burn;
	}

	inline unsigned BurnChannelMasked( unsigned base, unsigned blended, unsigned mask )
	{
		unsigned burn = BurnChannel( base, blended );
		unsigned masked = ( ( ( 255 - mask ) * base ) + ( mask * burn ) ) >> 8;
		return masked;
	}


    inline void ImageBurn( Image* pResult, const Image* pBase, vec3<float> col )
	{
        ImageLayerColour<BurnChannel,true>( pResult, pBase, col );
	}


    inline void ImageBurn( Image* pResult, const Image* pBase, const Image* pBlended,
                           bool applyToAlpha )
	{
        ImageLayer<BurnChannel,true>( pResult, pBase, pBlended, applyToAlpha );
	}


    inline void ImageBurn( Image* pResult, const Image* pBase, const Image* pMask, vec3<float> col )
	{
        ImageLayerColour<BurnChannelMasked,BurnChannel,true>( pResult, pBase, pMask, col );
	}


    inline void ImageBurn( Image* pResult,
                           const Image* pBase,
                           const Image* pMask,
                           const Image* pBlended,
                           bool applyToAlpha )
	{
        ImageLayer<BurnChannelMasked,BurnChannel,true>
                ( pResult, pBase, pMask, pBlended, applyToAlpha );
	}

}

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/OpImageLayer.h"

namespace mu
{

	inline unsigned HardLightChannel( unsigned base, unsigned blended )
	{
		// gimp-like


		// photoshop-like
		// if (Blend > ½) R = 1 - (1-Base) × (1-2×(Blend-½))
		// if (Blend <= ½) R = Base × (2×Blend)
		unsigned hardlight = blended > 128
				? 255 - ( ( ( 255 - base ) * ( 255 - 2*(blended - 128 ) ) ) >> 8 )
				: ( base * (2 * blended) ) >> 8;

		hardlight = FMath::Min( 255u, hardlight );

		return hardlight;
	}

	inline unsigned HardLightChannelMasked( unsigned base, unsigned blended, unsigned mask )
	{
		unsigned hardlight = HardLightChannel( base, blended );

		unsigned masked = ( ( ( 255 - mask ) * base ) + ( mask * hardlight ) ) >> 8;
		return masked;
	}


    inline void ImageHardLight( Image* pResult, const Image* pBase, vec3<float> col )
	{
        ImageLayerColour<HardLightChannel, true>( pResult, pBase, col );
	}


    inline void ImageHardLight( Image* pResult, const Image* pBase, const Image* pBlended,
                                bool applyToAlpha )
	{
        ImageLayer<HardLightChannel, true>( pResult, pBase, pBlended, applyToAlpha );
	}


    inline void ImageHardLight( Image* pResult, const Image* pBase, const Image* pMask, vec3<float> col )
	{
        ImageLayerColour<HardLightChannelMasked,HardLightChannel,true>( pResult, pBase, pMask, col );
	}


    inline void ImageHardLight( Image* pResult, const Image* pBase, const Image* pMask, const Image* pBlended,
                                bool applyToAlpha )
	{
        ImageLayer<HardLightChannelMasked,HardLightChannel,true>( pResult, pBase, pMask, pBlended, applyToAlpha );
	}

}

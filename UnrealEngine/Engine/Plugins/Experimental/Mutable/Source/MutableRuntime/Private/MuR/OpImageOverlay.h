// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/OpImageHardLight.h"


namespace mu
{
	inline unsigned OverlayChannelMasked( unsigned base, unsigned blended, unsigned mask )
	{
		// Photoshop like
		//return HardLightChannel( blended, base, mask );

		// gimp-like
		unsigned overlay = ( base * ( base + ( ( 2 * blended * ( 255 - base) ) >> 8  ) ) ) >> 8;
		unsigned masked = ( ( ( 255 - mask ) * base ) + ( mask * overlay ) ) >> 8;
		return masked;
	}


	inline unsigned OverlayChannel( unsigned base, unsigned blended )
	{
		// Photoshop like
		//return HardLightChannel( blended, base, mask );

		// gimp-like
		unsigned overlay = ( base * ( base + ( ( 2 * blended * ( 255 - base) ) >> 8  ) ) ) >> 8;
		return overlay;
	}


    inline void ImageOverlay( Image* pResult, const Image* pBase, const Image* pMask, vec3<float> col )
	{
        ImageLayerColour<OverlayChannelMasked,OverlayChannel,true>( pResult, pBase, pMask, col );
	}


    inline void ImageOverlay( Image* pResult, const Image* pBase, const Image* pMask,
                              const Image* pBlended,
                              bool applyToAlpha )
	{
        ImageLayer<OverlayChannelMasked,OverlayChannel,true>
                ( pResult, pBase, pMask, pBlended, applyToAlpha );
	}


    inline void ImageOverlay( Image* pResult, const Image* pBase, vec3<float> col )
	{
        ImageLayerColour<OverlayChannel,true>( pResult, pBase, col );
	}


    inline void ImageOverlay( Image* pResult, const Image* pBase, const Image* pBlended,
                              bool applyToAlpha )
	{
        ImageLayer<OverlayChannel,true>( pResult, pBase, pBlended, applyToAlpha );
	}

}

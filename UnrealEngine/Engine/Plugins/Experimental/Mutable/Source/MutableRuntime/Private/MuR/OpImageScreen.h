// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"


namespace mu
{

	inline unsigned ScreenChannelMasked( unsigned base, unsigned blended, unsigned mask )
	{
		// R = 1 - (1-Base) × (1-Blend)
		unsigned screen = 255 - ( ( ( 255 - base  ) * ( 255 - blended ) ) >> 8 );
		unsigned masked = ( ( ( 255 - mask ) * base ) + ( mask * screen ) ) >> 8;
		return masked;
	}

	inline unsigned ScreenChannel( unsigned base, unsigned blended )
	{
		// R = 1 - (1-Base) × (1-Blend)
		unsigned screen = 255 - ( ( ( 255 - base  ) * ( 255 - blended ) ) >> 8 );
		return screen;
	}


    inline void ImageScreen( Image* pResult, const Image* pBase, const Image* pMask, vec3<float> col )
	{
        ImageLayerColour<ScreenChannelMasked,ScreenChannel,false>( pResult, pBase, pMask, col );
	}


    inline void ImageScreen( Image* pResult, const Image* pBase, const Image* pMask,
                             const Image* pBlended,
                             bool applyToAlpha )
	{
        ImageLayer<ScreenChannelMasked,ScreenChannel,false>
                ( pResult, pBase, pMask, pBlended, applyToAlpha );
	}


    inline void ImageScreen( Image* pResult, const Image* pBase, vec3<float> col )
	{
        ImageLayerColour<ScreenChannel,false>( pResult, pBase, col );
	}


    inline void ImageScreen( Image* pResult, const Image* pBase, const Image* pBlended,
                             bool applyToAlpha )
	{
        ImageLayer<ScreenChannel,false>( pResult, pBase, pBlended, applyToAlpha );
	}

}

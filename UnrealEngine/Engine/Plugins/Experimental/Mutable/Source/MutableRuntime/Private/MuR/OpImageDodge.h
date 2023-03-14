// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"


namespace mu
{

	inline unsigned DodgeChannelMasked( unsigned base, unsigned blended, unsigned mask )
	{
		// R = Base / (1-Blend)
		unsigned dodge = ( base << 8 ) / ( 256 - blended );
		unsigned masked = ( ( ( 255 - mask ) * base ) + ( mask * dodge ) ) >> 8;
		return masked;
	}

	inline unsigned DodgeChannel( unsigned base, unsigned blended )
	{
		// R = Base / (1-Blend)
		unsigned dodge = ( base << 8 ) / ( 256 - blended );
		return dodge;
	}


    inline void ImageDodge( Image* pResult, const Image* pBase, const Image* pMask, vec3<float> col )
	{
        ImageLayerColour<DodgeChannelMasked,DodgeChannel, true>( pResult, pBase, pMask, col );
	}


    inline void ImageDodge( Image* pResult, const Image* pBase, const Image* pMask,
                            const Image* pBlended,
                            bool applyToAlpha )
	{
        ImageLayer<DodgeChannelMasked,DodgeChannel, true>
                ( pResult, pBase, pMask, pBlended, applyToAlpha );
	}


    inline void ImageDodge( Image* pResult, const Image* pBase, vec3<float> col )
	{
        ImageLayerColour<DodgeChannel, true>( pResult, pBase, col );
	}


    inline void ImageDodge( Image* pResult, const Image* pBase, const Image* pBlended,
                            bool applyToAlpha )
	{
        ImageLayer<DodgeChannel, true>( pResult, pBase, pBlended, applyToAlpha );
	}

}

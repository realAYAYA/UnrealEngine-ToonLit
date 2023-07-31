// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/OpImageLayer.h"


namespace mu
{

	inline unsigned BlendChannel( unsigned // base
		, unsigned blended )
	{
		return blended;
	}

	inline unsigned BlendChannelMasked( unsigned base, unsigned blended, unsigned mask )
	{
		unsigned blend = blended;
		unsigned masked = ( ( ( 255 - mask ) * base ) + ( mask * blend ) ) / 255;
		return masked;
	}


    inline void ImageBlend( Image* pResult, const Image* pBase, const Image* pMask, vec3<float> col )
	{
        ImageLayerColour<BlendChannelMasked, BlendChannel, false>( pResult, pBase, pMask, col );
	}


    inline void ImageBlend( Image* pResult, const Image* pBase, const Image* pMask,
                            const Image* pBlended,
                            bool applyToAlpha )
	{
        ImageLayer<BlendChannelMasked, BlendChannel, false>
                ( pResult, pBase, pMask, pBlended, applyToAlpha );
	}


    inline void ImageBlend( Image* pResult, const Image* pBase, const Image* pBlended,
                            bool applyToAlpha )
    {
        return ImageLayer<BlendChannel,false>( pResult, pBase, pBlended, applyToAlpha );
    }


	inline void ImageBlendOnBase( Image* pBase, const Image* pMask, vec3<float> col )
	{
		ImageLayerColourOnBase<BlendChannelMasked, BlendChannel, false>( pBase, pMask, col );
	}


    inline void ImageBlendOnBase( Image* pBase, const Image* pMask, const Image* pBlended,
                                  bool applyToAlpha )
	{
        ImageLayerOnBase<BlendChannelMasked, BlendChannel, false>
                 ( pBase, pMask, pBlended, applyToAlpha );
	}

	//! Blend a subimage on the base using a mask.
	inline void ImageBlendOnBaseNoAlpha( Image* pBase,
										 const Image* pMask,
										 const Image* pBlended,
										 const box< vec2<int> >& rect )
	{
		ImageLayerOnBaseNoAlpha< BlendChannel, false>
				( pBase, pMask, pBlended, rect );
	}


}

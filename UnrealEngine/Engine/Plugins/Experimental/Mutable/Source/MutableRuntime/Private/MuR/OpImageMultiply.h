// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/OpImageLayer.h"
#include "MuR/Platform.h"

namespace mu
{

	inline unsigned MultiplyChannelMasked( unsigned base, unsigned blended, unsigned mask )
	{
		unsigned multiply = ( base * blended ) >> 8;
		unsigned masked = ( ( ( 255 - mask ) * base ) + ( mask * multiply ) ) >> 8;
		return masked;
	}

	inline unsigned MultiplyChannel( unsigned base, unsigned blended )
	{
		unsigned multiply = ( base * blended ) >> 8;
		return multiply;
	}


    inline void ImageMultiply( Image* pResult, const Image* pBase, const Image* pMask, vec3<float> col )
	{
        ImageLayerColour<MultiplyChannelMasked,MultiplyChannel,false>( pResult, pBase, pMask, col );
	}


    inline void ImageMultiply( Image* pResult, const Image* pBase, const Image* pMask,
                               const Image* pBlended,
                               bool applyToAlpha )
	{
        return ImageLayer<MultiplyChannelMasked,MultiplyChannel,false>
                ( pResult, pBase, pMask, pBlended, applyToAlpha );
	}


    inline void ImageMultiply( Image* pResult, const Image* pBase, vec3<float> col )
	{
        return ImageLayerColour<MultiplyChannel,false>( pResult, pBase, col );
	}


    inline void ImageMultiply( Image* pResult, const Image* pBase, const Image* pBlended,
                               bool applyToAlpha )
	{
        return ImageLayer<MultiplyChannel,false>( pResult, pBase, pBlended, applyToAlpha );
	}


	inline void ImageMultiplyOnBase( Image* pBase, const Image* pMask, vec3<float> col )
	{
		ImageLayerColourOnBase<MultiplyChannelMasked,MultiplyChannel, false>( pBase, pMask, col );
	}

    inline void ImageMultiplyOnBase( Image* pBase, const Image* pMask, const Image* pBlended,
                                     bool applyToAlpha )
	{
        ImageLayerOnBase<MultiplyChannelMasked,MultiplyChannel, false>
                ( pBase, pMask, pBlended, applyToAlpha );
	}

    inline void ImageMultiplyOnBase( Image* pBase, const Image* pBlended,
                                     bool applyToAlpha )
	{
        ImageLayerOnBase<MultiplyChannel, false>( pBase, pBlended, applyToAlpha );
	}

}

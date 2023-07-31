// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/OpImageHardLight.h"


namespace mu
{
    inline unsigned AlphaOverlayChannelMasked( unsigned base, unsigned blended, unsigned mask )
    {
            // Photoshop like
            //return HardLightChannel( blended, base, mask );

            // gimp-like
            //unsigned overlay = ( base * ( base + ( ( 2 * blended * ( 255 - base) ) >> 8  ) ) ) >> 8;
            unsigned overlay = base + (blended * unsigned(255 - base) >> 8);
            unsigned masked = ( ( ( 255 - mask ) * base ) + ( mask * overlay ) ) >> 8;
            return masked;
    }


    inline unsigned AlphaOverlayChannel( unsigned base, unsigned blended )
    {
            // Photoshop like
            //return HardLightChannel( blended, base, mask );

            // gimp-like
            //unsigned overlay = ( base * ( base + ( ( 2 * blended * ( 255 - base) ) >> 8  ) ) ) >> 8;
            unsigned overlay = base + (blended * unsigned(255 - base) >> 8);
            return overlay;
    }


    inline void ImageAlphaOverlay( Image* pResult, const Image* pBase, const Image* pMask, vec3<float> col )
    {
        ImageLayerColour<AlphaOverlayChannelMasked,AlphaOverlayChannel,true>( pResult, pBase, pMask, col );
    }


    inline void ImageAlphaOverlay( Image* pResult, const Image* pBase, const Image* pMask,
                                   const Image* pBlended,
                                   bool applyToAlpha )
    {
        ImageLayer<AlphaOverlayChannelMasked,AlphaOverlayChannel,true>
                ( pResult, pBase, pMask, pBlended, applyToAlpha );
    }


    inline void ImageAlphaOverlay( Image* pResult, const Image* pBase, vec3<float> col )
    {
        ImageLayerColour<AlphaOverlayChannel,true>( pResult, pBase, col );
    }


    inline void ImageAlphaOverlay( Image* pResult, const Image* pBase, const Image* pBlended,
                                   bool applyToAlpha )
    {
        ImageLayer<AlphaOverlayChannel,true>( pResult, pBase, pBlended, applyToAlpha );
    }

}

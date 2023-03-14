// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/OpImageLayer.h"

namespace mu
{

	inline unsigned SoftLightChannel( unsigned base, unsigned blended )
	{
		// gimp-like
		unsigned mix = ( base * blended ) >> 8;
		unsigned screen = 255 - ( ( ( 255 - base  ) * ( 255 - blended ) ) >> 8 );
		unsigned softlight = ( ( ( 255 - base ) * mix ) + ( base * screen ) ) >> 8;
		return softlight;
	}


	inline unsigned SoftLightChannelMasked( unsigned base, unsigned blended, unsigned mask )
	{
		unsigned softlight = SoftLightChannel( base, blended );
		unsigned masked = ( ( ( 255 - mask ) * base ) + ( mask * softlight ) ) >> 8;
		return masked;
	}


    inline void ImageSoftLight( Image* pResult,
                                const Image* pBase,
                                const Image* pMask,
                                vec3<float> col,
                                TArray< TPair<uint8_t,uint8_t*> >* pTables )
	{
        check( pResult->GetFormat() == pBase->GetFormat() );
        check( pResult->GetSizeX() == pBase->GetSizeX() );
        check( pResult->GetSizeY() == pBase->GetSizeY() );
        check( pResult->GetLODCount() == pBase->GetLODCount() );

		EImageFormat format = pBase->GetFormat();

        bool done = false;

        // If we are reusing tables (single-threaded)
        if ( pTables )
		{
			switch ( format )
			{
			case EImageFormat::IF_RGB_UBYTE:
            case EImageFormat::IF_RGBA_UBYTE:
            case EImageFormat::IF_BGRA_UBYTE:
            {
                uint8_t* tables[3];

				for ( int t=0; t<3; ++t )
				{
                    uint8_t c = uint8_t( FMath::Min( 255.0f, FMath::Max( 0.0f, col[t]*255.0f ) ) );

					// Try to find an existing table
					tables[t] = 0;
					for ( size_t j=0; !tables[t] && j<pTables->Num(); ++j )
					{
						if ( (*pTables)[j].Key==c )
						{
							tables[t] = (*pTables)[j].Value;
						}
					}

					if (!tables[t])
					{
                        tables[t] = new uint8_t[256];
						for ( int i=0; i<256;++i )
						{
                            tables[t][i] = (uint8_t)SoftLightChannel( i, c );
						}
                        pTables->Add(TPair<uint8_t,uint8_t*>( c, tables[t] ) );
					}
				}

                ImageTable( pResult, pBase, pMask, tables[0] ,tables[1], tables[2] );
                done = true;
				break;
			}

			default:
				break;
			}
		}
        else
        {
            // If we are going to apply it to many pixels, make a single-use table
            int pixelCount = pResult->GetSizeX()*pResult->GetSizeY();
            if ( pixelCount>256*3*2 )
            {
                uint8_t tables[256*3];
                for ( int t=0; t<3; ++t )
                {
                    uint8_t c = uint8_t( FMath::Min( 255.0f, FMath::Max( 0.0f, col[t]*255.0f ) ) );
                    for ( int i=0; i<256;++i )
                    {
                        tables[t*256+i] = (uint8_t)SoftLightChannel( i, c );
                    }
                }

                ImageTable( pResult, pBase, pMask, &tables[256*0], &tables[256*1], &tables[256*2] );
                done = true;
            }
        }

        if (!done)
		{
            ImageLayerColour<SoftLightChannelMasked,SoftLightChannel,false>
                    ( pResult, pBase, pMask, col );
		}

	}


    inline void ImageSoftLight( Image* pResult, const Image* pBase, const Image* pMask,
                                const Image* pBlended,
                                bool applyToAlpha )
	{
        ImageLayer<SoftLightChannelMasked,SoftLightChannel,false>( pResult, pBase, pMask, pBlended, applyToAlpha );
	}


    inline void ImageSoftLight( Image* pResult, const Image* pBase, vec3<float> col )
	{
        ImageLayerColour<SoftLightChannel,false>( pResult, pBase, col );
	}


    inline void ImageSoftLight( Image* pResult, const Image* pBase, const Image* pBlended,
                                bool applyToAlpha )
	{
        ImageLayer<SoftLightChannel,false>( pResult, pBase, pBlended, applyToAlpha );
	}

}

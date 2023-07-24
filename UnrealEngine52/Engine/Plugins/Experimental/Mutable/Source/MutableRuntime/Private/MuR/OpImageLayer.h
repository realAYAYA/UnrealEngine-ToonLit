// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/ImageRLE.h"
#include "Async/ParallelFor.h"
#include "Templates/UnrealTemplate.h"

namespace mu
{
	inline bool IsAnyComponentLargerThan1( FVector4f v )
	{
		return (v[0] > 1) || (v[1] > 1) || (v[2] > 1) || (v[3] > 1);
	}
	

	//---------------------------------------------------------------------------------------------
	//! Apply a blending function to an image with a colour source
	//---------------------------------------------------------------------------------------------
	template< unsigned (*BLEND_FUNC)(unsigned,unsigned), bool CLAMP >
    inline void BufferLayerColour( Image* pResult, const Image* pBase, vec3<float> col )
	{
        check( pResult->GetFormat() == pBase->GetFormat() );
        check( pResult->GetSizeX() == pBase->GetSizeX() );
        check( pResult->GetSizeY() == pBase->GetSizeY() );
        check( pResult->GetLODCount() == pBase->GetLODCount() );

		EImageFormat baseFormat = pBase->GetFormat();

        uint8* pDestBuf = pResult->GetData();
        const uint8* pBaseBuf = pBase->GetData();

		unsigned top[3];
		top[0] = (unsigned)(255 * col[0]);
		top[1] = (unsigned)(255 * col[1]);
		top[2] = (unsigned)(255 * col[2]);

		// Generic implementation
		int32 pixelCount = pBase->CalculatePixelCount();

		constexpr int PixelCountConcurrencyThreshold = 0xff;

		switch (baseFormat)
		{
		case EImageFormat::IF_RGB_UBYTE:
		{
			const auto& ProcessPixel = [
				pBaseBuf, pDestBuf, top
			] (uint32 i)
			{
				for (int c = 0; c < 3; ++c)
				{
					unsigned base = pBaseBuf[3 * i + c];
					unsigned result = BLEND_FUNC(base, top[c]);
					if (CLAMP)
					{
						pDestBuf[3 * i + c] = (uint8)FMath::Min(255u, result);
					}
					else
					{
						pDestBuf[3 * i + c] = (uint8)result;
					}
				}
			};

			if (pixelCount > PixelCountConcurrencyThreshold)
			{
				ParallelFor(pixelCount, ProcessPixel);
			}
			else
			{
				for ( int p=0; p< pixelCount; ++p )
				{
					ProcessPixel(p);
				}
			}
			break;
		}
		case EImageFormat::IF_RGBA_UBYTE:
		{
			const auto& ProcessPixel = [
				pBaseBuf, pDestBuf, top
			] (uint32 i)
			{
				for (int c = 0; c < 3; ++c)
				{
					unsigned base = pBaseBuf[4 * i + c];
					unsigned result = BLEND_FUNC(base, top[c]);
					if (CLAMP)
					{
						pDestBuf[4 * i + c] = (uint8)FMath::Min(255u, result);
					}
					else
					{
						pDestBuf[4 * i + c] = (uint8)result;
					}
				}
				pDestBuf[4 * i + 3] = pBaseBuf[4 * i + 3];
			};

			if (pixelCount > PixelCountConcurrencyThreshold)
			{
				ParallelFor(pixelCount, ProcessPixel);
			}
			else
			{
				for (int p = 0; p < pixelCount; ++p)
				{
					ProcessPixel(p);
				}
			}
			break;
		}
		case EImageFormat::IF_BGRA_UBYTE:
		{
			const auto& ProcessPixel = [
					pBaseBuf, pDestBuf, top
				] (uint32 i)
				{
					for (int c = 0; c < 3; ++c)
					{
						unsigned base = pBaseBuf[4 * i + c];
						unsigned result = BLEND_FUNC(base, top[2 - c]);
						if (CLAMP)
						{
							pDestBuf[4 * i + c] = (uint8)FMath::Min(255u, result);
						}
						else
						{
							pDestBuf[4 * i + c] = (uint8)result;
						}
					}
					pDestBuf[4 * i + 3] = pBaseBuf[4 * i + 3];
				};

			if (pixelCount > PixelCountConcurrencyThreshold)
			{
				ParallelFor(pixelCount, ProcessPixel);
			}
			else
			{
				for (int p = 0; p < pixelCount; ++p)
				{
					ProcessPixel(p);
				}
			}
			break;
		}
		case EImageFormat::IF_L_UBYTE:
		{
			const auto& ProcessPixel = [
					pBaseBuf, pDestBuf, top
				] (uint32 i)
				{
					unsigned base = pBaseBuf[i];
					unsigned result = BLEND_FUNC(base, top[0]);
					if (CLAMP)
					{
						pDestBuf[i] = (uint8)FMath::Min(255u, result);
					}
					else
					{
						pDestBuf[i] = (uint8)result;
					}
				};


			if (pixelCount > PixelCountConcurrencyThreshold)
			{
				ParallelFor(pixelCount, ProcessPixel);
			}
			else
			{
				for (int p = 0; p < pixelCount; ++p)
				{
					ProcessPixel(p);
				}
			}
			break;
		}

		default:
			checkf(false, TEXT("Unsupported format."));
			break;
		}
    }


	template< unsigned (*BLEND_FUNC)(unsigned, unsigned) >
	inline void BufferLayerColour(Image* Result, const Image* Base, FVector4f Col)
	{
		bool bIsClampNeeded = IsAnyComponentLargerThan1(Col);
		if (bIsClampNeeded)
		{
			BufferLayerColour<BLEND_FUNC, true>(Result, Base, Col);
		}
		else
		{
			BufferLayerColour<BLEND_FUNC, false>(Result, Base, Col);
		}
	}


	//---------------------------------------------------------------------------------------------
	template< unsigned (*BLEND_FUNC)(unsigned, unsigned), bool CLAMP >
	inline void BufferLayerColourFromAlpha(Image* Result, const Image* Base, FVector4f Col)
	{
		check(Result->GetFormat() == Base->GetFormat());
		check(Result->GetSizeX() == Base->GetSizeX());
		check(Result->GetSizeY() == Base->GetSizeY());
		check(Result->GetLODCount() == Base->GetLODCount());

		EImageFormat baseFormat = Base->GetFormat();

		uint8* pDestBuf = Result->GetData();
		const uint8* pBaseBuf = Base->GetData();

		unsigned top[3];
		top[0] = (unsigned)(255 * Col[0]);
		top[1] = (unsigned)(255 * Col[1]);
		top[2] = (unsigned)(255 * Col[2]);

		// Generic implementation
		int32 pixelCount = Base->CalculatePixelCount();

		constexpr int PixelCountConcurrencyThreshold = 0xff;

		switch (baseFormat)
		{
		case EImageFormat::IF_RGB_UBYTE:
		{
			// There is no alpha so we assume 255
			const auto& ProcessPixel = [ pDestBuf, top ] (uint32 i)
			{
				for (int c = 0; c < 3; ++c)
				{
					unsigned base = 255;
					unsigned result = BLEND_FUNC(base, top[c]);
					if (CLAMP)
					{
						pDestBuf[3 * i + c] = (uint8)FMath::Min(255u, result);
					}
					else
					{
						pDestBuf[3 * i + c] = (uint8)result;
					}
				}
			};

			if (pixelCount > PixelCountConcurrencyThreshold)
			{
				ParallelFor(pixelCount, ProcessPixel);
			}
			else
			{
				for (int p = 0; p < pixelCount; ++p)
				{
					ProcessPixel(p);
				}
			}
			break;
		}
		case EImageFormat::IF_RGBA_UBYTE:
		{
			const auto& ProcessPixel = [ pBaseBuf, pDestBuf, top ] (uint32 i)
			{
				for (int c = 0; c < 3; ++c)
				{
					unsigned base = pBaseBuf[4 * i + 3];
					unsigned result = BLEND_FUNC(base, top[c]);
					if (CLAMP)
					{
						pDestBuf[4 * i + c] = (uint8)FMath::Min(255u, result);
					}
					else
					{
						pDestBuf[4 * i + c] = (uint8)result;
					}
				}
				pDestBuf[4 * i + 3] = pBaseBuf[4 * i + 3];
			};

			if (pixelCount > PixelCountConcurrencyThreshold)
			{
				ParallelFor(pixelCount, ProcessPixel);
			}
			else
			{
				for (int p = 0; p < pixelCount; ++p)
				{
					ProcessPixel(p);
				}
			}
			break;
		}
		case EImageFormat::IF_BGRA_UBYTE:
		{
			const auto& ProcessPixel = [ pBaseBuf, pDestBuf, top ] (uint32 i)
			{
				for (int c = 0; c < 3; ++c)
				{
					unsigned base = pBaseBuf[4 * i + 3];
					unsigned result = BLEND_FUNC(base, top[2 - c]);
					if (CLAMP)
					{
						pDestBuf[4 * i + c] = (uint8)FMath::Min(255u, result);
					}
					else
					{
						pDestBuf[4 * i + c] = (uint8)result;
					}
				}
				pDestBuf[4 * i + 3] = pBaseBuf[4 * i + 3];
			};

			if (pixelCount > PixelCountConcurrencyThreshold)
			{
				ParallelFor(pixelCount, ProcessPixel);
			}
			else
			{
				for (int p = 0; p < pixelCount; ++p)
				{
					ProcessPixel(p);
				}
			}
			break;
		}
		case EImageFormat::IF_L_UBYTE:
		{
			const auto& ProcessPixel = [ pBaseBuf, pDestBuf, top ] (uint32 i)
			{
				// There is no alpha so we assume 255
				unsigned base = 255;
				unsigned result = BLEND_FUNC(base, top[0]);
				if (CLAMP)
				{
					pDestBuf[i] = (uint8)FMath::Min(255u, result);
				}
				else
				{
					pDestBuf[i] = (uint8)result;
				}
			};


				if (pixelCount > PixelCountConcurrencyThreshold)
				{
					ParallelFor(pixelCount, ProcessPixel);
				}
				else
				{
					for (int p = 0; p < pixelCount; ++p)
					{
						ProcessPixel(p);
					}
				}
				break;
		}

		default:
			checkf(false, TEXT("Unsupported format."));
			break;
		}
	}


	//---------------------------------------------------------------------------------------------
	template< unsigned (*BLEND_FUNC)(unsigned, unsigned)>
	inline void BufferLayerColourFromAlpha(Image* Result, const Image* Base, FVector4f Col)
	{
		bool bIsClampNeeded = IsAnyComponentLargerThan1(Col);
		if (bIsClampNeeded)
		{
			BufferLayerColourFromAlpha<BLEND_FUNC,true>(Result, Base, Col);
		}
		else
		{
			BufferLayerColourFromAlpha<BLEND_FUNC,false>(Result, Base, Col);
		}
	}


	//---------------------------------------------------------------------------------------------
	template< unsigned (*BLEND_FUNC_MASKED)(unsigned,unsigned,unsigned),
		unsigned (*BLEND_FUNC)(unsigned,unsigned),
		bool CLAMP,
		// Number of total channels to actually process
		uint32 CHANNELS_TO_BLEND,
		// Number of total channels in the base image
		int32 BASE_CHANNEL_STRIDE >
    inline void BufferLayerColourFormat( uint8* StartDestBuf, const Image* Base, const Image* Mask, FVector4f Col, uint32 BaseOffset, bool bOnlyOneMip )
	{
		check(CHANNELS_TO_BLEND+BaseOffset<=BASE_CHANNEL_STRIDE);

		unsigned top[3];
		top[0] = (unsigned)(255 * Col[0]);
		top[1] = (unsigned)(255 * Col[1]);
		top[2] = (unsigned)(255 * Col[2]);

		uint8* pDestBuf = StartDestBuf+ BaseOffset;
        const uint8* pMaskBuf = Mask->GetData();
        const uint8* pBaseBuf = Base->GetData()+ BaseOffset;
		EImageFormat maskFormat = Mask->GetFormat();

		int32 LODCount = Base->GetLODCount();

		if (bOnlyOneMip)
		{
			LODCount = 1;
		}

        bool isUncompressed = ( maskFormat == EImageFormat::IF_L_UBYTE );

		constexpr uint32 NumColorChannels = FMath::Min(CHANNELS_TO_BLEND,3u);
        if ( isUncompressed )
        {
            int32 pixelCount = Base->CalculatePixelCount();
            for ( int i=0; i<pixelCount; ++i )
            {
                unsigned mask = pMaskBuf[i];
                for ( int32 c=0; c<NumColorChannels; ++c )
                {
                    unsigned base = pBaseBuf[BASE_CHANNEL_STRIDE *i+c];
                    unsigned result = BLEND_FUNC_MASKED( base, top[c], mask );
                    if ( CLAMP )
                    {
                        pDestBuf[BASE_CHANNEL_STRIDE *i+c] = (uint8)FMath::Min( 255u, result );
                    }
                    else
                    {
                        pDestBuf[BASE_CHANNEL_STRIDE *i+c] = (uint8)result;
                    }
                }

                constexpr bool isNC4 = (BASE_CHANNEL_STRIDE ==4);
                if ( isNC4 )
                {
                    pDestBuf[BASE_CHANNEL_STRIDE *i+3] = pBaseBuf[BASE_CHANNEL_STRIDE *i+3];
                }
            }
        }
        else if ( maskFormat==EImageFormat::IF_L_UBYTE_RLE )
        {
            int rows = Base->GetSizeY();
            int width = Base->GetSizeX();

			// debug test
			ImagePtr TempDecodedMask = new Image(Mask->GetSizeX(), Mask->GetSizeY(), Mask->GetLODCount(), EImageFormat::IF_L_UBYTE);
			UncompressRLE_L(Mask, TempDecodedMask.get());

            for (int32 lod=0;lod<LODCount; ++lod)
            {
				// Skip mip size and row sizes.
                pMaskBuf += sizeof(uint32) +rows*sizeof(uint32);

                for ( int r=0; r<rows; ++r )
                {
                    const uint8* pDestRowEnd = pDestBuf + width* BASE_CHANNEL_STRIDE;
                    while ( pDestBuf!=pDestRowEnd )
                    {
                        // Decode header
						uint16 equal = 0;
						FMemory::Memmove(&equal, pMaskBuf, sizeof(uint16));
                        pMaskBuf += 2;

                        uint8 different = *pMaskBuf;
                        ++pMaskBuf;

                        uint8 equalPixel = *pMaskBuf;
                        ++pMaskBuf;

                        // Equal pixels
						check(pDestBuf + BASE_CHANNEL_STRIDE * equal <= StartDestBuf + Base->GetDataSize());
                        if ( equalPixel==255 )
                        {
                            for ( int i=0; i<equal; ++i )
                            {
                                for ( int32 c=0; c<NumColorChannels; ++c )
                                {
                                    unsigned base = pBaseBuf[BASE_CHANNEL_STRIDE *i+c];
                                    unsigned result = BLEND_FUNC( base, top[c] );
                                    if ( CLAMP )
                                    {
                                        pDestBuf[BASE_CHANNEL_STRIDE *i+c] = (uint8)FMath::Min( 255u, result );
                                    }
                                    else
                                    {
                                        pDestBuf[BASE_CHANNEL_STRIDE *i+c] = (uint8)result;
                                    }
                                }

                                constexpr bool isNC4 = (BASE_CHANNEL_STRIDE ==4);
                                if ( isNC4 )
                                {
                                    pDestBuf[BASE_CHANNEL_STRIDE *i+3] = pBaseBuf[BASE_CHANNEL_STRIDE *i+3];
                                }
                            }
                        }
                        else if ( equalPixel>0 )
                        {
                            for ( int i=0; i<equal; ++i )
                            {
                                for ( int32 c=0; c<NumColorChannels; ++c )
                                {
                                    unsigned base = pBaseBuf[BASE_CHANNEL_STRIDE *i+c];
                                    unsigned result = BLEND_FUNC_MASKED( base, top[c], equalPixel );
                                    if ( CLAMP )
                                    {
                                        pDestBuf[BASE_CHANNEL_STRIDE *i+c] = (uint8)FMath::Min( 255u, result );
                                    }
                                    else
                                    {
                                        pDestBuf[BASE_CHANNEL_STRIDE *i+c] = (uint8)result;
                                    }
                                }

                                constexpr bool isNC4 = (BASE_CHANNEL_STRIDE ==4);
                                if ( isNC4 )
                                {
                                    pDestBuf[BASE_CHANNEL_STRIDE *i+3] = pBaseBuf[BASE_CHANNEL_STRIDE *i+3];
                                }
                            }
                        }
                        else
                        {
                            // It could happen if xxxxxOnBase
                            if (pDestBuf!=pBaseBuf)
                            {
                                FMemory::Memmove( pDestBuf, pBaseBuf, BASE_CHANNEL_STRIDE*equal );
                            }
                        }
                        pDestBuf += BASE_CHANNEL_STRIDE *equal;
                        pBaseBuf += BASE_CHANNEL_STRIDE *equal;

                        // Different pixels
						check(pDestBuf + BASE_CHANNEL_STRIDE * different <= StartDestBuf + Base->GetDataSize());
                        for ( int i=0; i<different; ++i )
                        {
                            for ( int32 c=0; c<NumColorChannels; ++c )
                            {
                                unsigned mask = pMaskBuf[i];
                                unsigned base = pBaseBuf[BASE_CHANNEL_STRIDE *i+c];
                                unsigned result = BLEND_FUNC_MASKED( base, top[c], mask );
                                if ( CLAMP )
                                {
                                    pDestBuf[BASE_CHANNEL_STRIDE *i+c] = (uint8)FMath::Min( 255u, result );
                                }
                                else
                                {
                                    pDestBuf[BASE_CHANNEL_STRIDE *i+c] = (uint8)result;
                                }
                            }

                            constexpr bool isNC4 = (BASE_CHANNEL_STRIDE ==4);
                            if ( isNC4 )
                            {
                                pDestBuf[BASE_CHANNEL_STRIDE *i+3] = pBaseBuf[BASE_CHANNEL_STRIDE *i+3];
                            }
                        }

                        pDestBuf += BASE_CHANNEL_STRIDE *different;
                        pBaseBuf += BASE_CHANNEL_STRIDE *different;
                        pMaskBuf += different;
                    }
                }

                rows = FMath::DivideAndRoundUp(rows,2);
                width = FMath::DivideAndRoundUp(width,2);
            }
        }
        else
        {
            checkf( false, TEXT("Unsupported mask format.") );
        }
	}


	//---------------------------------------------------------------------------------------------
	//! Apply a blending function to an image with a colour source and a mask
	//---------------------------------------------------------------------------------------------
	template< unsigned (*BLEND_FUNC_MASKED)(unsigned,unsigned,unsigned),
			  unsigned (*BLEND_FUNC)(unsigned,unsigned),
			  bool CLAMP
			  >
    inline void BufferLayerColour( uint8* DestBuf, const Image* Base, const Image* Mask, FVector4f Col )
	{
		check(Base->GetSizeX() == Mask->GetSizeX());
		check(Base->GetSizeY() == Mask->GetSizeY());
		check(Mask->GetFormat() == EImageFormat::IF_L_UBYTE
			||
			Mask->GetFormat() == EImageFormat::IF_L_UBYTE_RLE);

		bool bValid =
			(Base->GetSizeX() == Mask->GetSizeX())
			&&
			(Base->GetSizeY() == Mask->GetSizeY())
			&&
			(Mask->GetFormat() == EImageFormat::IF_L_UBYTE
				||
				Mask->GetFormat() == EImageFormat::IF_L_UBYTE_RLE);
		if (!bValid)
		{
			return;
		}

		EImageFormat baseFormat = Base->GetFormat();
		if ( baseFormat==EImageFormat::IF_RGB_UBYTE )
		{
			BufferLayerColourFormat<BLEND_FUNC_MASKED, BLEND_FUNC, CLAMP, 3, 3>( DestBuf, Base, Mask, Col, 0, false);
		}
        else if ( baseFormat==EImageFormat::IF_RGBA_UBYTE )
        {
            BufferLayerColourFormat<BLEND_FUNC_MASKED, BLEND_FUNC, CLAMP, 3, 4>( DestBuf, Base, Mask, Col, 0, false);
        }
        else if ( baseFormat==EImageFormat::IF_BGRA_UBYTE )
        {
            float temp = Col[0];
            Col[0] = Col[2];
            Col[2] = temp;
            BufferLayerColourFormat<BLEND_FUNC_MASKED, BLEND_FUNC, CLAMP, 3, 4>( DestBuf, Base, Mask, Col, 0, false);
        }
        else if ( baseFormat==EImageFormat::IF_L_UBYTE )
		{
			BufferLayerColourFormat<BLEND_FUNC_MASKED, BLEND_FUNC, CLAMP, 1, 1>( DestBuf, Base, Mask, Col, 0, false);
		}
		else
		{
			checkf( false, TEXT("Unsupported format.") );
		}
	}


	//---------------------------------------------------------------------------------------------
	template< unsigned (*BLEND_FUNC_MASKED)(unsigned, unsigned, unsigned),
		unsigned (*BLEND_FUNC)(unsigned, unsigned)
	>
	inline void BufferLayerColour(uint8* DestBuf, const Image* Base, const Image* Mask, FVector4f Col)
	{
		bool bIsClampNeeded = IsAnyComponentLargerThan1(Col);
		if (bIsClampNeeded)
		{
			BufferLayerColour<BLEND_FUNC_MASKED, BLEND_FUNC, true>(DestBuf, Base, Mask, Col );
		}
		else
		{
			BufferLayerColour<BLEND_FUNC_MASKED, BLEND_FUNC, false>(DestBuf, Base, Mask, Col );
		}
	}


	//---------------------------------------------------------------------------------------------
	//! Apply a blending function to an image with another image as blending layer
	//---------------------------------------------------------------------------------------------
	template< unsigned (*BLEND_FUNC)(unsigned, unsigned), bool CLAMP,
		// Number of total channels to actually process
		uint32 CHANNELS_TO_BLEND,
		// Number of total channels in the base image
		int32 BASE_CHANNEL_STRIDE>

	inline void BufferLayerColourFormatInPlace(Image* Base, FVector4f Col, uint32 BaseChannelOffset, bool bOnlyFirstLOD)
	{
		uint8* pBaseBuf = Base->GetData() + BaseChannelOffset;

		uint32 top[4];
		top[0] = (uint32)(255 * Col[0]);
		top[1] = (uint32)(255 * Col[1]);
		top[2] = (uint32)(255 * Col[2]);
		top[3] = (uint32)(255 * Col[0]); // used for 1 channel blends to alpha

		// Generic implementation
		int32 PixelCount = 0;
		if (bOnlyFirstLOD)
		{
			PixelCount = Base->GetSizeX() * Base->GetSizeY();
		}
		else
		{
			PixelCount = Base->CalculatePixelCount();
		}

		ParallelFor(PixelCount,
			[
				pBaseBuf, top, BaseChannelOffset
			] (uint32 i)
			{
				for (int c = 0; c < CHANNELS_TO_BLEND; ++c)
				{
					uint32 base = pBaseBuf[BASE_CHANNEL_STRIDE * i + c];
					uint32 blended = top[c + BaseChannelOffset];
					uint32 result = BLEND_FUNC(base, blended);
					if (CLAMP)
					{
						pBaseBuf[BASE_CHANNEL_STRIDE * i + c] = (uint8)FMath::Min(255u, result);
					}
					else
					{
						pBaseBuf[BASE_CHANNEL_STRIDE * i + c] = (uint8)result;
					}
				}
			});
	}


	//---------------------------------------------------------------------------------------------
	template< unsigned (*BLEND_FUNC)(unsigned, unsigned), bool CLAMP, uint32 CHANNEL_COUNT >
	inline void BufferLayerColourInPlace(Image* Base, FVector4f Col, bool bOnlyOneMip, uint32 BaseOffset)
	{
		EImageFormat baseFormat = Base->GetFormat();

		if (baseFormat == EImageFormat::IF_RGB_UBYTE)
		{
			check(BaseOffset + CHANNEL_COUNT <= 3);
			BufferLayerColourFormatInPlace< BLEND_FUNC, CLAMP, CHANNEL_COUNT, 3 >(Base, Col, BaseOffset, bOnlyOneMip);
		}
		else if (baseFormat == EImageFormat::IF_RGBA_UBYTE)
		{
			check(BaseOffset + CHANNEL_COUNT <= 4);
			BufferLayerColourFormatInPlace< BLEND_FUNC, CLAMP, CHANNEL_COUNT, 4 >(Base, Col, BaseOffset, bOnlyOneMip);
		}
		else if (baseFormat == EImageFormat::IF_BGRA_UBYTE)
		{
			float temp = Col[0];
			Col[0] = Col[2];
			Col[2] = temp;
			BufferLayerColourFormatInPlace< BLEND_FUNC, CLAMP, CHANNEL_COUNT, 4>(Base, Col, BaseOffset, bOnlyOneMip);
		}

		else if (baseFormat == EImageFormat::IF_L_UBYTE)
		{
			check(BaseOffset + CHANNEL_COUNT <= 1);
			BufferLayerColourFormatInPlace< BLEND_FUNC, CLAMP, CHANNEL_COUNT, 1 >(Base, Col, BaseOffset, bOnlyOneMip);
		}
		else
		{
			checkf(false, TEXT("Unsupported format."));
		}
	}


	//---------------------------------------------------------------------------------------------
	template< unsigned (*BLEND_FUNC)(unsigned, unsigned), uint32 CHANNEL_COUNT >
	inline void BufferLayerColourInPlace(Image* Base, FVector4f Col, bool bOnlyOneMip, uint32 BaseOffset)
	{
		bool bIsClampNeeded = IsAnyComponentLargerThan1(Col);
		if (bIsClampNeeded)
		{
			BufferLayerColourInPlace<BLEND_FUNC, true, CHANNEL_COUNT>(Base, Col, bOnlyOneMip, BaseOffset);
		}
		else
		{
			BufferLayerColourInPlace<BLEND_FUNC, false, CHANNEL_COUNT>(Base, Col, bOnlyOneMip, BaseOffset);
		}
	}


	//---------------------------------------------------------------------------------------------
	template< 
		unsigned (*BLEND_FUNC_MASKED)(unsigned, unsigned, unsigned),
		unsigned (*BLEND_FUNC)(unsigned, unsigned), 
		bool CLAMP, 
		int32 CHANNEL_COUNT >
	inline void BufferLayerColourInPlace(Image* Base, const Image* Mask, FVector4f Col, bool bOnlyOneMip, uint32 BaseOffset)
	{
		check(Base->GetSizeX() == Mask->GetSizeX());
		check(Base->GetSizeY() == Mask->GetSizeY());

		EImageFormat baseFormat = Base->GetFormat();

		if (baseFormat == EImageFormat::IF_RGB_UBYTE)
		{
			check(BaseOffset + CHANNEL_COUNT <= 3);
			BufferLayerColourFormat< BLEND_FUNC_MASKED, BLEND_FUNC, CLAMP, CHANNEL_COUNT, 3 >(Base->GetData(), Base, Mask, Col, BaseOffset, bOnlyOneMip);
		}
		else if (baseFormat == EImageFormat::IF_RGBA_UBYTE)
		{
			check(BaseOffset + CHANNEL_COUNT <= 4);
			BufferLayerColourFormat< BLEND_FUNC_MASKED, BLEND_FUNC, CLAMP, CHANNEL_COUNT, 4 >(Base->GetData(), Base, Mask, Col, BaseOffset, bOnlyOneMip);
		}
		else if (baseFormat == EImageFormat::IF_BGRA_UBYTE)
		{
			check(BaseOffset + CHANNEL_COUNT <= 4);
			float temp = Col[0];
			Col[0] = Col[2];
			Col[2] = temp;
			BufferLayerColourFormat< BLEND_FUNC_MASKED, BLEND_FUNC, CLAMP, CHANNEL_COUNT, 4 >(Base->GetData(), Base, Mask, Col, BaseOffset, bOnlyOneMip);
		}
		else if (baseFormat == EImageFormat::IF_L_UBYTE)
		{
			check(BaseOffset + CHANNEL_COUNT <= 1);
			BufferLayerColourFormat< BLEND_FUNC_MASKED, BLEND_FUNC, CLAMP, CHANNEL_COUNT, 1 >(Base->GetData(), Base, Mask, Col, BaseOffset, bOnlyOneMip);
		}
		else
		{
			checkf(false, TEXT("Unsupported format."));
		}
	}


	//---------------------------------------------------------------------------------------------
	template< unsigned (*BLEND_FUNC_MASKED)(unsigned, unsigned, unsigned), unsigned (*BLEND_FUNC)(unsigned, unsigned), uint32 CHANNEL_COUNT >
	inline void BufferLayerColourInPlace(Image* Base, const Image* Mask, FVector4f Col, bool bOnlyOneMip, uint32 BaseOffset)
	{
		bool bIsClampNeeded = IsAnyComponentLargerThan1(Col);
		if (bIsClampNeeded)
		{
			BufferLayerColourInPlace<BLEND_FUNC_MASKED, BLEND_FUNC, true, CHANNEL_COUNT>(Base, Mask, Col, bOnlyOneMip, BaseOffset);
		}
		else
		{
			BufferLayerColourInPlace<BLEND_FUNC_MASKED, BLEND_FUNC, false, CHANNEL_COUNT>(Base, Mask, Col, bOnlyOneMip, BaseOffset);
		}
	}


	//---------------------------------------------------------------------------------------------
	//! Apply a blending function to an image with another image as blending layer
	//---------------------------------------------------------------------------------------------
	template< unsigned (*BLEND_FUNC)(unsigned,unsigned), bool CLAMP,
		// Number of total channels to actually process
		int32 CHANNELS_TO_BLEND,
		// Number of total channels in the base image
		int32 BASE_CHANNEL_STRIDE,
		// Number of total channels in the blend image
		int32 BLENDED_CHANNEL_STRIDE>

    inline void BufferLayerFormatInPlace(Image* pBase, const Image* pBlended,
		uint32 BaseChannelOffset,
		uint32 BlendedChannelOffset,
		bool bOnlyFirstLOD )
	{
		check(pBase->GetSizeX() == pBlended->GetSizeX());
		check(pBase->GetSizeY() == pBlended->GetSizeY());
		check(pBase->GetFormat() == pBlended->GetFormat());
		check(bOnlyFirstLOD || pBase->GetLODCount() <= pBlended->GetLODCount());

        uint8* pBaseBuf = pBase->GetData() + BaseChannelOffset;
        const uint8* pBlendedBuf = pBlended->GetData() + BlendedChannelOffset;

		// Generic implementation
		int32 PixelCount = 0;
		if (bOnlyFirstLOD)
		{
			PixelCount = pBase->GetSizeX() * pBase->GetSizeY();
		}
		else
		{
			PixelCount = pBase->CalculatePixelCount();
		}

		ParallelFor(PixelCount,
			[
				pBaseBuf, pBlendedBuf
			] (uint32 i)
			{
				for (int c = 0; c < CHANNELS_TO_BLEND; ++c)
				{
					unsigned base = pBaseBuf[BASE_CHANNEL_STRIDE * i + c];
					unsigned blended = pBlendedBuf[BLENDED_CHANNEL_STRIDE * i + c];
					unsigned result = BLEND_FUNC(base, blended);
					if (CLAMP)
					{
						pBaseBuf[BASE_CHANNEL_STRIDE * i + c] = (uint8)FMath::Min(255u, result);
					}
					else
					{
						pBaseBuf[BASE_CHANNEL_STRIDE * i + c] = (uint8)result;
					}
				}				
			});
	}
	
	//---------------------------------------------------------------------------------------------
	//! Apply a blending function to an image with another image as blending layer
	//---------------------------------------------------------------------------------------------
	template< unsigned (*BLEND_FUNC)(unsigned, unsigned), bool CLAMP,
		// Number of total channels to actually process
		int32 CHANNELS_TO_BLEND,
		// Number of total channels in the base image
		int32 BASE_CHANNEL_STRIDE,
		// Number of total channels in the blend image
		int32 BLENDED_CHANNEL_STRIDE>

	inline void BufferLayerFormat(uint8* pDestBuf, const Image* pBase, const Image* pBlended,
		bool bOnlyFirstLOD)
	{
		check(pBase->GetSizeX() == pBlended->GetSizeX());
		check(pBase->GetSizeY() == pBlended->GetSizeY());
		check(pBase->GetFormat() == pBlended->GetFormat());
		check(bOnlyFirstLOD || pBase->GetLODCount() <= pBlended->GetLODCount());

		const uint8* pBaseBuf = pBase->GetData();
		const uint8* pBlendedBuf = pBlended->GetData();

		// Generic implementation
		int32 PixelCount = 0;
		if (bOnlyFirstLOD)
		{
			PixelCount = pBase->GetSizeX() * pBase->GetSizeY();
		}
		else
		{
			PixelCount = pBase->CalculatePixelCount();
		}

		int32 UnblendedChannels = BASE_CHANNEL_STRIDE - CHANNELS_TO_BLEND;

		ParallelFor(PixelCount,
			[
				pBaseBuf, pBlendedBuf, pDestBuf, UnblendedChannels
			] (uint32 i)
			{
				for (int c = 0; c < CHANNELS_TO_BLEND; ++c)
				{
					unsigned base = pBaseBuf[BASE_CHANNEL_STRIDE * i + c];
					unsigned blended = pBlendedBuf[BLENDED_CHANNEL_STRIDE * i + c];
					unsigned result = BLEND_FUNC(base, blended);
					if (CLAMP)
					{
						pDestBuf[BASE_CHANNEL_STRIDE * i + c] = (uint8)FMath::Min(255u, result);
					}
					else
					{
						pDestBuf[BASE_CHANNEL_STRIDE * i + c] = (uint8)result;
					}
				}

				// Copy the unblended channels
				// \TODO: unnecessary when doing it in-place?
				for (int32 c = 0; c < UnblendedChannels; ++c)
				{
					pDestBuf[BASE_CHANNEL_STRIDE * i + CHANNELS_TO_BLEND + c] = pBaseBuf[BASE_CHANNEL_STRIDE * i + CHANNELS_TO_BLEND + c];
				}
			});
	}


	//---------------------------------------------------------------------------------------------
	//! Apply a blending function to an image with another image as blending layer
	//---------------------------------------------------------------------------------------------
	template< unsigned (*BLEND_FUNC)(unsigned, unsigned), bool CLAMP >
	inline void BufferLayer(Image* pResult, const Image* pBase, const Image* pBlended, bool bApplyToAlpha, bool bOnlyOneMip)
	{
		check(pResult->GetFormat() == pBase->GetFormat());
		check(pResult->GetSizeX() == pBase->GetSizeX());
		check(pResult->GetSizeY() == pBase->GetSizeY());
		check(bOnlyOneMip || pResult->GetLODCount() == pBase->GetLODCount());
		check(pBase->GetSizeX() == pBlended->GetSizeX());
		check(pBase->GetSizeY() == pBlended->GetSizeY());
		check(pBase->GetFormat() == pBlended->GetFormat());
		check(bOnlyOneMip || pResult->GetLODCount() <= pBlended->GetLODCount());

		EImageFormat baseFormat = pBase->GetFormat();
		uint8* pDestBuf = pResult->GetData();

		if (baseFormat == EImageFormat::IF_RGB_UBYTE)
		{
			BufferLayerFormat< BLEND_FUNC, CLAMP, 3, 3, 3 >(pDestBuf, pBase, pBlended, bOnlyOneMip);
		}
		else if (baseFormat == EImageFormat::IF_RGBA_UBYTE || baseFormat == EImageFormat::IF_BGRA_UBYTE)
		{
			if (bApplyToAlpha)
			{
				BufferLayerFormat< BLEND_FUNC, CLAMP, 4, 4, 4 >(pDestBuf, pBase, pBlended, bOnlyOneMip);
			}
			else
			{
				BufferLayerFormat< BLEND_FUNC, CLAMP, 3, 4, 4 >(pDestBuf, pBase, pBlended, bOnlyOneMip);
			}
		}
		else if (baseFormat == EImageFormat::IF_L_UBYTE)
		{
			BufferLayerFormat< BLEND_FUNC, CLAMP, 1, 1, 1 >(pDestBuf, pBase, pBlended, bOnlyOneMip);
		}
		else
		{
			checkf(false, TEXT("Unsupported format."));
		}
	}


	//---------------------------------------------------------------------------------------------
	template< unsigned (*BLEND_FUNC)(unsigned, unsigned), bool CLAMP, int32 CHANNEL_COUNT >
	inline void BufferLayerInPlace(Image* pBase, const Image* pBlended, bool bOnlyOneMip, uint32 BaseOffset, uint32 BlendedOffset)
	{
		check(pBase->GetSizeX() == pBlended->GetSizeX());
		check(pBase->GetSizeY() == pBlended->GetSizeY());
		check(pBase->GetFormat() == pBlended->GetFormat());

		EImageFormat baseFormat = pBase->GetFormat();

		if (baseFormat == EImageFormat::IF_RGB_UBYTE)
		{
			check(BaseOffset+CHANNEL_COUNT <= 3);
			check(BlendedOffset+CHANNEL_COUNT <= 3);
			BufferLayerFormatInPlace< BLEND_FUNC, CLAMP, CHANNEL_COUNT, 3, 3 >(pBase, pBlended, BaseOffset, BlendedOffset, bOnlyOneMip);
		}
		else if (baseFormat == EImageFormat::IF_RGBA_UBYTE || baseFormat == EImageFormat::IF_BGRA_UBYTE)
		{
			check(BaseOffset+CHANNEL_COUNT <= 4);
			check(BlendedOffset+CHANNEL_COUNT <= 4);
			BufferLayerFormatInPlace< BLEND_FUNC, CLAMP, CHANNEL_COUNT, 4, 4 >(pBase, pBlended, BaseOffset, BlendedOffset, bOnlyOneMip);
		}
		else if (baseFormat == EImageFormat::IF_L_UBYTE)
		{
			check(BaseOffset+CHANNEL_COUNT <= 1);
			BufferLayerFormatInPlace< BLEND_FUNC, CLAMP, CHANNEL_COUNT, 1, 1 >(pBase, pBlended, BaseOffset, BlendedOffset, bOnlyOneMip);
		}
		else
		{
			checkf(false, TEXT("Unsupported format."));
		}
	}


	//---------------------------------------------------------------------------------------------
	template< unsigned (*BLEND_FUNC_MASKED)(unsigned, unsigned, unsigned),
		unsigned (*BLEND_FUNC)(unsigned, unsigned),
		bool CLAMP,
		// Number of total channels to actually process
		int32 CHANNELS_TO_BLEND,
		// Number of total channels in the base image
		int32 BASE_CHANNEL_STRIDE,
		// Number of total channels in the blend image
		int32 BLENDED_CHANNEL_STRIDE>
	inline void BufferLayerFormat(uint8* pStartDestBuf,
		const Image* pBase,
		const Image* pMask,
		const Image* pBlend,
		uint32 BaseChannelOffset,
		uint32 BlendedChannelOffset,
		bool bOnlyFirstLOD)
	{
		check(pBase->GetSizeX() == pMask->GetSizeX() && pBase->GetSizeY() == pMask->GetSizeY());
		check(pBase->GetSizeX() == pBlend->GetSizeX() && pBase->GetSizeY() == pBlend->GetSizeY());
		check(bOnlyFirstLOD || pBase->GetLODCount() <= pMask->GetLODCount());
		check(bOnlyFirstLOD || pBase->GetLODCount() <= pBlend->GetLODCount());
		check(pBase->GetFormat() == pBlend->GetFormat());
		check(pMask->GetFormat() == EImageFormat::IF_L_UBYTE
			||
			pMask->GetFormat() == EImageFormat::IF_L_UBYTE_RLE);

		uint8* pDestBuf = pStartDestBuf;
		const uint8* pMaskBuf = pMask->GetData();
		const uint8* pBaseBuf = pBase->GetData() + BaseChannelOffset;
		const uint8* pBlendedBuf = pBlend->GetData() + BlendedChannelOffset;

		EImageFormat maskFormat = pMask->GetFormat();
		bool bIsMaskUncompressed = (maskFormat == EImageFormat::IF_L_UBYTE);
		int32 UnblendedChannels = BASE_CHANNEL_STRIDE - CHANNELS_TO_BLEND;

		// The base determines the number of lods to process.
		int32 LODCount = bOnlyFirstLOD ? 1 : pBase->GetLODCount();

		if (bIsMaskUncompressed)
		{
			int32 PixelCount = 0;
			if (bOnlyFirstLOD)
			{
				PixelCount = pBase->GetSizeX() * pBase->GetSizeY();
			}
			else
			{
				PixelCount = pBase->CalculatePixelCount();
			}

			ParallelFor(PixelCount,
				[
					pBaseBuf, pBlendedBuf, pMaskBuf, pDestBuf, UnblendedChannels
				] (uint32 i)
				{
					unsigned mask = pMaskBuf[i];
					for (int c = 0; c < CHANNELS_TO_BLEND; ++c)
					{
						uint32 base = pBaseBuf[BASE_CHANNEL_STRIDE * i + c];
						uint32 blended = pBlendedBuf[BLENDED_CHANNEL_STRIDE * i + c];
						uint32 result = BLEND_FUNC_MASKED(base, blended, mask);
						if (CLAMP)
						{
							pDestBuf[BASE_CHANNEL_STRIDE * i + c] = (uint8)FMath::Min(255u, result);
						}
						else
						{
							pDestBuf[BASE_CHANNEL_STRIDE * i + c] = (uint8)result;
						}
					}
					// Copy the unblended channels
					// \TODO: unnecessary when doing it in-place?
					for (int c = 0; c < UnblendedChannels; ++c)
					{
						pDestBuf[BASE_CHANNEL_STRIDE * i + CHANNELS_TO_BLEND + c] = pBaseBuf[BASE_CHANNEL_STRIDE * i + CHANNELS_TO_BLEND + c];
					}
				});
		}
		else if (maskFormat == EImageFormat::IF_L_UBYTE_RLE)
		{
			int rows = pBase->GetSizeY();
			int width = pBase->GetSizeX();

			for (int lod = 0; lod < LODCount; ++lod)
			{
				pMaskBuf += 4 + rows * sizeof(uint32);

				// \todo: See how to handle mask buf here
				//ParallelFor(rows,
				//	[
				//		pBaseBuf, pBlendedBuf, pMaskBuf, pDestBuf, width, applyToAlpha
				//	] (uint32 r)
				for (int r = 0; r < rows; ++r)
				{
					const uint8* pDestRowEnd = pDestBuf + width * BASE_CHANNEL_STRIDE;
					while (pDestBuf != pDestRowEnd)
					{
						// Decode header
						uint16 equal = *(const uint16*)pMaskBuf;
						pMaskBuf += 2;

						uint8 different = *pMaskBuf;
						++pMaskBuf;

						uint8 equalPixel = *pMaskBuf;
						++pMaskBuf;

						// Equal pixels
						check(pDestBuf + BASE_CHANNEL_STRIDE * equal <= pStartDestBuf + pBase->GetDataSize());
						if (equalPixel == 255)
						{
							for (int i = 0; i < equal; ++i)
							{
								for (int c = 0; c < CHANNELS_TO_BLEND; ++c)
								{
									uint32 base = pBaseBuf[BASE_CHANNEL_STRIDE * i + c];
									uint32 blended = pBlendedBuf[BLENDED_CHANNEL_STRIDE * i + c];
									uint32 result = BLEND_FUNC(base, blended);
									if (CLAMP)
									{
										pDestBuf[BASE_CHANNEL_STRIDE * i + c] = (uint8)FMath::Min(255u, result);
									}
									else
									{
										pDestBuf[BASE_CHANNEL_STRIDE * i + c] = (uint8)result;
									}
								}

								// Copy the unblended channels
								// \TODO: unnecessary when doing it in-place?
								for (int32 c = 0; c < UnblendedChannels; ++c)
								{
									pDestBuf[BASE_CHANNEL_STRIDE * i + CHANNELS_TO_BLEND + c] = pBaseBuf[BASE_CHANNEL_STRIDE * i + CHANNELS_TO_BLEND + c];
								}
							}
						}
						else if (equalPixel > 0)
						{
							for (int i = 0; i < equal; ++i)
							{
								for (int c = 0; c < CHANNELS_TO_BLEND; ++c)
								{
									unsigned base = pBaseBuf[BASE_CHANNEL_STRIDE * i + c];
									unsigned blended = pBlendedBuf[BLENDED_CHANNEL_STRIDE * i + c];
									unsigned result = BLEND_FUNC_MASKED(base, blended, equalPixel);
									if (CLAMP)
									{
										pDestBuf[BASE_CHANNEL_STRIDE * i + c] = (uint8)FMath::Min(255u, result);
									}
									else
									{
										pDestBuf[BASE_CHANNEL_STRIDE * i + c] = (uint8)result;
									}
								}

								// Copy the unblended channels
								// \TODO: unnecessary when doing it in-place?
								for (int32 c = 0; c < UnblendedChannels; ++c)
								{
									pDestBuf[BASE_CHANNEL_STRIDE * i + CHANNELS_TO_BLEND + c] = pBaseBuf[BASE_CHANNEL_STRIDE * i + CHANNELS_TO_BLEND + c];
								}
							}
						}
						else
						{
							// It could happen if xxxxxOnBase
							if (pDestBuf != pBaseBuf)
							{
								FMemory::Memmove(pDestBuf, pBaseBuf, BASE_CHANNEL_STRIDE * equal);
							}
						}
						pDestBuf += BASE_CHANNEL_STRIDE * equal;
						pBaseBuf += BASE_CHANNEL_STRIDE * equal;
						pBlendedBuf += BLENDED_CHANNEL_STRIDE * equal;

						// Different pixels
						check(pDestBuf + BASE_CHANNEL_STRIDE * different <= pStartDestBuf + pBase->GetDataSize());
						for (int i = 0; i < different; ++i)
						{
							for (int c = 0; c < CHANNELS_TO_BLEND; ++c)
							{
								unsigned mask = pMaskBuf[i];
								unsigned base = pBaseBuf[BASE_CHANNEL_STRIDE * i + c];
								unsigned blended = pBlendedBuf[BLENDED_CHANNEL_STRIDE * i + c];
								unsigned result = BLEND_FUNC_MASKED(base, blended, mask);
								if (CLAMP)
								{
									pDestBuf[BASE_CHANNEL_STRIDE * i + c] = (uint8)FMath::Min(255u, result);
								}
								else
								{
									pDestBuf[BASE_CHANNEL_STRIDE * i + c] = (uint8)result;
								}
							}

							// Copy the unblended channels
							// \TODO: unnecessary when doing it in-place?
							for (int32 c = 0; c < UnblendedChannels; ++c)
							{
								pDestBuf[BASE_CHANNEL_STRIDE * i + CHANNELS_TO_BLEND + c] = pBaseBuf[BASE_CHANNEL_STRIDE * i + CHANNELS_TO_BLEND + c];
							}
						}

						pDestBuf += BASE_CHANNEL_STRIDE * different;
						pBaseBuf += BASE_CHANNEL_STRIDE * different;
						pBlendedBuf += BLENDED_CHANNEL_STRIDE * different;
						pMaskBuf += different;
					}
				}

				rows = FMath::DivideAndRoundUp(rows, 2);
				width = FMath::DivideAndRoundUp(width, 2);
			}
		}
		else
		{
			checkf(false, TEXT("Unsupported mask format."));
		}
	}


	//---------------------------------------------------------------------------------------------
	template< unsigned (*BLEND_FUNC_MASKED)(unsigned, unsigned, unsigned),
		unsigned (*BLEND_FUNC)(unsigned, unsigned),
		bool CLAMP,
		// Number of total channels to actually process
		int32 CHANNELS_TO_BLEND,
		// Number of total channels in the base image
		int32 BASE_CHANNEL_STRIDE,
		// Number of total channels in the blend image
		int32 BLENDED_CHANNEL_STRIDE>
	inline void BufferLayerFormatEmbeddedMask(uint8* pStartDestBuf,
		const Image* pBase,
		const Image* pBlend,
		uint32 BaseChannelOffset,
		bool bOnlyFirstLOD)
	{
		check(pBase->GetSizeX() == pBlend->GetSizeX() && pBase->GetSizeY() == pBlend->GetSizeY());
		check(bOnlyFirstLOD || pBase->GetLODCount() <= pBlend->GetLODCount());

		// The base determines the number of lods to process.
		int32 LODCount = bOnlyFirstLOD ? 1 : pBase->GetLODCount();

		int32 PixelCount = 0;
		if (bOnlyFirstLOD)
		{
			PixelCount = pBase->GetSizeX() * pBase->GetSizeY();
		}
		else
		{
			PixelCount = pBase->CalculatePixelCount();
		}

		int32 UnblendedChannels = BASE_CHANNEL_STRIDE - CHANNELS_TO_BLEND;

		uint8* pDestBuf = pStartDestBuf;
		const uint8* pBaseBuf = pBase->GetData() + BaseChannelOffset;
		const uint8* pBlendedBuf = pBlend->GetData() + BaseChannelOffset;

		if (pBlend->GetFormat() == EImageFormat::IF_RGBA_UBYTE)
		{
			const uint8* pMaskBuf = pBlend->GetData() + 3;

			ParallelFor(PixelCount,
				[
					pBaseBuf, pBlendedBuf, pDestBuf, pMaskBuf, UnblendedChannels
				] (uint32 i)
				{
					uint32 mask = pMaskBuf[BLENDED_CHANNEL_STRIDE * i];
					for (int c = 0; c < CHANNELS_TO_BLEND; ++c)
					{
						uint32 base = pBaseBuf[BASE_CHANNEL_STRIDE * i + c];
						uint32 blended = pBlendedBuf[BLENDED_CHANNEL_STRIDE * i + c];
						uint32 result = BLEND_FUNC_MASKED(base, blended, mask);
						if (CLAMP)
						{
							pDestBuf[BASE_CHANNEL_STRIDE * i + c] = (uint8)FMath::Min(255u, result);
						}
						else
						{
							pDestBuf[BASE_CHANNEL_STRIDE * i + c] = (uint8)result;
						}
					}
					// Copy the unblended channels
					// \TODO: unnecessary when doing it in-place?
					for (int c = 0; c < UnblendedChannels; ++c)
					{
						pDestBuf[BASE_CHANNEL_STRIDE * i + CHANNELS_TO_BLEND + c] = pBaseBuf[BASE_CHANNEL_STRIDE * i + CHANNELS_TO_BLEND + c];
					}
				});
		}
		else
		{
			ParallelFor(PixelCount,
				[
					pBaseBuf, pBlendedBuf, pDestBuf, UnblendedChannels
				] (uint32 i)
				{
					for (int c = 0; c < CHANNELS_TO_BLEND; ++c)
					{
						uint32 base = pBaseBuf[BASE_CHANNEL_STRIDE * i + c];
						uint32 blended = pBlendedBuf[BLENDED_CHANNEL_STRIDE * i + c];
						uint32 result = BLEND_FUNC(base, blended);
						if (CLAMP)
						{
							pDestBuf[BASE_CHANNEL_STRIDE * i + c] = (uint8)FMath::Min(255u, result);
						}
						else
						{
							pDestBuf[BASE_CHANNEL_STRIDE * i + c] = (uint8)result;
						}
					}
					// Copy the unblended channels
					// \TODO: unnecessary when doing it in-place?
					for (int c = 0; c < UnblendedChannels; ++c)
					{
						pDestBuf[BASE_CHANNEL_STRIDE * i + CHANNELS_TO_BLEND + c] = pBaseBuf[BASE_CHANNEL_STRIDE * i + CHANNELS_TO_BLEND + c];
					}
				});

		}
	}


	//---------------------------------------------------------------------------------------------
	template< unsigned (*BLEND_FUNC)(unsigned,unsigned),
			  bool CLAMP,
			  int NC >
    inline void BufferLayerFormatStrideNoAlpha( uint8* pDestBuf,
		int stride,
		const Image* pMask,
		const Image* pBlend, int32 LODCount )
	{
        const uint8* pMaskBuf = pMask->GetData();
        const uint8* pBlendedBuf = pBlend->GetData();

		EImageFormat maskFormat = pMask->GetFormat();
        bool isUncompressed = ( maskFormat == EImageFormat::IF_L_UBYTE );

        if ( isUncompressed )
		{
			int rowCount = pBlend->GetSizeY();
			int pixelCount = pBlend->GetSizeX();
			for ( int j=0; j<rowCount; ++j )
			{
				for ( int i=0; i<pixelCount; ++i )
				{
					unsigned mask = *pMaskBuf;
					if (mask)
					{
						for ( int c=0; c<NC; ++c )
						{
							unsigned base = *pDestBuf;
							unsigned blended = *pBlendedBuf;
							unsigned result = BLEND_FUNC( base, blended );
							if ( CLAMP )
							{
								*pDestBuf = (uint8)FMath::Min( 255u, result );
							}
							else
							{
								*pDestBuf = (uint8)result;
							}
							++pDestBuf;
							++pBlendedBuf;
						}
					}
					else
					{
						pDestBuf+=NC;
						pBlendedBuf+=NC;
					}
					++pMaskBuf;
				}

				pDestBuf+=stride;
			}
		}
        else if ( maskFormat==EImageFormat::IF_L_UBIT_RLE )
		{
			int rows = pMask->GetSizeY();
			int width = pMask->GetSizeX();

            for (int lod=0;lod< LODCount; ++lod)
            {
                pMaskBuf += 4+rows*sizeof(uint32);

                for ( int r=0; r<rows; ++r )
                {
                    const uint8* pDestRowEnd = pDestBuf + width*NC;
                    while ( pDestBuf!=pDestRowEnd )
                    {
                        // Decode header
                        uint16 zeros = *(const uint16*)pMaskBuf;
                        pMaskBuf += 2;

                        uint16 ones = *(const uint16*)pMaskBuf;
                        pMaskBuf += 2;

                        // Skip
                        pDestBuf += zeros*NC;
                        pBlendedBuf += zeros*NC;

                        // Copy
                        FMemory::Memmove( pDestBuf, pBlendedBuf, ones*NC );

                        pDestBuf += NC*ones;
                        pBlendedBuf += NC*ones;
                    }

                    pDestBuf += stride;
                }

                rows = FMath::DivideAndRoundUp(rows,2);
                width = FMath::DivideAndRoundUp(width,2);
            }
		}
		else
		{
			checkf( false, TEXT("Unsupported mask format.") );
		}
	}


	//---------------------------------------------------------------------------------------------
	template< unsigned (*BLEND_FUNC_MASKED)(unsigned,unsigned,unsigned),
			  unsigned (*BLEND_FUNC)(unsigned,unsigned),
			  bool CLAMP >
    inline void BufferLayer( uint8* pDestBuf,
							 const Image* pBase,
							 const Image* pMask,
                             const Image* pBlend,
                             bool bApplyToAlpha,
							 bool bOnlyFirstLOD )
	{
		if ( pBase->GetFormat()==EImageFormat::IF_RGB_UBYTE )
		{
            BufferLayerFormat< BLEND_FUNC_MASKED, BLEND_FUNC, CLAMP, 3, 3, 3 >
                    ( pDestBuf, pBase, pMask, pBlend, 0, 0, bOnlyFirstLOD) ;
		}
        else if ( pBase->GetFormat()==EImageFormat::IF_RGBA_UBYTE || pBase->GetFormat()==EImageFormat::IF_BGRA_UBYTE )
		{
			if (bApplyToAlpha)
			{
				BufferLayerFormat< BLEND_FUNC_MASKED, BLEND_FUNC, CLAMP, 4, 4, 4 >
					(pDestBuf, pBase, pMask, pBlend, 0, 0, bOnlyFirstLOD);
			}
			else
			{
				BufferLayerFormat< BLEND_FUNC_MASKED, BLEND_FUNC, CLAMP, 3, 4, 4 >
					(pDestBuf, pBase, pMask, pBlend, 0, 0, bOnlyFirstLOD);
			}
		}
		else if ( pBase->GetFormat()==EImageFormat::IF_L_UBYTE )
		{
            BufferLayerFormat< BLEND_FUNC_MASKED, BLEND_FUNC, CLAMP, 1, 1, 1 >
                    ( pDestBuf, pBase, pMask, pBlend, 0, 0, bOnlyFirstLOD) ;
		}
        else
		{
			checkf( false, TEXT("Unsupported format.") );
		}
	}


	//---------------------------------------------------------------------------------------------
	template< unsigned (*BLEND_FUNC_MASKED)(unsigned, unsigned, unsigned),
		unsigned (*BLEND_FUNC)(unsigned, unsigned),
		bool CLAMP >
	inline void BufferLayerEmbeddedMask(uint8* pDestBuf,
		const Image* pBase,
		const Image* pBlend,
		bool bApplyToAlpha,
		bool bOnlyFirstLOD)
	{
		if (pBase->GetFormat() == EImageFormat::IF_RGB_UBYTE)
		{
			BufferLayerFormatEmbeddedMask< BLEND_FUNC_MASKED, BLEND_FUNC, CLAMP, 3, 3, 3 >
				(pDestBuf, pBase, pBlend, 0, bOnlyFirstLOD);
		}
		else if (pBase->GetFormat() == EImageFormat::IF_RGBA_UBYTE || pBase->GetFormat() == EImageFormat::IF_BGRA_UBYTE)
		{
			if (bApplyToAlpha)
			{
				BufferLayerFormatEmbeddedMask< BLEND_FUNC_MASKED, BLEND_FUNC, CLAMP, 4, 4, 4 >
					(pDestBuf, pBase, pBlend, 0, bOnlyFirstLOD);
			}
			else
			{
				BufferLayerFormatEmbeddedMask< BLEND_FUNC_MASKED, BLEND_FUNC, CLAMP, 3, 4, 4 >
					(pDestBuf, pBase, pBlend, 0, bOnlyFirstLOD);
			}
		}
		else
		{
			checkf(false, TEXT("Unsupported format."));
		}
	}


	//---------------------------------------------------------------------------------------------
	template< unsigned (*RGB_FUNC_MASKED)(unsigned, unsigned, unsigned),
		unsigned (*A_FUNC)(unsigned, unsigned),
		bool CLAMP >
	inline void BufferLayerComposite(
		Image* pBase,
		const Image* pBlend,
		bool bOnlyFirstLOD)
	{
		check(pBase->GetFormat() == EImageFormat::IF_RGBA_UBYTE);
		check(pBlend->GetFormat() == EImageFormat::IF_RGBA_UBYTE);
		check(pBase->GetSizeX() == pBlend->GetSizeX() && pBase->GetSizeY() == pBlend->GetSizeY());
		check(bOnlyFirstLOD || pBase->GetLODCount() <= pBlend->GetLODCount());

		uint8* pBaseBuf = pBase->GetData();
		const uint8* pBlendedBuf = pBlend->GetData();

		// The base determines the number of lods to process.
		int32 LODCount = bOnlyFirstLOD ? 1 : pBase->GetLODCount();

		int32 PixelCount = 0;
		if (bOnlyFirstLOD)
		{
			PixelCount = pBase->GetSizeX() * pBase->GetSizeY();
		}
		else
		{
			PixelCount = pBase->CalculatePixelCount();
		}

		ParallelFor(PixelCount,
			[
				pBaseBuf, pBlendedBuf
			] (uint32 i)
			{
				// TODO: Optimize this (SIMD?)
				uint32 mask = pBlendedBuf[4 * i + 3];

				// RGB
				for (int c = 0; c < 3; ++c)
				{
					uint32 base = pBaseBuf[4 * i + c];
					uint32 blended = pBlendedBuf[4 * i + c];
					uint32 result = RGB_FUNC_MASKED(base, blended, mask);
					if (CLAMP)
					{
						pBaseBuf[4 * i + c] = (uint8)FMath::Min(255u, result);
					}
					else
					{
						pBaseBuf[4 * i + c] = (uint8)result;
					}
				}

				// A
				{
					uint32 base = pBaseBuf[4 * i + 3];
					uint32 blended = pBlendedBuf[4 * i + 3];
					uint32 result = A_FUNC(base, blended);
					if (CLAMP)
					{
						pBaseBuf[4 * i + 3] = (uint8)FMath::Min(255u, result);
					}
					else
					{
						pBaseBuf[4 * i + 3] = (uint8)result;
					}
				}
			});
	}


	//---------------------------------------------------------------------------------------------
	template< unsigned (*BLEND_FUNC)(unsigned,unsigned),
			  bool CLAMP >
    inline void BufferLayerStrideNoAlpha( uint8* pDestBuf, int stride, const Image* pMask, const Image* pBlend, int32 LODCount )
	{
		if ( pBlend->GetFormat()==EImageFormat::IF_RGB_UBYTE )
		{
			BufferLayerFormatStrideNoAlpha< BLEND_FUNC, CLAMP, 3 >
					( pDestBuf, stride, pMask, pBlend, LODCount ) ;
		}
        else if ( pBlend->GetFormat()==EImageFormat::IF_RGBA_UBYTE || pBlend->GetFormat()==EImageFormat::IF_BGRA_UBYTE )
		{
			BufferLayerFormatStrideNoAlpha< BLEND_FUNC, CLAMP, 4 >
					( pDestBuf, stride, pMask, pBlend, LODCount) ;
		}
		else if ( pBlend->GetFormat()==EImageFormat::IF_L_UBYTE )
		{
			BufferLayerFormatStrideNoAlpha< BLEND_FUNC, CLAMP, 1 >
					( pDestBuf, stride, pMask, pBlend, LODCount) ;
		}
		else
		{
			checkf( false, TEXT("Unsupported format.") );
		}
	}


	//---------------------------------------------------------------------------------------------
	//! Apply a blending function to an image with another image as blending layer, on a subrect of
	//! the base image.
	//! \warning this method applies the blending function to the alpha channel too
	//! \warning this method uses the mask as a binary mask (>0)
	//---------------------------------------------------------------------------------------------
	template< unsigned (*BLEND_FUNC)(unsigned,unsigned),
			  bool CLAMP >
	inline void ImageLayerOnBaseNoAlpha( Image* pBase,
										 const Image* pMask,
										 const Image* pBlended,
										 const box< vec2<int> >& rect )
	{
		check( pBase->GetSizeX() >= rect.min[0]+rect.size[0] );
		check( pBase->GetSizeY() >= rect.min[1]+rect.size[1] );
		check( pMask->GetSizeX() == pBlended->GetSizeX() );
		check( pMask->GetSizeY() == pBlended->GetSizeY() );
		check( pBase->GetFormat() == pBlended->GetFormat() );
		check( pMask->GetFormat() == EImageFormat::IF_L_UBYTE
						||
						pMask->GetFormat() == EImageFormat::IF_L_UBYTE_RLE
						||
						pMask->GetFormat() == EImageFormat::IF_L_UBIT_RLE );
        check( pBase->GetLODCount() <= pMask->GetLODCount() );
        check( pBase->GetLODCount() <= pBlended->GetLODCount() );

		int pixelSize = GetImageFormatData( pBase->GetFormat() ).m_bytesPerBlock;

		int start = ( pBase->GetSizeX()*rect.min[1]+rect.min[0] ) * pixelSize;
		int stride = ( pBase->GetSizeX() - rect.size[0] ) * pixelSize;
		BufferLayerStrideNoAlpha<BLEND_FUNC,CLAMP>( pBase->GetData()+start, stride, pMask, pBlended, pBase->GetLODCount());
	}


	//---------------------------------------------------------------------------------------------
	// \TODO: Unused?
	template< int NC >
    inline void BufferTableColourFormat( uint8* pStartDestBuf,
										 const Image* pBase,
										 const Image* pMask,
                                         const uint8* pTable0,
                                         const uint8* pTable1,
                                         const uint8* pTable2 )
	{
		uint8* pDestBuf = pStartDestBuf;
        const uint8* pMaskBuf = pMask->GetData();
        const uint8* pBaseBuf = pBase->GetData();
		EImageFormat maskFormat = pMask->GetFormat();

        bool isUncompressed = ( maskFormat == EImageFormat::IF_L_UBYTE );

		// The base determines the lods to process.
		int32 LODCount = pBase->GetLODCount();

        if ( isUncompressed )
		{
			int pixelCount = (int)pBase->CalculatePixelCount();
			for ( int i=0; i<pixelCount; ++i )
			{
				unsigned mask = pMaskBuf[i];

				if (mask)
				{
					if (mask<255)
					{
                        pDestBuf[NC*i+0] = (uint8)( ( ( ( 255 - mask ) * pBaseBuf[NC*i+0] )
                                             + ( mask * pTable0[ pBaseBuf[NC*i+0] ] ) ) >> 8 );
                        pDestBuf[NC*i+1] = (uint8)( ( ( ( 255 - mask ) * pBaseBuf[NC*i+1] )
                                             + ( mask * pTable1[ pBaseBuf[NC*i+1] ] ) ) >> 8 );
                        pDestBuf[NC*i+2] = (uint8)( ( ( ( 255 - mask ) * pBaseBuf[NC*i+2] )
                                             + ( mask * pTable2[ pBaseBuf[NC*i+2] ] ) ) >> 8 );
					}
					else
					{
						pDestBuf[NC*i+0] = pTable0[ pBaseBuf[NC*i+0] ];
						pDestBuf[NC*i+1] = pTable1[ pBaseBuf[NC*i+1] ];
						pDestBuf[NC*i+2] = pTable2[ pBaseBuf[NC*i+2] ];
					}
				}
				else
				{
					pDestBuf[NC*i+0] = pBaseBuf[NC*i+0];
					pDestBuf[NC*i+1] = pBaseBuf[NC*i+1];
					pDestBuf[NC*i+2] = pBaseBuf[NC*i+2];
				}

                constexpr bool isNC4 = (NC==4);
                if ( isNC4 )
				{
					pDestBuf[NC*i+3] = pBaseBuf[NC*i+3];
				}
			}
		}
		else if ( maskFormat==EImageFormat::IF_L_UBYTE_RLE )
		{
			int rows = pBase->GetSizeY();
			int width = pBase->GetSizeX();

            for (int32 lod=0;lod< LODCount; ++lod)
            {
                pMaskBuf += 4+rows*sizeof(uint32);

                for ( int r=0; r<rows; ++r )
                {
                    const uint8* pDestRowEnd = pDestBuf + width*NC;
                    while ( pDestBuf!=pDestRowEnd )
                    {
                        // Decode header
                        uint16 equal = *(const uint16*)pMaskBuf;
                        pMaskBuf += 2;

                        uint8 different = *pMaskBuf;
                        ++pMaskBuf;

                        unsigned equalPixel = *pMaskBuf;
                        ++pMaskBuf;

                        // Equal pixels
						check(pDestBuf + NC * equal <= pStartDestBuf + pBase->GetDataSize());
                        if ( equalPixel==255 )
                        {
                            for ( int i=0; i<equal; ++i )
                            {
                                pDestBuf[NC*i+0] = pTable0[ pBaseBuf[NC*i+0] ];
                                pDestBuf[NC*i+1] = pTable1[ pBaseBuf[NC*i+1] ];
                                pDestBuf[NC*i+2] = pTable2[ pBaseBuf[NC*i+2] ];

                                constexpr bool isNC4 = (NC==4);
                                if ( isNC4 )
                                {
                                    pDestBuf[NC*i+3] = pBaseBuf[NC*i+3];
                                }
                            }
                        }
                        else if ( equalPixel>0 )
                        {
                            for ( int i=0; i<equal; ++i )
                            {
                                pDestBuf[NC*i+0] = (uint8)( ( ( ( 255 - equalPixel ) * pBaseBuf[NC*i+0] )
                                                     + ( equalPixel * pTable0[ pBaseBuf[NC*i+0] ] ) ) >> 8 );
                                pDestBuf[NC*i+1] = (uint8)( ( ( ( 255 - equalPixel ) * pBaseBuf[NC*i+1] )
                                                     + ( equalPixel * pTable1[ pBaseBuf[NC*i+1] ] ) ) >> 8 );
                                pDestBuf[NC*i+2] = (uint8)( ( ( ( 255 - equalPixel ) * pBaseBuf[NC*i+2] )
                                                     + ( equalPixel * pTable2[ pBaseBuf[NC*i+2] ] ) ) >> 8 );

                                constexpr bool isNC4 = (NC==4);
                                if ( isNC4 )
                                {
                                    pDestBuf[NC*i+3] = pBaseBuf[NC*i+3];
                                }
                            }
                        }
                        else
                        {
                            // It could happen if xxxxxOnBase
                            if (pDestBuf!=pBaseBuf)
                            {
								FMemory::Memmove( pDestBuf, pBaseBuf, NC*equal );
                            }
                        }
                        pDestBuf += NC*equal;
                        pBaseBuf += NC*equal;

                        // Different pixels
						check(pDestBuf + NC * different <= pStartDestBuf + pBase->GetDataSize());
                        for ( int i=0; i<different; ++i )
                        {
                            unsigned mask = pMaskBuf[i];
                            pDestBuf[NC*i+0] = (uint8)( ( ( ( 255 - mask ) * pBaseBuf[NC*i+0] )
                                                 + ( mask * pTable0[ pBaseBuf[NC*i+0] ] ) ) >> 8 );
                            pDestBuf[NC*i+1] = (uint8)( ( ( ( 255 - mask ) * pBaseBuf[NC*i+1] )
                                                 + ( mask * pTable1[ pBaseBuf[NC*i+1] ] ) ) >> 8 );
                            pDestBuf[NC*i+2] = (uint8)( ( ( ( 255 - mask ) * pBaseBuf[NC*i+2] )
                                                 + ( mask * pTable2[ pBaseBuf[NC*i+2] ] ) ) >> 8 );

                            constexpr bool isNC4 = (NC==4);
                            if ( isNC4 )
                            {
                                pDestBuf[NC*i+3] = pBaseBuf[NC*i+3];
                            }
                        }

                        pDestBuf += NC*different;
                        pBaseBuf += NC*different;
                        pMaskBuf += different;
                    }
                }

                rows = FMath::DivideAndRoundUp(rows,2);
                width = FMath::DivideAndRoundUp(width,2);
            }
		}
		else
		{
			checkf( false, TEXT("Unsupported mask format.") );
		}
	}


	//---------------------------------------------------------------------------------------------
	// \TODO: Unused?
    inline void ImageTable( Image* pResult,
                                const Image* pBase,
								const Image* pMask,
                                const uint8* pTable0,
                                const uint8* pTable1,
                                const uint8* pTable2
								)
	{
        check( pResult->GetFormat() == pBase->GetFormat() );
        check( pResult->GetSizeX() == pBase->GetSizeX() );
        check( pResult->GetSizeY() == pBase->GetSizeY() );
        check( pResult->GetLODCount() == pBase->GetLODCount() );
        check( pBase->GetSizeX() == pMask->GetSizeX() );
		check( pBase->GetSizeY() == pMask->GetSizeY() );
		check( pMask->GetFormat() == EImageFormat::IF_L_UBYTE
						||
						pMask->GetFormat() == EImageFormat::IF_L_UBYTE_RLE );
        check( pBase->GetLODCount() <= pMask->GetLODCount() );

		EImageFormat baseFormat = pBase->GetFormat();
		if ( baseFormat==EImageFormat::IF_RGB_UBYTE )
		{
            BufferTableColourFormat<3>( pResult->GetData(), pBase, pMask, pTable0, pTable1, pTable2 );
		}
        else if ( baseFormat==EImageFormat::IF_RGBA_UBYTE )
        {
            BufferTableColourFormat<4>( pResult->GetData(), pBase, pMask, pTable0, pTable1, pTable2 );
        }
        else if ( baseFormat==EImageFormat::IF_BGRA_UBYTE )
        {
            BufferTableColourFormat<4>( pResult->GetData(), pBase, pMask, pTable2, pTable1, pTable0 );
        }
        else
		{
			checkf( false, TEXT("Unsupported format.") );
		}
	}


	template<size_t NC>
	inline uint32 PackPixel(const uint8* pPixel)
	{
		static_assert(NC > 0 && NC <= 4);

		uint32 PixelPack = 0;

		// The compiler should be able to optimize this given that NC is a constant expression
		FMemory::Memcpy( &PixelPack, pPixel, NC );

		return PixelPack;
	}

	template<size_t NC>
	inline void UnpackPixel(uint8* const pPixel, uint32 pixelData )
	{
		static_assert(NC > 0 && NC <= 4);

		// The compiler should be able to optimize this given that NC is a constant expression
		FMemory::Memcpy( pPixel, &pixelData, NC );
	}

	template< 
			uint32 (*BLEND_FUNC)(uint32, uint32), 
			size_t NC >
	inline void BufferLayerCombineColour(uint8* pDestBuf, const Image* pBase, vec3<float> col)
	{
		//static_assert(NC > 0 && NC <= 4);

		const uint8* pBaseBuf = pBase->GetData();

		uint32 top = 
			(uint32)(255.0f * col[0]) << 0 |
			(uint32)(255.0f * col[1]) << 8 |
			(uint32)(255.0f * col[2]) << 16;

		int32 pixelCount = pBase->CalculatePixelCount();

		for (int i = 0; i < pixelCount; ++i)
		{
			uint32 base = PackPixel<NC>(&pBaseBuf[NC * i]);

			uint32 result = BLEND_FUNC(base, top);

			UnpackPixel<NC>(&pDestBuf[NC * i], result);
		}
	}

	template< 
			uint32 (*BLEND_FUNC)(uint32, uint32), 
			size_t NC >
	inline void BufferLayerCombine(uint8* pDestBuf, const Image* pBase, const Image* pBlended, bool bOnlyFirstLOD)
	{
		//static_assert(NC > 0 && NC <= 4);

		check(pBase->GetSizeX() == pBlended->GetSizeX());
		check(pBase->GetSizeY() == pBlended->GetSizeY());
		check(pBase->GetFormat() == pBlended->GetFormat());

		const uint8* pBaseBuf = pBase->GetData();
		const uint8* pBlendedBuf = pBlended->GetData();

		int32 PixelCount = 0;
		if (bOnlyFirstLOD)
		{
			PixelCount = pBase->GetSizeX() * pBase->GetSizeX();
		}
		else
		{
			PixelCount = pBase->CalculatePixelCount();
		}		

		for (int i = 0; i < PixelCount; ++i)
		{
			const uint32 base = PackPixel<NC>(&pBaseBuf[NC * i]);
			const uint32 blend = PackPixel<NC>(&pBlendedBuf[NC * i]);

			const uint32 result = BLEND_FUNC(base, blend);

			UnpackPixel<NC>(&pDestBuf[NC * i], result);
		}
	}

	template< 
			uint32 (*BLEND_FUNC_MASKED)(uint32, uint32, uint32), 
			size_t NC >
	inline void BufferLayerCombine(uint8* pDestBuf, const Image* pBase, const Image* pMask, const Image* pBlended, bool bOnlyFirstLOD)
	{
		//static_assert(NC > 0 && NC <= 4);

		check(pBase->GetSizeX() == pBlended->GetSizeX());
		check(pBase->GetSizeY() == pBlended->GetSizeY());
		check(pBase->GetSizeX() == pMask->GetSizeX());
		check(pBase->GetSizeY() == pMask->GetSizeY());
		check(pBase->GetFormat() == pBlended->GetFormat());

		const uint8* pBaseBuf = pBase->GetData();
		const uint8* pMaskBuf = pMask->GetData();
		const uint8* pBlendedBuf = pBlended->GetData();

		int32 PixelCount = 0;
		if (bOnlyFirstLOD)
		{
			PixelCount = pBase->GetSizeX() * pBase->GetSizeX();
		}
		else
		{
			PixelCount = pBase->CalculatePixelCount();
		}

		for (int i = 0; i < PixelCount; ++i)
		{
			const uint32 base = PackPixel<NC>(&pBaseBuf[NC * i]);
			const uint32 blend = PackPixel<NC>(&pBlendedBuf[NC * i]);
			const uint32 mask = PackPixel<1>(&pMaskBuf[i]);

			const uint32 result = BLEND_FUNC_MASKED(base, blend, mask);

			UnpackPixel<NC>(&pDestBuf[NC * i], result);
		}
	}

	template< 
			uint32 (*BLEND_FUNC_MASKED)(uint32, uint32, uint32), 
			size_t NC >
	inline void BufferLayerCombineColour(uint8* pDestBuf, const Image* pBase, const Image* pMask, vec3<float> col)
	{
		//static_assert(NC > 0 && NC <= 4);

		check(pBase->GetSizeX() == pMask->GetSizeX());
		check(pBase->GetSizeY() == pMask->GetSizeY());


		const uint8* pBaseBuf = pBase->GetData();
		const uint8* pMaskBuf = pMask->GetData();

		uint32 top = 
			(uint32)(255.0f * col[0]) << 0 |
			(uint32)(255.0f * col[1]) << 8 |
			(uint32)(255.0f * col[2]) << 16;


		int32 pixelCount = pBase->CalculatePixelCount();

		for (int i = 0; i < pixelCount; ++i)
		{
			const uint32 base = PackPixel<NC>(&pBaseBuf[NC * i]);
			const uint32 mask = PackPixel<1>(&pMaskBuf[i]);

			const uint32 result = BLEND_FUNC_MASKED(base, top, mask);

			UnpackPixel<NC>(&pDestBuf[NC * i], result);
		}
	}

	template< uint32 (*BLEND_FUNC)(uint32, uint32) >
	inline void ImageLayerCombine(Image* pResult, const Image* pBase, const Image* pBlended, bool bOnlyFirstLOD)
	{
		check(pResult->GetFormat() == pBase->GetFormat());
		check(pResult->GetSizeX() == pBase->GetSizeX());
		check(pResult->GetSizeY() == pBase->GetSizeY());
		check(bOnlyFirstLOD || pResult->GetLODCount() == pBase->GetLODCount());
		check(pBase->GetSizeX() == pBlended->GetSizeX());
		check(pBase->GetSizeY() == pBlended->GetSizeY());
		check(pBase->GetFormat() == pBlended->GetFormat());
		check(bOnlyFirstLOD || pResult->GetLODCount() <= pBlended->GetLODCount());

		const EImageFormat baseFormat = pBase->GetFormat();

		uint8* pDestBuf = pResult->GetData();
		if (baseFormat == EImageFormat::IF_L_UBYTE)
		{
			BufferLayerCombine<BLEND_FUNC, 1 >(pDestBuf, pBase, pBlended, bOnlyFirstLOD);
		}
		else if (baseFormat == EImageFormat::IF_RGB_UBYTE)
		{
			BufferLayerCombine< BLEND_FUNC, 3 >(pDestBuf, pBase, pBlended, bOnlyFirstLOD);
		}
		else if (baseFormat == EImageFormat::IF_RGBA_UBYTE || baseFormat == EImageFormat::IF_BGRA_UBYTE)
		{
			// \todo: pass swizzle template argument if BGRA_UBYTE, not yet supported.
			BufferLayerCombine< BLEND_FUNC, 4 >(pDestBuf, pBase, pBlended, bOnlyFirstLOD);
		}
		else
		{
			checkf(false, TEXT("Unsupported format."));
		}
	}

	template< 
		uint32 (*BLEND_FUNC)(uint32, uint32), 
		uint32 (*BLEND_FUNC_MASKED)(uint32, uint32, uint32) >
	inline void ImageLayerCombine(Image* pResult, const Image* pBase, const Image* pMask, const Image* pBlended, bool bOnlyFirstLOD)
	{
		check(pResult->GetFormat() == pBase->GetFormat());
		check(pResult->GetSizeX() == pBase->GetSizeX());
		check(pResult->GetSizeY() == pBase->GetSizeY());
		check(bOnlyFirstLOD || pResult->GetLODCount() == pBase->GetLODCount());
		check(pBase->GetSizeX() == pBlended->GetSizeX());
		check(pBase->GetSizeY() == pBlended->GetSizeY());
		check(pBase->GetFormat() == pBlended->GetFormat());
		check(bOnlyFirstLOD || pResult->GetLODCount() <= pBlended->GetLODCount());

		const EImageFormat baseFormat = pBase->GetFormat();

		uint8* pDestBuf = pResult->GetData();

		if (pMask->GetFormat() != EImageFormat::IF_L_UBYTE)
		{
			checkf(false, TEXT("Unsupported mask format."));

			BufferLayerCombine< BLEND_FUNC, 1 >(pDestBuf, pBase, pBlended, bOnlyFirstLOD);
		}

		if (baseFormat == EImageFormat::IF_L_UBYTE)
		{
			BufferLayerCombine< BLEND_FUNC_MASKED, 1 >(pDestBuf, pBase, pMask, pBlended, bOnlyFirstLOD);
		}
		else if (baseFormat == EImageFormat::IF_RGB_UBYTE)
		{
			BufferLayerCombine< BLEND_FUNC_MASKED, 3 >(pDestBuf, pBase, pMask, pBlended, bOnlyFirstLOD);
		}
		else if (baseFormat == EImageFormat::IF_RGBA_UBYTE || baseFormat == EImageFormat::IF_BGRA_UBYTE)
		{
			// \todo: pass swizzle template argument if BGRA_UBYTE, not yet supported.
			BufferLayerCombine< BLEND_FUNC_MASKED, 4 >(pDestBuf, pBase, pMask, pBlended, bOnlyFirstLOD);
		}
		else
		{
			checkf(false, TEXT("Unsupported format."));
		}
	}

	template< uint32 (*BLEND_FUNC)(uint32, uint32)>
	inline void ImageLayerCombineColour(Image* pResult, const Image* pBase, vec3<float> col)
	{
		check(pResult->GetFormat() == pBase->GetFormat());
		check(pResult->GetSizeX() == pBase->GetSizeX());
		check(pResult->GetSizeY() == pBase->GetSizeY());
		check(pResult->GetLODCount() == pBase->GetLODCount());

		const EImageFormat baseFormat = pBase->GetFormat();

		uint8* pDestBuf = pResult->GetData();

		if (baseFormat == EImageFormat::IF_L_UBYTE)
		{
			BufferLayerCombineColour< BLEND_FUNC, 1 >(pDestBuf, pBase, col);
		}
		else if (baseFormat == EImageFormat::IF_RGB_UBYTE)
		{
			BufferLayerCombineColour< BLEND_FUNC, 3 >(pDestBuf, pBase, col);
		}
		else if (baseFormat == EImageFormat::IF_RGBA_UBYTE || baseFormat == EImageFormat::IF_BGRA_UBYTE)
		{
			// \todo: pass swizzle template argument if BGRA_UBYTE, not yet supported.
 			BufferLayerCombineColour< BLEND_FUNC, 4 >(pDestBuf, pBase, col);
		}
		else
		{
			checkf(false, TEXT("Unsupported format."));
		}
	}

	template< 
		uint32 (*BLEND_FUNC)(uint32, uint32),
		uint32 (*BLEND_FUNC_MASKED)(uint32, uint32, uint32) >
	inline void ImageLayerCombineColour(Image* pResult, const Image* pBase, const Image* pMask, vec3<float> col)
	{
		check(pResult->GetFormat() == pBase->GetFormat());
		check(pResult->GetSizeX() == pBase->GetSizeX());
		check(pResult->GetSizeY() == pBase->GetSizeY());
		check(pResult->GetLODCount() == pBase->GetLODCount());

		const EImageFormat baseFormat = pBase->GetFormat();

		uint8* pDestBuf = pResult->GetData();

		if (pMask->GetFormat() != EImageFormat::IF_L_UBYTE)
		{
			checkf(false, TEXT("Unsupported mask format."));

			BufferLayerCombineColour< BLEND_FUNC, 1 >(pDestBuf, pBase, col);
		}

		if (baseFormat == EImageFormat::IF_L_UBYTE)
		{
			BufferLayerCombineColour< BLEND_FUNC_MASKED, 1 >(pDestBuf, pBase, pMask, col);
		}
		else if (baseFormat == EImageFormat::IF_RGB_UBYTE)
		{
			BufferLayerCombineColour< BLEND_FUNC_MASKED, 3 >(pDestBuf, pBase, pMask, col);
		}
		else if (baseFormat == EImageFormat::IF_RGBA_UBYTE || baseFormat == EImageFormat::IF_BGRA_UBYTE)
		{
			// \todo: pass swizzle template argument if BGRA_UBYTE, not yet supported.
 			BufferLayerCombineColour< BLEND_FUNC_MASKED, 4 >(pDestBuf, pBase, pMask, col);
		}
		else
		{
			checkf(false, TEXT("Unsupported format."));
		}
	}

	template< size_t NCBase, size_t NCBlend, class ImageCombineFn >
	inline void BufferLayerCombineFunctor(uint8* pDestBuf, const Image* pBase, const Image* pBlended, ImageCombineFn&& ImageCombine)
	{
		//static_assert(NCBase > 0 && NCBase <= 4);
		//static_assert(NCBlend > 0 && NCBlend <= 4);

		check(pBase->GetSizeX() == pBlended->GetSizeX());
		check(pBase->GetSizeY() == pBlended->GetSizeY());
		check(pBase->GetLODCount() <= pBlended->GetLODCount());

		const uint8* pBaseBuf = pBase->GetData();
		const uint8* pBlendedBuf = pBlended->GetData();

		int32 pixelCount = pBase->CalculatePixelCount();

		for (int i = 0; i < pixelCount; ++i)
		{
			const uint32 base = PackPixel<NCBase>(&pBaseBuf[NCBase * i]);
			const uint32 blend = PackPixel<NCBlend>(&pBlendedBuf[NCBlend * i]);

			const uint32 result = ImageCombine(base, blend);

			UnpackPixel<NCBase>(&pDestBuf[NCBase* i], result);
		}
	}
	// Same functionality as above, in this case we use a functor which allows to pass user data. 
	template< class ImageCombineFn >
	inline void ImageLayerCombineFunctor(Image* pResult, const Image* pBase, const Image* pBlended, ImageCombineFn&& ImageCombine)
	{
		check(pResult->GetFormat() == pBase->GetFormat());
		check(pResult->GetSizeX() == pBase->GetSizeX());
		check(pResult->GetSizeY() == pBase->GetSizeY());
		check(pResult->GetLODCount() == pBase->GetLODCount());
		check(pBase->GetSizeX() == pBlended->GetSizeX());
		check(pBase->GetSizeY() == pBlended->GetSizeY());
		check(pResult->GetLODCount() <= pBlended->GetLODCount());

		const EImageFormat baseFormat = pBase->GetFormat();
		const EImageFormat blendFormat = pBlended->GetFormat();

		uint8* pDestBuf = pResult->GetData();
		if (baseFormat == EImageFormat::IF_L_UBYTE )
		{
			if (blendFormat == EImageFormat::IF_L_UBYTE )
			{
				BufferLayerCombineFunctor< 1, 1 >(pDestBuf, pBase, pBlended, Forward<ImageCombineFn>(ImageCombine));
			}
			else if (blendFormat == EImageFormat::IF_RGB_UBYTE )
			{
				BufferLayerCombineFunctor< 1, 3 >(pDestBuf, pBase, pBlended, Forward<ImageCombineFn>(ImageCombine));
			}
			else if (blendFormat == EImageFormat::IF_RGBA_UBYTE || blendFormat == EImageFormat::IF_BGRA_UBYTE )
			{
				BufferLayerCombineFunctor< 1, 4 >(pDestBuf, pBase, pBlended, Forward<ImageCombineFn>(ImageCombine));
			}
			else
			{
				checkf(false, TEXT("Unsupported format."));
			}
		}
		else if (baseFormat == EImageFormat::IF_RGB_UBYTE)
		{
			if (blendFormat == EImageFormat::IF_L_UBYTE )
			{
				BufferLayerCombineFunctor< 3, 1 >(pDestBuf, pBase, pBlended, Forward<ImageCombineFn>(ImageCombine));
			}
			else if (blendFormat == EImageFormat::IF_RGB_UBYTE )
			{
				BufferLayerCombineFunctor< 3, 3 >(pDestBuf, pBase, pBlended, Forward<ImageCombineFn>(ImageCombine));
			}
			else if (blendFormat == EImageFormat::IF_RGBA_UBYTE || blendFormat == EImageFormat::IF_BGRA_UBYTE )
			{
				BufferLayerCombineFunctor< 3, 4 >(pDestBuf, pBase, pBlended, Forward<ImageCombineFn>(ImageCombine));
			}
			else
			{
				checkf(false, TEXT("Unsupported format."));
			}
		}
		else if (baseFormat == EImageFormat::IF_RGBA_UBYTE || baseFormat == EImageFormat::IF_BGRA_UBYTE)
		{
			if (blendFormat == EImageFormat::IF_L_UBYTE )
			{
				BufferLayerCombineFunctor< 4, 1 >(pDestBuf, pBase, pBlended, Forward<ImageCombineFn>(ImageCombine));
			}
			else if (blendFormat == EImageFormat::IF_RGB_UBYTE )
			{
				BufferLayerCombineFunctor< 4, 3 >(pDestBuf, pBase, pBlended, Forward<ImageCombineFn>(ImageCombine));
			}
			else if (blendFormat == EImageFormat::IF_RGBA_UBYTE || blendFormat == EImageFormat::IF_BGRA_UBYTE )
			{
				BufferLayerCombineFunctor< 4, 4 >(pDestBuf, pBase, pBlended, Forward<ImageCombineFn>(ImageCombine));
			}
			else
			{
				checkf(false, TEXT("Unsupported format."));
			}
		}
		else
		{
			checkf(false, TEXT("Unsupported format."));
		}
	}

}

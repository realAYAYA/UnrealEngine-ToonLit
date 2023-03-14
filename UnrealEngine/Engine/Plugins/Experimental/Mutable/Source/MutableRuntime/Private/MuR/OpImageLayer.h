// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/ImageRLE.h"
#include "Async/ParallelFor.h"
#include "Templates/UnrealTemplate.h"

namespace mu
{


	//---------------------------------------------------------------------------------------------
	//! Apply a blending function to an image with a colour source
	//---------------------------------------------------------------------------------------------
	template< unsigned (*BLEND_FUNC)(unsigned,unsigned),
			  bool CLAMP >
    inline void ImageLayerColour( Image* pResult, const Image* pBase, vec3<float> col )
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


	//---------------------------------------------------------------------------------------------
	template< unsigned (*BLEND_FUNC_MASKED)(unsigned,unsigned,unsigned),
			  unsigned (*BLEND_FUNC)(unsigned,unsigned),
			  bool CLAMP,
			  int NC >
    inline void BufferLayerColourFormat( uint8* pStartDestBuf,
								   const Image* pBase,
								   const Image* pMask,
								   vec3<float> col )
	{
		unsigned top[3];
		top[0] = (unsigned)(255 * col[0]);
		top[1] = (unsigned)(255 * col[1]);
		top[2] = (unsigned)(255 * col[2]);

		uint8* pDestBuf = pStartDestBuf;
        const uint8* pMaskBuf = pMask->GetData();
        const uint8* pBaseBuf = pBase->GetData();
		EImageFormat maskFormat = pMask->GetFormat();

		int32 LODCount = pBase->GetLODCount();

        bool isUncompressed = ( maskFormat == EImageFormat::IF_L_UBYTE );

		constexpr int32 NumColorChannels = FMath::Min(NC,3);
        if ( isUncompressed )
        {
            int32 pixelCount = pBase->CalculatePixelCount();
            for ( int i=0; i<pixelCount; ++i )
            {
                unsigned mask = pMaskBuf[i];
                for ( int32 c=0; c<NumColorChannels; ++c )
                {
                    unsigned base = pBaseBuf[NC*i+c];
                    unsigned result = BLEND_FUNC_MASKED( base, top[c], mask );
                    if ( CLAMP )
                    {
                        pDestBuf[NC*i+c] = (uint8)FMath::Min( 255u, result );
                    }
                    else
                    {
                        pDestBuf[NC*i+c] = (uint8)result;
                    }
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

			// debug test
			ImagePtr TempDecodedMask = new Image(pMask->GetSizeX(), pMask->GetSizeY(), pMask->GetLODCount(), EImageFormat::IF_L_UBYTE);
			UncompressRLE_L(pMask, TempDecodedMask.get());

            for (int32 lod=0;lod<LODCount; ++lod)
            {
				// Skip mip size and row sizes.
                pMaskBuf += sizeof(uint32) +rows*sizeof(uint32);

                for ( int r=0; r<rows; ++r )
                {
                    const uint8* pDestRowEnd = pDestBuf + width*NC;
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
						check(pDestBuf + NC * equal <= pStartDestBuf + pBase->GetDataSize());
                        if ( equalPixel==255 )
                        {
                            for ( int i=0; i<equal; ++i )
                            {
                                for ( int32 c=0; c<NumColorChannels; ++c )
                                {
                                    unsigned base = pBaseBuf[NC*i+c];
                                    unsigned result = BLEND_FUNC( base, top[c] );
                                    if ( CLAMP )
                                    {
                                        pDestBuf[NC*i+c] = (uint8)FMath::Min( 255u, result );
                                    }
                                    else
                                    {
                                        pDestBuf[NC*i+c] = (uint8)result;
                                    }
                                }

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
                                for ( int32 c=0; c<NumColorChannels; ++c )
                                {
                                    unsigned base = pBaseBuf[NC*i+c];
                                    unsigned result = BLEND_FUNC_MASKED( base, top[c], equalPixel );
                                    if ( CLAMP )
                                    {
                                        pDestBuf[NC*i+c] = (uint8)FMath::Min( 255u, result );
                                    }
                                    else
                                    {
                                        pDestBuf[NC*i+c] = (uint8)result;
                                    }
                                }

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
                            for ( int32 c=0; c<NumColorChannels; ++c )
                            {
                                unsigned mask = pMaskBuf[i];
                                unsigned base = pBaseBuf[NC*i+c];
                                unsigned result = BLEND_FUNC_MASKED( base, top[c], mask );
                                if ( CLAMP )
                                {
                                    pDestBuf[NC*i+c] = (uint8)FMath::Min( 255u, result );
                                }
                                else
                                {
                                    pDestBuf[NC*i+c] = (uint8)result;
                                }
                            }

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
	template< unsigned (*BLEND_FUNC_MASKED)(unsigned,unsigned,unsigned),
			  unsigned (*BLEND_FUNC)(unsigned,unsigned),
			  bool CLAMP
			  >
    inline void BufferLayerColour( uint8* pDestBuf,
								   const Image* pBase,
								   const Image* pMask,
								   vec3<float> col )
	{
		EImageFormat baseFormat = pBase->GetFormat();
		if ( baseFormat==EImageFormat::IF_RGB_UBYTE )
		{
			BufferLayerColourFormat<BLEND_FUNC_MASKED, BLEND_FUNC, CLAMP, 3>( pDestBuf, pBase, pMask, col );
		}
        else if ( baseFormat==EImageFormat::IF_RGBA_UBYTE )
        {
            BufferLayerColourFormat<BLEND_FUNC_MASKED, BLEND_FUNC, CLAMP, 4>( pDestBuf, pBase, pMask, col );
        }
        else if ( baseFormat==EImageFormat::IF_BGRA_UBYTE )
        {
            float temp = col[0];
            col[0] = col[2];
            col[2] = temp;
            BufferLayerColourFormat<BLEND_FUNC_MASKED, BLEND_FUNC, CLAMP, 4>( pDestBuf, pBase, pMask, col );
        }
        else if ( baseFormat==EImageFormat::IF_L_UBYTE )
		{
			BufferLayerColourFormat<BLEND_FUNC_MASKED, BLEND_FUNC, CLAMP, 1>( pDestBuf, pBase, pMask, col );
		}
		else
		{
			checkf( false, TEXT("Unsupported format.") );
		}
	}

	//---------------------------------------------------------------------------------------------
	//! Apply a blending function to an image with a colour source and a mask
	//---------------------------------------------------------------------------------------------
	template< unsigned (*BLEND_FUNC_MASKED)(unsigned,unsigned,unsigned),
			  unsigned (*BLEND_FUNC)(unsigned,unsigned),
			  bool CLAMP >
    inline void ImageLayerColour( Image* pResult,
                                  const Image* pBase,
                                  const Image* pMask,
                                  vec3<float> col )
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

        bool valid =
                ( pResult->GetFormat() == pBase->GetFormat() )
                &&
                ( pResult->GetSizeX() == pBase->GetSizeX() )
                &&
                ( pResult->GetSizeY() == pBase->GetSizeY() )
                &&
                ( pResult->GetLODCount() == pBase->GetLODCount() )
                &&
                ( pBase->GetSizeX() == pMask->GetSizeX() )
                &&
                ( pBase->GetSizeY() == pMask->GetSizeY() )
                &&
                ( pMask->GetFormat() == EImageFormat::IF_L_UBYTE
                                        ||
                                        pMask->GetFormat() == EImageFormat::IF_L_UBYTE_RLE );
        if (valid)
        {
            BufferLayerColour<BLEND_FUNC_MASKED, BLEND_FUNC, CLAMP>( pResult->GetData(), pBase, pMask, col );
        }
	}


	//---------------------------------------------------------------------------------------------
	//! Apply a blending function to an image with a colour source on the base image itself
	//---------------------------------------------------------------------------------------------
	template< unsigned (*BLEND_FUNC_MASKED)(unsigned,unsigned,unsigned),
			  unsigned (*BLEND_FUNC)(unsigned,unsigned),
			  bool CLAMP >
	inline void ImageLayerColourOnBase( Image* pBase, const Image* pMask, vec3<float> col )
	{
		check( pBase->GetSizeX() == pMask->GetSizeX() );
		check( pBase->GetSizeY() == pMask->GetSizeY() );
		check( pMask->GetFormat() == EImageFormat::IF_L_UBYTE
						||
						pMask->GetFormat() == EImageFormat::IF_L_UBYTE_RLE );

		BufferLayerColour<BLEND_FUNC_MASKED,BLEND_FUNC,CLAMP>( pBase->GetData(), pBase, pMask, col );
	}


	//---------------------------------------------------------------------------------------------
	//! Apply a blending function to an image with another image as blending layer
	//---------------------------------------------------------------------------------------------
	template< unsigned (*BLEND_FUNC)(unsigned,unsigned), bool CLAMP, int NC >
    inline void BufferLayer(uint8* pDestBuf, const Image* pBase, const Image* pBlended,
                            bool applyToAlpha )
	{
		check( pBase->GetSizeX() == pBlended->GetSizeX() );
		check( pBase->GetSizeY() == pBlended->GetSizeY() );
		check( pBase->GetFormat() == pBlended->GetFormat() );

        const uint8* pBaseBuf = pBase->GetData();
        const uint8* pBlendedBuf = pBlended->GetData();

		// Generic implementation
		int32 pixelCount = pBase->CalculatePixelCount();

		ParallelFor(pixelCount,
			[
				pBaseBuf, pBlendedBuf, pDestBuf, applyToAlpha
			] (uint32 i)
			{
				if (applyToAlpha && NC > 3)
				{
					for (int c = 0; c < 3; ++c)
					{
						unsigned base = pBaseBuf[NC * i + c];
						unsigned blended = pBlendedBuf[NC * i + c];
						unsigned result = BLEND_FUNC(base, blended);
						if (CLAMP)
						{
							pDestBuf[NC * i + c] = (uint8)FMath::Min(255u, result);
						}
						else
						{
							pDestBuf[NC * i + c] = (uint8)result;
						}
					}

					pDestBuf[NC * i + 3] = pBaseBuf[NC * i + 3];
				}
				else
				{
					for (int c = 0; c < NC; ++c)
					{
						unsigned base = pBaseBuf[NC * i + c];
						unsigned blended = pBlendedBuf[NC * i + c];
						unsigned result = BLEND_FUNC(base, blended);
						if (CLAMP)
						{
							pDestBuf[NC * i + c] = (uint8)FMath::Min(255u, result);
						}
						else
						{
							pDestBuf[NC * i + c] = (uint8)result;
						}
					}
				}
			});
	}


	//---------------------------------------------------------------------------------------------
	//! Apply a blending function to an image with another image as blending layer
	//---------------------------------------------------------------------------------------------
	template< unsigned (*BLEND_FUNC)(unsigned,unsigned), bool CLAMP >
    inline void ImageLayer( Image* pResult, const Image* pBase, const Image* pBlended,
                            bool applyToAlpha )
	{
        check( pResult->GetFormat() == pBase->GetFormat() );
        check( pResult->GetSizeX() == pBase->GetSizeX() );
        check( pResult->GetSizeY() == pBase->GetSizeY() );
        check( pResult->GetLODCount() == pBase->GetLODCount() );
        check( pBase->GetSizeX() == pBlended->GetSizeX() );
		check( pBase->GetSizeY() == pBlended->GetSizeY() );
		check( pBase->GetFormat() == pBlended->GetFormat() );
        check( pResult->GetLODCount() <= pBlended->GetLODCount() );

		EImageFormat baseFormat = pBase->GetFormat();

        uint8* pDestBuf = pResult->GetData();

		if ( baseFormat==EImageFormat::IF_RGB_UBYTE )
		{
            BufferLayer< BLEND_FUNC, CLAMP, 3 > ( pDestBuf, pBase, pBlended, applyToAlpha );
		}
        else if ( baseFormat==EImageFormat::IF_RGBA_UBYTE || baseFormat==EImageFormat::IF_BGRA_UBYTE )
		{
            BufferLayer< BLEND_FUNC, CLAMP, 4 > ( pDestBuf, pBase, pBlended, applyToAlpha );
		}
		else if ( baseFormat==EImageFormat::IF_L_UBYTE )
		{
            BufferLayer< BLEND_FUNC, CLAMP, 1 > ( pDestBuf, pBase, pBlended, applyToAlpha );
		}
		else
		{
			checkf( false, TEXT("Unsupported format.") );
		}
	}


	//---------------------------------------------------------------------------------------------
	//! Apply a blending function to an image with another image as blending layer on the base image
	//---------------------------------------------------------------------------------------------
	template< unsigned (*BLEND_FUNC)(unsigned,unsigned), bool CLAMP >
    inline void ImageLayerOnBase( Image* pBase, const Image* pBlended,
                                  bool applyToAlpha )
	{
		check( pBase->GetSizeX() == pBlended->GetSizeX() );
		check( pBase->GetSizeY() == pBlended->GetSizeY() );
		check( pBase->GetFormat() == pBlended->GetFormat() );

		EImageFormat baseFormat = pBase->GetFormat();
        uint8* pDestBuf = pBase->GetData();

		if ( baseFormat==EImageFormat::IF_RGB_UBYTE )
		{
            BufferLayer< BLEND_FUNC, CLAMP, 3 > ( pDestBuf, pBase, pBlended, applyToAlpha );
		}
        else if ( baseFormat==EImageFormat::IF_RGBA_UBYTE || baseFormat==EImageFormat::IF_BGRA_UBYTE )
		{
            BufferLayer< BLEND_FUNC, CLAMP, 4 > ( pDestBuf, pBase, pBlended, applyToAlpha );
		}
		else if ( baseFormat==EImageFormat::IF_L_UBYTE )
		{
            BufferLayer< BLEND_FUNC, CLAMP, 1 > ( pDestBuf, pBase, pBlended, applyToAlpha );
		}
		else
		{
			checkf( false, TEXT("Unsupported format.") );
		}
	}


	//---------------------------------------------------------------------------------------------
	template< unsigned (*BLEND_FUNC_MASKED)(unsigned,unsigned,unsigned),
			  unsigned (*BLEND_FUNC)(unsigned,unsigned),
			  bool CLAMP,
			  int NC >
    inline void BufferLayerFormat( uint8* pStartDestBuf,
                                   const Image* pBase,
                                   const Image* pMask,
                                   const Image* pBlend,
                                   bool applyToAlpha )
	{
        check(pBase->GetSizeX()==pMask->GetSizeX() && pBase->GetSizeY()==pMask->GetSizeY());
        check(pBase->GetSizeX()==pBlend->GetSizeX() && pBase->GetSizeY()==pBlend->GetSizeY());
        check(pBase->GetLODCount() <= pMask->GetLODCount() );
        check(pBase->GetLODCount() <= pBlend->GetLODCount() );

		uint8* pDestBuf = pStartDestBuf;
        const uint8* pMaskBuf = pMask->GetData();
        const uint8* pBaseBuf = pBase->GetData();
        const uint8* pBlendedBuf = pBlend->GetData();

		EImageFormat maskFormat = pMask->GetFormat();
        bool isUncompressed = ( maskFormat == EImageFormat::IF_L_UBYTE );

		// The base determines the number of lods to process.
		int32 LODCount = pBase->GetLODCount();

        if ( isUncompressed )
		{
			int32 pixelCount = pBase->CalculatePixelCount();

            if (!applyToAlpha && NC>3)
            {
				ParallelFor(pixelCount,
					[
						pBaseBuf, pBlendedBuf, pMaskBuf, pDestBuf
					] (uint32 i)
					{
						unsigned mask = pMaskBuf[i];
						constexpr int32 NumColorChannels = FMath::Min(NC, 3);
						for (int c = 0; c <NumColorChannels; ++c)
						{
							unsigned base = pBaseBuf[NC * i + c];
							unsigned blended = pBlendedBuf[NC * i + c];
							unsigned result = BLEND_FUNC_MASKED(base, blended, mask);
							if (CLAMP)
							{
								pDestBuf[NC * i + c] = (uint8)FMath::Min(255u, result);
							}
							else
							{
								pDestBuf[NC * i + c] = (uint8)result;
							}
						}

						pDestBuf[NC * i + 3] = pBaseBuf[NC * i + 3];
					});
            }
            else
            {
				ParallelFor(pixelCount,
					[
						pBaseBuf, pBlendedBuf, pMaskBuf, pDestBuf
					] (uint32 i)
					{
						unsigned mask = pMaskBuf[i];
						for (int c = 0; c < NC; ++c)
						{
							unsigned base = pBaseBuf[NC * i + c];
							unsigned blended = pBlendedBuf[NC * i + c];
							unsigned result = BLEND_FUNC_MASKED(base, blended, mask);
							if (CLAMP)
							{
								pDestBuf[NC * i + c] = (uint8)FMath::Min(255u, result);
							}
							else
							{
								pDestBuf[NC * i + c] = (uint8)result;
							}
						}
					});
            }
		}
        else if ( maskFormat==EImageFormat::IF_L_UBYTE_RLE )
		{
			int rows = pBase->GetSizeY();
			int width = pBase->GetSizeX();

            for (int lod=0;lod<LODCount; ++lod)
            {
                pMaskBuf += 4+rows*sizeof(uint32);

				// \todo: See how to handle mask buf here
				//ParallelFor(rows,
				//	[
				//		pBaseBuf, pBlendedBuf, pMaskBuf, pDestBuf, width, applyToAlpha
				//	] (uint32 r)
				for (int r = 0; r < rows; ++r)
				{						
					//UE_LOG(LogMutableCore, Warning, "row: %d", r);

					const uint8* pDestRowEnd = pDestBuf + width*NC;
					while ( pDestBuf!=pDestRowEnd )
					{
						// Decode header
						uint16 equal = *(const uint16*)pMaskBuf;
						pMaskBuf += 2;

						uint8 different = *pMaskBuf;
						++pMaskBuf;

						uint8 equalPixel = *pMaskBuf;
						++pMaskBuf;

						//UE_LOG(LogMutableCore, Warning, "    block: %d %d", (int)equal, (int)different );

						// Equal pixels
						check(pDestBuf + NC * equal <= pStartDestBuf + pBase->GetDataSize());
						if ( equalPixel==255 )
						{
							for ( int i=0; i<equal; ++i )
							{
								if (!applyToAlpha && NC>3)
								{
									for ( int c=0; c<3; ++c )
									{
										unsigned base = pBaseBuf[NC*i+c];
										unsigned blended = pBlendedBuf[NC*i+c];
										unsigned result = BLEND_FUNC( base, blended );
										if ( CLAMP )
										{
											pDestBuf[NC*i+c] = (uint8)FMath::Min( 255u, result );
										}
										else
										{
											pDestBuf[NC*i+c] = (uint8)result;
										}
									}

									pDestBuf[NC*i+3] = (uint8)pBaseBuf[NC*i+3];
								}
								else
								{
									for ( int c=0; c<NC; ++c )
									{
										unsigned base = pBaseBuf[NC*i+c];
										unsigned blended = pBlendedBuf[NC*i+c];
										unsigned result = BLEND_FUNC( base, blended );
										if ( CLAMP )
										{
											pDestBuf[NC*i+c] = (uint8)FMath::Min( 255u, result );
										}
										else
										{
											pDestBuf[NC*i+c] = (uint8)result;
										}
									}
								}
							}
						}
						else if ( equalPixel>0 )
						{
							for ( int i=0; i<equal; ++i )
							{
								if (!applyToAlpha && NC>3)
								{
									for ( int c=0; c<3; ++c )
									{
										unsigned base = pBaseBuf[NC*i+c];
										unsigned blended = pBlendedBuf[NC*i+c];
										unsigned result = BLEND_FUNC_MASKED( base, blended, equalPixel );
										if ( CLAMP )
										{
											pDestBuf[NC*i+c] = (uint8)FMath::Min( 255u, result );
										}
										else
										{
											pDestBuf[NC*i+c] = (uint8)result;
										}
									}

									pDestBuf[NC*i+3] = (uint8)pBaseBuf[NC*i+3];
								}
								else
								{
									for ( int c=0; c<NC; ++c )
									{
										unsigned base = pBaseBuf[NC*i+c];
										unsigned blended = pBlendedBuf[NC*i+c];
										unsigned result = BLEND_FUNC_MASKED( base, blended, equalPixel );
										if ( CLAMP )
										{
											pDestBuf[NC*i+c] = (uint8)FMath::Min( 255u, result );
										}
										else
										{
											pDestBuf[NC*i+c] = (uint8)result;
										}
									}
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
						pBlendedBuf += NC*equal;

						// Different pixels
						check(pDestBuf + NC * different <= pStartDestBuf + pBase->GetDataSize());
						for ( int i=0; i<different; ++i )
						{
							if (!applyToAlpha && NC>3)
							{
								for ( int c=0; c<3; ++c )
								{
									unsigned mask = pMaskBuf[i];
									unsigned base = pBaseBuf[NC*i+c];
									unsigned blended = pBlendedBuf[NC*i+c];
									unsigned result = BLEND_FUNC_MASKED( base, blended, mask );
									if ( CLAMP )
									{
										pDestBuf[NC*i+c] = (uint8)FMath::Min( 255u, result );
									}
									else
									{
										pDestBuf[NC*i+c] = (uint8)result;
									}
								}

								pDestBuf[NC*i+3] = (uint8)pBaseBuf[NC*i+3];
							}
							else
							{
								for ( int c=0; c<NC; ++c )
								{
									unsigned mask = pMaskBuf[i];
									unsigned base = pBaseBuf[NC*i+c];
									unsigned blended = pBlendedBuf[NC*i+c];
									unsigned result = BLEND_FUNC_MASKED( base, blended, mask );
									if ( CLAMP )
									{
										pDestBuf[NC*i+c] = (uint8)FMath::Min( 255u, result );
									}
									else
									{
										pDestBuf[NC*i+c] = (uint8)result;
									}
								}
							}
						}

						pDestBuf += NC*different;
						pBaseBuf += NC*different;
						pBlendedBuf += NC*different;
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
                             bool applyToAlpha )
	{
		if ( pBase->GetFormat()==EImageFormat::IF_RGB_UBYTE )
		{
            BufferLayerFormat< BLEND_FUNC_MASKED, BLEND_FUNC, CLAMP, 3 >
                    ( pDestBuf, pBase, pMask, pBlend, applyToAlpha ) ;
		}
        else if ( pBase->GetFormat()==EImageFormat::IF_RGBA_UBYTE || pBase->GetFormat()==EImageFormat::IF_BGRA_UBYTE )
		{
            BufferLayerFormat< BLEND_FUNC_MASKED, BLEND_FUNC, CLAMP, 4 >
                    ( pDestBuf, pBase, pMask, pBlend, applyToAlpha ) ;
		}
		else if ( pBase->GetFormat()==EImageFormat::IF_L_UBYTE )
		{
            BufferLayerFormat< BLEND_FUNC_MASKED, BLEND_FUNC, CLAMP, 1 >
                    ( pDestBuf, pBase, pMask, pBlend, applyToAlpha ) ;
		}
        else
		{
			checkf( false, TEXT("Unsupported format.") );
		}
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
	//! Apply a blending function to an image with another image as blending layer with a mask
	//---------------------------------------------------------------------------------------------
	template< unsigned (*BLEND_FUNC_MASKED)(unsigned,unsigned,unsigned),
			  unsigned (*BLEND_FUNC)(unsigned,unsigned),
			  bool CLAMP >
    inline void ImageLayer( Image* pResult,
                            const Image* pBase,
                            const Image* pMask,
                            const Image* pBlended,
                            bool applyToAlpha )
	{
        check( pResult->GetFormat() == pBase->GetFormat() );
        check( pResult->GetSizeX() == pBase->GetSizeX() );
        check( pResult->GetSizeY() == pBase->GetSizeY() );
        check( pResult->GetLODCount() == pBase->GetLODCount() );
		check( pBase->GetSizeX() == pMask->GetSizeX() );
		check( pBase->GetSizeY() == pMask->GetSizeY() );
		check( pBase->GetSizeX() == pBlended->GetSizeX() );
		check( pBase->GetSizeY() == pBlended->GetSizeY() );
		check( pBase->GetFormat() == pBlended->GetFormat() );
		check( pMask->GetFormat() == EImageFormat::IF_L_UBYTE
						||
						pMask->GetFormat() == EImageFormat::IF_L_UBYTE_RLE );
        check( pBase->GetLODCount() <= pMask->GetLODCount() );
        check( pBase->GetLODCount() <= pBlended->GetLODCount() );

        BufferLayer<BLEND_FUNC_MASKED,BLEND_FUNC,CLAMP>( pResult->GetData(), pBase, pMask,
                                                         pBlended, applyToAlpha );
	}


	//---------------------------------------------------------------------------------------------
	//! Apply a blending function to an image with another image as blending layer, on the base
	//! image itself
	//---------------------------------------------------------------------------------------------
	template< unsigned (*BLEND_FUNC_MASKED)(unsigned,unsigned,unsigned),
			  unsigned (*BLEND_FUNC)(unsigned,unsigned),
			  bool CLAMP >
    inline void ImageLayerOnBase( Image* pBase, const Image* pMask, const Image* pBlended,
                                  bool applyToAlpha )
	{
		check( pBase->GetSizeX() == pMask->GetSizeX() );
		check( pBase->GetSizeY() == pMask->GetSizeY() );
		check( pBase->GetSizeX() == pBlended->GetSizeX() );
		check( pBase->GetSizeY() == pBlended->GetSizeY() );
		check( pBase->GetFormat() == pBlended->GetFormat() );
		check( pMask->GetFormat() == EImageFormat::IF_L_UBYTE
						||
						pMask->GetFormat() == EImageFormat::IF_L_UBYTE_RLE );
        check( pBase->GetLODCount() <= pMask->GetLODCount() );
        check( pBase->GetLODCount() <= pBlended->GetLODCount() );

        BufferLayer<BLEND_FUNC_MASKED,BLEND_FUNC,CLAMP>( pBase->GetData(), pBase, pMask,
                                                         pBlended, applyToAlpha );
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
	inline void BufferLayerCombine(uint8* pDestBuf, const Image* pBase, const Image* pBlended)
	{
		//static_assert(NC > 0 && NC <= 4);

		check(pBase->GetSizeX() == pBlended->GetSizeX());
		check(pBase->GetSizeY() == pBlended->GetSizeY());
		check(pBase->GetFormat() == pBlended->GetFormat());

		const uint8* pBaseBuf = pBase->GetData();
		const uint8* pBlendedBuf = pBlended->GetData();

		int32 pixelCount = pBase->CalculatePixelCount();

		for (int i = 0; i < pixelCount; ++i)
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
	inline void BufferLayerCombine(uint8* pDestBuf, const Image* pBase, const Image* pMask, const Image* pBlended)
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

		int32 pixelCount = pBase->CalculatePixelCount();

		for (int i = 0; i < pixelCount; ++i)
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
	inline void ImageLayerCombine(Image* pResult, const Image* pBase, const Image* pBlended)
	{
		check(pResult->GetFormat() == pBase->GetFormat());
		check(pResult->GetSizeX() == pBase->GetSizeX());
		check(pResult->GetSizeY() == pBase->GetSizeY());
		check(pResult->GetLODCount() == pBase->GetLODCount());
		check(pBase->GetSizeX() == pBlended->GetSizeX());
		check(pBase->GetSizeY() == pBlended->GetSizeY());
		check(pBase->GetFormat() == pBlended->GetFormat());
		check(pResult->GetLODCount() <= pBlended->GetLODCount());

		const EImageFormat baseFormat = pBase->GetFormat();

		uint8* pDestBuf = pResult->GetData();
		if (baseFormat == EImageFormat::IF_L_UBYTE)
		{
			BufferLayerCombine<BLEND_FUNC, 1 >(pDestBuf, pBase, pBlended);
		}
		else if (baseFormat == EImageFormat::IF_RGB_UBYTE)
		{
			BufferLayerCombine< BLEND_FUNC, 3 >(pDestBuf, pBase, pBlended);
		}
		else if (baseFormat == EImageFormat::IF_RGBA_UBYTE || baseFormat == EImageFormat::IF_BGRA_UBYTE)
		{
			// \todo: pass swizzle template argument if BGRA_UBYTE, not yet supported.
			BufferLayerCombine< BLEND_FUNC, 4 >(pDestBuf, pBase, pBlended);
		}
		else
		{
			checkf(false, TEXT("Unsupported format."));
		}
	}

	template< 
		uint32 (*BLEND_FUNC)(uint32, uint32), 
		uint32 (*BLEND_FUNC_MASKED)(uint32, uint32, uint32) >
	inline void ImageLayerCombine(Image* pResult, const Image* pBase, const Image* pMask, const Image* pBlended)
	{
		check(pResult->GetFormat() == pBase->GetFormat());
		check(pResult->GetSizeX() == pBase->GetSizeX());
		check(pResult->GetSizeY() == pBase->GetSizeY());
		check(pResult->GetLODCount() == pBase->GetLODCount());
		check(pBase->GetSizeX() == pBlended->GetSizeX());
		check(pBase->GetSizeY() == pBlended->GetSizeY());
		check(pBase->GetFormat() == pBlended->GetFormat());
		check(pResult->GetLODCount() <= pBlended->GetLODCount());

		const EImageFormat baseFormat = pBase->GetFormat();

		uint8* pDestBuf = pResult->GetData();

		if (pMask->GetFormat() != EImageFormat::IF_L_UBYTE)
		{
			checkf(false, TEXT("Unsupported mask format."));

			BufferLayerCombine< BLEND_FUNC, 1 >(pDestBuf, pBase, pBlended);
		}

		if (baseFormat == EImageFormat::IF_L_UBYTE)
		{
			BufferLayerCombine< BLEND_FUNC_MASKED, 1 >(pDestBuf, pBase, pMask, pBlended);
		}
		else if (baseFormat == EImageFormat::IF_RGB_UBYTE)
		{
			BufferLayerCombine< BLEND_FUNC_MASKED, 3 >(pDestBuf, pBase, pMask, pBlended);
		}
		else if (baseFormat == EImageFormat::IF_RGBA_UBYTE || baseFormat == EImageFormat::IF_BGRA_UBYTE)
		{
			// \todo: pass swizzle template argument if BGRA_UBYTE, not yet supported.
			BufferLayerCombine< BLEND_FUNC_MASKED, 4 >(pDestBuf, pBase, pMask, pBlended);
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

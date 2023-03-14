// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"
#include "MuR/OpImagePixelFormat.h"
#include "MuR/MutableTrace.h"
#include "Async/ParallelFor.h"


namespace mu
{

    //---------------------------------------------------------------------------------------------
    //! Point-filter image resize
    //! TODO: Optimise
    //---------------------------------------------------------------------------------------------
    inline ImagePtr ImageResize( const Image* pBase, FImageSize destSize )
    {
		MUTABLE_CPUPROFILER_SCOPE(ImageResizePoint)

		check( !(pBase->m_flags&Image::IF_CANNOT_BE_SCALED) );

        FImageSize baseSize = FImageSize( pBase->GetSizeX(), pBase->GetSizeY() );

        ImagePtr pDest = new Image( destSize[0], destSize[1], 1, pBase->GetFormat() );

        switch ( pBase->GetFormat() )
        {

        case EImageFormat::IF_L_UBYTE:
        {
            const uint8_t* pBaseBuf = pBase->GetData();
            uint8_t* pDestBuf = pDest->GetData();

            // Simple nearest pixel implementation
            for ( int y=0; y<destSize[1]; ++y )
            {
                int sy = (y*baseSize[1])/destSize[1];

                for ( int x=0; x<destSize[0]; ++x )
                {
                    int sx = (x*baseSize[0])/destSize[0];
                    *pDestBuf = pBaseBuf[sy*baseSize[0]+sx];
                    pDestBuf++;
                }
            }

            break;
        }

        case EImageFormat::IF_RGB_UBYTE:
        {
            const uint8_t* pBaseBuf = pBase->GetData();
            uint8_t* pDestBuf = pDest->GetData();

            // Simple nearest pixel implementation
            for ( int y=0; y<destSize[1]; ++y )
            {
                int sy = (y*baseSize[1])/destSize[1];

                for ( int x=0; x<destSize[0]; ++x )
                {
                    int sx = (x*baseSize[0])/destSize[0];
                    pDestBuf[0] = pBaseBuf[(sy*baseSize[0]+sx)*3+0];
                    pDestBuf[1] = pBaseBuf[(sy*baseSize[0]+sx)*3+1];
                    pDestBuf[2] = pBaseBuf[(sy*baseSize[0]+sx)*3+2];
                    pDestBuf+=3;
                }
            }

            break;
        }

        case EImageFormat::IF_BGRA_UBYTE:
        case EImageFormat::IF_RGBA_UBYTE:
        {
            const uint8_t* pBaseBuf = pBase->GetData();
            uint8_t* pDestBuf = pDest->GetData();

            // Simple nearest pixel implementation
            for ( int y=0; y<destSize[1]; ++y )
            {
                int sy = (y*baseSize[1])/destSize[1];

                for ( int x=0; x<destSize[0]; ++x )
                {
                    int sx = (x*baseSize[0])/destSize[0];
                    pDestBuf[0] = pBaseBuf[(sy*baseSize[0]+sx)*4+0];
                    pDestBuf[1] = pBaseBuf[(sy*baseSize[0]+sx)*4+1];
                    pDestBuf[2] = pBaseBuf[(sy*baseSize[0]+sx)*4+2];
                    pDestBuf[3] = pBaseBuf[(sy*baseSize[0]+sx)*4+3];
                    pDestBuf+=4;
                }
            }

            break;
        }

        default:
            // Case not implemented
            check( false );
            //mu::Halt();
        }

        return pDest;
    }



    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    template< int NC >
    void ImageMagnifyX( Image* pDest, const Image* pBase )
    {
        int baseSizeX = pBase->GetSizeX();
        int destSizeX = pDest->GetSizeX();
        int sizeY = pBase->GetSizeY();

        uint32_t dx_16 = (uint32_t(baseSizeX)<<16) / destSizeX;

        // Linear filtering
        //for ( int y=0; y<sizeY; ++y )
		const auto& ProcessLine = [
			pDest, pBase, dx_16, baseSizeX, destSizeX
		] (uint32 y)
		{
			uint32_t px_16 = 0;
			const uint8_t* pBaseBuf = pBase->GetData() + y * baseSizeX * NC;
			uint8_t* pDestBuf = pDest->GetData() + y * destSizeX * NC;

			for (int x = 0; x < destSizeX; ++x)
			{
				uint32_t px = px_16 >> 16;
				uint32_t epx_16 = px_16 + dx_16;

				if ((px_16 & 0xffff0000) == ((epx_16 - 1) & 0xffff0000))
				{
					// One fraction
					for (int c = 0; c < NC; ++c)
					{
						pDestBuf[c] += pBaseBuf[px * NC + c];
					}
				}
				else
				{
					// Two fractions
					uint32_t frac1 = (px_16 & 0xffff);
					uint32_t frac0 = 0x10000 - frac1;

					for (int c = 0; c < NC; ++c)
					{
						pDestBuf[c] = uint8_t( (pBaseBuf[px * NC + c] * frac0 + pBaseBuf[(px + 1) * NC + c] * frac1) >> 16 );
					}

					++px;
				}

				px_16 = epx_16;
				pDestBuf += NC;
			}
		};

		ParallelFor(sizeY, ProcessLine);
    }


    inline void ImageMagnifyX( Image* pDest, const Image* pBase )
    {
		MUTABLE_CPUPROFILER_SCOPE(ImageMagnifyX)

        check( pDest->GetSizeY() == pBase->GetSizeY() );
        check( pDest->GetSizeX() > pBase->GetSizeX() );

        switch ( pBase->GetFormat() )
        {

        case EImageFormat::IF_L_UBYTE:
        {
            ImageMagnifyX<1>( pDest, pBase );
            break;
        }

        case EImageFormat::IF_RGB_UBYTE:
        {
            ImageMagnifyX<3>( pDest, pBase );
            break;
        }

        case EImageFormat::IF_BGRA_UBYTE:
        case EImageFormat::IF_RGBA_UBYTE:
        {
            ImageMagnifyX<4>( pDest, pBase );
            break;
        }

        default:
            // Case not implemented
            check( false );
            mu::Halt();
        }
    }


    //---------------------------------------------------------------------------------------------
    //! General image minimisation
    //---------------------------------------------------------------------------------------------
    template< int NC >
    void ImageMinifyX( Image* pDest, const Image* pBase )
    {
        int baseSizeX = pBase->GetSizeX();
        int destSizeX = pDest->GetSizeX();
        int sizeY = pBase->GetSizeY();

        uint32_t dx_16 = (uint32_t(baseSizeX)<<16) / destSizeX;

        // Linear filtering
        //for ( int y=0; y<sizeY; ++y )
		const auto& ProcessLine = [
			pDest, pBase, dx_16, baseSizeX, destSizeX
		] (uint32 y)
		{
			const uint8_t* pBaseBuf = pBase->GetData() + y * baseSizeX * NC;
			uint8_t* pDestBuf = pDest->GetData() + y * destSizeX * NC;

			uint32_t px_16 = 0;
			for (int x = 0; x < destSizeX; ++x)
			{
				uint32_t r_16[NC];
				for (int c = 0; c < NC; ++c)
				{
					r_16[c] = 0;
				}

				uint32_t epx_16 = px_16 + dx_16;
				uint32_t px = px_16 >> 16;
				uint32_t epx = epx_16 >> 16;

				// First fraction
				uint32_t frac0 = px_16 & 0xffff;
				if (frac0)
				{
					for (int c = 0; c < NC; ++c)
					{
						r_16[c] += (0x10000 - frac0) * pBaseBuf[px * NC + c];
					}

					++px;
				}

				// Whole pixels
				while (px < epx)
				{
					for (int c = 0; c < NC; ++c)
					{
						r_16[c] += uint32_t(pBaseBuf[px * NC + c]) << 16;
					}

					++px;
				}

				// Second fraction
				uint32_t frac1 = epx_16 & 0xffff;
				if (frac1)
				{
					for (int c = 0; c < NC; ++c)
					{
						r_16[c] += frac1 * pBaseBuf[px * NC + c];
					}
				}

				for (int c = 0; c < NC; ++c)
				{
					pDestBuf[c] = (uint8_t)(r_16[c] / dx_16);
				}

				px_16 = epx_16;
				pDestBuf += NC;
			}
		};

		ParallelFor(sizeY, ProcessLine);
    }


    //---------------------------------------------------------------------------------------------
    //! Optimised for whole factors
    //---------------------------------------------------------------------------------------------
    template< int NC, int FACTOR >
    void ImageMinifyX_Exact( Image* pDest, const Image* pBase )
    {
		int baseSizeX = pBase->GetSizeX();
        int destSizeX = pDest->GetSizeX();
        int sizeY = pBase->GetSizeY();

        const uint8_t* pBaseBuf = pBase->GetData();
        uint8_t* pDestBuf = pDest->GetData();

        // Linear filtering
		const auto& ProcessLine = [
			pDest, pBase, baseSizeX, destSizeX
		] (uint32 y)
		{
			const uint8_t* pBaseBuf = pBase->GetData() + y * baseSizeX * NC;
			uint8_t* pDestBuf = pDest->GetData() + y * destSizeX * NC;

			uint32_t r[NC];
			for (int x = 0; x < destSizeX; ++x)
			{
				for (int c = 0; c < NC; ++c)
				{
					r[c] = 0;
					for (int a = 0; a < FACTOR; ++a)
					{
						r[c] += pBaseBuf[a * NC + c];
					}
				}

				for (int c = 0; c < NC; ++c)
				{
					pDestBuf[c] = (uint8_t)(r[c] / FACTOR);
				}

				pDestBuf += NC;
				pBaseBuf += NC * FACTOR;
			}
		};

		ParallelFor(sizeY, ProcessLine);
    }


    //---------------------------------------------------------------------------------------------
    //! Image minify X version hub.
    //---------------------------------------------------------------------------------------------
    inline void ImageMinifyX( Image* pDest, const Image* pBase )
    {
		MUTABLE_CPUPROFILER_SCOPE(ImageMinifyX)

        check( pDest->GetSizeY() == pBase->GetSizeY() );
        check( pDest->GetSizeX() < pBase->GetSizeX() );

        switch ( pBase->GetFormat() )
        {

        case EImageFormat::IF_L_UBYTE:
        {
            if ( 2*pDest->GetSizeX()==pBase->GetSizeX() )
            {
                // Optimised case
                ImageMinifyX_Exact<1,2>( pDest, pBase );
            }
            else
            {
                // Generic case
                ImageMinifyX<1>( pDest, pBase );
            }
            break;
        }

        case EImageFormat::IF_RGB_UBYTE:
        {
            if ( 2*pDest->GetSizeX()==pBase->GetSizeX() )
            {
                // Optimised case
                ImageMinifyX_Exact<3,2>( pDest, pBase );
            }
            else
            {
                // Generic case
                ImageMinifyX<3>( pDest, pBase );
            }
            break;
        }

        case EImageFormat::IF_BGRA_UBYTE:
        case EImageFormat::IF_RGBA_UBYTE:
        {
            if ( 2*pDest->GetSizeX()==pBase->GetSizeX() )
            {
                // Optimised case
                ImageMinifyX_Exact<4,2>( pDest, pBase );
            }
            else
            {
                // Generic case
                ImageMinifyX<4>( pDest, pBase );
            }
            break;
        }

        default:
            // Case not implemented
            check( false );
            mu::Halt();
        }

    }




    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    template< int NC >
    void ImageMagnifyY( Image* pDest, const Image* pBase )
    {
		if (!pBase || !pDest || 
			!pBase->GetSizeX() || !pBase->GetSizeY() || !pDest->GetSizeX() || !pDest->GetSizeY())
		{
			return;
		}
			
		int baseSizeY = pBase->GetSizeY();
        int destSizeY = pDest->GetSizeY();
        int sizeX = pBase->GetSizeX();

        size_t rowSize = sizeX * NC;

        // Common case, optimised.
        if (destSizeY==baseSizeY*2)
        {
			//for (int y = 0; y < baseSizeY; ++y)
			const auto& ProcessLine = [
				pDest, pBase, rowSize
			] (uint32 y)
            {
				uint8_t* pDestBuf = pDest->GetData()+ 2*y*rowSize;
				const uint8_t* pBaseBuf = pBase->GetData() + y * rowSize;

                memcpy( pDestBuf, pBaseBuf, rowSize );
                pDestBuf += rowSize;

                memcpy( pDestBuf, pBaseBuf, rowSize );
            };

			ParallelFor(baseSizeY, ProcessLine);
        }
        else
        {
            uint32_t dy_16 = ( uint32_t( baseSizeY ) << 16 ) / destSizeY;

            // Linear filtering
            // \todo: optimise: swap loops, etc.
            //for ( int x=0; x<sizeX; ++x )
			const auto& ProcessColumn = [
				pDest, pBase, sizeX, destSizeY, dy_16
			] (uint32 x)
            {
                uint32_t py_16 = 0;
                uint8_t* pDestBuf = pDest->GetData()+x*NC;
				const uint8_t* pBaseBuf = pBase->GetData();

                for ( int y=0; y<destSizeY; ++y )
                {
                    uint32_t py = py_16 >> 16;
                    uint32_t epy_16 = py_16+dy_16;

                    if ( (py_16 & 0xffff0000) == ((epy_16-1) & 0xffff0000) )
                    {
                        // One fraction
                        for ( int c=0; c<NC; ++c )
                        {
                            pDestBuf[c] += pBaseBuf[(py*sizeX+x)*NC+c];
                        }
                    }
                    else
                    {
                        // Two fractions
                        uint32_t frac1 = (py_16 & 0xffff);
                        uint32_t frac0 = 0x10000 - frac1;

                        for ( int c=0; c<NC; ++c )
                        {
                            pDestBuf[c] = (uint8_t)( ( pBaseBuf[(py*sizeX+x)*NC+c] * frac0 +
                                            pBaseBuf[((py+1)*sizeX+x)*NC+c] * frac1
                                            ) >> 16 );
                        }

                        ++py;
                    }

                    py_16 = epy_16;
                    pDestBuf+=sizeX*NC;
                }
            };

			ParallelFor(sizeX, ProcessColumn);
        }
    }


    inline void ImageMagnifyY( Image* pDest, const Image* pBase )
    {
        check( pDest->GetSizeY() > pBase->GetSizeY() );
        check( pDest->GetSizeX() == pBase->GetSizeX() );

		MUTABLE_CPUPROFILER_SCOPE(ImageMagnifyY)

        switch ( pBase->GetFormat() )
        {

        case EImageFormat::IF_L_UBYTE:
        {
            ImageMagnifyY<1>( pDest, pBase );
            break;
        }

        case EImageFormat::IF_RGB_UBYTE:
        {
            ImageMagnifyY<3>( pDest, pBase );
            break;
        }

        case EImageFormat::IF_RGBA_UBYTE:
        case EImageFormat::IF_BGRA_UBYTE:
        {
            ImageMagnifyY<4>( pDest, pBase );
            break;
        }

        default:
            // Case not implemented
            check( false );
            mu::Halt();
        }
    }


    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    template< int NC >
    void ImageMinifyY( Image* pDest, const Image* pBase )
    {
        int baseSizeY = pBase->GetSizeY();
        int destSizeY = pDest->GetSizeY();
        int sizeX = pBase->GetSizeX();

        uint32_t dy_16 = (uint32_t(baseSizeY)<<16) / destSizeY;

        const uint8_t* pBaseBuf = pBase->GetData();

        // Linear filtering
		//for (int x = 0; x < sizeX; ++x)
		const auto& ProcessColumn = [
			pDest, pBaseBuf, sizeX, destSizeY, dy_16
		] (uint32 x) 
		{
			uint8_t* pDestBuf = pDest->GetData() + x * NC;
			uint32_t py_16 = 0;
			for (int y = 0; y < destSizeY; ++y)
			{
				uint32_t r_16[NC];
				for (int c = 0; c < NC; ++c)
				{
					r_16[c] = 0;
				}

				uint32_t epy_16 = py_16 + dy_16;
				uint32_t py = py_16 >> 16;
				uint32_t epy = epy_16 >> 16;

				// First fraction
				uint32_t frac0 = py_16 & 0xffff;
				if (frac0)
				{
					for (int c = 0; c < NC; ++c)
					{
						r_16[c] += (0x10000 - frac0) * pBaseBuf[(py * sizeX + x) * NC + c];
					}

					++py;
				}

				// Whole pixels
				while (py < epy)
				{
					for (int c = 0; c < NC; ++c)
					{
						r_16[c] += uint32_t(pBaseBuf[(py * sizeX + x) * NC + c]) << 16;
					}

					++py;
				}

				// Second fraction
				uint32_t frac1 = epy_16 & 0xffff;
				if (frac1)
				{
					for (int c = 0; c < NC; ++c)
					{
						r_16[c] += frac1 * pBaseBuf[(py * sizeX + x) * NC + c];
					}
				}

				for (int c = 0; c < NC; ++c)
				{
					pDestBuf[c] = (uint8_t)(r_16[c] / dy_16);
				}

				py_16 = epy_16;
				pDestBuf += sizeX * NC;
			}
		};

		ParallelFor(sizeX, ProcessColumn);

    }


    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    template< int NC, int FACTOR >
    void ImageMinifyY_Exact( Image* pDest, const Image* pBase )
    {
        int destSizeY = pDest->GetSizeY();
        int sizeX = pBase->GetSizeX();


        // Linear filtering
        //for ( int y=0; y<destSizeY; ++y )
		const auto& ProcessRow = [
			pDest, pBase, sizeX, destSizeY
		] (uint32 y)
		{
			uint8_t* pDestBuf = pDest->GetData() + y*NC*sizeX;
			const uint8_t* pBaseBuf = pBase->GetData() + y * FACTOR * sizeX * NC;

			for ( int x=0; x<sizeX; ++x )
            {
                uint32_t r[NC];
                for ( int c=0; c<NC; ++c)
                {
                    r[c] = 0;
                }

                // Whole pixels
                for ( int f=0; f<FACTOR; ++f )
                {
                    for ( int c=0; c<NC; ++c)
                    {
                        r[c] += pBaseBuf[ sizeX*NC*f + x*NC + c ];
                    }
                }

                for ( int c=0; c<NC; ++c)
                {
                    pDestBuf[c] = (uint8_t)(r[c]/FACTOR);
                }

                pDestBuf += NC;
            }
            pBaseBuf += FACTOR*sizeX*NC;
        };

		ParallelFor(destSizeY, ProcessRow);
    }


    //---------------------------------------------------------------------------------------------
    //! Image minify Y version hub.
    //---------------------------------------------------------------------------------------------
    inline void ImageMinifyY( Image* pDest, const Image* pBase )
    {
        check( pDest->GetSizeY() < pBase->GetSizeY() );
        check( pDest->GetSizeX() == pBase->GetSizeX() );

		MUTABLE_CPUPROFILER_SCOPE(ImageMinifyY)

        switch ( pBase->GetFormat() )
        {

        case EImageFormat::IF_L_UBYTE:
        {
            if ( 2*pDest->GetSizeY()==pBase->GetSizeY() )
            {
                // Optimised case
                ImageMinifyY_Exact<1,2>( pDest, pBase );
            }
            else
            {
                // Generic case
                ImageMinifyY<1>( pDest, pBase );
            }
            break;
        }

        case EImageFormat::IF_RGB_UBYTE:
        {
            if ( 2*pDest->GetSizeY()==pBase->GetSizeY() )
            {
                // Optimised case
                ImageMinifyY_Exact<3,2>( pDest, pBase );
            }
            else
            {
                // Generic case
                ImageMinifyY<3>( pDest, pBase );
            }
            break;
        }

        case EImageFormat::IF_RGBA_UBYTE:
        case EImageFormat::IF_BGRA_UBYTE:
        {
            if ( 2*pDest->GetSizeY()==pBase->GetSizeY() )
            {
                // Optimised case
                ImageMinifyY_Exact<4,2>( pDest, pBase );
            }
            else
            {
                // Generic case
                ImageMinifyY<4>( pDest, pBase );
            }
            break;
        }

        default:
            // Case not implemented
            check( false );
            //mu::Halt();
        }

    }


    //---------------------------------------------------------------------------------------------
    //! Bilinear filter image resize.
    //---------------------------------------------------------------------------------------------
    inline ImagePtr ImageResizeLinear( int imageCompressionQuality, const Image* pBasePtr,
                                       FImageSize destSize )
    {
		MUTABLE_CPUPROFILER_SCOPE(ImageResizeLinear)

		check(!(pBasePtr->m_flags & Image::IF_CANNOT_BE_SCALED));

        ImagePtrConst pBase = pBasePtr;

        // Shouldn't happen! But if it does...
        EImageFormat sourceFormat = pBase->GetFormat();
		EImageFormat uncompressedFormat = GetUncompressedFormat( sourceFormat );
        if ( sourceFormat!=uncompressedFormat )
        {
            pBase = ImagePixelFormat( imageCompressionQuality, pBasePtr, uncompressedFormat );
        }

        FImageSize baseSize = FImageSize( pBase->GetSizeX(), pBase->GetSizeY() );

        ImagePtr pDest = new Image( destSize[0], destSize[1], 1, pBase->GetFormat() );
        if (!destSize[0] || !destSize[1] || !baseSize[0] || !baseSize[1])
        {
            return pDest;
        }

        // First resize X
        ImagePtr pTemp;
        if ( destSize[0] > baseSize[0] )
        {
            pTemp = new Image( destSize[0], baseSize[1], 1, pBase->GetFormat() );
            ImageMagnifyX( pTemp.get(), pBase.get() );
        }
        else if ( destSize[0] < baseSize[0] )
        {
            pTemp = new Image( destSize[0], baseSize[1], 1, pBase->GetFormat() );
            ImageMinifyX( pTemp.get(), pBase.get() );
        }
        else
        {
            pTemp = pBase->Clone();
        }

        // Now resize Y
        if ( destSize[1] > baseSize[1] )
        {
            ImageMagnifyY( pDest.get(), pTemp.get() );
        }
        else if ( destSize[1] < baseSize[1] )
        {
            ImageMinifyY( pDest.get(), pTemp.get() );
        }
        else
        {
            pDest = pTemp;
        }


        // Reset format if it was changed to scale
        if ( sourceFormat!=uncompressedFormat )
        {
            pDest = ImagePixelFormat( imageCompressionQuality, pDest.get(), sourceFormat );
        }

        return pDest;
    }


	inline mu::ImagePtr ImageReduceSize( const Image* pImage, const vec2<int32>& size, int32 imageCompressionQuality)
	{
		check( size[0] < pImage->GetSizeX() || size[1] < pImage->GetSizeY() );

		check(!(pImage->m_flags & Image::IF_CANNOT_BE_SCALED));

		mu::ImagePtr Result;
		int32 ResizeByDroppingMips = 0;

		// Is it smaller
		if (size[0] < pImage->GetSizeX() && size[1] < pImage->GetSizeY()
			&&
			// It is a multiple
			(pImage->GetSizeX() % size[0]) == 0 && (pImage->GetSizeY() % size[1]) == 0
			&&
			// It is the same multiple
			(pImage->GetSizeX() / size[0]) == (pImage->GetSizeY() / size[1]))
		{
			ResizeByDroppingMips = FMath::CountTrailingZeros64(pImage->GetSizeX() / size[0]);
		}

		if (ResizeByDroppingMips > 0 && pImage->GetLODCount() > ResizeByDroppingMips)
		{
			Result = pImage->Clone();
			Result->ReduceLODs(ResizeByDroppingMips);
		}
		else
		{
			if (IsCompressedFormat(pImage->GetFormat()))
			{
				EImageFormat format = GetUncompressedFormat(pImage->GetFormat());
				Result = ImagePixelFormat(
					imageCompressionQuality,
					pImage,
					format);
			}

			FImageSize blockSize((uint16)size[0], (uint16)size[1]);
			Result = ImageResizeLinear(imageCompressionQuality, pImage, blockSize);
		}

		return Result;
	}
}

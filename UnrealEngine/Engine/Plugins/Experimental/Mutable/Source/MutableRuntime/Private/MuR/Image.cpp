// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuR/Image.h"

#include "HAL/UnrealMemory.h"
#include "Math/UnrealMathSSE.h"
#include "MuR/ImagePrivate.h"
#include "MuR/MutableMath.h"
#include "MuR/MutableTrace.h"
#include "MuR/OpImagePixelFormat.h"
#include "MuR/OpImageResize.h"

namespace mu
{

    //---------------------------------------------------------------------------------------------
    static FImageFormatData s_imageFormatData[uint32(EImageFormat::IF_COUNT)] =
    {
		FImageFormatData( 0, 0, 0, 0 ),	// IF_NONE
		FImageFormatData( 1, 1, 3, 3 ),	// IF_RGB_UBYTE
		FImageFormatData( 1, 1, 4, 4 ),	// IF_RGBA_UBYTE
		FImageFormatData( 1, 1, 1, 1 ),	// IF_U_UBYTE

		FImageFormatData( 0, 0, 0, 0 ),	// IF_PVRTC2 (deprecated)
        FImageFormatData( 0, 0, 0, 0 ),	// IF_PVRTC4 (deprecated)
        FImageFormatData( 0, 0, 0, 0 ),	// IF_ETC1 (deprecated)
        FImageFormatData( 0, 0, 0, 0 ),	// IF_ETC2 (deprecated)

        FImageFormatData( 0, 0, 0, 1 ),	// IF_L_UBYTE_RLE
        FImageFormatData( 0, 0, 0, 3 ),	// IF_RGB_UBYTE_RLE
        FImageFormatData( 0, 0, 0, 4 ),	// IF_RGBA_UBYTE_RLE
        FImageFormatData( 0, 0, 0, 1 ),	// IF_L_UBIT_RLE

        FImageFormatData( 4, 4, 8,  4 ),	// IF_BC1
        FImageFormatData( 4, 4, 16, 4 ),	// IF_BC2
        FImageFormatData( 4, 4, 16, 4 ),	// IF_BC3
        FImageFormatData( 4, 4, 8,  1 ),	// IF_BC4
        FImageFormatData( 4, 4, 16, 2 ),	// IF_BC5
        FImageFormatData( 4, 4, 16, 3 ),	// IF_BC6
        FImageFormatData( 4, 4, 16, 4 ),	// IF_BC7

        FImageFormatData( 1, 1, 4, 4 ),	// IF_BGRA_UBYTE

        FImageFormatData( 4, 4, 16, 3 ),	// IF_ASTC_4x4_RGB_LDR
        FImageFormatData( 4, 4, 16, 4 ),	// IF_ASTC_4x4_RGBA_LDR
        FImageFormatData( 4, 4, 16, 2 ),	// IF_ASTC_4x4_RG_LDR
    };


    //---------------------------------------------------------------------------------------------
    const FImageFormatData& GetImageFormatData( EImageFormat format )
    {
        check( uint8(format) < int8(EImageFormat::IF_COUNT) );
        return s_imageFormatData[ uint8(format) ];
    }


    //---------------------------------------------------------------------------------------------
    Image::Image()
    {
    }


    //---------------------------------------------------------------------------------------------
    Image::Image( uint32_t sizeX, uint32_t sizeY, uint32_t lods, EImageFormat format )
    {
		MUTABLE_CPUPROFILER_SCOPE(NewImage)

        check( format!= EImageFormat::IF_NONE );
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

        m_format = format;
        m_size = FImageSize( (uint16)sizeX, (uint16)sizeY );
        m_lods = (uint8_t)lods;
        m_internalId = 0;

        const FImageFormatData& fdata = GetImageFormatData( format );
        int pixelsPerBlock = fdata.m_pixelsPerBlockX*fdata.m_pixelsPerBlockY;
        if ( pixelsPerBlock )
        {
			int32 DataSize = CalculateDataSize();
            m_data.SetNum( DataSize );
        }
    }


    //---------------------------------------------------------------------------------------------
    void Image::Serialise( const Image* p, OutputArchive& arch )
    {
        arch << *p;
    }


    //---------------------------------------------------------------------------------------------
    ImagePtr Image::StaticUnserialise( InputArchive& arch )
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
        ImagePtr pResult = new Image();
        arch >> *pResult;
        return pResult;
    }


	//---------------------------------------------------------------------------------------------
	ImagePtr Image::ExtractMip(int32 Mip) const
	{
		if (Mip == 0 && m_lods == 1)
		{
			return Clone();
		}

		vec2<int> MipSize = CalculateMipSize(Mip);

		int32 Quality = 4;

		if (m_lods > Mip)
		{
			const uint8* SourceData = GetMipData(Mip);
			int32 DataSize = CalculateDataSize(Mip);

			if (DataSize)
			{
				ImagePtr pResult = new Image(MipSize[0], MipSize[1], 1, m_format );
				uint8* DestData = pResult->GetData();
				FMemory::Memcpy(DestData, SourceData, DataSize);
				return pResult;
			}
			else
			{
				EImageFormat uncompressedFormat = GetUncompressedFormat(m_format);
				ImagePtr pTemp = ImagePixelFormat(Quality, this, uncompressedFormat);
				pTemp = pTemp->ExtractMip(Mip);
				ImagePtr pResult = ImagePixelFormat(Quality, pTemp.get(), m_format);
				return pResult;
			}
		}

		// We need to generate the mip
		// \TODO: optimize, quality
		return ImageResizeLinear(Quality, this, FImageSize(MipSize[0],MipSize[1]))->ExtractMip(0);
	}


    //---------------------------------------------------------------------------------------------
    uint16 Image::GetSizeX() const
    {
        return m_size[0];
    }


    //---------------------------------------------------------------------------------------------
    uint16 Image::GetSizeY() const
    {
        return m_size[1];
    }


	//---------------------------------------------------------------------------------------------
	const FImageSize& Image::GetSize() const
	{
		return m_size;
	}


    //---------------------------------------------------------------------------------------------
	EImageFormat Image::GetFormat() const
    {
        return m_format;
    }


    //---------------------------------------------------------------------------------------------
    int Image::GetLODCount() const
    {
        return FMath::Max( 1, (int)m_lods );
    }


    //---------------------------------------------------------------------------------------------
    const uint8_t* Image::GetData() const
    {
        return m_data.GetData();
    }


    //---------------------------------------------------------------------------------------------
    int32_t Image::GetDataSize() const
    {
        return m_data.Num();
    }


    //---------------------------------------------------------------------------------------------
    int32_t Image::GetLODDataSize( int lod ) const
    {
        return CalculateDataSize( lod );
    }


    //---------------------------------------------------------------------------------------------
    uint8_t* Image::GetData()
    {
		return m_data.GetData();
    }


    //---------------------------------------------------------------------------------------------
    uint32_t Image::GetId() const
    {
        return m_internalId;
    }


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
	int32 Image::CalculateDataSize() const
    {
		int32 res = 0;

        const FImageFormatData& fdata = GetImageFormatData( m_format );
        if (fdata.m_bytesPerBlock)
        {
            FImageSize s = m_size;

            for ( int l=0; l<FMath::Max(1,(int)m_lods); ++l )
            {
                int blocksX = FMath::DivideAndRoundUp( s[0], (uint16)fdata.m_pixelsPerBlockX );
                int blocksY = FMath::DivideAndRoundUp( s[1], (uint16)fdata.m_pixelsPerBlockY );

                res += (blocksX*blocksY) * fdata.m_bytesPerBlock;

                s[0] = FMath::DivideAndRoundUp( s[0], (uint16)2 );
                s[1] = FMath::DivideAndRoundUp( s[1], (uint16)2 );
            }
        }

        return res;
    }


    //---------------------------------------------------------------------------------------------
	int32 Image::CalculateDataSize( int mip ) const
    {
		int32 res = 0;

        const FImageFormatData& fdata = GetImageFormatData( m_format );
        if (fdata.m_bytesPerBlock)
        {
            FImageSize s = m_size;

            for ( int l=0; l< FMath::Max(1,(int)m_lods); ++l )
            {
                int blocksX = FMath::DivideAndRoundUp( s[0], (uint16)fdata.m_pixelsPerBlockX );
                int blocksY = FMath::DivideAndRoundUp( s[1], (uint16)fdata.m_pixelsPerBlockY );

                if ( mip==l )
                {
                    res = (blocksX*blocksY) * fdata.m_bytesPerBlock;
                    break;
                }

                s[0] = FMath::DivideAndRoundUp( s[0], (uint16)2 );
                s[1] = FMath::DivideAndRoundUp( s[1], (uint16)2 );
            }
        }

        return res;
    }


    //---------------------------------------------------------------------------------------------
    int32 Image::CalculatePixelCount() const
    {
		int32 res = 0;

        const FImageFormatData& fdata = GetImageFormatData( m_format );
        if (fdata.m_bytesPerBlock)
        {
            FImageSize s = m_size;

            for ( int l=0; l<FMath::Max(1,(int)m_lods); ++l )
            {
                int blocksX = FMath::DivideAndRoundUp( s[0], (uint16)fdata.m_pixelsPerBlockX );
                int blocksY = FMath::DivideAndRoundUp( s[1], (uint16)fdata.m_pixelsPerBlockY );

                res += (blocksX*blocksY) * fdata.m_pixelsPerBlockX * fdata.m_pixelsPerBlockY;

                s[0] = FMath::DivideAndRoundUp( s[0], (uint16)2 );
                s[1] = FMath::DivideAndRoundUp( s[1], (uint16)2 );
            }
        }
        else
        {
            // An RLE image.
            for ( int l=0; l<FMath::Max(1,(int)m_lods); ++l )
            {
                auto mipSize = CalculateMipSize(l);
                res += mipSize[0] * mipSize[1];
            }
        }

        return res;
    }


    //---------------------------------------------------------------------------------------------
	int32 Image::CalculatePixelCount( int mip ) const
    {
		int32 res = 0;

        const FImageFormatData& fdata = GetImageFormatData( m_format );
        if (fdata.m_bytesPerBlock)
        {
            FImageSize s = m_size;

            for ( int l=0; l<mip+1; ++l )
            {
                int blocksX = FMath::DivideAndRoundUp( s[0], (uint16)fdata.m_pixelsPerBlockX );
                int blocksY = FMath::DivideAndRoundUp( s[1], (uint16)fdata.m_pixelsPerBlockY );

                if ( l==mip )
                {
                    res = (blocksX*blocksY) * fdata.m_pixelsPerBlockX * fdata.m_pixelsPerBlockY;
                }

                s[0] = FMath::DivideAndRoundUp( s[0], (uint16)2 );
                s[1] = FMath::DivideAndRoundUp( s[1], (uint16)2 );
            }
        }
        else
        {
            // An RLE image.
            auto mipSize = CalculateMipSize(mip);
            res += mipSize[0] * mipSize[1];
        }

        return res;
    }


    //---------------------------------------------------------------------------------------------
    vec2<int> Image::CalculateMipSize( int mip ) const
    {
        vec2<int> res;

        FImageSize s = m_size;

        for ( int l=0; l<mip+1; ++l )
        {
            if ( l==mip )
            {
                res[0] = s[0];
                res[1] = s[1];
                return res;
            }

            s[0] = FMath::DivideAndRoundUp( s[0], (uint16)2 );
            s[1] = FMath::DivideAndRoundUp( s[1], (uint16)2 );
        }

        return res;
    }

	
	//---------------------------------------------------------------------------------------------
	int Image::GetMipmapCount(int SizeX, int SizeY)
	{
		if (SizeX <= 0 || SizeY <= 0)
		{
			return 0;
		}

		int MaxLevel = FMath::CeilLogTwo(FMath::Max(SizeX, SizeY)) + 1;
		return MaxLevel;
	}


	//---------------------------------------------------------------------------------------------
	const uint8_t* Image::GetMipData(int mip) const
	{
		check((mip >= 0 && mip < m_lods) || (m_lods == 0));

		const uint8_t* pResult = m_data.GetData();

		const FImageFormatData& fdata = GetImageFormatData(m_format);

		if (fdata.m_bytesPerBlock)
		{
			// Fixed-sized formats
			FImageSize s = m_size;

			for (int l = 0; l < mip; ++l)
			{
				int blocksX = FMath::DivideAndRoundUp(s[0], (uint16)fdata.m_pixelsPerBlockX);
				int blocksY = FMath::DivideAndRoundUp(s[1], (uint16)fdata.m_pixelsPerBlockY);

				pResult += (blocksX * blocksY) * fdata.m_bytesPerBlock;

				s[0] = FMath::DivideAndRoundUp(s[0], (uint16)2);
				s[1] = FMath::DivideAndRoundUp(s[1], (uint16)2);
			}
		}

		else if (m_format == EImageFormat::IF_L_UBYTE_RLE ||
			m_format == EImageFormat::IF_L_UBIT_RLE)
		{
			// Every mip has variable size, but it is stored in the first 4 bytes.
			for (int l = 0; l < mip; ++l)
			{
				uint32_t mipSize = *(const uint32_t*)pResult;
				pResult += mipSize;
			}
		}
		else
		{
			checkf(false, TEXT("Trying to get mipmap pointer in an unsupported pixel format."));
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	uint8_t* Image::GetMipData(int mip)
	{
		check((mip >= 0 && mip < m_lods) || (m_lods == 0));

		uint8_t* pResult = m_data.GetData();

		const FImageFormatData& fdata = GetImageFormatData(m_format);

		if (fdata.m_bytesPerBlock)
		{
			// Fixed-sized formats
			FImageSize s = m_size;

			for (int l = 0; l < mip; ++l)
			{
				int blocksX = FMath::DivideAndRoundUp(s[0], (uint16)fdata.m_pixelsPerBlockX);
				int blocksY = FMath::DivideAndRoundUp(s[1], (uint16)fdata.m_pixelsPerBlockY);

				pResult += (blocksX * blocksY) * fdata.m_bytesPerBlock;

				s[0] = FMath::DivideAndRoundUp(s[0], (uint16)2);
				s[1] = FMath::DivideAndRoundUp(s[1], (uint16)2);
			}
		}

		else if (m_format == EImageFormat::IF_L_UBYTE_RLE ||
			m_format == EImageFormat::IF_L_UBIT_RLE)
		{
			// Every mip has variable size, but it is stored in the first 4 bytes.
			for (int l = 0; l < mip; ++l)
			{
				uint32_t mipSize = *(const uint32_t*)pResult;
				pResult += mipSize;
			}
		}
		else
		{
			checkf(false, TEXT("Trying to get mipmap pointer in an unsupported pixel format."));
		}

		return pResult;
	}

	int32 Image::GetMipsDataSize() const
	{
		int32 res = 0;

        const FImageFormatData& fdata = GetImageFormatData( m_format );
        if (fdata.m_bytesPerBlock)
        {
			return CalculateDataSize();
        }
		else if (m_format == EImageFormat::IF_L_UBYTE_RLE ||
				 m_format == EImageFormat::IF_L_UBIT_RLE)
		{	
			if (m_data.Num())
			{ 
			    // Every mip has variable size, but it is stored in the first 4 bytes.
				const uint8* pMipData = m_data.GetData();
				for (int l = 0; l < m_lods; ++l)
				{
					const uint32_t mipSize = *(const uint32_t*)pMipData;
					pMipData += mipSize;
					res += mipSize;
				}	
			}
		}
		else
		{
			checkf(false, TEXT("Trying to get mips data size in an unsupported pixel format."));
		}

        return res;
	}

    //---------------------------------------------------------------------------------------------
    vec4<float> Image::Sample( vec2<float> coords ) const
    {
        vec4<float> result;

        if (m_size[0]==0 || m_size[1]==0) { return result; }

        const FImageFormatData& fdata = GetImageFormatData( m_format );

        int pixelX = FMath::Max( 0, FMath::Min( m_size[0]-1, (int)(coords[0] * m_size[0] )));
        int blockX = pixelX / fdata.m_pixelsPerBlockX;
        int blockPixelX = pixelX % fdata.m_pixelsPerBlockX;

        int pixelY = FMath::Max( 0, FMath::Min( m_size[1]-1, (int)(coords[1] * m_size[1] )));
        int blockY = pixelY / fdata.m_pixelsPerBlockY;
        int blockPixelY = pixelY % fdata.m_pixelsPerBlockY;

        int blocksPerRow = m_size[0] / fdata.m_pixelsPerBlockX;
        int blockOffset = blockX + blockY * blocksPerRow;

        // Non-generic part
        if ( m_format== EImageFormat::IF_RGB_UBYTE )
        {
            int byteOffset = blockOffset * fdata.m_bytesPerBlock
                    + ( blockPixelY * fdata.m_pixelsPerBlockX + blockPixelX ) * 3;

            result[0] = m_data[ byteOffset+0 ] / 255.0f;
            result[1] = m_data[ byteOffset+1 ] / 255.0f;
            result[2] = m_data[ byteOffset+2 ] / 255.0f;
            result[3] = 1;
        }
        else if ( m_format== EImageFormat::IF_RGBA_UBYTE )
        {
            int byteOffset = blockOffset * fdata.m_bytesPerBlock
                    + ( blockPixelY * fdata.m_pixelsPerBlockX + blockPixelX ) * 4;

            result[0] = m_data[ byteOffset+0 ] / 255.0f;
            result[1] = m_data[ byteOffset+1 ] / 255.0f;
            result[2] = m_data[ byteOffset+2 ] / 255.0f;
            result[3] = m_data[ byteOffset+3 ] / 255.0f;
        }
        else if ( m_format== EImageFormat::IF_BGRA_UBYTE )
        {
            int byteOffset = blockOffset * fdata.m_bytesPerBlock
                    + ( blockPixelY * fdata.m_pixelsPerBlockX + blockPixelX ) * 4;

            result[0] = m_data[ byteOffset+2 ] / 255.0f;
            result[1] = m_data[ byteOffset+1 ] / 255.0f;
            result[2] = m_data[ byteOffset+0 ] / 255.0f;
            result[3] = m_data[ byteOffset+3 ] / 255.0f;
        }
        else if ( m_format== EImageFormat::IF_L_UBYTE )
        {
            int byteOffset = blockOffset * fdata.m_bytesPerBlock
                    + ( blockPixelY * fdata.m_pixelsPerBlockX + blockPixelX ) * 1;

            result[0] = m_data[ byteOffset ] / 255.0f;
            result[1] = m_data[ byteOffset ] / 255.0f;
            result[2] = m_data[ byteOffset ] / 255.0f;
            result[3] = 1.0f;
        }
        else
        {
            check( false );
        }

        return result;
    }

    //---------------------------------------------------------------------------------------------
    bool Image::IsPlainColour( vec4<float>& colour ) const
    {
        bool res = true;

        int pixelCount = m_size[0] * m_size[1];

        if ( pixelCount )
        {
            switch( m_format )
            {
            case EImageFormat::IF_L_UBYTE:
            {
                const uint8_t* pData = &m_data[0];
                uint8_t v = pData[0];

                for ( int p=0; res && p<pixelCount; ++p )
                {
                    uint8_t nv = *pData++;
                    res &= ( nv==v );
                }

                if (res)
                {
                    colour[0] = colour[1] = colour[2] = float(v)/255.0f;
                    colour[3] = 1.0f;
                }
                break;
            }

            default:
                res = false;
                break;
            }
        }

        return res;
    }


    //---------------------------------------------------------------------------------------------
    bool Image::IsFullAlpha() const
    {
        int pixelCount = m_size[0] * m_size[1];

        if ( pixelCount )
        {
            switch( m_format )
            {
            case EImageFormat::IF_RGBA_UBYTE:
            case EImageFormat::IF_BGRA_UBYTE:
            {
                const uint8_t* pData = &m_data[3];

                for ( int p=0; p<pixelCount; ++p )
                {
                    uint8_t nv = *pData;
                    if (nv!=255) return false;
                    pData+=4;
                }
                return true;
                break;
            }

            case EImageFormat::IF_RGB_UBYTE:
            {
                return true;
                break;
            }

            default:
                return false;
            }
        }

        return true;
    }


    //---------------------------------------------------------------------------------------------
    namespace
    {
        bool is_zero( const uint8_t* buff, size_t size )
        {
            return (*buff==0) && (FMemory::Memcmp( buff, buff + 1, size - 1 )==0);
        }
    }


    //---------------------------------------------------------------------------------------------
     void Image::GetNonBlackRect_Reference(FImageRect& rect) const
     {            
         rect.min[0] = rect.min[1] = 0;
         rect.size = m_size;

         if ( !rect.size[0] || !rect.size[1] )
             return;

         bool first = true;
         uint16 sx = m_size[0];
         uint16 sy = m_size[1];

         // Slow reference implementation
         uint16 top = 0;
         uint16 left = 0;
         uint16 right = sx - 1;
         uint16 bottom = sy - 1;

         for ( uint16 y=0; y<sy; ++y )
         {
             for ( uint16 x=0; x<sx; ++x )
             {
                 bool black = false;
                 switch( m_format )
                 {
                 case EImageFormat::IF_L_UBYTE:
                 {
                     uint8_t nv = m_data[y*sx+x];
                     if (nv==0)
                     {
                         black = true;
                     }
                     break;
                 }

                 case EImageFormat::IF_RGB_UBYTE:
                 {
                     const uint8_t* pData = &m_data[(y*sx+x)*3];
                     if (!pData[0] && !pData[1] && !pData[2])
                     {
                         black = true;
                     }
                     break;
                 }

                 case EImageFormat::IF_RGBA_UBYTE:
                 case EImageFormat::IF_BGRA_UBYTE:
                 {
                     const uint8_t* pData = &m_data[(y*sx+x)*4];
                     if (!pData[0] && !pData[1] && !pData[2] && !pData[3])
                     {
                         black = true;
                     }
                     break;
                 }

                 default:
                     check(false);
                     break;
                 }

                 if (!black)
                 {
                     if (first)
                     {
                         first = false;
                         left = right = x;
                         top = bottom = y;
                     }
                     else
                     {
                         left = FMath::Min( left, x );
                         right = FMath::Max( right, x );
                         top = FMath::Min( top, y );
                         bottom = FMath::Max( bottom, y );
                     }
                 }
             }
         }

         rect.min[0] = left;
         rect.min[1] = top;
         rect.size[0] = right - left + 1;
         rect.size[1] = bottom - top + 1;
     }


    //---------------------------------------------------------------------------------------------
    void Image::GetNonBlackRect(FImageRect& rect) const
    {            
		// TODO: There is a bug here with cyborg-windows.
		// Meanwhile do this.
		GetNonBlackRect_Reference(rect);
		return;

  //      rect.min[0] = rect.min[1] = 0;
  //      rect.size = m_size;

		//check(rect.size[0] > 0);
		//check(rect.size[1] > 0);

  //      if ( !rect.size[0] || !rect.size[1] )
  //          return;

  //      uint16 sx = m_size[0];
  //      uint16 sy = m_size[1];

  //      // Somewhat faster implementation
  //      uint16 top = 0;
  //      uint16 left = 0;
  //      uint16 right = sx - 1;
  //      uint16 bottom = sy - 1;

  //      switch ( m_format )
  //      {
  //      case IF_L_UBYTE:
  //      {
  //          size_t rowStride = sx;

  //          // Find top
  //          const uint8_t* pRow = m_data.GetData();
  //          while ( top < sy )
  //          {
  //              if ( !is_zero( pRow, rowStride ) )
  //                  break;
  //              pRow += rowStride;
  //              ++top;
  //          }

  //          // Find bottom
  //          pRow = m_data.GetData() + rowStride * bottom;
  //          while ( bottom > top )
  //          {
  //              if ( !is_zero( pRow, rowStride ) )
  //                  break;
  //              pRow -= rowStride;
  //              --bottom;
  //          }

  //          // Find left and right
  //          left = sx - 1;
  //          right = 0;
  //          int16_t currentRow = top;
  //          pRow = m_data.GetData() + rowStride * currentRow;
  //          while ( currentRow <= bottom && ( left > 0 || right < ( sx - 1 ) ) )
  //          {
  //              for ( uint16 x = 0; x < left; ++x )
  //              {
  //                  if ( pRow[x] )
  //                  {
  //                      left = x;
  //                      break;
  //                  }
  //              }
  //              for ( uint16 x = sx - 1; x > right; --x )
  //              {
  //                  if ( pRow[x] )
  //                  {
  //                      right = x;
  //                      break;
  //                  }
  //              }
  //              pRow += rowStride;
  //              ++currentRow;
  //          }

  //          break;
  //      }

  //      case IF_RGB_UBYTE:
  //      case IF_RGBA_UBYTE:
  //      case IF_BGRA_UBYTE:
  //      {
  //          size_t bytesPerPixel = GetImageFormatData( m_format ).m_bytesPerBlock;
  //          size_t rowStride = sx * bytesPerPixel;

  //          // Find top
  //          const uint8_t* pRow = m_data.GetData();
  //          while(top<sy)
  //          {
  //              if ( !is_zero( pRow, rowStride ) )
  //                  break;
  //              pRow += rowStride;
  //              ++top;
  //          }

  //          // Find bottom
  //          pRow = m_data.GetData() + rowStride * bottom;
  //          while ( bottom > top )
  //          {
  //              if ( !is_zero( pRow, rowStride ) )
  //                  break;
  //              pRow -= rowStride;
  //              --bottom;
  //          }

  //          // Find left and right
  //          left = sx - 1;
  //          right = 0;
  //          int16_t currentRow = top;
  //          pRow = m_data.GetData() + rowStride * currentRow;
  //          uint8_t zeroPixel[16] = { 0 };
  //          check(bytesPerPixel<16);
  //          while ( currentRow <= bottom && (left > 0 || right<(sx-1)) )
  //          {
  //              for ( uint16 x = 0; x < left; ++x )
  //              {
  //                  if ( memcmp( pRow + x * bytesPerPixel, zeroPixel, bytesPerPixel ) )
  //                  {
  //                      left = x;
  //                      break;
  //                  }
  //              }
  //              for ( uint16 x = sx - 1; x > right; --x )
  //              {
  //                  if ( memcmp( pRow + x * bytesPerPixel, zeroPixel, bytesPerPixel ) )
  //                  {
  //                      right = x;
  //                      break;
  //                  }
  //              }
  //              pRow += rowStride;
  //              ++currentRow;
  //          }

  //          break;
  //      }

  //      default:
  //          check(false);
  //          break;
  //      }

  //      rect.min[0] = left;
  //      rect.min[1] = top;
  //      rect.size[0] = right - left + 1;
  //      rect.size[1] = bottom - top + 1;

  //      // debug
  //       FImageRect debugRect;
  //       GetNonBlackRect_Reference( debugRect );
  //       if (!(rect==debugRect))
  //       {
  //           mu::Halt();
  //       }
    }


	//---------------------------------------------------------------------------------------------
	void Image::ReduceLODsTo(int32 NewLODCount)
	{
		bool bIsBlockBased = GetImageFormatData(m_format).m_bytesPerBlock > 0;
		int32 MaxLODs = GetMipmapCount(m_size[0], m_size[1]);
		int32 LODSToSkip = MaxLODs - NewLODCount;
		if (LODSToSkip > 0 && bIsBlockBased)
		{
			check(LODSToSkip < m_lods);
			
			uint32 DataToSkip = 0;
			for (int32 l = 0; l < LODSToSkip; ++l)
			{
				uint32 MipDataSize = CalculateDataSize(l);
				check(MipDataSize > 0);
				DataToSkip += MipDataSize;
			}

			vec2<int> FinalSize = CalculateMipSize(LODSToSkip);
			m_size[0] = uint16(FinalSize[0]);
			m_size[1] = uint16(FinalSize[1]);
			m_lods = NewLODCount;

			int32 FinalDataSize = m_data.Num() - int32(DataToSkip);
			FMemory::Memmove(m_data.GetData(), m_data.GetData() + DataToSkip, FinalDataSize);
		}
	}


	//---------------------------------------------------------------------------------------------
	void Image::ReduceLODs(int32 LODSToSkip)
	{
		bool bIsBlockBased = GetImageFormatData(m_format).m_bytesPerBlock > 0;
		if (LODSToSkip > 0 && bIsBlockBased)
		{
			check(LODSToSkip < m_lods);
			
			uint32 DataToSkip = 0;
			for (int32 l = 0; l < LODSToSkip; ++l)
			{
				uint32 MipDataSize = CalculateDataSize(l);
				check(MipDataSize>0);
				DataToSkip += MipDataSize;
			}

			vec2<int> FinalSize = CalculateMipSize(LODSToSkip);
			m_size[0] = uint16(FinalSize[0]);
			m_size[1] = uint16(FinalSize[1]);
			m_lods -= LODSToSkip;

			int32 FinalDataSize = m_data.Num() - int32(DataToSkip);
			FMemory::Memmove(m_data.GetData(), m_data.GetData() + DataToSkip, FinalDataSize);
		}
	}


}


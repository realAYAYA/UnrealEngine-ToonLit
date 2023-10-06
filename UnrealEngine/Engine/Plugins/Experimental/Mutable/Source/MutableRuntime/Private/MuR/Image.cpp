// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuR/Image.h"

#include "HAL/UnrealMemory.h"
#include "HAL/IConsoleManager.h"
#include "Math/UnrealMathSSE.h"
#include "Math/NumericLimits.h"
#include "MuR/ImagePrivate.h"
#include "MuR/MutableMath.h"
#include "MuR/MutableTrace.h"

#include <initializer_list>

namespace
{
	static bool bDisableCompressedImageBlackBlockInit = false;
	static FAutoConsoleVariableRef CVarDisableCompressedImageBlackBlockInit(
		TEXT("mutable.DisableCompressedImageBlackBlockInit"),
		bDisableCompressedImageBlackBlockInit,
		TEXT("A value of 1 disables mutable compressed black block initialization"),
		ECVF_Default);
}

namespace mu
{
	MUTABLE_IMPLEMENT_ENUM_SERIALISABLE( EBlendType );
	MUTABLE_IMPLEMENT_ENUM_SERIALISABLE( EMipmapFilterType );
	MUTABLE_IMPLEMENT_ENUM_SERIALISABLE( ECompositeImageMode );
	MUTABLE_IMPLEMENT_ENUM_SERIALISABLE( ESamplingMethod );
	MUTABLE_IMPLEMENT_ENUM_SERIALISABLE( EMinFilterMethod );
	MUTABLE_IMPLEMENT_ENUM_SERIALISABLE( EImageFormat );

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

        FImageFormatData( 1, 1, 4, 4 ),		// IF_BGRA_UBYTE

		FImageFormatData(4, 4, 16, 3, {252, 253, 255, 255, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 255, 255}), // IF_ASTC_4x4_RGB_LDR
		FImageFormatData(4, 4, 16, 4, {252, 253, 255, 255, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0}),     // IF_ASTC_4x4_RGBA_LDR
        FImageFormatData( 4, 4, 16, 2 ),	// IF_ASTC_4x4_RG_LDR // TODO: check black block for RG.
		
		FImageFormatData(8, 8, 16, 3),		// IF_ASTC_8x8_RGB_LDR,
		FImageFormatData(8, 8, 16, 4),		// IF_ASTC_8x8_RGBA_LDR,
		FImageFormatData(8, 8, 16, 2),		// IF_ASTC_8x8_RG_LDR,
		FImageFormatData(12, 12, 16, 3),	// IF_ASTC_12x12_RGB_LDR
		FImageFormatData(12, 12, 16, 4),	// IF_ASTC_12x12_RGBA_LDR
		FImageFormatData(12, 12, 16, 2),	// IF_ASTC_12x12_RG_LDR

    };


    //---------------------------------------------------------------------------------------------
    const FImageFormatData& GetImageFormatData( EImageFormat format )
    {
        check( format < EImageFormat::IF_COUNT );
        return s_imageFormatData[ uint8(format) ];
    }


	//---------------------------------------------------------------------------------------------
	void FMipmapGenerationSettings::Serialise(OutputArchive& arch) const
	{
		uint32 ver = 0;
		arch << ver;

		arch << m_sharpenFactor;
		arch << m_filterType;
		arch << m_ditherMipmapAlpha;
	}

	void FMipmapGenerationSettings::Unserialise(InputArchive& arch)
	{
		uint32 ver = 0;
		arch >> ver;
		check(ver == 0);

		arch >> m_sharpenFactor;
		arch >> m_filterType;
		arch >> m_ditherMipmapAlpha;
	}


    //---------------------------------------------------------------------------------------------
    Image::Image()
    {
    }


    //---------------------------------------------------------------------------------------------
    Image::Image( uint32 sizeX, uint32 sizeY, uint32 lods, EImageFormat format, EInitializationType Init )
    {
		MUTABLE_CPUPROFILER_SCOPE(NewImage)
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

        check(format != EImageFormat::IF_NONE);
		check(format < EImageFormat::IF_COUNT);

		check(sizeX <= TNumericLimits<uint16>::Max());
		check(sizeY <= TNumericLimits<uint16>::Max());
		check(lods <= TNumericLimits<uint8>::Max());

		// TODO: check that lods is sensible for the size.

        m_format = format;
        m_size = FImageSize( (uint16)sizeX, (uint16)sizeY );
        m_lods = (uint8)lods;

        const FImageFormatData& fdata = GetImageFormatData( format );
        int32 PixelsPerBlock = fdata.PixelsPerBlockX*fdata.PixelsPerBlockY;
        if (PixelsPerBlock)
        {
			if (Init == EInitializationType::Black)
			{
				InitToBlack();
			}
			else
			{
				int32 DataSize = CalculateDataSize();
				m_data.SetNumUninitialized(DataSize);
			}
        }
    }


	//---------------------------------------------------------------------------------------------
	void Image::InitToBlack()
	{
		int32 DataSize = CalculateDataSize();
		if (bDisableCompressedImageBlackBlockInit)
		{
			m_data.SetNumUninitialized(DataSize);
			// Do it in separate steps in case we are reusing a buffer.
			FMemory::Memzero(m_data.GetData(), DataSize);
		}
		else
		{
			const FImageFormatData& fdata = GetImageFormatData(m_format);
			check(fdata.BytesPerBlock <= FImageFormatData::MAX_BYTES_PER_BLOCK);

			constexpr uint8 ZeroedBlock[FImageFormatData::MAX_BYTES_PER_BLOCK] = { 0 };

			const SIZE_T BlockSizeSanitized = FMath::Min<SIZE_T>(FImageFormatData::MAX_BYTES_PER_BLOCK, fdata.BytesPerBlock);
			const bool bIsFormatBlackBlockZeroed = FMemory::Memcmp(fdata.BlackBlock, ZeroedBlock, BlockSizeSanitized) == 0;

			if (bIsFormatBlackBlockZeroed)
			{
				m_data.SetNumUninitialized(DataSize);
				// Do it in separate steps in case we are reusing a buffer.
				FMemory::Memzero(m_data.GetData(), DataSize);
			}
			else
			{
				m_data.SetNumUninitialized(DataSize);

				check(fdata.BytesPerBlock > 0);
				for (int32 BlockDataOffset = 0; BlockDataOffset < DataSize; BlockDataOffset += fdata.BytesPerBlock)
				{
					FMemory::Memcpy(m_data.GetData() + BlockDataOffset, fdata.BlackBlock, fdata.BytesPerBlock);
				}
			}
		}

		m_flags = 0;
		RelevancyMinY = 0;
		RelevancyMaxY = 0;
	}

	//---------------------------------------------------------------------------------------------
	Ptr<Image> Image::CreateAsReference(uint32 ID)
	{
		Ptr<Image> Result = new Image;
		Result->ReferenceID = ID;
		Result->m_flags = EImageFlags::IF_IS_REFERENCE;
		return Result;
	}


    //---------------------------------------------------------------------------------------------
    void Image::Serialise( const Image* p, OutputArchive& arch )
    {
        arch << *p;
    }

	void Image::Serialise(OutputArchive& arch) const
	{
		uint32 ver = 3;
		arch << ver;

		arch << m_size;
		arch << m_lods;
		arch << (uint8)m_format;
		arch << m_data;

		// Remove non-persistent flags.
		uint8 flags = m_flags & ~IF_HAS_RELEVANCY_MAP;
		arch << flags;
	}

	void Image::Unserialise(InputArchive& arch)
	{
		uint32 ver;
		arch >> ver;
		check(ver == 3);

		arch >> m_size;
		arch >> m_lods;

		uint8 format;
		arch >> format;
		m_format = (EImageFormat)format;
		arch >> m_data;

		arch >> m_flags;
	}


    //---------------------------------------------------------------------------------------------
    Ptr<Image> Image::StaticUnserialise( InputArchive& arch )
    {
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
        Ptr<Image> pResult = new Image();
        arch >> *pResult;
        return pResult;
    }


	//---------------------------------------------------------------------------------------------
	Ptr<Image> FImageOperator::ExtractMip(const Image* This, int32 Mip)
	{
		if (Mip == 0 && This->m_lods == 1)
		{
			return CloneImage(This);
		}

		FIntVector2 MipSize = This->CalculateMipSize(Mip);

		int32 Quality = 4;

		if (This->m_lods > Mip)
		{
			const uint8* SourceData = This->GetMipData(Mip);
			int32 DataSize = This->CalculateDataSize(Mip);

			if (DataSize)
			{
				Ptr<Image> pResult = CreateImage(MipSize[0], MipSize[1], 1, This->m_format, EInitializationType::NotInitialized);
				pResult->m_flags = This->m_flags;
				uint8* DestData = pResult->GetData();
				FMemory::Memcpy(DestData, SourceData, DataSize);
				return pResult;
			}
			else
			{
				EImageFormat uncompressedFormat = GetUncompressedFormat(This->m_format);
				// TODO: OnlyLOD=Mip and then ExtractMip(0)?
				Ptr<Image> Temp = ImagePixelFormat(Quality, This, uncompressedFormat);
				Temp = ExtractMip(Temp.get(), Mip);
				Ptr<Image> Result = ImagePixelFormat(Quality, Temp.get(), This->m_format);
				Result->m_flags = This->m_flags;
				ReleaseImage(Temp);
				return Result;
			}
		}

		// We need to generate the mip
		// \TODO: optimize, quality
		Ptr<Image> Resized = CreateImage(MipSize[0], MipSize[1],1,This->GetFormat(),EInitializationType::NotInitialized);
		ImageResizeLinear(Resized.get(), Quality, This);
		Ptr<Image> Result = ExtractMip( Resized.get(), 0);
		ReleaseImage(Resized);
		return Result;
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
    const uint8* Image::GetData() const
    {
        return m_data.GetData();
    }


    //---------------------------------------------------------------------------------------------
    int32 Image::GetDataSize() const
    {
        return m_data.Num();
    }


    //---------------------------------------------------------------------------------------------
    int32 Image::GetLODDataSize( int lod ) const
    {
        return CalculateDataSize( lod );
    }


    //---------------------------------------------------------------------------------------------
    uint8* Image::GetData()
    {
		return m_data.GetData();
    }

	//---------------------------------------------------------------------------------------------
	bool Image::IsReference() const
	{
		return m_flags & EImageFlags::IF_IS_REFERENCE;
	}

	//---------------------------------------------------------------------------------------------
	uint32 Image::GetReferencedTexture() const
	{
		ensure(IsReference());
		return ReferenceID;
	}


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	int32 Image::CalculateDataSize( int32 SizeX, int32 SizeY, int32 LodCount, EImageFormat Format )
	{
		int32 res = 0;

		const FImageFormatData& fdata = GetImageFormatData(Format);
		if (fdata.BytesPerBlock)
		{
			for (int32 LodIndex = 0; LodIndex < FMath::Max(1, LodCount); ++LodIndex)
			{
				int32 blocksX = FMath::DivideAndRoundUp(SizeX, int32(fdata.PixelsPerBlockX));
				int32 blocksY = FMath::DivideAndRoundUp(SizeY, int32(fdata.PixelsPerBlockY));

				res += (blocksX * blocksY) * fdata.BytesPerBlock;

				SizeX = FMath::DivideAndRoundUp(SizeX, 2);
				SizeY = FMath::DivideAndRoundUp(SizeY, 2);
			}
		}

		return res;
	}


	//---------------------------------------------------------------------------------------------
	int32 Image::CalculateDataSize() const
	{
		return CalculateDataSize( m_size[0], m_size[1], m_lods, m_format );
	}


	//---------------------------------------------------------------------------------------------
	int32 Image::CalculateDataSize( int mip ) const
    {
		int32 res = 0;

        const FImageFormatData& fdata = GetImageFormatData( m_format );
        if (fdata.BytesPerBlock)
        {
            FImageSize s = m_size;

            for ( int l=0; l< FMath::Max(1,(int)m_lods); ++l )
            {
                int blocksX = FMath::DivideAndRoundUp( s[0], (uint16)fdata.PixelsPerBlockX );
                int blocksY = FMath::DivideAndRoundUp( s[1], (uint16)fdata.PixelsPerBlockY );

                if ( mip==l )
                {
                    res = (blocksX*blocksY) * fdata.BytesPerBlock;
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
        if (fdata.BytesPerBlock)
        {
            FImageSize s = m_size;

            for ( int l=0; l<FMath::Max(1,(int)m_lods); ++l )
            {
                int blocksX = FMath::DivideAndRoundUp( s[0], (uint16)fdata.PixelsPerBlockX );
                int blocksY = FMath::DivideAndRoundUp( s[1], (uint16)fdata.PixelsPerBlockY );

                res += (blocksX*blocksY) * fdata.PixelsPerBlockX * fdata.PixelsPerBlockY;

                s[0] = FMath::DivideAndRoundUp( s[0], (uint16)2 );
                s[1] = FMath::DivideAndRoundUp( s[1], (uint16)2 );
            }
        }
        else
        {
            // An RLE image.
            for ( int l=0; l<FMath::Max(1,(int)m_lods); ++l )
            {
				FIntVector2 mipSize = CalculateMipSize(l);
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
        if (fdata.BytesPerBlock)
        {
            FImageSize s = m_size;

            for ( int l=0; l<mip+1; ++l )
            {
                int blocksX = FMath::DivideAndRoundUp( s[0], (uint16)fdata.PixelsPerBlockX );
                int blocksY = FMath::DivideAndRoundUp( s[1], (uint16)fdata.PixelsPerBlockY );

                if ( l==mip )
                {
                    res = (blocksX*blocksY) * fdata.PixelsPerBlockX * fdata.PixelsPerBlockY;
                }

                s[0] = FMath::DivideAndRoundUp( s[0], (uint16)2 );
                s[1] = FMath::DivideAndRoundUp( s[1], (uint16)2 );
            }
        }
        else
        {
            // An RLE image.
			FIntVector2 mipSize = CalculateMipSize(mip);
            res += mipSize[0] * mipSize[1];
        }

        return res;
    }


    //---------------------------------------------------------------------------------------------
	FIntVector2 Image::CalculateMipSize( int mip ) const
    {
		FIntVector2 res(0,0);

		FIntVector2 s = FIntVector2(m_size[0], m_size[1]);

        for ( int l=0; l<mip+1; ++l )
        {
            if ( l==mip )
            {
                res[0] = s[0];
                res[1] = s[1];
                return res;
            }

            s[0] = FMath::DivideAndRoundUp( s[0], 2 );
            s[1] = FMath::DivideAndRoundUp( s[1], 2 );
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

		if (fdata.BytesPerBlock)
		{
			// Fixed-sized formats
			FImageSize s = m_size;

			for (int l = 0; l < mip; ++l)
			{
				int blocksX = FMath::DivideAndRoundUp(s[0], (uint16)fdata.PixelsPerBlockX);
				int blocksY = FMath::DivideAndRoundUp(s[1], (uint16)fdata.PixelsPerBlockY);

				pResult += (blocksX * blocksY) * fdata.BytesPerBlock;

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

		if (fdata.BytesPerBlock)
		{
			// Fixed-sized formats
			FImageSize s = m_size;

			for (int l = 0; l < mip; ++l)
			{
				int blocksX = FMath::DivideAndRoundUp(s[0], (uint16)fdata.PixelsPerBlockX);
				int blocksY = FMath::DivideAndRoundUp(s[1], (uint16)fdata.PixelsPerBlockY);

				pResult += (blocksX * blocksY) * fdata.BytesPerBlock;

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
        if (fdata.BytesPerBlock)
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
    FVector4f Image::Sample( FVector2f coords ) const
    {
		FVector4f result;

        if (m_size[0]==0 || m_size[1]==0) { return result; }

        const FImageFormatData& fdata = GetImageFormatData( m_format );

        int pixelX = FMath::Max( 0, FMath::Min( m_size[0]-1, (int)(coords[0] * m_size[0] )));
        int blockX = pixelX / fdata.PixelsPerBlockX;
        int blockPixelX = pixelX % fdata.PixelsPerBlockX;

        int pixelY = FMath::Max( 0, FMath::Min( m_size[1]-1, (int)(coords[1] * m_size[1] )));
        int blockY = pixelY / fdata.PixelsPerBlockY;
        int blockPixelY = pixelY % fdata.PixelsPerBlockY;

        int blocksPerRow = m_size[0] / fdata.PixelsPerBlockX;
        int blockOffset = blockX + blockY * blocksPerRow;

        // Non-generic part
        if ( m_format== EImageFormat::IF_RGB_UBYTE )
        {
            int byteOffset = blockOffset * fdata.BytesPerBlock
                    + ( blockPixelY * fdata.PixelsPerBlockX + blockPixelX ) * 3;

            result[0] = m_data[ byteOffset+0 ] / 255.0f;
            result[1] = m_data[ byteOffset+1 ] / 255.0f;
            result[2] = m_data[ byteOffset+2 ] / 255.0f;
            result[3] = 1;
        }
        else if ( m_format== EImageFormat::IF_RGBA_UBYTE )
        {
            int byteOffset = blockOffset * fdata.BytesPerBlock
                    + ( blockPixelY * fdata.PixelsPerBlockX + blockPixelX ) * 4;

            result[0] = m_data[ byteOffset+0 ] / 255.0f;
            result[1] = m_data[ byteOffset+1 ] / 255.0f;
            result[2] = m_data[ byteOffset+2 ] / 255.0f;
            result[3] = m_data[ byteOffset+3 ] / 255.0f;
        }
        else if ( m_format== EImageFormat::IF_BGRA_UBYTE )
        {
            int byteOffset = blockOffset * fdata.BytesPerBlock
                    + ( blockPixelY * fdata.PixelsPerBlockX + blockPixelX ) * 4;

            result[0] = m_data[ byteOffset+2 ] / 255.0f;
            result[1] = m_data[ byteOffset+1 ] / 255.0f;
            result[2] = m_data[ byteOffset+0 ] / 255.0f;
            result[3] = m_data[ byteOffset+3 ] / 255.0f;
        }
        else if ( m_format== EImageFormat::IF_L_UBYTE )
        {
            int byteOffset = blockOffset * fdata.BytesPerBlock
                    + ( blockPixelY * fdata.PixelsPerBlockX + blockPixelX ) * 1;

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
    bool Image::IsPlainColour(FVector4f& colour) const
    {
        bool res = true;

        int pixelCount = m_size[0] * m_size[1];

        if ( pixelCount )
        {
            switch( m_format )
            {
			case EImageFormat::IF_L_UBYTE:
			{
				const uint8* pData = m_data.GetData();
				uint8 v = pData[0];

				for (int p = 0; res && p < pixelCount; ++p)
				{
					uint8 nv = *pData++;
					res &= (nv == v);
				}

				if (res)
				{
					colour[0] = colour[1] = colour[2] = float(v) / 255.0f;
					colour[3] = 1.0f;
				}
				break;
			}

			case EImageFormat::IF_RGB_UBYTE:
			{
				const uint8* pData = m_data.GetData();
				uint8 r = pData[0];
				uint8 g = pData[1];
				uint8 b = pData[2];

				for (int p = 0; res && p < pixelCount; ++p)
				{
					uint8 nr = *pData++;
					uint8 ng = *pData++;
					uint8 nb = *pData++;
					res &= (nr == r) && (ng==g) && (nb==b);
				}

				if (res)
				{
					colour[0] = r / 255.0f;
					colour[1] = g / 255.0f;
					colour[2] = b / 255.0f;
					colour[3] = 1.0f;
				}
				break;
			}

			case EImageFormat::IF_RGBA_UBYTE:
			case EImageFormat::IF_BGRA_UBYTE:
			{
				const uint32* pData = reinterpret_cast<const uint32*>(m_data.GetData());
				uint32 v = pData[0];

				for (int p = 0; res && p < pixelCount; ++p)
				{
					uint32 nv = *pData++;
					res &= (nv == v);
				}

				if (res)
				{
					const uint8* pByteData = m_data.GetData();
					if (m_format == EImageFormat::IF_RGBA_UBYTE)
					{
						colour[0] = float(pByteData[0]) / 255.0f;
						colour[1] = float(pByteData[1]) / 255.0f;
						colour[2] = float(pByteData[2]) / 255.0f;
					}
					else
					{
						colour[0] = float(pByteData[2]) / 255.0f;
						colour[1] = float(pByteData[1]) / 255.0f;
						colour[2] = float(pByteData[0]) / 255.0f;
					}
					colour[3] = float(pByteData[3]) / 255.0f;
				}
				break;
			}

			// TODO: Other formats could also be implemented. For compressed types,
			// the compressed block could be compared and only uncompress if all are the same to
			// check if all pixels in the block are also equal.
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
		 rect.size[0] = m_size[0];
		 rect.size[1] = m_size[1];

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
  //          size_t bytesPerPixel = GetImageFormatData( m_format ).BytesPerBlock;
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
		bool bIsBlockBased = GetImageFormatData(m_format).BytesPerBlock > 0;
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

			FIntVector2 FinalSize = CalculateMipSize(LODSToSkip);
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
		bool bIsBlockBased = GetImageFormatData(m_format).BytesPerBlock > 0;
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

			FIntVector2 FinalSize = CalculateMipSize(LODSToSkip);
			m_size[0] = uint16(FinalSize[0]);
			m_size[1] = uint16(FinalSize[1]);
			m_lods -= LODSToSkip;

			int32 FinalDataSize = m_data.Num() - int32(DataToSkip);
			FMemory::Memmove(m_data.GetData(), m_data.GetData() + DataToSkip, FinalDataSize);
		}
	}


	//---------------------------------------------------------------------------------------------
	void FImageOperator::FillColor(Image* Target, FVector4f Color)
	{
		EImageFormat Format = Target->GetFormat();
		switch (Format)
		{
		case EImageFormat::IF_RGB_UBYTE:
		{
			// TODO: Optimize: don't write bytes one by one
			int pixelCount = Target->CalculatePixelCount();
			uint8* pData = Target->GetData();
			uint8 r = uint8(FMath::Clamp(255.0f * Color[0], 0.0f, 255.0f));
			uint8 g = uint8(FMath::Clamp(255.0f * Color[1], 0.0f, 255.0f));
			uint8 b = uint8(FMath::Clamp(255.0f * Color[2], 0.0f, 255.0f));
			for (int p = 0; p < pixelCount; ++p)
			{
				pData[0] = r;
				pData[1] = g;
				pData[2] = b;
				pData += 3;
			}
			break;
		}

		case EImageFormat::IF_RGBA_UBYTE:
		{
			// TODO: Optimize: don't write bytes one by one
			int pixelCount = Target->CalculatePixelCount();
			uint8* pData = Target->GetData();
			uint8 r = uint8(FMath::Clamp(255.0f * Color[0], 0.0f, 255.0f));
			uint8 g = uint8(FMath::Clamp(255.0f * Color[1], 0.0f, 255.0f));
			uint8 b = uint8(FMath::Clamp(255.0f * Color[2], 0.0f, 255.0f));
			uint8 a = uint8(FMath::Clamp(255.0f * Color[3], 0.0f, 255.0f));
			for (int p = 0; p < pixelCount; ++p)
			{
				pData[0] = r;
				pData[1] = g;
				pData[2] = b;
				pData[3] = a;
				pData += 4;
			}
			break;
		}

		case EImageFormat::IF_BGRA_UBYTE:
		{
			// TODO: Optimize: don't write bytes one by one
			int pixelCount = Target->CalculatePixelCount();
			uint8* pData = Target->GetData();
			uint8 r = uint8(FMath::Clamp(255.0f * Color[0], 0.0f, 255.0f));
			uint8 g = uint8(FMath::Clamp(255.0f * Color[1], 0.0f, 255.0f));
			uint8 b = uint8(FMath::Clamp(255.0f * Color[2], 0.0f, 255.0f));
			uint8 a = uint8(FMath::Clamp(255.0f * Color[3], 0.0f, 255.0f));
			for (int p = 0; p < pixelCount; ++p)
			{
				pData[0] = b;
				pData[1] = g;
				pData[2] = r;
				pData[3] = a;
				pData += 4;
			}
			break;
		}

		case EImageFormat::IF_L_UBYTE:
		{
			int pixelCount = Target->CalculatePixelCount();
			uint8* pData = Target->GetData();
			uint8 v = FMath::Min<uint8>(255, FMath::Max<uint8>(0, uint8(255.0f * Color[0])));
			FMemory::Memset(pData, v, pixelCount);
			break;
		}

		default:
		{
			// Generic case that supports compressed formats.
			const FImageFormatData& FormatData = GetImageFormatData(Format);
			Ptr<Image> Block = CreateImage(FormatData.PixelsPerBlockX, FormatData.PixelsPerBlockY, 1, EImageFormat::IF_RGBA_UBYTE, EInitializationType::NotInitialized);

			uint32 Pixel = (uint32(FMath::Clamp(255.0f * Color[0], 0.0f, 255.0f)) << 0)
				| (uint32(FMath::Clamp(255.0f * Color[1], 0.0f, 255.0f)) << 8)
				| (uint32(FMath::Clamp(255.0f * Color[2], 0.0f, 255.0f)) << 16)
				| (uint32(FMath::Clamp(255.0f * Color[3], 0.0f, 255.0f)) << 24);
			uint32* BlockData = reinterpret_cast<uint32*>(Block->GetData());
			for (int32 I = 0; I < FormatData.PixelsPerBlockX * FormatData.PixelsPerBlockY; ++I)
			{
				*BlockData = Pixel;
				BlockData += 1;
			}
			Ptr<Image> Converted = ImagePixelFormat(0, Block.get(), Format);
			ReleaseImage(Block);

			int32 DataSize = Target->CalculateDataSize();
			check(DataSize % FormatData.BytesPerBlock == 0);
			int32 BlockCount = DataSize / FormatData.BytesPerBlock;
			uint8* TargetData = Target->GetData();
			while (BlockCount)
			{
				FMemory::Memcpy(TargetData, Converted->GetData(), FormatData.BytesPerBlock);
				TargetData += FormatData.BytesPerBlock;
				--BlockCount;
			}

			ReleaseImage(Converted);
			break;
		}


		}

	}


}


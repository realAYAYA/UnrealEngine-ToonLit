// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Image.h"

#include "MuR/SerialisationPrivate.h"
#include "MuR/MutableMath.h"
#include "MuR/MemoryPrivate.h"


namespace mu
{

	MUTABLE_DEFINE_ENUM_SERIALISABLE( EBlendType );
	MUTABLE_DEFINE_ENUM_SERIALISABLE( EMipmapFilterType );
	MUTABLE_DEFINE_ENUM_SERIALISABLE( ECompositeImageMode );


    struct FImageFormatData
	{
		FImageFormatData
			(
				unsigned pixelsPerBlockX = 0,
				unsigned pixelsPerBlockY = 0,
				unsigned bytesPerBlock = 0,
				unsigned channels = 0
			)
		{
			m_pixelsPerBlockX = (uint8)pixelsPerBlockX;
			m_pixelsPerBlockY = (uint8)pixelsPerBlockY;
			m_bytesPerBlock = (uint16)bytesPerBlock;
			m_channels = (uint16)channels;
		}

		//! For block based formats, size of the block size. For uncompressed formats it will
		//! always be 1,1. For non-block-based compressed formats, it will be 0,0.
        uint8 m_pixelsPerBlockX, m_pixelsPerBlockY;

		//! Number of bytes used by every pixel block, if uncompressed or block-compressed format.
		//! For non-block-compressed formats, it returns 0.
        uint16 m_bytesPerBlock;

		//! Channels in every pixel of the image.
        uint16 m_channels;
	};


	struct FMipmapGenerationSettings
	{
		float m_sharpenFactor = 0.0f;
		EMipmapFilterType m_filterType = EMipmapFilterType::MFT_SimpleAverage;
		EAddressMode m_addressMode = EAddressMode::AM_NONE;
		bool m_ditherMipmapAlpha = false;

		void Serialise( OutputArchive& arch ) const
		{
			uint32 ver = 0;
			arch << ver;

			arch << m_sharpenFactor;
			arch << m_filterType;
			arch << m_ditherMipmapAlpha;
		}

		void Unserialise( InputArchive& arch )
		{
			uint32 ver = 0;
			arch >> ver;
			check(ver == 0);

			arch >> m_sharpenFactor;
			arch >> m_filterType;
			arch >> m_ditherMipmapAlpha;
		}
	};


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	MUTABLERUNTIME_API const FImageFormatData& GetImageFormatData(EImageFormat format );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	MUTABLERUNTIME_API inline EImageFormat GetUncompressedFormat(EImageFormat f )
	{
		EImageFormat r = f;

		switch ( r )
		{
		case EImageFormat::IF_L_UBIT_RLE: r = EImageFormat::IF_L_UBYTE; break;
		case EImageFormat::IF_L_UBYTE_RLE: r = EImageFormat::IF_L_UBYTE; break;
		case EImageFormat::IF_RGB_UBYTE_RLE: r = EImageFormat::IF_RGB_UBYTE; break;
        case EImageFormat::IF_RGBA_UBYTE_RLE: r = EImageFormat::IF_RGBA_UBYTE; break;
        case EImageFormat::IF_BC1: r = EImageFormat::IF_RGBA_UBYTE; break;
        case EImageFormat::IF_BC2: r = EImageFormat::IF_RGBA_UBYTE; break;
        case EImageFormat::IF_BC3: r = EImageFormat::IF_RGBA_UBYTE; break;
        case EImageFormat::IF_BC4: r = EImageFormat::IF_L_UBYTE; break;
        case EImageFormat::IF_BC5: r = EImageFormat::IF_RGB_UBYTE; break;
        case EImageFormat::IF_ASTC_4x4_RGB_LDR: r = EImageFormat::IF_RGB_UBYTE; break;
        case EImageFormat::IF_ASTC_4x4_RGBA_LDR: r = EImageFormat::IF_RGBA_UBYTE; break;
        case EImageFormat::IF_ASTC_4x4_RG_LDR: r = EImageFormat::IF_RGB_UBYTE; break;
        default: break;
		}

		return r;
	}

    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
	inline EImageFormat GetMostGenericFormat(EImageFormat a, EImageFormat b)
    {
        if (a==b) return a;
        if ( GetImageFormatData(a).m_channels>GetImageFormatData(b).m_channels ) return a;
        if ( GetImageFormatData(b).m_channels>GetImageFormatData(a).m_channels ) return b;
        if (a== EImageFormat::IF_BC2 || a== EImageFormat::IF_BC3 || a== EImageFormat::IF_ASTC_4x4_RGBA_LDR) return a;
        if (b== EImageFormat::IF_BC2 || b== EImageFormat::IF_BC3 || b== EImageFormat::IF_ASTC_4x4_RGBA_LDR) return b;

        return a;
    }


    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    inline bool IsCompressedFormat(EImageFormat f )
    {
        return f!=GetUncompressedFormat(f);
    }


    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    inline void FillPlainColourImage( Image* pImage, vec4<float> c )
    {
        int pixelCount = pImage->GetSizeX() * pImage->GetSizeY();

        switch (pImage->GetFormat())
        {
		case EImageFormat::IF_RGB_UBYTE:
        {
            uint8* pData = pImage->GetData();
            uint8 r = uint8(FMath::Clamp(255.0f * c[0], 0.0f, 255.0f));
            uint8 g = uint8(FMath::Clamp(255.0f * c[1], 0.0f, 255.0f));
            uint8 b = uint8(FMath::Clamp(255.0f * c[2], 0.0f, 255.0f));
            for ( int p = 0; p < pixelCount; ++p )
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
            uint8* pData = pImage->GetData();
			uint8 r = uint8(FMath::Clamp(255.0f * c[0], 0.0f, 255.0f));
			uint8 g = uint8(FMath::Clamp(255.0f * c[1], 0.0f, 255.0f));
			uint8 b = uint8(FMath::Clamp(255.0f * c[2], 0.0f, 255.0f));
			for ( int p = 0; p < pixelCount; ++p )
            {
                pData[0] = r;
                pData[1] = g;
                pData[2] = b;
                pData[3] = 255;
                pData += 4;
            }
            break;
        }

        case EImageFormat::IF_BGRA_UBYTE:
        {
            uint8* pData = pImage->GetData();
			uint8 r = uint8(FMath::Clamp(255.0f * c[0], 0.0f, 255.0f));
			uint8 g = uint8(FMath::Clamp(255.0f * c[1], 0.0f, 255.0f));
			uint8 b = uint8(FMath::Clamp(255.0f * c[2], 0.0f, 255.0f));
			for ( int p = 0; p < pixelCount; ++p )
            {
                pData[0] = b;
                pData[1] = g;
                pData[2] = r;
                pData[3] = 255;
                pData += 4;
            }
            break;
        }

        case EImageFormat::IF_L_UBYTE:
        {
            uint8* pData = pImage->GetData();
            uint8 v = FMath::Min<uint8>( 255, FMath::Max<uint8>( 0, uint8( 255.0f * c[0] ) ) );
            FMemory::Memset( pData, v, pixelCount );
            break;
        }

        default:
            check(false);
        }

    }


    //---------------------------------------------------------------------------------------------
    //! Use with care.
    //---------------------------------------------------------------------------------------------
    template<class T>
    Ptr<T> CloneOrTakeOver( const T* source )
    {
        Ptr<T> result;
        if (source->GetRefCount()==1)
        {
            result = const_cast<T*>(source);
        }
        else
        {
            result = source->Clone();
        }

        return result;
    }

}


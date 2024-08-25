// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuR/ImageTypes.h"
#include "MuR/SerialisationPrivate.h"

namespace mu
{
	MUTABLE_IMPLEMENT_ENUM_SERIALISABLE(EBlendType);
	MUTABLE_IMPLEMENT_ENUM_SERIALISABLE(EMipmapFilterType);
	MUTABLE_IMPLEMENT_ENUM_SERIALISABLE(ECompositeImageMode);
	MUTABLE_IMPLEMENT_ENUM_SERIALISABLE(ESamplingMethod);
	MUTABLE_IMPLEMENT_ENUM_SERIALISABLE(EMinFilterMethod);
	MUTABLE_IMPLEMENT_ENUM_SERIALISABLE(EImageFormat);

    static FImageFormatData s_imageFormatData[uint32(EImageFormat::IF_COUNT)] =
    {
		FImageFormatData(0, 0, 0, 0),	// IF_NONE
		FImageFormatData(1, 1, 3, 3),	// IF_RGB_UBYTE
		FImageFormatData(1, 1, 4, 4),	// IF_RGBA_UBYTE
		FImageFormatData(1, 1, 1, 1),	// IF_U_UBYTE

		FImageFormatData(0, 0, 0, 0),	// IF_PVRTC2 (deprecated)
        FImageFormatData(0, 0, 0, 0),	// IF_PVRTC4 (deprecated)
        FImageFormatData(0, 0, 0, 0),	// IF_ETC1 (deprecated)
        FImageFormatData(0, 0, 0, 0),	// IF_ETC2 (deprecated)

        FImageFormatData(0, 0, 0, 1),	// IF_L_UBYTE_RLE
        FImageFormatData(0, 0, 0, 3),	// IF_RGB_UBYTE_RLE
        FImageFormatData(0, 0, 0, 4),	// IF_RGBA_UBYTE_RLE
        FImageFormatData(0, 0, 0, 1),	// IF_L_UBIT_RLE

        FImageFormatData(4, 4, 8,  4),	// IF_BC1
        FImageFormatData(4, 4, 16, 4),	// IF_BC2
        FImageFormatData(4, 4, 16, 4),	// IF_BC3
        FImageFormatData(4, 4, 8,  1),	// IF_BC4
        FImageFormatData(4, 4, 16, 2),	// IF_BC5
        FImageFormatData(4, 4, 16, 3),	// IF_BC6
        FImageFormatData(4, 4, 16, 4),	// IF_BC7

        FImageFormatData(1, 1, 4, 4 ),		// IF_BGRA_UBYTE

		FImageFormatData(4, 4, 16, 3, {252, 253, 255, 255, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 255, 255}), // IF_ASTC_4x4_RGB_LDR
		FImageFormatData(4, 4, 16, 4, {252, 253, 255, 255, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0}),     // IF_ASTC_4x4_RGBA_LDR
        FImageFormatData(4, 4, 16, 2),	// IF_ASTC_4x4_RG_LDR // TODO: check black block for RG.
		
		FImageFormatData(8, 8, 16, 3, {252, 253, 255, 255, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 255, 255}),	// IF_ASTC_8x8_RGB_LDR,
		FImageFormatData(8, 8, 16, 4, {252, 253, 255, 255, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0}),		// IF_ASTC_8x8_RGBA_LDR,
		FImageFormatData(8, 8, 16, 2),		// IF_ASTC_8x8_RG_LDR,
		FImageFormatData(12, 12, 16, 3, {252, 253, 255, 255, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 255, 255}),	// IF_ASTC_12x12_RGB_LDR
		FImageFormatData(12, 12, 16, 4, {252, 253, 255, 255, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0}),		// IF_ASTC_12x12_RGBA_LDR
		FImageFormatData(12, 12, 16, 2),	// IF_ASTC_12x12_RG_LDR
		FImageFormatData(6, 6, 16, 3, {252, 253, 255, 255, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 255, 255}),	// IF_ASTC_6x6_RGB_LDR,
		FImageFormatData(6, 6, 16, 4, {252, 253, 255, 255, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0}),		// IF_ASTC_6x6_RGBA_LDR,
		FImageFormatData(6, 6, 16, 2),		// IF_ASTC_6x6_RG_LDR,
		FImageFormatData(10, 10, 16, 3, {252, 253, 255, 255, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 255, 255}),	// IF_ASTC_6x6_RGB_LDR,
		FImageFormatData(10, 10, 16, 4, {252, 253, 255, 255, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0}),		// IF_ASTC_6x6_RGBA_LDR,
		FImageFormatData(10, 10, 16, 2),	// IF_ASTC_6x6_RG_LDR,
    };

    const FImageFormatData& GetImageFormatData( EImageFormat format )
    {
        check( format < EImageFormat::IF_COUNT );
        return s_imageFormatData[ uint8(format) ];
    }

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
}


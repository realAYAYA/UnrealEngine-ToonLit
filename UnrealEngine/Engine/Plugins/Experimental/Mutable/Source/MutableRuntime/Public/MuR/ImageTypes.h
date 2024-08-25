// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/MutableMath.h"
#include "MuR/Serialisation.h"

#include <initializer_list>

namespace mu
{
	//!
	using FImageSize = UE::Math::TIntVector2<uint16>;
	using FImageRect = box<FImageSize>;

	//! Pixel formats supported by the images.
	//! \ingroup runtime
	enum class EImageFormat : uint8
	{
		IF_NONE,
		IF_RGB_UBYTE,
		IF_RGBA_UBYTE,
		IF_L_UBYTE,

        //! Deprecated formats
        IF_PVRTC2_DEPRECATED,
        IF_PVRTC4_DEPRECATED,
        IF_ETC1_DEPRECATED,
        IF_ETC2_DEPRECATED,

		IF_L_UBYTE_RLE,
		IF_RGB_UBYTE_RLE,
		IF_RGBA_UBYTE_RLE,
		IF_L_UBIT_RLE,

        //! Common S3TC formats
        IF_BC1,
        IF_BC2,
        IF_BC3,
        IF_BC4,
        IF_BC5,

        //! Not really supported yet
        IF_BC6,
        IF_BC7,

        //! Swizzled versions, engineers be damned.
        IF_BGRA_UBYTE,

        //! The new standard
        IF_ASTC_4x4_RGB_LDR,
        IF_ASTC_4x4_RGBA_LDR,
        IF_ASTC_4x4_RG_LDR,

		IF_ASTC_8x8_RGB_LDR,
		IF_ASTC_8x8_RGBA_LDR,
		IF_ASTC_8x8_RG_LDR,
		IF_ASTC_12x12_RGB_LDR,
		IF_ASTC_12x12_RGBA_LDR,
		IF_ASTC_12x12_RG_LDR,
		IF_ASTC_6x6_RGB_LDR,
		IF_ASTC_6x6_RGBA_LDR,
		IF_ASTC_6x6_RG_LDR,
		IF_ASTC_10x10_RGB_LDR,
		IF_ASTC_10x10_RGBA_LDR,
		IF_ASTC_10x10_RG_LDR,

        IF_COUNT
	};
	MUTABLE_DEFINE_ENUM_SERIALISABLE(EImageFormat);


	struct MUTABLERUNTIME_API FImageDesc
	{
		FImageDesc()
		{
		}

		FImageDesc(const FImageSize& InSize, EImageFormat InFormat, uint8 InLods)
			: m_size(InSize), m_format(InFormat), m_lods(InLods) 
		{
		}

		//!
		FImageSize m_size = FImageSize(0, 0);

		//!
		EImageFormat m_format = EImageFormat::IF_NONE;

		//! Levels of detail (mipmaps)
		uint8 m_lods = 0;

		//!
		FORCEINLINE bool operator==(const FImageDesc& Other) const
		{
			return m_format == Other.m_format &&
				   m_lods == Other.m_lods     &&
				   m_size == Other.m_size;
		}
	};


	//! List of supported modes in generic image layering operations.
	enum class EBlendType
	{
		BT_NONE = 0,
		BT_SOFTLIGHT,
		BT_HARDLIGHT,
		BT_BURN,
		BT_DODGE,
		BT_SCREEN,
		BT_OVERLAY,
		BT_BLEND,
		BT_MULTIPLY,
		BT_LIGHTEN,				// Increase the channel value by a given proportion of what is missing from white 
		BT_NORMAL_COMBINE,
		_BT_COUNT
	};	
	MUTABLE_DEFINE_ENUM_SERIALISABLE(EBlendType);

	enum class EAddressMode
	{
		None,
		Wrap,
		ClampToEdge,
		ClampToBlack,
	};

	enum class EMipmapFilterType
	{
		MFT_Unfiltered,
		MFT_SimpleAverage,
		MFT_Sharpen,
		_MFT_COUNT
	};	
	MUTABLE_DEFINE_ENUM_SERIALISABLE(EMipmapFilterType);

	enum class ECompositeImageMode
	{
		CIM_Disabled,
		CIM_NormalRoughnessToRed,
		CIM_NormalRoughnessToGreen,
		CIM_NormalRoughnessToBlue,
		CIM_NormalRoughnessToAlpha,
		_CIM_COUNT
	};	
	MUTABLE_DEFINE_ENUM_SERIALISABLE(ECompositeImageMode);

	enum class ESamplingMethod : uint8
	{
		Point = 0,
		BiLinear,
		MaxValue
	};
	static_assert(uint32(ESamplingMethod::MaxValue) <= (1 << 3), "ESampligMethod enum cannot hold more than 8 values");
	MUTABLE_DEFINE_ENUM_SERIALISABLE(ESamplingMethod);

	enum class EInitializationType
	{
		NotInitialized,
		Black
	};

	enum class EMinFilterMethod : uint8
	{
		None = 0,
		TotalAreaHeuristic,
		MaxValue
	};
	static_assert(uint32(EMinFilterMethod::MaxValue) <= (1 << 3), "EMinFilterMethod enum cannot hold more than 8 values");
	MUTABLE_DEFINE_ENUM_SERIALISABLE(EMinFilterMethod);

	/** */
	struct FImageFormatData
	{
		static constexpr SIZE_T MAX_BYTES_PER_BLOCK = 16;

		FImageFormatData
		(
			uint8 InPixelsPerBlockX = 0,
			uint8 InPixelsPerBlockY = 0,
			uint16 InBytesPerBlock = 0,
			uint16 InChannels = 0
		)
		{
			PixelsPerBlockX = InPixelsPerBlockX;
			PixelsPerBlockY = InPixelsPerBlockY;
			BytesPerBlock = InBytesPerBlock;
			Channels = InChannels;
		}

		FImageFormatData
		(
			uint8 InPixelsPerBlockX,
			uint8 InPixelsPerBlockY,
			uint16 InBytesPerBlock,
			uint16 InChannels,
			std::initializer_list<uint8> BlackBlockInit
		)
			: FImageFormatData(InPixelsPerBlockX, InPixelsPerBlockY, InBytesPerBlock, InChannels)
		{
			check(MAX_BYTES_PER_BLOCK >= BlackBlockInit.size());

			const SIZE_T SanitizedBlockSize = FMath::Min<SIZE_T>(MAX_BYTES_PER_BLOCK, BlackBlockInit.size());
			FMemory::Memcpy(BlackBlock, BlackBlockInit.begin(), SanitizedBlockSize);
		}

		/** For block based formats, size of the block size.For uncompressed formats it will always be 1,1. For non-block-based compressed formats, it will be 0,0. */
		uint8 PixelsPerBlockX, PixelsPerBlockY;

		/** Number of bytes used by every pixel block, if uncompressed or block-compressed format.
		 * For non-block-compressed formats, it returns 0.
		 */
		uint16 BytesPerBlock;

		/** Channels in every pixel of the image. */
		uint16 Channels;

		/** Representation of a black block of the image. */
		uint8 BlackBlock[MAX_BYTES_PER_BLOCK] = { 0 };
	};

	MUTABLERUNTIME_API const FImageFormatData& GetImageFormatData(EImageFormat format);

	struct MUTABLERUNTIME_API FMipmapGenerationSettings
	{
		float m_sharpenFactor = 0.0f;
		EMipmapFilterType m_filterType = EMipmapFilterType::MFT_SimpleAverage;
		EAddressMode m_addressMode = EAddressMode::None;
		bool m_ditherMipmapAlpha = false;

		void Serialise(OutputArchive& arch) const;
		void Unserialise(InputArchive& arch);
	};
}


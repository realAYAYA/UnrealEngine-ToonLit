// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/MutableMath.h"
#include "MuR/MutableTrace.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Serialisation.h"
#include "MuR/SerialisationPrivate.h"
#include "Containers/Array.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/Platform.h"
#include "Misc/AssertionMacros.h"

namespace mu
{

	// Forward declarations
	class Image;

	typedef Ptr<Image> ImagePtr;
	typedef Ptr<const Image> ImagePtrConst;

	//!
	using FImageSize = vec2<uint16>;
	using FImageRect = box<vec2<uint16>>;

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

        IF_COUNT
	};


	struct MUTABLERUNTIME_API FImageDesc
	{
		FImageDesc()
		{
			m_format = EImageFormat::IF_NONE;
			m_size = FImageSize(0, 0);
			m_lods = 0;
		}

		FImageDesc(const FImageSize& size, EImageFormat format, uint8 lods)
		{
			m_format = format;
			m_size = size;
			m_lods = lods;
		}

		//!
		FImageSize m_size;

		//!
		EImageFormat m_format;

		//! Levels of detail (mipmaps)
		uint8 m_lods;

		//!
		inline bool operator==(const FImageDesc& o) const
		{
			return  (m_format == o.m_format) &&
				(m_lods == o.m_lods) &&
				(m_size == o.m_size);
		}
	};


	//! List of supported modes in generic image layering operations.
	enum class EBlendType
	{
		BT_SOFTLIGHT = 0,
		BT_HARDLIGHT,
		BT_BURN,
		BT_DODGE,
		BT_SCREEN,
		BT_OVERLAY,
		BT_BLEND,
		BT_MULTIPLY,
		BT_ALPHA_OVERLAY, // TODO: This name is not descriptive.
		BT_NORMAL_COMBINE,
		_BT_COUNT
	};

	enum class EAddressMode
	{
		AM_NONE,
		AM_WRAP,
		AM_CLAMP,
		AM_BLACK_BORDER,
		_AM_COUNT

	};

	enum class EMipmapFilterType
	{
		MFT_Unfiltered,
		MFT_SimpleAverage,
		MFT_Sharpen,
		_MFT_COUNT
	};

	enum class ECompositeImageMode
	{
		CIM_Disabled,
		CIM_NormalRoughnessToRed,
		CIM_NormalRoughnessToGreen,
		CIM_NormalRoughnessToBlue,
		CIM_NormalRoughnessToAlpha,
		_CIM_COUNT
	};

    //! \brief 2D image resource with mipmaps.
	//! \ingroup runtime
    class MUTABLERUNTIME_API Image : public RefCounted
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		Image();

		//! Constructor from the size and format.
		//! It will allocate the memory for the image, but its content will be undefined.
		//! \param sizeX Width of the image.
        //! \param sizeY Height of the image.
        //! \param lods Number of levels of detail (mipmaps) to include in the image, inclduing the
        //!         base level. It must be a number between 1 and the maximum possible levels, which
        //!         depends on the image size.
        //! \param format Pixel format.
        Image( uint32 sizeX, uint32 sizeY, uint32 lods, EImageFormat format );

		/** */
		ImagePtr ExtractMip(int32 Mip) const;

		//! Serialisation
		static void Serialise( const Image* p, OutputArchive& arch );
		static ImagePtr StaticUnserialise( InputArchive& arch );

		//-----------------------------------------------------------------------------------------
		// Own interface
		//-----------------------------------------------------------------------------------------

		//! Return the width of the image.
        uint16 GetSizeX() const;

		//! Return the height of the image.
        uint16 GetSizeY() const;

		const FImageSize& GetSize() const;

		//! Return the pixel format of the image.
		EImageFormat GetFormat() const;

		//! Return the number of levels of detail (mipmaps) in the texture. The base lavel is also
		//! counted, so the minimum is 1.
		int GetLODCount() const;

		//! Return a pointer to a instance-owned buffer where the image pixels are. The pixels are
		//! stored without any padding. All the LODs are stored in the same buffer, contiguously.
        const uint8* GetData() const;
        uint8* GetData();

        //! Return the size in bytes of all the LODs of the image.
        int32 GetDataSize() const;

        //! Return the size in bytes of a specific LOD of the image.
        int32 GetLODDataSize( int lod ) const;

		//! Get an internal identifier used to reference this image in operations like deferred
		//! image building, or instance updating.
        uint32 GetId() const;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~Image() {}

	public:

		// This used to be the data in the private implementation of the image interface
		//-----------------------------------------------------------------------------------------

		//!
		FImageSize m_size;

		//! Non-persistent internal id
		mutable uint32 m_internalId = 0;

		//!
		EImageFormat m_format;

		//! Levels of detail (mipmaps)
		uint8 m_lods;

		//! These set of flags are used to cache information for images at runtime.
		typedef enum
		{
			// Set if the next flag has been calculated and its value is valid.
			IF_IS_PLAIN_COLOUR_VALID = 1 << 0,

			// If the previous flag is set and this one too, the image is single colour.
			IF_IS_PLAIN_COLOUR = 1 << 1,

			// If this is set, the image shouldn't be scaled: it's contents is resoultion-dependent.
			IF_CANNOT_BE_SCALED = 2 << 1
		} EImageFlags;

		//! Persistent flags with some image properties. The meaning will depend of every
		//! context.
		mutable uint8 m_flags = 0;

		//! Pixel data
		TArray<uint8> m_data;


		// This used to be the methods in the private implementation of the image interface
		//-----------------------------------------------------------------------------------------

		//! Deep clone.
		ImagePtr Clone() const
		{
			LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
			MUTABLE_CPUPROFILER_SCOPE(ImageClone)

			ImagePtr pResult = new Image();

			pResult->m_size = m_size;
			pResult->m_internalId = m_internalId;
			pResult->m_format = m_format;
			pResult->m_lods = m_lods;
			pResult->m_flags = m_flags;
			pResult->m_data = m_data;

			return pResult;
		}


		//!
		void Serialise(OutputArchive& arch) const
		{
			uint32 ver = 3;
			arch << ver;

			arch << m_size;
			arch << m_lods;
			arch << (uint8)m_format;
			arch << m_data;
			arch << m_flags;
		}

		//!
		void Unserialise(InputArchive& arch)
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

		//!
		inline bool operator==(const Image& o) const
		{
			return (m_lods == o.m_lods) &&
				(m_size == o.m_size) &&
				(m_format == o.m_format) &&
				(m_data == o.m_data);
		}

		//-----------------------------------------------------------------------------------------

		//! Sample the image and return an RGB colour.
		vec4<float> Sample(vec2<float> coords) const;

		//! Calculate the size of the image data in bytes, regardless of what is allocated in
		//! m_data, only using the image descriptions. For non-block-compressed images, it returns
		//! 0.
		int32 CalculateDataSize() const;

		//! Calculate the size of a lod of the image data in bytes, regardless of what is allocated
		//! in m_data, only using the image descriptions. For non-block-compressed images, it
		//! returns 0.
		int32 CalculateDataSize(int lod) const;

		//! Calculate the number of pixels of the image, regardless of what is allocated in
		//! m_data, only using the image descriptions.
		//! For block-compressed images, it includes the wasted pixels in the blocks, in case
		//! the size is not a multiple of it in all lods.
		//! It includes the pixels in all lods
		int32 CalculatePixelCount() const;

		//! Same as above, but only calculates the pixels of an lod
		int32 CalculatePixelCount(int lod) const;

		//! Calculate the size in pixels of a particular mipmap of this image. The size doesn't
		//! include pixels necessary for completing blocks in block-compressed formats.
		vec2<int> CalculateMipSize(int lod) const;

		//! Return a pointer to the beginning of the data for a particular mip.
		uint8* GetMipData(int mip);
		const uint8* GetMipData(int mip) const;
		
		//! Return the size of the resident mips. for block-compressed images the result is the same as in CalculateDataSize()
		//! for non-block-compressed images, the sizes encoded in the image data is used to compute the final size.
		int32 GetMipsDataSize() const;

		//! See if all the pixels in the image are equal and return the colour.
		bool IsPlainColour(vec4<float>& colour) const;

		//! See if all the pixels in the alpha channel are the max value of the pixel format
		//! (white).
		bool IsFullAlpha() const;

		//! Reduce the LODs of the image to the given amount. Don't do anything if there are already 
		//! less LODs than specified.
		void ReduceLODsTo(int32 NewLODCount);
		void ReduceLODs(int32 LODsToSkip);

		//! Calculate the number of mipmaps for a particular image size.
		static int GetMipmapCount(int SizeX, int SizeY);

		//! Get the rect inside the image bounding the non-black content of the image.
		void GetNonBlackRect(FImageRect& rect) const;
		void GetNonBlackRect_Reference(FImageRect& rect) const;

	};
}


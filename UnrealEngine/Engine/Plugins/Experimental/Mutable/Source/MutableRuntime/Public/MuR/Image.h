// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/MutableMath.h"
#include "MuR/MutableTrace.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Serialisation.h"
#include "Containers/Array.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/Platform.h"
#include "Misc/AssertionMacros.h"

#include "MuR/MemoryTrackingAllocationPolicy.h"

namespace mu::MemoryCounters
{
	struct FImageMemoryCounterTag {};
	using FImageMemoryCounter = TMemoryCounter<FImageMemoryCounterTag>;
}

namespace mu
{

	// Forward declarations
	class Image;

	typedef Ptr<Image> ImagePtr;
	typedef Ptr<const Image> ImagePtrConst;

	//!
	using FImageSize = UE::Math::TIntVector2<uint16>;
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

		IF_ASTC_8x8_RGB_LDR,
		IF_ASTC_8x8_RGBA_LDR,
		IF_ASTC_8x8_RG_LDR,
		IF_ASTC_12x12_RGB_LDR,
		IF_ASTC_12x12_RGBA_LDR,
		IF_ASTC_12x12_RG_LDR,

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
		FImageSize m_size = FImageSize(0, 0);

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

	enum class ECompositeImageMode
	{
		CIM_Disabled,
		CIM_NormalRoughnessToRed,
		CIM_NormalRoughnessToGreen,
		CIM_NormalRoughnessToBlue,
		CIM_NormalRoughnessToAlpha,
		_CIM_COUNT
	};

	enum class EInitializationType
	{
		NotInitialized,
		Black
	};

	enum class ESamplingMethod : uint8
	{
		Point = 0,
		BiLinear,
		MaxValue
	};
	static_assert(uint32(ESamplingMethod::MaxValue) <= (1 << 3), "ESampligMethod enum cannot hold more than 8 values");

	enum class EMinFilterMethod : uint8
	{
		None = 0,
		TotalAreaHeuristic,
		MaxValue
	};

	static_assert(uint32(EMinFilterMethod::MaxValue) <= (1 << 3), "EMinFilterMethod enum cannot hold more than 8 values");

    //! \brief 2D image resource with mipmaps.
	//! \ingroup runtime
    class MUTABLERUNTIME_API Image : public Resource
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
        Image( uint32 sizeX, uint32 sizeY, uint32 lods, EImageFormat format, EInitializationType Init );
		
		/** */
		static Ptr<Image> CreateAsReference( uint32 ID );

		//! Serialisation
		static void Serialise( const Image* p, OutputArchive& arch );
		static Ptr<Image> StaticUnserialise( InputArchive& arch );

		// Resource interface
		int32 GetDataSize() const override;

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

        //! Return the size in bytes of a specific LOD of the image.
        int32 GetLODDataSize( int lod ) const;

		/** Return true if this is a reference to an engine image. */
		bool IsReference() const;

		/** Return the id of the engine referenced texture. Only valid if IsReference. */
		uint32 GetReferencedTexture() const;

		/** Clear the image to black colour. */
		void InitToBlack();

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~Image() {}

	public:

		// This used to be the data in the private implementation of the image interface
		//-----------------------------------------------------------------------------------------

		/** */
		FImageSize m_size = FImageSize(0, 0);

		/** */
		EImageFormat m_format = EImageFormat::IF_NONE;

		/** Levels of detail (mipmaps) */
		uint8 m_lods = 0;

		/** These set of flags are used to cache information for images at runtime. */
		typedef enum
		{
			// Set if the next flag has been calculated and its value is valid.
			IF_IS_PLAIN_COLOUR_VALID = 1 << 0,

			// If the previous flag is set and this one too, the image is single colour.
			IF_IS_PLAIN_COLOUR = 1 << 1,

			// If this is set, the image shouldn't be scaled: it's contents is resoultion-dependent.
			IF_CANNOT_BE_SCALED = 1 << 2,

			// If this is set, the image has an updated relevancy map. This flag is not persisent.
			IF_HAS_RELEVANCY_MAP = 1 << 3,

			/** If this is set, this is a reference to an external image, and the ReferenceID is valid. */
			IF_IS_REFERENCE = 1 << 4,
		} EImageFlags;

		/** Persistent flags with some image properties. The meaning will depend of every context. */
		mutable uint8 m_flags = 0;

		/** Non-persistent relevancy map. */
		uint16 RelevancyMinY = 0;
		uint16 RelevancyMaxY = 0;

		/** Only valid if the right flags are set, this identifies a referenced image. */
		uint32 ReferenceID = 0;

		/** Pixel data for all lods. */

		using ImageDataContainerType = TArray<uint8, FDefaultMemoryTrackingAllocator<MemoryCounters::FImageMemoryCounter>>;
		ImageDataContainerType m_data;


		// This used to be the methods in the private implementation of the image interface
		//-----------------------------------------------------------------------------------------

		//! Deep clone.
		Ptr<Image> Clone() const
		{
			LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
			MUTABLE_CPUPROFILER_SCOPE(ImageClone)

			Ptr<Image> pResult = new Image();

			pResult->m_size = m_size;
			pResult->m_format = m_format;
			pResult->m_lods = m_lods;
			pResult->m_flags = m_flags;
			pResult->RelevancyMinY = RelevancyMinY;
			pResult->RelevancyMaxY = RelevancyMaxY;
			pResult->m_data = m_data;
			pResult->ReferenceID = ReferenceID;

			return pResult;
		}

		//! Copy another image.
		void Copy(const Image* Other)
		{
			LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
			MUTABLE_CPUPROFILER_SCOPE(Copy);

			if (Other == this)
			{
				return;
			}

			m_size = Other->m_size;
			m_format = Other->m_format;
			m_lods = Other->m_lods;
			m_flags = Other->m_flags;
			RelevancyMinY = Other->RelevancyMinY;
			RelevancyMaxY = Other->RelevancyMaxY;
			m_data =Other->m_data;
			ReferenceID = Other->ReferenceID;
		}

		void CopyMove(Image* Other)
		{
			LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
			MUTABLE_CPUPROFILER_SCOPE(CopyMove);

			if (Other == this)
			{
				return;
			}

			m_size = Other->m_size;
			m_format = Other->m_format;
			m_lods = Other->m_lods;
			m_flags = Other->m_flags;
			RelevancyMinY = Other->RelevancyMinY;
			RelevancyMaxY = Other->RelevancyMaxY;
			m_data = MoveTemp(Other->m_data);
			ReferenceID = Other->ReferenceID;

			Other->m_size = FImageSize(0, 0);
			Other->m_format = EImageFormat::IF_NONE;
			Other->m_lods = 0;
			Other->m_flags = 0;
			Other->RelevancyMinY = 0;
			Other->RelevancyMaxY = 0;
			Other->m_data.SetNum(0);
			Other->ReferenceID = 0;
		}


		//!
		void Serialise(OutputArchive& arch) const;

		//!
		void Unserialise(InputArchive& arch);

		//!
		inline bool operator==(const Image& o) const
		{
			return (m_lods == o.m_lods) &&
				(m_size == o.m_size) &&
				(m_format == o.m_format) &&
				(m_data == o.m_data) &&
				(ReferenceID == o.ReferenceID);
		}

		//-----------------------------------------------------------------------------------------

		//! Sample the image and return an RGB colour.
		FVector4f Sample(FVector2f coords) const;

		//! Calculate the size of the image data in bytes, regardless of what is allocated in
		//! m_data, only using the image descriptions. For non-block-compressed images, it returns
		//! 0.
		int32 CalculateDataSize() const;
		static int32 CalculateDataSize(int32 SizeX, int32 SizeY, int32 LodCount, EImageFormat Format);


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
		FIntVector2 CalculateMipSize(int lod) const;

		//! Return a pointer to the beginning of the data for a particular mip.
		uint8* GetMipData(int mip);
		const uint8* GetMipData(int mip) const;
		
		//! Return the size of the resident mips. for block-compressed images the result is the same as in CalculateDataSize()
		//! for non-block-compressed images, the sizes encoded in the image data is used to compute the final size.
		int32 GetMipsDataSize() const;

		//! See if all the pixels in the image are equal and return the colour.
		bool IsPlainColour(FVector4f& colour) const;

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

	/** This struct contains image operations that may need to allocate and free temporary images to work.
	* It's purpose is to override the creation, release and clone functions depending on the context.
	*/
	struct FImageOperator
	{
		/** Common callback used for functions that can create temporary images. */
		typedef TFunction<Ptr<Image>(int32, int32, int32, EImageFormat, EInitializationType)> FImageCreateFunc;
		typedef TFunction<void(Ptr<Image>&)> FImageReleaseFunc;
		typedef TFunction<Ptr<Image>(const Image*)> FImageCloneFunc;

		FImageCreateFunc CreateImage;
		FImageReleaseFunc ReleaseImage;
		FImageCloneFunc CloneImage;

		FImageOperator(FImageCreateFunc Create, FImageReleaseFunc Release, FImageCloneFunc Clone) : CreateImage(Create), ReleaseImage(Release), CloneImage(Clone) {};

		/** Create an default version for untracked resources. */
		static inline FImageOperator GetDefault()
		{
			return FImageOperator(
				[](int32 x, int32 y, int32 m, EImageFormat f, EInitializationType i) { return new Image(x, y, m, f, EInitializationType::NotInitialized); },
				[](Ptr<Image>& i) {i = nullptr; },
				[](const Image* i) { return i->Clone(); }
			);
		}

		/** Convert an image to another pixel format.
		* Allocates the destination image.
		* \warning Not all format conversions are implemented.
		* \param onlyLOD If different than -1, only the specified lod level will be converted in the returned image.
		* \return nullptr if the conversion failed, usually because not enough memory was allocated in the result. This is only checked for RLE compression.
		*/
		MUTABLERUNTIME_API Ptr<Image> ImagePixelFormat(int32 Quality, const Image* Base, EImageFormat TargetFormat, int32 OnlyLOD = -1);

		/** Convert an image to another pixel format.
		* \warning Not all format conversions are implemented.
		* \param onlyLOD If different than -1, only the specified lod level will be converted in the returned image.
		* \return false if the conversion failed, usually because not enough memory was allocated in the result. This is only checked for RLE compression.
		*/
		MUTABLERUNTIME_API void ImagePixelFormat(bool& bOutSuccess, int32 Quality, Image* Result, const Image* Base, int32 OnlyLOD = -1);

		MUTABLERUNTIME_API Ptr<Image> ImageSwizzle(EImageFormat Format, const Ptr<const Image> Sources[], const uint8 Channels[]);

		/** */
		MUTABLERUNTIME_API Ptr<Image> ExtractMip(const Image* From, int32 Mip);

		/** Bilinear filter image resize. */
		void ImageResizeLinear(Image* Dest, int32 ImageCompressionQuality, const Image* Base);

		/** Fill the image with a plain colour. */
		void FillColor(Image* Image, FVector4f Color);

		/** Support struct to keep preallocated data required for some mipmap operations. */
		struct FScratchImageMipmap
		{
			Ptr<Image> Uncompressed;
			Ptr<Image> UncompressedMips;
			Ptr<Image> CompressedMips;
		};

		/** Generate the mipmaps for images.
		* if bGenerateOnlyTail is true, generates the mips missing from Base to LevelCount and sets
		* them in Dest (the full chain is spit in two images). Otherwise generate the mips missing
		* from Base up to LevelCount and append them in Dest to the already generated Base's mips.
		*/
		void ImageMipmap(int32 CompressionQuality, Image* Dest, const Image* Base,
			int32 LevelCount,
			const FMipmapGenerationSettings&, bool bGenerateOnlyTail = false);

		/** Mipmap separating the worst case treatment in 3 steps to manage allocations of temp data. */
		void ImageMipmap_PrepareScratch(Image* Dest, const Image* Base, int32 LevelCount, FScratchImageMipmap&);
		void ImageMipmap(FImageOperator::FScratchImageMipmap&, int32 CompressionQuality, Image* Dest, const Image* Base,
			int32 LevelCount,
			const FMipmapGenerationSettings&, bool bGenerateOnlyTail = false);
		void ImageMipmap_ReleaseScratch(FScratchImageMipmap&);

		/** */
		MUTABLERUNTIME_API bool ImageCrop(Image* Cropped, int32 CompressionQuality, const Image* Base, const box<FIntVector2>& Rect);

		/** */
		void ImageCompose(Image* Base, const Image* Block, const box< UE::Math::TIntVector2<uint16> >& Rect);

	};

	/** Update all the mipmaps in the image from the data in the base one.
	* Only the mipmaps already existing in the image are updated.
	*/
	MUTABLERUNTIME_API void ImageMipmapInPlace(int32 CompressionQuality, Image* Base, const FMipmapGenerationSettings&);

	/** */
	MUTABLERUNTIME_API void ImageSwizzle(Image* Result, const Ptr<const Image> Sources[], const uint8 Channels[]);


}


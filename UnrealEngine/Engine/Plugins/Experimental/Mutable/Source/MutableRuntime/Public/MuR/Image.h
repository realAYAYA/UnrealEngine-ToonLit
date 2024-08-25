// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImageTypes.h"

#include "MuR/MutableMath.h"
#include "MuR/MutableTrace.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Serialisation.h"
#include "Containers/Array.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/Platform.h"
#include "Misc/AssertionMacros.h"

#include "MuR/ImageDataStorage.h"

namespace mu
{
	// Forward declarations
	class Image;

	typedef Ptr<Image> ImagePtr;
	typedef Ptr<const Image> ImagePtrConst;

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
        Image(uint32 SizeX, uint32 SizeY, uint32 lods, EImageFormat Format, EInitializationType InitType);

		/** */
		static Ptr<Image> CreateAsReference(uint32 ID, const FImageDesc& Desc, bool bForceLoad);

		//! Serialisation
		static void Serialise( const Image* p, OutputArchive& arch );
		static Ptr<Image> StaticUnserialise( InputArchive& arch );

		// Resource interface
		int32 GetDataSize() const override;

		//-----------------------------------------------------------------------------------------
		// Own interface
		//-----------------------------------------------------------------------------------------

		/** */
		void Init(uint32 SizeX, uint32 SizeY, uint32 Lods, EImageFormat Format, EInitializationType InitType);

		//! Return the width of the image.
        uint16 GetSizeX() const;

		//! Return the height of the image.
        uint16 GetSizeY() const;

		const FImageSize& GetSize() const;

		//! Return the pixel format of the image.
		EImageFormat GetFormat() const;

		//! Return the number of levels of detail (mipmaps) in the texture. The base lavel is also
		//! counted, so the minimum is 1.
		int32 GetLODCount() const;
		void SetLODCount(int32 LODCount);

		//! Return a pointer to a instance-owned buffer where the image pixels are.
        const uint8* GetLODData(int32 LODIndex) const;
        uint8* GetLODData(int32 LODIndex);

        //! Return the size in bytes of a specific LOD of the image.
        int32 GetLODDataSize(int32 LODIndex) const;

		/** Return true if this is a reference to an engine image. */
		bool IsReference() const;

		/** If true, this is a reference that must be resolved at compile time. */
		bool IsForceLoad() const;

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

			/** For reference images, this indicates that they should be loaded into full images as soon as they are generated. */
			IF_IS_FORCELOAD = 1 << 5
		} EImageFlags;

		/** Persistent flags with some image properties. The meaning will depend of every context. */
		mutable uint8 m_flags = 0;

		/** Non-persistent relevancy map. */
		uint16 RelevancyMinY = 0;
		uint16 RelevancyMaxY = 0;

		/** Only valid if the right flags are set, this identifies a referenced image. */
		uint32 ReferenceID = 0;

		/** Pixel data for all lods. */

		FImageDataStorage DataStorage;

		// This used to be the methods in the private implementation of the image interface
		//-----------------------------------------------------------------------------------------

		//! Deep clone.
		Ptr<Image> Clone() const
		{
			LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
			MUTABLE_CPUPROFILER_SCOPE(ImageClone)

			Ptr<Image> pResult = new Image();

			pResult->m_flags = m_flags;
			pResult->RelevancyMinY = RelevancyMinY;
			pResult->RelevancyMaxY = RelevancyMaxY;
			pResult->DataStorage = DataStorage;
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

			m_flags = Other->m_flags;
			RelevancyMinY = Other->RelevancyMinY;
			RelevancyMaxY = Other->RelevancyMaxY;
			DataStorage = Other->DataStorage;
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

			m_flags = Other->m_flags;
			RelevancyMinY = Other->RelevancyMinY;
			RelevancyMaxY = Other->RelevancyMaxY;
			ReferenceID = Other->ReferenceID;
			
			DataStorage = MoveTemp(Other->DataStorage);
		}


		//!
		void Serialise(OutputArchive& arch) const;

		//!
		void Unserialise(InputArchive& arch);

		//!
		inline bool operator==(const Image& Other) const
		{
			return 
				(DataStorage == Other.DataStorage) &&
				(ReferenceID == Other.ReferenceID);
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
		//int32 CalculateDataSize(int lod) const;

		////! Calculate the number of pixels of the image, regardless of what is allocated in
		////! m_data, only using the image descriptions.
		////! For block-compressed images, it includes the wasted pixels in the blocks, in case
		////! the size is not a multiple of it in all lods.
		////! It includes the pixels in all lods
		//int32 CalculatePixelCount() const;

		////! Same as above, but only calculates the pixels of an lod
		//int32 CalculatePixelCount(int32 LOD) const;

		//! Calculate the size in pixels of a particular mipmap of this image. The size doesn't
		//! include pixels necessary for completing blocks in block-compressed formats.
		FIntVector2 CalculateMipSize(int32 LOD) const;

		//! Return a pointer to the beginning of the data for a particular mip.
		uint8* GetMipData(int32 Mip);
		const uint8* GetMipData(int32 Mip) const;
		
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

		/** Interface to override the internal mutable image pixel format conversion functions.
		* Arguments match the FImageOperator::ImagePixelFormat function.
		*/
		typedef TFunction<void(bool&, int32, Image*, const Image*, int32)> FImagePixelFormatFunc;
		FImagePixelFormatFunc FormatImageOverride;

		FImageOperator(FImageCreateFunc Create, FImageReleaseFunc Release, FImageCloneFunc Clone, FImagePixelFormatFunc FormatOverride) : CreateImage(Create), ReleaseImage(Release), CloneImage(Clone), FormatImageOverride(FormatOverride) {};

		/** Create an default version for untracked resources. */
		static FImageOperator GetDefault( const FImagePixelFormatFunc& InFormatOverride )
		{
			return FImageOperator(
				[](int32 x, int32 y, int32 m, EImageFormat f, EInitializationType i) { return new Image(x, y, m, f, i); },
				[](Ptr<Image>& i) {i = nullptr; },
				[](const Image* i) { return i->Clone(); },
				InFormatOverride
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
		MUTABLERUNTIME_API void ImagePixelFormat(bool& bOutSuccess, int32 Quality, Image* Result, const Image* Base, int32 BeginResultLOD, int32 BeginBaseLOD, int32 NumLODs);

		MUTABLERUNTIME_API Ptr<Image> ImageSwizzle(EImageFormat Format, const Ptr<const Image> Sources[], const uint8 Channels[]);

		/** */
		MUTABLERUNTIME_API Ptr<Image> ExtractMip(const Image* From, int32 Mip);

		/** Bilinear filter image resize. */
		MUTABLERUNTIME_API void ImageResizeLinear(Image* Dest, int32 ImageCompressionQuality, const Image* Base);

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
			int32 StartLevel, int32 LevelCount,
			const FMipmapGenerationSettings&, bool bGenerateOnlyTail = false);

		/** Mipmap separating the worst case treatment in 3 steps to manage allocations of temp data. */
		void ImageMipmap_PrepareScratch(const Image* DestImage, int32 StartLevel, int32 LevelCount, FScratchImageMipmap&);
		void ImageMipmap(FImageOperator::FScratchImageMipmap&, int32 CompressionQuality, Image* Dest, const Image* Base,
			int32 LevelStart, int32 LevelCount,
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

	MUTABLERUNTIME_API inline EImageFormat GetUncompressedFormat(EImageFormat Format)
	{
		check(Format < EImageFormat::IF_COUNT);

		EImageFormat Result = Format;

		switch (Result)
		{
		case EImageFormat::IF_L_UBIT_RLE: Result = EImageFormat::IF_L_UBYTE; break;
		case EImageFormat::IF_L_UBYTE_RLE: Result = EImageFormat::IF_L_UBYTE; break;
		case EImageFormat::IF_RGB_UBYTE_RLE: Result = EImageFormat::IF_RGB_UBYTE; break;
        case EImageFormat::IF_RGBA_UBYTE_RLE: Result = EImageFormat::IF_RGBA_UBYTE; break;
        case EImageFormat::IF_BC1: Result = EImageFormat::IF_RGBA_UBYTE; break;
        case EImageFormat::IF_BC2: Result = EImageFormat::IF_RGBA_UBYTE; break;
        case EImageFormat::IF_BC3: Result = EImageFormat::IF_RGBA_UBYTE; break;
        case EImageFormat::IF_BC4: Result = EImageFormat::IF_L_UBYTE; break;
        case EImageFormat::IF_BC5: Result = EImageFormat::IF_RGB_UBYTE; break;
		case EImageFormat::IF_ASTC_4x4_RGB_LDR: Result = EImageFormat::IF_RGB_UBYTE; break;
		case EImageFormat::IF_ASTC_4x4_RGBA_LDR: Result = EImageFormat::IF_RGBA_UBYTE; break;
		case EImageFormat::IF_ASTC_4x4_RG_LDR: Result = EImageFormat::IF_RGB_UBYTE; break;
		case EImageFormat::IF_ASTC_6x6_RGB_LDR: Result = EImageFormat::IF_RGB_UBYTE; break;
		case EImageFormat::IF_ASTC_6x6_RGBA_LDR: Result = EImageFormat::IF_RGBA_UBYTE; break;
		case EImageFormat::IF_ASTC_6x6_RG_LDR: Result = EImageFormat::IF_RGB_UBYTE; break;
		case EImageFormat::IF_ASTC_8x8_RGB_LDR: Result = EImageFormat::IF_RGB_UBYTE; break;
		case EImageFormat::IF_ASTC_8x8_RGBA_LDR: Result = EImageFormat::IF_RGBA_UBYTE; break;
		case EImageFormat::IF_ASTC_8x8_RG_LDR: Result = EImageFormat::IF_RGB_UBYTE; break;
		case EImageFormat::IF_ASTC_10x10_RGB_LDR: Result = EImageFormat::IF_RGB_UBYTE; break;
		case EImageFormat::IF_ASTC_10x10_RGBA_LDR: Result = EImageFormat::IF_RGBA_UBYTE; break;
		case EImageFormat::IF_ASTC_10x10_RG_LDR: Result = EImageFormat::IF_RGB_UBYTE; break;
		case EImageFormat::IF_ASTC_12x12_RGB_LDR: Result = EImageFormat::IF_RGB_UBYTE; break;
		case EImageFormat::IF_ASTC_12x12_RGBA_LDR: Result = EImageFormat::IF_RGBA_UBYTE; break;
		case EImageFormat::IF_ASTC_12x12_RG_LDR: Result = EImageFormat::IF_RGB_UBYTE; break;
		default: break;
		}

		return Result;
	}

	//---------------------------------------------------------------------------------------------
	//! Use with care.
	//---------------------------------------------------------------------------------------------
	template<class T>
	Ptr<T> CloneOrTakeOver( const T* source )
	{
		Ptr<T> result;
		if (source->IsUnique())
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


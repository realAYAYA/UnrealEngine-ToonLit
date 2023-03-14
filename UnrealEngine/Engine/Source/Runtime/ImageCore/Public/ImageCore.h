// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/****
* 
* ImageCore : low level pixel surface types
* 
* FImage owns an array of pixels
* FImageView can point at an array of pixels (FImage or otherwise)
* both have an FImageInfo
* 
* ERawImageFormat::Type is a pixel format that can be used in FImage (or TextureSource)
* 
* helpers to load/save/convert FImage are in Engine in FImageUtils
* 
* ImageCore does not use Engine, only Core
* it can be used in standalone apps that don't have Engine
* 
*/


/* Dependencies
 *****************************************************************************/

#include "Containers/ContainersFwd.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "Math/Color.h"
#include "Math/Float16.h"
#include "Math/Float16Color.h"
#include "Math/Vector2D.h"
#include "Misc/AssertionMacros.h"

/* Types
 *****************************************************************************/

// EGammaSpace is in Color.h

namespace ERawImageFormat
{
	/**
	 * Enumerates supported raw image formats.
	 * should map one-to-one with ETextureSourceFormat
	 */
	enum Type : uint8
	{
		G8,
		BGRA8,   // FColor
		BGRE8,
		RGBA16,
		RGBA16F, // FFloat16Color
		RGBA32F, // FLinearColor
		G16,     // note G8/G16 = gray = replicate to 3 channels, R16F = just in red channel
		R16F,
		R32F,
		Invalid = 0xFF
	};
	
	// RGBA -> G8 takes from the  *R* channel
	// G8 -> RGBA replicated to gray
 
	IMAGECORE_API int32 GetBytesPerPixel(Type Format);
	
	IMAGECORE_API const TCHAR * GetName(Type Format);
	
	IMAGECORE_API bool IsHDR(Type Format);
	
	// Get one pixel of Format type from PixelData and return in Linear color
	IMAGECORE_API FLinearColor GetOnePixelLinear(const void * PixelData,Type Format,bool bSRGB);	
	
	// G8 and BGRA8 are affected by Gamma
	//	16/32 is NOT
	FORCEINLINE bool GetFormatNeedsGammaSpace(Type Format)
	{
		if ( Format == G8 || Format == BGRA8 )
		{
			return true;
		}
		else
		{
			// these formats ignore GammaSpace setting
			return false;
		}
	}

	// when converting from pixel bags that don't store gamma to FImage
	//	you can use this to guess the gamma they probably wanted
	//	do not use when a real gamma flag is available!
	FORCEINLINE EGammaSpace GetDefaultGammaSpace(Type Format)
	{
		if ( GetFormatNeedsGammaSpace(Format) )
		{
			return EGammaSpace::sRGB;
		}
		else
		{
			return EGammaSpace::Linear;
		}
	}
};

namespace UE {
	namespace Color {
		enum class EChromaticAdaptationMethod : uint8;
	}
}

/**
* 
* FImageInfo describes a 2d pixel surface
* it can be used by FImage or FImageView
* 
*/
struct FImageInfo
{
	/** Width of the image. */
	int32 SizeX = 0;
	
	/** Height of the image. */
	int32 SizeY = 0;
	
	/** Number of image slices. */
	int32 NumSlices = 0;
	
	/** Format in which the image is stored. */
	ERawImageFormat::Type Format = ERawImageFormat::BGRA8;
	
	/** The gamma space the image is stored in. */
	EGammaSpace GammaSpace = EGammaSpace::sRGB;

	FImageInfo() { }
	
	FImageInfo(int32 InSizeX, int32 InSizeY, int32 InNumSlices, ERawImageFormat::Type InFormat, EGammaSpace InGammaSpace)
		: SizeX(InSizeX)
		, SizeY(InSizeY)
		, NumSlices(InNumSlices)
		, Format(InFormat)
		, GammaSpace(InGammaSpace)
	{
	}
		
	FORCEINLINE bool IsGammaCorrected() const
	{
		return GammaSpace != EGammaSpace::Linear;
	}

	/**
	 * Gets the number of bytes per pixel.
	 *
	 * @return Bytes per pixel.
	 */
	FORCEINLINE int32 GetBytesPerPixel() const
	{
		return ERawImageFormat::GetBytesPerPixel(Format);
	}
	
	FORCEINLINE int64 GetNumPixels() const
	{
		return (int64) SizeX * SizeY * NumSlices;
	}
	
	FORCEINLINE int64 GetImageSizeBytes() const
	{
		return GetNumPixels() * GetBytesPerPixel();
	}
	
	FORCEINLINE int64 GetSliceNumPixels() const
	{
		return (int64) SizeX * SizeY;
	}
	
	FORCEINLINE int64 GetSliceSizeBytes() const
	{
		return GetSliceNumPixels() * GetBytesPerPixel();
	}
	
	FORCEINLINE int32 GetWidth()  const { return SizeX; }
	FORCEINLINE int32 GetHeight() const { return SizeY; }
	
	FORCEINLINE EGammaSpace GetGammaSpace() const
	{
		// Gamma is ignored unless GetFormatNeedsGammaSpace, so make sure it is Linear
		check( GetFormatNeedsGammaSpace(Format) || GammaSpace == EGammaSpace::Linear );

		return GammaSpace;
	}
};

/***
* 
* Image functions should take an FImageView as input and output to FImage
* 
* FImageView can point at an FImage or any pixel surface
* 
* FImage hold allocations.  FImageView is non-owning.  Make sure the memory pointed at is not freed.
* 
*/
struct FImageView : public FImageInfo
{
	void * RawData = nullptr;

	FImageView() { }
	
	FImageView(const FImageInfo & InInfo,void * InRawData) : FImageInfo(InInfo), RawData(InRawData)
	{
	}

	// Make an FImageView from an array of FColor pixels as BGRA8
	FImageView(const FColor * InColors,int32 InSizeX,int32 InSizeY,EGammaSpace InGammaSpace = EGammaSpace::sRGB)
	{
		RawData = (void *) InColors;
		SizeX = InSizeX;
		SizeY = InSizeY;
		NumSlices = 1;
		Format = ERawImageFormat::BGRA8;
		GammaSpace = InGammaSpace; 
	}
	
	// Make an FImageView from an array of FLinearColor pixels as RGBA32F
	FImageView(const FLinearColor * InColors,int32 InSizeX,int32 InSizeY)
	{
		RawData = (void *) InColors;
		SizeX = InSizeX;
		SizeY = InSizeY;
		NumSlices = 1;
		Format = ERawImageFormat::RGBA32F;
		GammaSpace = EGammaSpace::Linear;
	}
	
	// Make an FImageView from an array of FLinearColor pixels as RGBA16F
	FImageView(const FFloat16Color * InColors,int32 InSizeX,int32 InSizeY)
	{
		RawData = (void *) InColors;
		SizeX = InSizeX;
		SizeY = InSizeY;
		NumSlices = 1;
		Format = ERawImageFormat::RGBA16F;
		GammaSpace = EGammaSpace::Linear;
	}

	FImageView(void * InData,int32 InSizeX, int32 InSizeY, int32 InNumSlices, ERawImageFormat::Type InFormat, EGammaSpace InGammaSpace)
		: FImageInfo(InSizeX,InSizeY,InNumSlices,InFormat,InGammaSpace),
		RawData(InData)
	{
	}
	
	FImageView(void * InData,int32 InSizeX, int32 InSizeY, ERawImageFormat::Type InFormat)
		: FImageInfo(InSizeX,InSizeY,1,InFormat, ERawImageFormat::GetDefaultGammaSpace(InFormat) ),
		RawData(InData)
	{
	}

	// get an FImageView to one 2d slice
	IMAGECORE_API FImageView GetSlice(int32 SliceIndex) const;
};

/**
 * Structure for raw image data.
 */
struct FImage : public FImageInfo
{
	/** Raw image data. */
	TArray64<uint8> RawData;

public:

	/**
	 * Default constructor.
	 */
	FImage()
	{
	}

	/**
	 * Creates and initializes a new image with the specified number of slices.
	 *
	 * @param InSizeX - The image width.
	 * @param InSizeY - The image height.
	 * @param InNumSlices - The number of slices.
	 * @param InFormat - The image format.
	 * @param InGammaSpace - (optional) Image Gamma Space.  GetDefaultGammaSpace() if not provided.
	 */
	IMAGECORE_API FImage(int32 InSizeX, int32 InSizeY, int32 InNumSlices, ERawImageFormat::Type InFormat, EGammaSpace InGammaSpace);

	// note: changed default GammaSpace from Linear to GetDefaultGammaSpace
	FORCEINLINE FImage(int32 InSizeX, int32 InSizeY, int32 InNumSlices, ERawImageFormat::Type InFormat) :
		FImage(InSizeX,InSizeY,InNumSlices,InFormat,ERawImageFormat::GetDefaultGammaSpace(InFormat))
	{
	}

	/**
	 * Creates and initializes a new image with a single slice.
	 *
	 * @param InSizeX - The image width.
	 * @param InSizeY - The image height.
	 * @param InFormat - The image format.
	 * @param InGammaSpace - (optional) Image Gamma Space.  GetDefaultGammaSpace() if not provided.
	 */
	FORCEINLINE FImage(int32 InSizeX, int32 InSizeY, ERawImageFormat::Type InFormat, EGammaSpace InGammaSpace) :
		FImage(InSizeX,InSizeY,1,InFormat,InGammaSpace)
	{
	}

	// note: changed default GammaSpace from Linear to GetDefaultGammaSpace
	FORCEINLINE FImage(int32 InSizeX, int32 InSizeY, ERawImageFormat::Type InFormat) :
		FImage(InSizeX,InSizeY,1,InFormat,ERawImageFormat::GetDefaultGammaSpace(InFormat))
	{
	}


public:

	// Swap the contents of this FImage with another
	IMAGECORE_API void Swap(FImage & Other);
	
	// implicit conversion to FImageView
	// allows FImage to be passed to functions that take FImageView
	// functions that read images should use "const FImageView &" as their argument type
	FORCEINLINE operator FImageView () const
	{
		// copy the shared FImageInfo part :
		const FImageInfo & Info = *this;
		return FImageView( Info , (void *) RawData.GetData() );
	}

	// get an FImageView to one 2d slice
	IMAGECORE_API FImageView GetSlice(int32 SliceIndex) const;
	
	/**
	 * Copies the image to a destination image with the specified format.
	 *
	 * @param DestImage - The destination image.
	 * @param DestFormat - The destination image format.
	 * @param DestSRGB - Whether the destination image is in SRGB format.
	 */
	IMAGECORE_API void CopyTo(FImage& DestImage, ERawImageFormat::Type DestFormat, EGammaSpace DestGammaSpace) const;
	
	// CopyTo same format
	IMAGECORE_API void CopyTo(FImage& DestImage) const
	{
		CopyTo(DestImage,Format,GammaSpace);
	}
	
	// in-place format change :
	// does nothing if already in the desired format
	IMAGECORE_API void ChangeFormat(ERawImageFormat::Type DestFormat, EGammaSpace DestGammaSpace);

	/**
	 * Copies and resizes the image to a destination image with the specified size and format.
	 * Resize is done using bilinear filtering
	 *
	 * @param DestImage - The destination image.
	 * @param DestSizeX - Width of the resized image
	 * @param DestSizeY - Height of the resized image
	 * @param DestFormat - The destination image format.
	 * @param DestSRGB - Whether the destination image is in SRGB format.
	 */
	IMAGECORE_API void ResizeTo(FImage& DestImage, int32 DestSizeX, int32 DestSizeY, ERawImageFormat::Type DestFormat, EGammaSpace DestGammaSpace) const;

	/**
	 * Linearize to a RGBA32F destination image by applying the decoding function that corresponds to the specified source encoding.
	 * If None, this call will be equivalent to CopyTo(DestImage, ERawImageFormat::RGBA32F, EGammaSpace::Linear).
	 *
	 * @param SourceEncoding - Opaque source encoding (matching UE::Color::EEncoding).
	 * @param DestImage - The destination image.
	 * 
	 * SourceEncoding = None (0) to Linearize using the GammaSpace in this FImage
	 */
	IMAGECORE_API void Linearize(uint8 SourceEncoding, FImage& DestImage) const;

	FORCEINLINE void Linearize(FImage& DestImage) const
	{
		Linearize(0,DestImage);
	}

	/**
	 * Apply a color space transformation from the source chromaticities to the engine's working color space.
	 *
	 * @param SourceRedChromaticity - The red chromaticity coordinate of the source color space.
	 * @param SourceGreenChromaticity - The green chromaticity coordinate of the source color space.
	 * @param SourceBlueChromaticity - The blue chromaticity coordinate of the source color space.
	 * @param SourceWhiteChromaticity - The white chromaticity coordinate of the source color space.
	 * @param Method - The chromatic adapation method.
	 * @param EqualityTolerance - The tolerance for the source and working color space chromaticities to be considered equal, bypassing the transform.
	 */
	IMAGECORE_API void TransformToWorkingColorSpace(const FVector2d& SourceRedChromaticity, const FVector2d& SourceGreenChromaticity, const FVector2d& SourceBlueChromaticity, const FVector2d& SourceWhiteChromaticity, UE::Color::EChromaticAdaptationMethod Method, double EqualityTolerance = 1.e-7);

	/**
	 * Initializes this image with the specified number of slices.
	 * Allocates RawData.
	 *
	 * @param InSizeX - The image width.
	 * @param InSizeY - The image height.
	 * @param InNumSlices - The number of slices.
	 * @param InFormat - The image format.
	 * @param bInSRGB - Whether the color values are in SRGB format.
	 */
	IMAGECORE_API void Init(int32 InSizeX, int32 InSizeY, int32 InNumSlices, ERawImageFormat::Type InFormat, EGammaSpace InGammaSpace);
	
	FORCEINLINE void Init(int32 InSizeX, int32 InSizeY, int32 InNumSlices, ERawImageFormat::Type InFormat)
	{
		Init(InSizeX,InSizeY,InNumSlices,InFormat,ERawImageFormat::GetDefaultGammaSpace(InFormat));
	}

	/**
	 * Initializes this image with a single slice.
	 * Allocates RawData.
	 *
	 * @param InSizeX - The image width.
	 * @param InSizeY - The image height.
	 * @param InFormat - The image format.
	 * @param bInSRGB - Whether the color values are in SRGB format.
	 */
	IMAGECORE_API void Init(int32 InSizeX, int32 InSizeY, ERawImageFormat::Type InFormat, EGammaSpace InGammaSpace);
	
	FORCEINLINE void Init(int32 InSizeX, int32 InSizeY, ERawImageFormat::Type InFormat)
	{
		Init(InSizeX,InSizeY,InFormat,ERawImageFormat::GetDefaultGammaSpace(InFormat));
	}

	/**
	 * Initializes this image with specified Info.
	 * Allocates RawData.
	 *
	 * @param Info - The image description.  Can also pass an FImage or FImageView to this function.
	 */
	IMAGECORE_API void Init(const FImageInfo & Info);

public:

	// Convenience accessors to raw data
	
	TArrayView64<uint8> AsG8()
	{
		check(Format == ERawImageFormat::G8);
		return { RawData.GetData(), int64(RawData.Num()) };
	}

	TArrayView64<uint16> AsG16()
	{
		check(Format == ERawImageFormat::G16);
		return { (uint16*)RawData.GetData(), int64(RawData.Num() / sizeof(uint16)) };
	}

	TArrayView64<FColor> AsBGRA8()
	{
		check(Format == ERawImageFormat::BGRA8);
		return { (struct FColor*)RawData.GetData(), int64(RawData.Num() / sizeof(FColor)) };
	}

	TArrayView64<FColor> AsBGRE8()
	{
		check(Format == ERawImageFormat::BGRE8);
		return { (struct FColor*)RawData.GetData(), int64(RawData.Num() / sizeof(FColor)) };
	}

	TArrayView64<uint16> AsRGBA16()
	{
		check(Format == ERawImageFormat::RGBA16);
		return { (uint16*)RawData.GetData(), int64(RawData.Num() / sizeof(uint16)) };
	}

	TArrayView64<FFloat16Color> AsRGBA16F()
	{
		check(Format == ERawImageFormat::RGBA16F);
		return { (class FFloat16Color*)RawData.GetData(), int64(RawData.Num() / sizeof(FFloat16Color)) };
	}

	TArrayView64<FLinearColor> AsRGBA32F()
	{
		check(Format == ERawImageFormat::RGBA32F);
		return { (struct FLinearColor*)RawData.GetData(), int64(RawData.Num() / sizeof(FLinearColor)) };
	}

	TArrayView64<FFloat16> AsR16F()
	{
		check(Format == ERawImageFormat::R16F);
		return { (class FFloat16*)RawData.GetData(), int64(RawData.Num() / sizeof(FFloat16)) };
	}
	
	TArrayView64<float> AsR32F()
	{
		check(Format == ERawImageFormat::R32F);
		return { (float *)RawData.GetData(), int64(RawData.Num() / sizeof(float)) };
	}

	// Convenience accessors to const raw data

	TArrayView64<const uint8> AsG8() const
	{
		check(Format == ERawImageFormat::G8);
		return { RawData.GetData(), int64(RawData.Num()) };
	}

	TArrayView64<const uint16> AsG16() const
	{
		check(Format == ERawImageFormat::G16);
		return { (const uint16*)RawData.GetData(), int64(RawData.Num() / sizeof(uint16)) };
	}

	TArrayView64<const FColor> AsBGRA8() const
	{
		check(Format == ERawImageFormat::BGRA8);
		return { (const struct FColor*)RawData.GetData(), int64(RawData.Num() / sizeof(FColor)) };
	}

	TArrayView64<const FColor> AsBGRE8() const
	{
		check(Format == ERawImageFormat::BGRE8);
		return { (struct FColor*)RawData.GetData(), int64(RawData.Num() / sizeof(FColor)) };
	}

	TArrayView64<const uint16> AsRGBA16() const
	{
		check(Format == ERawImageFormat::RGBA16);
		return { (const uint16*)RawData.GetData(), int64(RawData.Num() / sizeof(uint16)) };
	}

	TArrayView64<const FFloat16Color> AsRGBA16F() const
	{
		check(Format == ERawImageFormat::RGBA16F);
		return { (const class FFloat16Color*)RawData.GetData(), int64(RawData.Num() / sizeof(FFloat16Color)) };
	}

	TArrayView64<const FLinearColor> AsRGBA32F() const
	{
		check(Format == ERawImageFormat::RGBA32F);
		return { (struct FLinearColor*)RawData.GetData(), int64(RawData.Num() / sizeof(FLinearColor)) };
	}

	TArrayView64<const FFloat16> AsR16F() const
	{
		check(Format == ERawImageFormat::R16F);
		return { (const class FFloat16*)RawData.GetData(), int64(RawData.Num() / sizeof(FFloat16)) };
	}
	
	TArrayView64<const float> AsR32F() const
	{
		check(Format == ERawImageFormat::R32F);
		return { (const float*)RawData.GetData(), int64(RawData.Num() / sizeof(float)) };
	}
};

/* Functions
 *****************************************************************************/

IMAGECORE_API int32 ImageParallelForComputeNumJobsForPixels(int64 & OutNumPixelsPerJob,int64 NumPixels);

IMAGECORE_API int32 ImageParallelForComputeNumJobsForRows(int32 & OutNumItemsPerJob,int32 SizeX,int32 SizeY);

namespace FImageCore
{

/**
 * Copy FImageView data, converting pixels if necessary.
 * Sizes must match. Dest must already be allocated.
 * For a function that allocates Dest, use CopyTo().
 * 
 * Because FImage implicitly convers to FImageView, these functions can be used on FImage types as well.
 *
 * @param SrcImage - The source image to copy from.
 * @param DestImage - The destination image to copy to. (the FImageView is const but what it points at is not)
 */
IMAGECORE_API void CopyImage(const FImageView & SrcImage,const FImageView & DestImage);

/**
 * Copy Image, swapping RB, SrcImage must be BGRA8 or RGBA16.
 * Sizes must match. Dest must already be allocated.
 * Src == Dest is okay for in-place transpose.
 *
 * @param SrcImage - The source image to copy from.
 * @param DestImage - The destination image to copy to. (the FImageView is const but what it points at is not)
 */
IMAGECORE_API void CopyImageRGBABGRA(const FImageView & SrcImage,const FImageView & DestImage);

/**
* Swap RB channels on Image.  Image must be BGRA8 or RGBA16.
* 
* @param  Image is modified (the FImageView is const but what it points at is not)
*/
IMAGECORE_API void TransposeImageRGBABGRA(const FImageView & Image);


/**
* Clamp Float16 colors which aren't encodable in the BC6H format
* RGB is clamped to non-negative and finite
* A is set to 1.0
*
* @param  InOutImage is modified.  Must be RGBA16F. (the FImageView is const but what it points at is not)
*/
IMAGECORE_API void SanitizeFloat16AndSetAlphaOpaqueForBC6H(const FImageView & InOutImage);


/**
 * Detects whether or not the image contains an alpha channel where at least one texel is != 255.
 * 
 * @param InImage - The image to look for alpha in.
 * @return True if the image both supports and uses the alpha channel. False otherwise.
 */
IMAGECORE_API bool DetectAlphaChannel(const FImage & InImage);

};


// non-namespaced CopyImage for backwards compatibility
UE_DEPRECATED(5.1,"Use FImageCore::CopyImage")
FORCEINLINE void CopyImage(const FImageView & SrcImage,const FImageView & DestImage)
{
	return FImageCore::CopyImage(SrcImage,DestImage);
}
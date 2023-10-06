// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformStackWalk.h"
#include "Misc/Paths.h"
#include "ImageCore.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/ITextureFormat.h"
#include "Interfaces/ITextureFormatModule.h"
#include "TextureCompressorModule.h"
#include "PixelFormat.h"
#include "HAL/PlatformProcess.h"
#include "TextureBuildFunction.h"
#include "DerivedDataBuildFunctionFactory.h"
#include "DerivedDataSharedString.h"

#ifndef __APPLE__
#define __APPLE__ 0
#endif
#ifndef __unix__
#define __unix__ 0
#endif
#include "Etc.h"
#include "EtcErrorMetric.h"
#include "EtcImage.h"

// Workaround for: error LNK2019: unresolved external symbol __imp___std_init_once_begin_initialize referenced in function "void __cdecl std::call_once
// https://developercommunity.visualstudio.com/t/-imp-std-init-once-complete-unresolved-external-sy/1684365
#if defined(_MSC_VER) && (_MSC_VER >= 1932)  // Visual Studio 2022 version 17.2+
#    pragma comment(linker, "/alternatename:__imp___std_init_once_complete=__imp_InitOnceComplete")
#    pragma comment(linker, "/alternatename:__imp___std_init_once_begin_initialize=__imp_InitOnceBeginInitialize")
#endif

DEFINE_LOG_CATEGORY_STATIC(LogTextureFormatETC2, Log, All);

class FETC2TextureBuildFunction final : public FTextureBuildFunction
{
	const UE::DerivedData::FUtf8SharedString& GetName() const final
	{
		static const UE::DerivedData::FUtf8SharedString Name(UTF8TEXTVIEW("ETC2Texture"));
		return Name;
	}

	void GetVersion(UE::DerivedData::FBuildVersionBuilder& Builder, ITextureFormat*& OutTextureFormatVersioning) const final
	{
		static FGuid Version(TEXT("af5192f4-351f-422f-b539-f6bd4abadfae"));
		Builder << Version;
		OutTextureFormatVersioning = FModuleManager::GetModuleChecked<ITextureFormatModule>(TEXT("TextureFormatETC2")).GetTextureFormat();
	}
};

/**
 * Macro trickery for supported format names.
 */
#define ENUM_SUPPORTED_FORMATS(op) \
	op(ETC2_RGB) \
	op(ETC2_RGBA) \
	op(ETC2_R11) \
	op(ETC2_RG11) \
	op(AutoETC2)

#define DECL_FORMAT_NAME(FormatName) static FName GTextureFormatName##FormatName = FName(TEXT(#FormatName));
ENUM_SUPPORTED_FORMATS(DECL_FORMAT_NAME);
#undef DECL_FORMAT_NAME

#define DECL_FORMAT_NAME_ENTRY(FormatName) GTextureFormatName##FormatName ,
static FName GSupportedTextureFormatNames[] =
{
	ENUM_SUPPORTED_FORMATS(DECL_FORMAT_NAME_ENTRY)
};
#undef DECL_FORMAT_NAME_ENTRY

#undef ENUM_SUPPORTED_FORMATS

// note InSourceData is not const, can be mutated by sanitize
static bool CompressImageUsingEtc2comp(
	FLinearColor * InSourceColors,
	EPixelFormat PixelFormat,
	int32 SizeX,
	int32 SizeY,
	int64 NumPixels,
	EGammaSpace TargetGammaSpace,
	TArray64<uint8>& OutCompressedData)
{
	using namespace Etc;
		
	Image::Format EtcFormat = Image::Format::UNKNOWN;
	switch (PixelFormat)
	{
	case PF_ETC2_RGB:
		EtcFormat = Image::Format::RGB8;
		break;
	case PF_ETC2_RGBA:
		EtcFormat = Image::Format::RGBA8;
		break;
	case PF_ETC2_R11_EAC:
		EtcFormat = Image::Format::R11;
		break;
	case PF_ETC2_RG11_EAC:
		EtcFormat = Image::Format::RG11;
		break;
	default:
		UE_LOG(LogTextureFormatETC2, Fatal, TEXT("Unsupported EPixelFormat for compression: %u"), (uint32)PixelFormat);
		return false;
	}

	// RGBA, REC709, NUMERIC will set RGB to 0 if all pixels in the block are transparent (A=0)
	const Etc::ErrorMetric EtcErrorMetric = Etc::RGBX;
	const float EtcEffort = ETCCOMP_DEFAULT_EFFORT_LEVEL;
	// threads used by etc2comp :
	const unsigned int MAX_JOBS = 8;
	const unsigned int NUM_JOBS = 8;
	// to run etc2comp synchronously :
	//const unsigned int MAX_JOBS = 0;
	//const unsigned int NUM_JOBS = 0;

	unsigned char* paucEncodingBits = nullptr;
	unsigned int uiEncodingBitsBytes = 0;
	unsigned int uiExtendedWidth = 0;
	unsigned int uiExtendedHeight = 0;
	int iEncodingTime_ms = 0;
	float* SourceData = &InSourceColors[0].Component(0);
	
	// InSourceData is a linear color, we need to feed float* data to the codec in a target color space
	TArray64<float> IntermediateData;
	if (TargetGammaSpace == EGammaSpace::sRGB)
	{
		IntermediateData.Reserve(NumPixels * 4);
		IntermediateData.AddUninitialized(NumPixels * 4);

		for (int64 Idx = 0; Idx < IntermediateData.Num(); Idx += 4)
		{
			const FLinearColor& LinColor = *(FLinearColor*)(SourceData + Idx);
			FColor Color = LinColor.ToFColorSRGB();
			IntermediateData[Idx + 0] = Color.R / 255.f;
			IntermediateData[Idx + 1] = Color.G / 255.f;
			IntermediateData[Idx + 2] = Color.B / 255.f;
			IntermediateData[Idx + 3] = Color.A / 255.f;
		}
		
		SourceData = IntermediateData.GetData();
	}
	else
	{
		int64 NumFloats = NumPixels * 4;

		for(int64 Idx =0 ;Idx < NumFloats;Idx++)
		{
			// sanitize inf and nan :
			float f = SourceData[Idx];
			if ( f >= -FLT_MAX && f <= FLT_MAX )
			{
				// finite, leave it
				// nans will fail all compares so not go in here
			}
			else if ( f > FLT_MAX )
			{
				// +inf
				SourceData[Idx] = FLT_MAX;
			}
			else if ( f < -FLT_MAX )
			{
				// -inf
				SourceData[Idx] = -FLT_MAX;
			}
			else
			{
				// nan
				SourceData[Idx] = 0.f;
			}

			//check( ! FMath::IsNaN( SourceData[Idx] ) );
		}
	}

	Encode(
		SourceData,
		SizeX, SizeY,
		EtcFormat,
		EtcErrorMetric,
		EtcEffort,
		NUM_JOBS,
		MAX_JOBS,
		&paucEncodingBits, &uiEncodingBitsBytes,
		&uiExtendedWidth, &uiExtendedHeight,
		&iEncodingTime_ms
	);

	OutCompressedData.SetNumUninitialized(uiEncodingBitsBytes);
	FMemory::Memcpy(OutCompressedData.GetData(), paucEncodingBits, uiEncodingBitsBytes);
	delete[] paucEncodingBits;
	return true;
}

/**
 * ETC2 texture format handler.
 */
class FTextureFormatETC2 : public ITextureFormat
{
public:
	virtual bool AllowParallelBuild() const override
	{
		return true;
	}

	virtual uint16 GetVersion(
		FName Format,
		const struct FTextureBuildSettings* BuildSettings = nullptr
	) const override
	{
		return 3;
	}

	virtual FName GetEncoderName(FName Format) const override
	{
		static const FName ETC2Name("ETC2");
		return ETC2Name;
	}

	virtual void GetSupportedFormats(TArray<FName>& OutFormats) const override
	{
		OutFormats.Append(GSupportedTextureFormatNames, UE_ARRAY_COUNT(GSupportedTextureFormatNames));
	}

	virtual EPixelFormat GetEncodedPixelFormat(const FTextureBuildSettings& BuildSettings, bool bImageHasAlphaChannel) const override
	{
		if (BuildSettings.TextureFormatName == GTextureFormatNameETC2_RGB ||
			BuildSettings.TextureFormatName == GTextureFormatNameETC2_RGBA ||
			BuildSettings.TextureFormatName == GTextureFormatNameAutoETC2 )
		{
			if ( BuildSettings.TextureFormatName == GTextureFormatNameETC2_RGB || !bImageHasAlphaChannel )
			{
				// even if Name was RGBA we still use the RGB profile if !bImageHasAlphaChannel
				//	so that "Compress Without Alpha" can force us to opaque

				return PF_ETC2_RGB;
			}
			else
			{
				return PF_ETC2_RGBA;
			}
		}

		if (BuildSettings.TextureFormatName == GTextureFormatNameETC2_R11)
		{
			return PF_ETC2_R11_EAC;
		}
		else if (BuildSettings.TextureFormatName == GTextureFormatNameETC2_RG11)
		{
			return PF_ETC2_RG11_EAC;
		}

		UE_LOG(LogTextureFormatETC2, Fatal, TEXT("Unhandled texture format '%s' given to FTextureFormatAndroid::GetEncodedPixelFormat()"), *BuildSettings.TextureFormatName.ToString());
		return PF_Unknown;
	}

	virtual bool CompressImage(
		const FImage& InImage,
		const struct FTextureBuildSettings& BuildSettings,
		const FIntVector3& InMip0Dimensions,
		int32 InMip0NumSlicesNoDepth,
		int32 InMipIndex,
		int32 InMipCount,
		FStringView DebugTexturePathName,
		bool bImageHasAlphaChannel,
		FCompressedImage2D& OutCompressedImage
		) const override
	{
		const FImage& Image = InImage;
		// Source is expected to be F32 linear color
		check(Image.Format == ERawImageFormat::RGBA32F);

		EPixelFormat CompressedPixelFormat = GetEncodedPixelFormat(BuildSettings, bImageHasAlphaChannel);

		bool bCompressionSucceeded = true;
		int64 SliceSize = Image.GetSliceNumPixels();
		for (int32 SliceIndex = 0; SliceIndex < Image.NumSlices && bCompressionSucceeded; ++SliceIndex)
		{
			TArray64<uint8> CompressedSliceData;
			
			const FLinearColor * SlicePixels = Image.AsRGBA32F().GetData() + SliceIndex * SliceSize;
			bCompressionSucceeded = CompressImageUsingEtc2comp(
				const_cast<FLinearColor *>(SlicePixels),
				CompressedPixelFormat,
				Image.SizeX,
				Image.SizeY,
				SliceSize,
				BuildSettings.GetDestGammaSpace(),
				CompressedSliceData
			);
			OutCompressedImage.RawData.Append(CompressedSliceData);
		}

		if (bCompressionSucceeded)
		{
			OutCompressedImage.SizeX = Image.SizeX;
			OutCompressedImage.SizeY = Image.SizeY;
			OutCompressedImage.SizeZ = (BuildSettings.bVolume || BuildSettings.bTextureArray) ? Image.NumSlices : 1;
			OutCompressedImage.PixelFormat = CompressedPixelFormat;
		}

		return bCompressionSucceeded;
	}
};

class FTextureFormatETC2Module : public ITextureFormatModule
{
public:
	ITextureFormat* Singleton = NULL;

	FTextureFormatETC2Module() { }
	virtual ~FTextureFormatETC2Module()
	{
		if ( Singleton )
		{
			delete Singleton;
			Singleton = nullptr;
		}
	}

	virtual void StartupModule() override
	{
	}
	
	virtual bool CanCallGetTextureFormats() override { return false; }

	virtual ITextureFormat* GetTextureFormat()
	{
		if ( Singleton == nullptr )  // not thread safe
		{
			FTextureFormatETC2* ptr = new FTextureFormatETC2();
			Singleton = ptr;
		}
		return Singleton;
	}

	static inline UE::DerivedData::TBuildFunctionFactory<FETC2TextureBuildFunction> BuildFunctionFactory;
};

IMPLEMENT_MODULE(FTextureFormatETC2Module, TextureFormatETC2);

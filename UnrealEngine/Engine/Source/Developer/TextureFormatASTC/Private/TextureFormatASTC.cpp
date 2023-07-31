// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Misc/ConfigCacheIni.h"
#include "ImageCore.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/ITextureFormat.h"
#include "Interfaces/ITextureFormatModule.h"
#include "TextureCompressorModule.h"
#include "PixelFormat.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinaryWriter.h"
#include "TextureBuildFunction.h"
#include "DerivedDataBuildFunctionFactory.h"
#include "DerivedDataSharedString.h"
#include "HAL/IConsoleManager.h"

/****
* 
* TextureFormatASTC runs the ARM astcenc.exe command line tool
* 
* or (by default) redirects to Intel ISPC texcomp* 
* 
*****/

// when GASTCCompressor == 0 ,use TextureFormatIntelISPCTexComp instead of this
// @todo Oodle : GASTCCompressor global breaks DDC2.  Need to pass through so TBW can see.
int32 GASTCCompressor = 0;
static FAutoConsoleVariableRef CVarASTCCompressor(
	TEXT("cook.ASTCTextureCompressor"),
	GASTCCompressor,
	TEXT("0: IntelISPC, 1: Arm"),
	ECVF_Default | ECVF_ReadOnly
);

// turn on to leave temp files in Intermediate/Cache for debugging
static int32 GASTCDebugLeaveTempFiles = 0;
static FAutoConsoleVariableRef CVarDebugLeaveTempFiles(
	TEXT("cook.ASTCDebugLeaveTempFiles"),
	GASTCDebugLeaveTempFiles,
	TEXT("0: default, 1: leave debug temp files in Intermediate/Cache"),
	ECVF_Default | ECVF_ReadOnly
);

// turn on to write decoded image in Intermediate/Cache for debugging
static int32 GASTCDebugWriteDecodedImage = 0;
static FAutoConsoleVariableRef CVarDebugWriteDecodedImage(
	TEXT("cook.ASTCDebugWriteDecodedImage"),
	GASTCDebugWriteDecodedImage,
	TEXT("0: default, 1: write decoded image in Intermediate/Cache"),
	ECVF_Default | ECVF_ReadOnly
);


#if PLATFORM_WINDOWS || PLATFORM_LINUX || PLATFORM_MAC
	#define SUPPORTS_ISPC_ASTC	1
#else
	#define SUPPORTS_ISPC_ASTC	0
#endif

// increment this if you change anything that will affect compression in this file
#define BASE_ASTC_FORMAT_VERSION 44

#define MAX_QUALITY_BY_SIZE 4
#define MAX_QUALITY_BY_SPEED 3


DEFINE_LOG_CATEGORY_STATIC(LogTextureFormatASTC, Log, All);

class FASTCTextureBuildFunction final : public FTextureBuildFunction
{
	const UE::DerivedData::FUtf8SharedString& GetName() const final
	{
		static const UE::DerivedData::FUtf8SharedString Name(UTF8TEXTVIEW("ASTCTexture"));
		return Name;
	}

	void GetVersion(UE::DerivedData::FBuildVersionBuilder& Builder, ITextureFormat*& OutTextureFormatVersioning) const final
	{
		static FGuid Version(TEXT("4788dab5-b99c-479f-bc34-6d7df1cf30e5"));
		Builder << Version;
		OutTextureFormatVersioning = FModuleManager::GetModuleChecked<ITextureFormatModule>(TEXT("TextureFormatASTC")).GetTextureFormat();
	}
};

/**
 * Macro trickery for supported format names.
 */
#define ENUM_SUPPORTED_FORMATS(op) \
	op(ASTC_RGB) \
	op(ASTC_RGBA) \
	op(ASTC_RGBAuto) \
	op(ASTC_RGBA_HQ) \
	op(ASTC_RGB_HDR) \
	op(ASTC_NormalAG) \
	op(ASTC_NormalRG)

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

// ASTC file header format
#if PLATFORM_SUPPORTS_PRAGMA_PACK
	#pragma pack(push, 4)
#endif

	#define ASTC_MAGIC_CONSTANT 0x5CA1AB13
	struct FASTCHeader
	{
		uint32 Magic;
		uint8  BlockSizeX;
		uint8  BlockSizeY;
		uint8  BlockSizeZ;
		uint8  TexelCountX[3];
		uint8  TexelCountY[3];
		uint8  TexelCountZ[3];
	};

#if PLATFORM_SUPPORTS_PRAGMA_PACK
	#pragma pack(pop)
#endif


static int32 GetDefaultCompressionBySizeValue(FCbObjectView InFormatConfigOverride)
{
	// this is code duped between TextureFormatASTC and TextureFormatISPC
	if (InFormatConfigOverride)
	{
		// If we have an explicit format config, then use it directly
		FCbFieldView FieldView = InFormatConfigOverride.FindView("DefaultASTCQualityBySize");
		checkf(FieldView.HasValue(), TEXT("Missing DefaultASTCQualityBySize key from FormatConfigOverride"));
		int32 CompressionModeValue = FieldView.AsInt32();
		checkf(!FieldView.HasError(), TEXT("Failed to parse DefaultASTCQualityBySize value from FormatConfigOverride"));
		return CompressionModeValue;
	}
	else
	{
		// default of 0 == 12x12 ?
		// BaseEngine.ini sets DefaultASTCQualityBySize to 3 == 6x6

		auto GetCompressionModeValue = []() {
			// start at default quality, then lookup in .ini file
			int32 CompressionModeValue = 0;
			GConfig->GetInt(TEXT("/Script/UnrealEd.CookerSettings"), TEXT("DefaultASTCQualityBySize"), CompressionModeValue, GEngineIni);
	
			FParse::Value(FCommandLine::Get(), TEXT("-astcqualitybysize="), CompressionModeValue);
			
			return FMath::Min<uint32>(CompressionModeValue, MAX_QUALITY_BY_SIZE);
		};

		static int32 CompressionModeValue = GetCompressionModeValue();

		return CompressionModeValue;
	}
}

static int32 GetDefaultCompressionBySpeedValue(FCbObjectView InFormatConfigOverride)
{
	if (InFormatConfigOverride)
	{
		// If we have an explicit format config, then use it directly
		FCbFieldView FieldView = InFormatConfigOverride.FindView("DefaultASTCQualityBySpeed");
		checkf(FieldView.HasValue(), TEXT("Missing DefaultASTCQualityBySpeed key from FormatConfigOverride"));
		int32 CompressionModeValue = FieldView.AsInt32();
		checkf(!FieldView.HasError(), TEXT("Failed to parse DefaultASTCQualityBySpeed value from FormatConfigOverride"));
		return CompressionModeValue;
	}
	else
	{

		// default of 0 == "fastest"

		auto GetCompressionModeValue = []() {
			// start at default quality, then lookup in .ini file
			int32 CompressionModeValue = 0;
			GConfig->GetInt(TEXT("/Script/UnrealEd.CookerSettings"), TEXT("DefaultASTCQualityBySpeed"), CompressionModeValue, GEngineIni);
	
			FParse::Value(FCommandLine::Get(), TEXT("-astcqualitybyspeed="), CompressionModeValue);
			
			return FMath::Min<uint32>(CompressionModeValue, MAX_QUALITY_BY_SPEED);
		};

		static int32 CompressionModeValue = GetCompressionModeValue();

		return CompressionModeValue;
	}
}

static FString GetQualityString(EPixelFormat PixelFormat,const FCbObjectView& InFormatConfigOverride)
{
	// convert to a string
	FString CompressionMode;

	switch (PixelFormat)
	{
		case PF_ASTC_12x12:
		case PF_ASTC_12x12_HDR:
			CompressionMode = TEXT("12x12"); 
			break;
		case PF_ASTC_10x10:
		case PF_ASTC_10x10_HDR:
			CompressionMode = TEXT("10x10"); 
			break;
		case PF_ASTC_8x8:
		case PF_ASTC_8x8_HDR:
			CompressionMode = TEXT("8x8"); 
			break;
		case PF_ASTC_6x6:
		case PF_ASTC_6x6_HDR:
			CompressionMode = TEXT("6x6");
			break;
		case PF_ASTC_4x4:
		case PF_ASTC_4x4_HDR:
			CompressionMode = TEXT("4x4");
			break;
		default:
			UE_LOG(LogTextureFormatASTC, Fatal, TEXT("ASTC size quality higher than expected"));
	}
	
	switch ( GetDefaultCompressionBySpeedValue(InFormatConfigOverride) )
	{
		case 0:	CompressionMode += TEXT(" -fastest"); break;
		case 1:	CompressionMode += TEXT(" -fast"); break;
		case 2:	CompressionMode += TEXT(" -medium"); break;
		case 3:	CompressionMode += TEXT(" -thorough"); break;
		default: UE_LOG(LogTextureFormatASTC, Fatal, TEXT("ASTC speed quality higher than expected"));
	}

	return CompressionMode;
}

static EPixelFormat GetQualityFormat(const FTextureBuildSettings& BuildSettings)
{
	// code dupe between TextureFormatASTC  and TextureFormatISPC

	const FCbObjectView& InFormatConfigOverride = BuildSettings.FormatConfigOverride;
	int32 OverrideSizeValue= BuildSettings.CompressionQuality;

	bool bIsNormalMap = (BuildSettings.TextureFormatName == GTextureFormatNameASTC_NormalAG || BuildSettings.TextureFormatName == GTextureFormatNameASTC_NormalRG);
	bool bIsHQ = BuildSettings.TextureFormatName == GTextureFormatNameASTC_RGBA_HQ;
	bool bHDRFormat = BuildSettings.TextureFormatName == GTextureFormatNameASTC_RGB_HDR;

	if ( bIsNormalMap )
	{
		return PF_ASTC_6x6;
	}
	else if ( bIsHQ )
	{
		return PF_ASTC_4x4;
	}
	else if (BuildSettings.bVirtualStreamable)
	{
		return PF_ASTC_4x4;		
	}

	// CompressionQuality value here is ETextureCompressionQuality minus 1

	// convert to a string
	EPixelFormat Format = PF_Unknown;
	if (bHDRFormat)
	{
		switch (OverrideSizeValue >= 0 ? OverrideSizeValue : GetDefaultCompressionBySizeValue(InFormatConfigOverride))
		{
			case 0:	Format = PF_ASTC_12x12_HDR; break;
			case 1:	Format = PF_ASTC_10x10_HDR; break;
			case 2:	Format = PF_ASTC_8x8_HDR; break;
			case 3:	Format = PF_ASTC_6x6_HDR; break;
			case 4:	Format = PF_ASTC_4x4_HDR; break;
			default: UE_LOG(LogTextureFormatASTC, Fatal, TEXT("Max quality higher than expected"));
		}
	}
	else
	{
		switch (OverrideSizeValue >= 0 ? OverrideSizeValue : GetDefaultCompressionBySizeValue(InFormatConfigOverride))
		{
			case 0:	Format = PF_ASTC_12x12; break;
			case 1:	Format = PF_ASTC_10x10; break;
			case 2:	Format = PF_ASTC_8x8; break;
			case 3:	Format = PF_ASTC_6x6; break;
			case 4:	Format = PF_ASTC_4x4; break;
			default: UE_LOG(LogTextureFormatASTC, Fatal, TEXT("Max quality higher than expected"));
		}
	}
	return Format;
}

static bool CompressSliceToASTC(
	const FImageView & SourceImage,
	FString CompressionParameters,
	TArray64<uint8>& OutCompressedData,
	IImageWrapperModule& ImageWrapperModule
	)
{
	// at this point, SourceImage has been converted to RGBA8 or RGBA16F based on whether
	//	 the request TextureFormatName is "ASTC_RGB_HDR" or not, so we can ask if "source" is HDR :
	bool bHDR = ERawImageFormat::IsHDR(SourceImage.Format);
	
	EImageFormat FileFormat = bHDR ? EImageFormat::EXR : EImageFormat::PNG;
	TArray64<uint8> FileData;
	bool bCompressSucceeded = ImageWrapperModule.CompressImage(FileData,FileFormat,SourceImage,(int32)EImageCompressionQuality::Uncompressed);
	if ( ! bCompressSucceeded )
	{
		UE_LOG(LogTextureFormatASTC, Error, TEXT("CompressSliceToASTC CompressImage failed"));
		return false;
	}

	int64 FileDataSize = FileData.Num();

	// make a random file name to write the image :
	FGuid Guid;
	FPlatformMisc::CreateGuid(Guid);
	FString BaseFilePath = FPaths::ProjectIntermediateDir() + FString::Printf(TEXT("Cache/TFASTC-%08x-%08x-%08x-%08x-"), Guid.A, Guid.B, Guid.C, Guid.D);
	FString InputFilePath = BaseFilePath + TEXT("In.") + ImageWrapperModule.GetExtension(FileFormat);
	FString OutputFilePath = BaseFilePath + TEXT("Out.astc");

	// write to InputFilePath :
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ASTC.WriteFile);

		FArchive* PNGFile = IFileManager::Get().CreateFileWriter(*InputFilePath);
		while (!PNGFile)
		{
			// CreateFileWriter occasionally returns NULL due to error code ERROR_SHARING_VIOLATION
			// ... no choice but to wait for the file to become free to access
		
			UE_LOG(LogTextureFormatASTC, Display, TEXT("CreateFileWriter for %s failed, trying again..."), *InputFilePath);

			FPlatformProcess::Sleep(0.01f);                             
			PNGFile = IFileManager::Get().CreateFileWriter(*InputFilePath);   
		}
		PNGFile->Serialize((void*)&FileData[0], FileDataSize);
		delete PNGFile;
	}

	// FileData written, can free now :
	FileData.Reset();
	
	// @@CB @todo Oodle : why do we use -cl (LDR linear) and not -cs (LDR sRGB) ?

	// Compress PNG file to ASTC (using the reference astcenc.exe from ARM)
	// -j option to set thread count ? when we're running lots of textures at the same time in a cook,
	//	 it might be better to limit the astcenc process to fewer threads?
	FString Params = (bHDR ? TEXT("-ch ") : TEXT("-cl ")) + FString::Printf(TEXT("\"%s\" \"%s\" %s -silent"),
		*InputFilePath,
		*OutputFilePath,
		*CompressionParameters
	);

	UE_LOG(LogTextureFormatASTC, Verbose, TEXT("Compressing to ASTC (options = '%s')..."), *CompressionParameters);

	// Start Compressor
	// @todo Oodle : check if we have sse4 and use those
#if PLATFORM_MAC_X86
	FString CompressorPath(FPaths::EngineDir() + TEXT("Binaries/ThirdParty/ARM/Mac/astcenc-sse2"));
	//FString CompressorPath(FPaths::EngineDir() + TEXT("Binaries/ThirdParty/ARM/Mac/astcenc-sse4.1"));
#elif PLATFORM_MAC_ARM64
	FString CompressorPath(FPaths::EngineDir() + TEXT("Binaries/ThirdParty/ARM/Mac/astcenc-neon"));
#elif PLATFORM_LINUX
	FString CompressorPath(FPaths::EngineDir() + TEXT("Binaries/ThirdParty/ARM/Linux64/astcenc-sse2"));
	//FString CompressorPath(FPaths::EngineDir() + TEXT("Binaries/ThirdParty/ARM/Linux64/astcenc-sse4.1"));
#elif PLATFORM_WINDOWS
	FString CompressorPath(FPaths::EngineDir() + TEXT("Binaries/ThirdParty/ARM/Win64/astcenc-sse2.exe"));
	//FString CompressorPath(FPaths::EngineDir() + TEXT("Binaries/ThirdParty/ARM/Win64/astcenc-sse4.1.exe"));

	// avx2 is no faster than sse4 , so just use the sse4 variant when possible
	//FString CompressorPath(FPaths::EngineDir() + TEXT("Binaries/ThirdParty/ARM/Win64/astcenc-avx2.exe"));
#else
#error Unsupported platform
#endif

	// run the astcenc process :
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ASTC.RunProc);

		FProcHandle Proc = FPlatformProcess::CreateProc(*CompressorPath, *Params, true, false, false, NULL, -1, NULL, NULL);

		// Failed to start the compressor process
		if (!Proc.IsValid())
		{
			UE_LOG(LogTextureFormatASTC, Error, TEXT("Failed to start astcenc for compressing images (%s)"), *CompressorPath);
			return false;
		}

		// Wait for the process to complete
		FPlatformProcess::WaitForProc(Proc);
		int ReturnCode = -1;
		FPlatformProcess::GetProcReturnCode(Proc, &ReturnCode);
		FPlatformProcess::CloseProc(Proc);
		
		// Did it work?
		if ( ReturnCode != 0)
		{
			UE_LOG(LogTextureFormatASTC, Error, TEXT("ASTC encoder failed with return code %d, mip size (%d, %d). Leaving '%s' for testing.  Full params = '%s'"), 
				ReturnCode, SourceImage.SizeX, SourceImage.SizeY, *InputFilePath, *Params);
			return false;
		}
	}

	// Open compressed file and put the data in OutCompressedImage
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ASTC.ReadFile);

		// Get raw file data
		TArray64<uint8> ASTCData;
		if ( ! FFileHelper::LoadFileToArray(ASTCData, *OutputFilePath) )
		{
			UE_LOG(LogTextureFormatASTC, Error, TEXT("Failed load output of astcenc (%s -> %s)"),*InputFilePath,*OutputFilePath);
			return false;
		}

		// Process it
		FASTCHeader* Header = (FASTCHeader*)ASTCData.GetData();
			
		// Fiddle with the texel count data to get the right value
		uint32 TexelCountX =
			(Header->TexelCountX[0] <<  0) + 
			(Header->TexelCountX[1] <<  8) + 
			(Header->TexelCountX[2] << 16);
		uint32 TexelCountY =
			(Header->TexelCountY[0] <<  0) + 
			(Header->TexelCountY[1] <<  8) + 
			(Header->TexelCountY[2] << 16);
		uint32 TexelCountZ =
			(Header->TexelCountZ[0] <<  0) + 
			(Header->TexelCountZ[1] <<  8) + 
			(Header->TexelCountZ[2] << 16);

		if ( TexelCountX != SourceImage.SizeX ||
			 TexelCountY != SourceImage.SizeY )
		{
			UE_LOG(LogTextureFormatASTC, Warning, TEXT("Unexpected image size mismatch : %d x %d != %d x %d"),
				TexelCountX,TexelCountY,SourceImage.SizeX,SourceImage.SizeY);
		}

		// Calculate size of this mip in blocks
		uint32 MipSizeX = (TexelCountX + Header->BlockSizeX - 1) / Header->BlockSizeX;
		uint32 MipSizeY = (TexelCountY + Header->BlockSizeY - 1) / Header->BlockSizeY;

		// TexelCountZ ignored

		// A block is always 16 bytes
		uint64 MipSize = (uint64)MipSizeX * MipSizeY * 16;

		// Copy the compressed data
		OutCompressedData.Empty(MipSize);
		OutCompressedData.AddUninitialized(MipSize);
		void* MipData = OutCompressedData.GetData();

		// Calculate the offset to get to the mip data
		check(sizeof(FASTCHeader) == 16);
		check(ASTCData.Num() == (sizeof(FASTCHeader) + MipSize));
		FMemory::Memcpy(MipData, ASTCData.GetData() + sizeof(FASTCHeader), MipSize);
	}

	if ( GASTCDebugWriteDecodedImage )
	{	
		FString DecodedFilePath = BaseFilePath + TEXT("Dec.") + ImageWrapperModule.GetExtension(FileFormat);

		// Params starts with -cl or -ch or -cs , grab that character and change to -dl etc :
		check( Params[0] == TEXT('-') );
		check( Params[1] == TEXT('c') );

		FString DecoderParams = TEXT("-d");
		DecoderParams += Params[2];
		DecoderParams += FString::Printf(TEXT(" \"%s\" \"%s\""),*OutputFilePath,*DecodedFilePath);
		
		UE_LOG(LogTextureFormatASTC, Verbose, TEXT("Decoding ASTC (encode options = '%s' , decode = '%s')..."), *CompressionParameters, *DecoderParams);

		FProcHandle Proc = FPlatformProcess::CreateProc(*CompressorPath, *DecoderParams, true, false, false, NULL, -1, NULL, NULL);

		// Failed to start the compressor process
		if (!Proc.IsValid())
		{
			UE_LOG(LogTextureFormatASTC, Error, TEXT("Failed to start astcenc for decompressing images (%s)"), *CompressorPath);
		}
		else
		{
			FPlatformProcess::WaitForProc(Proc);
			FPlatformProcess::CloseProc(Proc);

			// right after we make it, delete it to clean up
			// break point here to examine
			// or turn on GASTCDebugLeaveTempFiles

			if ( ! GASTCDebugLeaveTempFiles )
			{
				IFileManager::Get().Delete(*DecodedFilePath);
			}
		}
	}

	// Delete intermediate files
	if ( ! GASTCDebugLeaveTempFiles )
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ASTC.DeleteFiles);

		IFileManager::Get().Delete(*InputFilePath);
		IFileManager::Get().Delete(*OutputFilePath);
	}

	return true;
}

/**
 * ASTC texture format handler.
 */
class FTextureFormatASTC : public ITextureFormat
{
public:
	FTextureFormatASTC()
	:	IntelISPCTexCompFormat(*FModuleManager::LoadModuleChecked<ITextureFormatModule>(TEXT("TextureFormatIntelISPCTexComp")).GetTextureFormat()),
		ImageWrapperModule(FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper")))
	{
		// LoadModule has to be done on Main thread
		// can't be done on-demand in the Compress call
	}

	virtual bool AllowParallelBuild() const override
	{
#if SUPPORTS_ISPC_ASTC
		if(GASTCCompressor == 0)
		{
			return IntelISPCTexCompFormat.AllowParallelBuild();
		}
#endif
		return true;
	}
	virtual FName GetEncoderName(FName Format) const override
	{
#if SUPPORTS_ISPC_ASTC
		if (GASTCCompressor == 0)
		{
			return IntelISPCTexCompFormat.GetEncoderName(Format);
		}
#endif
		static const FName ASTCName("ArmASTC");
		return ASTCName;
	}

	virtual FCbObject ExportGlobalFormatConfig(const FTextureBuildSettings& BuildSettings) const override
	{
#if SUPPORTS_ISPC_ASTC
		if(GASTCCompressor == 0)
		{
			return IntelISPCTexCompFormat.ExportGlobalFormatConfig(BuildSettings);
		}
#endif
		FCbWriter Writer;
		Writer.BeginObject("TextureFormatASTCSettings");
		Writer.AddInteger("DefaultASTCQualityBySize", GetDefaultCompressionBySizeValue(FCbObjectView()));
		Writer.AddInteger("DefaultASTCQualityBySpeed", GetDefaultCompressionBySpeedValue(FCbObjectView()));
		Writer.EndObject();
		return Writer.Save().AsObject();
	}

	// Version for all ASTC textures, whether it's handled by the ARM encoder or the ISPC encoder.
	virtual uint16 GetVersion(
		FName Format,
		const FTextureBuildSettings* BuildSettings = nullptr
	) const override
	{
#if SUPPORTS_ISPC_ASTC
		if(GASTCCompressor == 0)
		{
			// set high bit so version numbers of ISPC and ASTC don't overlap :
			check( BASE_ASTC_FORMAT_VERSION < 0x80 );
			return 0x80 | IntelISPCTexCompFormat.GetVersion(Format,BuildSettings);
		}
#endif

		return BASE_ASTC_FORMAT_VERSION;
	}

	virtual FString GetDerivedDataKeyString(const FTextureBuildSettings& BuildSettings) const override
	{
#if SUPPORTS_ISPC_ASTC
		if(GASTCCompressor == 0)
		{
			return IntelISPCTexCompFormat.GetDerivedDataKeyString(BuildSettings);
		}
#endif

		// ASTC block size chosen is in PixelFormat
		EPixelFormat PixelFormat = GetQualityFormat(BuildSettings);
		int Speed = GetDefaultCompressionBySpeedValue(BuildSettings.FormatConfigOverride);

 		return FString::Printf(TEXT("ASTC_%d_%d"), (int)PixelFormat,Speed);
	}

	virtual void GetSupportedFormats(TArray<FName>& OutFormats) const override
	{
		OutFormats.Append(GSupportedTextureFormatNames, sizeof(GSupportedTextureFormatNames)/sizeof(GSupportedTextureFormatNames[0]) ); 
	}

	virtual EPixelFormat GetEncodedPixelFormat(const FTextureBuildSettings& InBuildSettings, bool bInImageHasAlphaChannel) const override
	{
		return GetQualityFormat(InBuildSettings);
	}

	virtual bool CompressImage(
			FImage& InImage,
			const FTextureBuildSettings& BuildSettings,
			FStringView DebugTexturePathName,
			bool bImageHasAlphaChannel,
			FCompressedImage2D& OutCompressedImage
		) const override
	{
#if SUPPORTS_ISPC_ASTC
		if(GASTCCompressor == 0)
		{
			UE_CALL_ONCE( [&](){
				UE_LOG(LogTextureFormatASTC, Display, TEXT("TextureFormatASTC using ISPC"))
			} );

			// Route ASTC compression work to the ISPC module instead.
			// note: ISPC can't do HDR, will throw an error
			return IntelISPCTexCompFormat.CompressImage(InImage, BuildSettings, DebugTexturePathName, bImageHasAlphaChannel, OutCompressedImage);
		}
#endif

		TRACE_CPUPROFILER_EVENT_SCOPE(ASTC.CompressImage);

		UE_CALL_ONCE( [&](){
			UE_LOG(LogTextureFormatASTC, Display, TEXT("TextureFormatASTC using astcenc"))
		} );

		bool bHDRImage = BuildSettings.TextureFormatName == GTextureFormatNameASTC_RGB_HDR;

		// Get Raw Image Data from passed in FImage & convert to BGRA8 or RGBA16F
		// note: wasteful, often copies image to same format
		FImage Image;
		InImage.CopyTo(Image, bHDRImage ? ERawImageFormat::RGBA16F : ERawImageFormat::BGRA8, BuildSettings.GetDestGammaSpace());

		// Determine the compressed pixel format and compression parameters
		EPixelFormat CompressedPixelFormat = GetEncodedPixelFormat(BuildSettings, bImageHasAlphaChannel);

		FString CompressionParameters = TEXT("");

		FString QualityString = GetQualityString(CompressedPixelFormat,BuildSettings.FormatConfigOverride);
		
		/*

		The modes available are:

			-cl : use the linear LDR color profile.
			-cs : use the sRGB LDR color profile.
			-ch : use the HDR color profile, tuned for HDR RGB and LDR A.
			-cH : use the HDR color profile, tuned for HDR RGBA.

			-ch or -cl is added later in CompressSlice

		*/


		if (bHDRImage)
		{
			CompressionParameters = QualityString;
			
			// ASTC can encode floats that BC6H can't
			//  but still clamp as if we were BC6H, so that the same output is made
			// (eg. ASTC can encode A but BC6 can't; we stuff 1 in A here)
			FImageCore::SanitizeFloat16AndSetAlphaOpaqueForBC6H(Image);
		}
		else if ( BuildSettings.TextureFormatName == GTextureFormatNameASTC_RGB ||
			BuildSettings.TextureFormatName == GTextureFormatNameASTC_RGBA || 
			BuildSettings.TextureFormatName == GTextureFormatNameASTC_RGBAuto || 
			BuildSettings.TextureFormatName == GTextureFormatNameASTC_RGBA_HQ )
		{
			// astcenc has "-perceptual" but it just does luma weighting so its not very interesting

			if ( BuildSettings.TextureFormatName == GTextureFormatNameASTC_RGB ||
				! bImageHasAlphaChannel )
			{
				// even if Name was RGBA we still use the RGB profile if !bImageHasAlphaChannel
				//	so that "Compress Without Alpha" can force us to opaque

				// we need to set alpha to opaque here
				// can do it using "1" in the bgra swizzle to astcenc
				
				CompressionParameters = FString::Printf(TEXT("%s -esw rgb1"), *QualityString );
			}
			else
			{
				CompressionParameters = QualityString;
			}
		}
		else if (BuildSettings.TextureFormatName == GTextureFormatNameASTC_NormalAG)
		{
			// or "gggr" ?
			// note that DXT5n processing does "1g0r"
			CompressionParameters = FString::Printf(TEXT("%s -esw 0g0r -cw 0 1 0 1 -dblimit 60"), *QualityString);
		}
		else if (BuildSettings.TextureFormatName == GTextureFormatNameASTC_NormalRG)
		{
			CompressionParameters = FString::Printf(TEXT("%s -esw rg00 -cw 1 1 0 0 -dblimit 60"), *QualityString);
		}
		else
		{
			check(false);
		}

		// Compress the image, slice by slice
		bool bCompressionSucceeded = true;

		for (int32 SliceIndex = 0; SliceIndex < Image.NumSlices; ++SliceIndex)
		{
			TArray64<uint8> CompressedSliceData;

			FImageView Slice = Image.GetSlice(SliceIndex);
			
			bCompressionSucceeded = CompressSliceToASTC(Slice,CompressionParameters,CompressedSliceData,ImageWrapperModule);

			if ( ! bCompressionSucceeded )
			{
				return false;
			}
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

private:
	const ITextureFormat& IntelISPCTexCompFormat;
	
	IImageWrapperModule& ImageWrapperModule;
};

/**
 * Module for ASTC texture compression.
 */
static ITextureFormat* Singleton = NULL;

class FTextureFormatASTCModule : public ITextureFormatModule
{
public:
	FTextureFormatASTCModule()
	{
	}
	virtual ~FTextureFormatASTCModule()
	{
		delete Singleton;
		Singleton = NULL;
	}
	
	virtual void StartupModule() override
	{
	}

	virtual bool CanCallGetTextureFormats() override { return false; }

	virtual ITextureFormat* GetTextureFormat()
	{
		if (!Singleton)
		{
			Singleton = new FTextureFormatASTC();
		}
		return Singleton;
	}

	static inline UE::DerivedData::TBuildFunctionFactory<FASTCTextureBuildFunction> BuildFunctionFactory;
};

IMPLEMENT_MODULE(FTextureFormatASTCModule, TextureFormatASTC);

// Copyright Epic Games, Inc. All Rights Reserved.



// OodleDataCompressionFormat :
//	Oodle in Unreal for pak/iostore
//	sets compression options from command line
//	(automation tool turns config into command line for us)
//  installs the "Oodle" named CompressionFormat


/*****************
* 
* Oodle Data compression format
* provides Oodle compression for Pak files & iostore
* is not for generic compression usage
* 
* The Oodle LZ codecs are extremely fast to decode and almost always speed up load times
* 
* The codecs are :
* Kraken  : high compression with good decode speed, the usual default
* Mermaid : less compression and faster decode speed; good when CPU bound or on platforms with less CPU power
* Selkie  : even less compression and faster that Mermaid
* Leviathan : more compression and slower to decode than Kraken
* 
* The encode time is mostly independent of the codec.  Use the codec to choose decode speed, and the encoder effort
* level to control encode time.
* 
* For daily iteration you might want level 3 ("Fast").  For shipping packages you might want level 6 ("optimal2") or higher.
* The valid level range is -4 to 9
* 
* This plugin reads its options on the command line via "compressmethod" and "compresslevel"
* eg. "-compressmethod=Kraken -compresslevel=4"
* 
* The Oodle decoder can decode any codec used, it doesn't need to know which one was used.
* 
* Compression options should be set up in your Game.ini ; for example :
* 

[/Script/UnrealEd.ProjectPackagingSettings]
bCompressed=True
bForceUseProjectCompressionFormat=False
PackageCompressionFormat=Oodle
PackageAdditionalCompressionOptions=-compressionblocksize=256KB
PackageCompressionMethod=Kraken
PackageCompressionLevel_Distribution=7
PackageCompressionLevel_TestShipping=5
PackageCompressionLevel_DebugDevelopment=4

* This can be set in DefaultGame.ini then overrides set up per-platform.
* 
* The Engine also has a veto compressionformat set up in the DataDrivenPlatformInfo.ini for each platform in the field
* "HardwareCompressionFormat"
* eg. platforms that don't want any software compressor can set "HardwareCompressionFormat=None" and this will override what you
* set in "PackageCompressionFormat".
* 
* The idea is in typical use, you set "PackageCompressionFormat" for your Game, and you get that compressor on most platforms, but on
* some platforms that don't want compression, it automatically turns off.
* 
* If you want to force use of your Game.ini compressor (ignore the HardwareCompressionFormat) you can set bForceUseProjectCompressionFormat
* in ProjectPackagingSettings.
* 
* 
* ***************************/

#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "Misc/Compression.h"
#include "Misc/ICompressionFormat.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"

/*
#if WITH_EDITOR
#include "Settings/ProjectPackagingSettings.h"
#endif
*/

#include "HAL/PlatformProcess.h"

#include "Compression/OodleDataCompression.h"
#include "oodle2.h"


#define OODLE_DERIVEDDATA_VER TEXT("BA7AA26CD1C3498787A3F3AA53895042")

// function pointer for old DLL import :
extern "C"
{
typedef OO_SINTa (OOLINK t_fp_OodleLZ_Compress)(OodleLZ_Compressor compressor,
    const void * rawBuf,OO_SINTa rawLen,void * compBuf,
    OodleLZ_CompressionLevel selection,
    const OodleLZ_CompressOptions * pOptions,
    const void * dictionaryBase,
    const void * lrm);

typedef OO_SINTa (OOLINK t_fp_OodleSetAllocators)(
	t_fp_OodleCore_Plugin_MallocAligned* fp_OodleMallocAligned,
	t_fp_OodleCore_Plugin_Free* fp_OodleFree);
};


struct FOodleDataCompressionFormat : ICompressionFormat
{
	OodleLZ_Compressor Compressor;
	OodleLZ_CompressionLevel CompressionLevel;
	OodleLZ_CompressOptions CompressionOptions;
	
	//  OodleLZCompressFuncPtr is non-null of a DLL/so was loaded for encoding
	t_fp_OodleLZ_Compress  * OodleLZCompressFuncPtr = nullptr;

	static void* OodleAlloc(OO_SINTa Size, OO_S32 Alignment)
	{
		return FMemory::Malloc(SIZE_T(Size), uint32(Alignment));
	}

	static void OodleFree(void* Ptr)
	{
		FMemory::Free(Ptr);
	}

	FOodleDataCompressionFormat(OodleLZ_Compressor InCompressor, OodleLZ_CompressionLevel InCompressionLevel, int InSpaceSpeedTradeoffBytes, void * InOodleCompressFuncPtr, void * InOodleSetAllocatorsFuncPtr)
	{
		Compressor = InCompressor;
		CompressionLevel = InCompressionLevel;
		CompressionOptions = *OodleLZ_CompressOptions_GetDefault(Compressor, CompressionLevel);
		CompressionOptions.spaceSpeedTradeoffBytes = InSpaceSpeedTradeoffBytes;
		// we're usually doing limited chunks, no need for LRM :
		CompressionOptions.makeLongRangeMatcher = false;
		
		OodleLZCompressFuncPtr = (t_fp_OodleLZ_Compress *) InOodleCompressFuncPtr;
		if (InOodleSetAllocatorsFuncPtr)
		{
			((t_fp_OodleSetAllocators *)(InOodleSetAllocatorsFuncPtr))(OodleAlloc, OodleFree);
		}
	}

	virtual ~FOodleDataCompressionFormat() 
	{
	}

	FString GetCompressorString() const
	{
		// convert values to enums
		switch (Compressor)
		{
			case OodleLZ_Compressor_Selkie:				return TEXT("Selkie");
			case OodleLZ_Compressor_Mermaid:			return TEXT("Mermaid");
			case OodleLZ_Compressor_Kraken: 			return TEXT("Kraken");
			case OodleLZ_Compressor_Leviathan:			return TEXT("Leviathan");
			case OodleLZ_Compressor_Hydra:				return TEXT("Hydra");
			default: break;
		}
		return TEXT("Unknown");
	}

	FString GetCompressionLevelString() const
	{
		switch (CompressionLevel)
		{
			case OodleLZ_CompressionLevel_HyperFast4:	return TEXT("HyperFast4");
			case OodleLZ_CompressionLevel_HyperFast3:	return TEXT("HyperFast3");
			case OodleLZ_CompressionLevel_HyperFast2:	return TEXT("HyperFast2");
			case OodleLZ_CompressionLevel_HyperFast1:	return TEXT("HyperFast1");
			case OodleLZ_CompressionLevel_None:			return TEXT("None");
			case OodleLZ_CompressionLevel_SuperFast:	return TEXT("SuperFast");
			case OodleLZ_CompressionLevel_VeryFast:		return TEXT("VeryFast");
			case OodleLZ_CompressionLevel_Fast:			return TEXT("Fast");
			case OodleLZ_CompressionLevel_Normal:		return TEXT("Normal");
			case OodleLZ_CompressionLevel_Optimal1:		return TEXT("Optimal1");
			case OodleLZ_CompressionLevel_Optimal2:		return TEXT("Optimal2");
			case OodleLZ_CompressionLevel_Optimal3:		return TEXT("Optimal3");
			case OodleLZ_CompressionLevel_Optimal4:		return TEXT("Optimal4");
			case OodleLZ_CompressionLevel_Optimal5:		return TEXT("Optimal5");
			default: break;
		}
		return TEXT("Unknown");
	}
	
	virtual bool DoesOwnWorthDecompressingCheck() override
	{
		// Oodle does own "worth it" check internally, don't add one
		return true;
	}

	virtual FName GetCompressionFormatName() override
	{
		return NAME_Oodle;
	}

	virtual uint32 GetVersion() override
	{
		return 20000 + OODLE2_VERSION_MAJOR*100 + OODLE2_VERSION_MINOR;
	}
	

	virtual FString GetDDCKeySuffix() override
	{
		// DerivedDataCache key string
		//  ideally this should be unique for any settings changed
		return FString::Printf(TEXT("C_%s_CL_%s_%s"), *GetCompressorString(), *GetCompressionLevelString(), OODLE_DERIVEDDATA_VER);
	}

	virtual bool Compress(void* OutCompressedBuffer, int32& OutCompressedSize, const void* InUncompressedBuffer, int32 InUncompressedSize, int32 InCompressionData, ECompressionFlags Flags) override
	{
		// OutCompressedSize is read-write
		int32 CompressedBufferSize = OutCompressedSize;

		// CompressedSize should be >= GetCompressedBufferSize(UncompressedSize, CompressionData)
		check(CompressedBufferSize >= GetCompressedBufferSize(InUncompressedSize, InCompressionData));

		if ( Flags & COMPRESS_ForPackaging )
		{
			// Compressing for pak/iostore
			// use the options from the command line (ProjectPackagingSettings) to choose compression settings

			OO_SINTa Result = 0;

			if ( OodleLZCompressFuncPtr != nullptr )
			{
				// encode using func pointer to loaded DLL :
				Result = (*OodleLZCompressFuncPtr)(Compressor, InUncompressedBuffer, InUncompressedSize, OutCompressedBuffer, CompressionLevel, NULL, NULL, NULL);

				// NOTE: not using &CompressOptions
				//	because OodleLZ_CompressOptions_GetDefault has changed from v5
				//	need to either call that in the DLL func ptr or not use it
				//	(SSTB == 0 doesn't mean default in old Oodle)

				#if 0 // DO_CHECK
				if ( Result > 0 )
				{
					// verify we can decode data made with DLL-enocder using the current decoder :

					int32 DecodeSpaceSize = InUncompressedSize + 16; // FuzzSafe_Yes so shouldn't need big padding even on Oodle v5
					void * DecodeSpace = FMemory::Malloc(DecodeSpaceSize);
					check( DecodeSpace != nullptr );

					OO_SINTa DecRet = OodleLZ_Decompress(OutCompressedBuffer,Result,DecodeSpace,InUncompressedSize,OodleLZ_FuzzSafe_Yes);
					check( DecRet == InUncompressedSize );

					int cmp = memcmp(DecodeSpace,InUncompressedBuffer,InUncompressedSize);
					check( cmp == 0 );

					FMemory::Free(DecodeSpace);
				}
				#endif
			}
			else
			{
				Result = OodleLZ_Compress(Compressor, InUncompressedBuffer, InUncompressedSize, OutCompressedBuffer, CompressionLevel, &CompressionOptions);
			}

			// verbose log all compresses :
			//UE_LOG(OodleDataCompression, Display, TEXT("Oodle Compress : %d -> %d"), UncompressedSize, Result);
		
			if (Result <= 0)
			{
				OutCompressedSize = -1;
				return false;
			}
			else
			{
				OutCompressedSize = (int32) Result;
				return true;
			}
		}
		else
		{
			// Not for Packaging
			// in-game or tools compress
			// interpret the legacy Flags if any
			// most new users of Oodle should go through OodleDataCompress instead
			// this supports legacy users

			// Compressor and CompressionLevel are member vars, watch out
			FOodleDataCompression::ECompressor CurCompressor;
			FOodleDataCompression::ECompressionLevel CurCompressionLevel;
			FOodleDataCompression::ECompressionCommonUsage Usage = FOodleDataCompression::GetCommonUsageFromLegacyCompressionFlags(Flags);
			GetCompressorAndLevelForCommonUsage(Usage,CurCompressor,CurCompressionLevel);

			int64 Result = FOodleDataCompression::Compress(
							OutCompressedBuffer,CompressedBufferSize,
							InUncompressedBuffer,InUncompressedSize,
							CurCompressor,CurCompressionLevel);

			if ( Result <= 0 )
			{
				OutCompressedSize = -1;
				return false;
			}
			else
			{
				OutCompressedSize = (int32) Result;
				return true;
			}
		}
	
	}

	virtual bool Uncompress(void* OutUncompressedBuffer, int32& OutUncompressedSize, const void* InCompressedBuffer, int32 InCompressedSize, int32 CompressionData) override
	{
		// OutUncompressedSize is read-write
		return FOodleDataCompression::Decompress(
			OutUncompressedBuffer, OutUncompressedSize,
			InCompressedBuffer,InCompressedSize);
	}

	virtual int32 GetCompressedBufferSize(int32 UncompressedSize, int32 CompressionData) override
	{
		// CompressionData is not used
		int32 Needed = (int32)OodleLZ_GetCompressedBufferSizeNeeded(Compressor, UncompressedSize);

		if ( OodleLZCompressFuncPtr != nullptr )
		{
			// older versions of Oodle needed larger padding
			// old rule was 274 per 256KB :

			int32 NumSeekChunks = (UncompressedSize + OODLELZ_BLOCK_LEN-1)/OODLELZ_BLOCK_LEN;

			Needed += 274*NumSeekChunks;

			// can we call OodleLZ_GetCompressedBufferSizeNeeded from the DLL func ptr?
			//  OodleLZ_GetCompressedBufferSizeNeeded changed signature at one point to add Compressor in 2.8.0
			//	so you'd need to know what version to use
		}

		return Needed;
	}
};

extern ICompressionFormat * CreateOodleDataCompressionFormat();

ICompressionFormat * CreateOodleDataCompressionFormat()
{
	// set up Oodle for packaging by parsing the command line options
	// this is now down on first use of a pluggable ICompressionFormat


	// settings to use in non-tools context (eg. runtime game encoding) :
	// (SetDefaultOodleOptionsForPackaging sets options for pak compression & iostore)
	OodleLZ_Compressor UsedCompressor = OodleLZ_Compressor_Mermaid;
	OodleLZ_CompressionLevel UsedLevel = OodleLZ_CompressionLevel_VeryFast;
	int32 SpaceSpeedTradeoff = OODLELZ_SPACESPEEDTRADEOFFBYTES_DEFAULT;
	void * OodleCompressFuncPtr = nullptr;
	void * OodleSetAllocatorsFuncPtr = nullptr;

	#if ( (!UE_BUILD_SHIPPING) || WITH_EDITOR )
	{

	// parse the command line to get compressor & level settings

	// this Startup is done in various different contexts;
	// when the editor loads up
	// when the game loads (we will be used to decompress only and encode settings are not used)
	// when the package cooking tool loads up <- this is when we set the relevant encode settings
		
	// beware init order; instead of calling IsRunningCommandlet it's safer to just look for -run=

	// is_program is true for cooker & UnrealPak (not Editor or Game)
	bool IsProgram = IS_PROGRAM;
	bool IsIOStore = FCString::Strifind(FCommandLine::Get(), TEXT("-run=iostore")) != NULL;
				
	// we only need to be doing all this when run as UnrealPak or iostore commandlet
	//	(IsProgram also picks up cooker and a few other things, that's okay)
	if ( IsProgram || IsIOStore )
	{
		// defaults if no options set :
		UsedCompressor = OodleLZ_Compressor_Kraken;
		// Kraken is a good compromise of compression ratio & speed

		UsedLevel = OodleLZ_CompressionLevel_Normal;
		// for faster iteration time during development

		SpaceSpeedTradeoff = OODLELZ_SPACESPEEDTRADEOFFBYTES_DEFAULT;
		// SpaceSpeedTradeoff is mainly for tuning the Hydra compressor
		//	it can also be used to skew your compression towards higher ratio vs faster decode

		// convert values to enums
		TMap<FString, OodleLZ_Compressor> MethodMap =
		{ 
			{ TEXT("Selkie"), OodleLZ_Compressor_Selkie },
			{ TEXT("Mermaid"), OodleLZ_Compressor_Mermaid },
			{ TEXT("Kraken"), OodleLZ_Compressor_Kraken },
			{ TEXT("Leviathan"), OodleLZ_Compressor_Leviathan },
			{ TEXT("Hydra"), OodleLZ_Compressor_Hydra },
			// when adding here remember to update FOodleDataCompressionFormat::GetCompressorString()
		};
		TMap<FString, OodleLZ_CompressionLevel> LevelMap = 
		{ 
			{ TEXT("HyperFast4"), OodleLZ_CompressionLevel_HyperFast4 },
			{ TEXT("HyperFast3"), OodleLZ_CompressionLevel_HyperFast3 },
			{ TEXT("HyperFast2"), OodleLZ_CompressionLevel_HyperFast2 },
			{ TEXT("HyperFast1"), OodleLZ_CompressionLevel_HyperFast1 },
			{ TEXT("HyperFast"), OodleLZ_CompressionLevel_HyperFast1 },
			{ TEXT("None")     , OodleLZ_CompressionLevel_None },
			{ TEXT("SuperFast"), OodleLZ_CompressionLevel_SuperFast },
			{ TEXT("VeryFast"), OodleLZ_CompressionLevel_VeryFast },
			{ TEXT("Fast")  , OodleLZ_CompressionLevel_Fast },
			{ TEXT("Normal"), OodleLZ_CompressionLevel_Normal },
			{ TEXT("Optimal1"), OodleLZ_CompressionLevel_Optimal1 },
			{ TEXT("Optimal2"), OodleLZ_CompressionLevel_Optimal2 },
			{ TEXT("Optimal") , OodleLZ_CompressionLevel_Optimal2 },
			{ TEXT("Optimal3"), OodleLZ_CompressionLevel_Optimal3 },
			{ TEXT("Optimal4"), OodleLZ_CompressionLevel_Optimal4 },
			{ TEXT("Optimal5"), OodleLZ_CompressionLevel_Optimal5 },
				
			{ TEXT("-4"), OodleLZ_CompressionLevel_HyperFast4 },
			{ TEXT("-3"), OodleLZ_CompressionLevel_HyperFast3 },
			{ TEXT("-2"), OodleLZ_CompressionLevel_HyperFast2 },
			{ TEXT("-1"), OodleLZ_CompressionLevel_HyperFast1 },
			{ TEXT("0"), OodleLZ_CompressionLevel_None },
			{ TEXT("1"), OodleLZ_CompressionLevel_SuperFast },
			{ TEXT("2"), OodleLZ_CompressionLevel_VeryFast },
			{ TEXT("3"), OodleLZ_CompressionLevel_Fast },
			{ TEXT("4"), OodleLZ_CompressionLevel_Normal },
			{ TEXT("5"), OodleLZ_CompressionLevel_Optimal1 },
			{ TEXT("6"), OodleLZ_CompressionLevel_Optimal2 },
			{ TEXT("7"), OodleLZ_CompressionLevel_Optimal3 },
			{ TEXT("8"), OodleLZ_CompressionLevel_Optimal4 },
			{ TEXT("9"), OodleLZ_CompressionLevel_Optimal5 },
			// when adding here remember to update FOodleDataCompressionFormat::GetCompressionLevelString()
		};

		// override from command line :
		FString Method = "";
		FString Level = "";

		// let commandline override
		//FParse::Value does not change output fields if they are not found
		FParse::Value(FCommandLine::Get(), TEXT("compressmethod="), Method);
		FParse::Value(FCommandLine::Get(), TEXT("compresslevel="), Level);
		FParse::Value(FCommandLine::Get(), TEXT("OodleSpaceSpeedTradeoff="), SpaceSpeedTradeoff);
		
		OodleLZ_Compressor * pUsedCompressor = MethodMap.Find(Method);
		if (pUsedCompressor)
		{
			UsedCompressor = *pUsedCompressor;
		}

		OodleLZ_CompressionLevel * pUsedLevel = LevelMap.Find(Level);
		if (pUsedLevel)
		{
			UsedLevel = *pUsedLevel;
		}

		// no init log line if we're not enabled for pak/iostore :
		bool bUseCompressionFormatOodle = 
			( FCString::Strifind(FCommandLine::Get(), TEXT("-compressionformats=oodle")) != NULL ) ||
			( FCString::Strifind(FCommandLine::Get(), TEXT("-compressionformat=oodle")) != NULL );

		if ( bUseCompressionFormatOodle )			
		{
			UE_LOG(OodleDataCompression, Display, TEXT("Oodle v%s format for pak/iostore with method=%s, level=%d=%s"), TEXT(OodleVersion), **MethodMap.FindKey(UsedCompressor), (int)UsedLevel, **LevelMap.FindKey(UsedLevel) );
		
			// OodleCompressDLL to use an earlier *encoder* to maintain identical bit streams
			// mainly for shipped games to avoid patches
			// do not specify this if you want to use the latest Engine version of Oodle for encoding

			// NOTE : we get OodleCompressDLL from Engine.ini for the platform we are running *on* not the platform we are packaging *for*
			// the ProjectPackaging settings we get on the command line from Game.ini come from the *target* platform
			FString OodleDLL = "";

			// check command line first : 
			FParse::Value(FCommandLine::Get(), TEXT("OodleCompressDLL="), OodleDLL);

			if ( OodleDLL.IsEmpty() && GConfig )
			{
				// @todo Oodle : possibly remove this? unnecessary if it's always been put on command line
				// UnrealPak and other "programs" do not read the project config hierarchy
				// CopyBuildToStagingDirectory.Automation.cs reads this config value and passes it on the command line
				GConfig->GetString(TEXT("OodleDataCompressionFormat"), TEXT("OodleCompressDLL"), OodleDLL, GEngineIni);
			}
		
			if ( ! OodleDLL.IsEmpty() )
			{
				UE_LOG(OodleDataCompression, Display, TEXT("OodleCompressDLL=%s"), *OodleDLL);
				
				// just load from default locations :
				//  the DLL you want here should be added in RuntimeDependencies.Add in a build.cs
				//  to ensure it is in the binaries directory of the process
				void * 	OodleDLLHandle = FPlatformProcess::GetDllHandle(*OodleDLL);

				if ( OodleDLLHandle == nullptr )
				{
					UE_LOG(OodleDataCompression, Warning, TEXT("OodleCompressDLL %s requested but could not be loaded"), *OodleDLL);
				}
				else
				{
					// GetDllExport is GetProcAddress or dlsym on Mac/Linux
					OodleCompressFuncPtr = FPlatformProcess::GetDllExport( OodleDLLHandle, TEXT("OodleLZ_Compress") );
					if ( OodleCompressFuncPtr == nullptr )
					{
						UE_LOG(OodleDataCompression, Warning, TEXT("OodleCompressDLL %s loaded but no OodleLZ_Compress !?"), *OodleDLL);
					}
					OodleSetAllocatorsFuncPtr = FPlatformProcess::GetDllExport( OodleDLLHandle, TEXT("OodlePlugins_SetAllocators") );
				}

				// OodleDLLHandle is never freed
			}
		}
	}

	}
	#endif // SHIPPING

	//-----------------------------------
	// register the compression format :
	//  this is used by the shipping game to decode any paks compressed with Oodle :

	ICompressionFormat * CompressionFormat = new FOodleDataCompressionFormat(UsedCompressor, UsedLevel, SpaceSpeedTradeoff, OodleCompressFuncPtr, OodleSetAllocatorsFuncPtr);

	IModularFeatures::Get().RegisterModularFeature(COMPRESSION_FORMAT_FEATURE_NAME, CompressionFormat);

	// NOTE: currently never shutdown and unregistered

	return CompressionFormat;
}


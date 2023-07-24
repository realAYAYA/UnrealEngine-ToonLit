// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compression/OodleDataCompression.h"

#include "CoreGlobals.h"
#include "HAL/CriticalSection.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProperties.h"
#include "HAL/UnrealMemory.h"
#include "Logging/LogCategory.h"
#include "Misc/AssertionMacros.h"
#include "Misc/ConfigCacheIni.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Templates/UnrealTemplate.h"
#include "oodle2.h"
#include "oodle2base.h"

struct ICompressionFormat;

DEFINE_LOG_CATEGORY(OodleDataCompression);
LLM_DEFINE_TAG(OodleData);

extern ICompressionFormat * CreateOodleDataCompressionFormat();

namespace FOodleDataCompression
{

static struct { ECompressor Compressor; const TCHAR* Name; } CompressorNameMap[] = 
{
	{ECompressor::NotSet, TEXT("Not Set")},
	{ECompressor::Selkie, TEXT("Selkie")},
	{ECompressor::Mermaid, TEXT("Mermaid")},
	{ECompressor::Kraken, TEXT("Kraken")},
	{ECompressor::Leviathan, TEXT("Leviathan")}
};

CORE_API bool ECompressorToString(ECompressor InCompressor, const TCHAR** OutName)
{
	if ((SIZE_T)InCompressor >= sizeof(CompressorNameMap) / sizeof(CompressorNameMap[0]))
	{
		return false;
	}

	*OutName = CompressorNameMap[(uint32)InCompressor].Name;
	return true;
}

CORE_API const TCHAR* ECompressorToString(ECompressor InCompressor)
{
	const TCHAR* Ret = nullptr;
	ECompressorToString(InCompressor,&Ret);
	return Ret;
}

CORE_API bool ECompressorFromString(const FString& InName, ECompressor& OutCompressor)
{
	for (SIZE_T i = 0; i < sizeof(CompressorNameMap) / sizeof(CompressorNameMap[0]); i++)
	{
		if (InName == CompressorNameMap[i].Name)
		{
			OutCompressor = CompressorNameMap[i].Compressor;
			return true;
		}
	}
	return false;
}

static struct { ECompressionLevel Level; const TCHAR* Name; } CompressionLevelNameMap[] =
{
	{ECompressionLevel::HyperFast4, TEXT("HyperFast4")},
	{ECompressionLevel::HyperFast3, TEXT("HyperFast3")},
	{ECompressionLevel::HyperFast2, TEXT("HyperFast2")},
	{ECompressionLevel::HyperFast1, TEXT("HyperFast1")},
	{ECompressionLevel::None, TEXT("None")},
	{ECompressionLevel::SuperFast, TEXT("SuperFast")},
	{ECompressionLevel::VeryFast, TEXT("VeryFast")},
	{ECompressionLevel::Fast, TEXT("Fast")},
	{ECompressionLevel::Normal, TEXT("Normal")},
	{ECompressionLevel::Optimal1, TEXT("Optimal1")},
	{ECompressionLevel::Optimal2, TEXT("Optimal2")},
	{ECompressionLevel::Optimal3, TEXT("Optimal3")},
	{ECompressionLevel::Optimal4, TEXT("Optimal4")}
};

CORE_API bool ECompressionLevelToString(ECompressionLevel InLevel, const TCHAR** OutName)
{
	for (SIZE_T i = 0; i < sizeof(CompressionLevelNameMap) / sizeof(CompressionLevelNameMap[0]); i++)
	{
		if (CompressionLevelNameMap[i].Level == InLevel)
		{
			*OutName = CompressionLevelNameMap[i].Name;
			return true;
		}
	}
	return false;
}

CORE_API const TCHAR* ECompressionLevelToString(ECompressionLevel InLevel)
{
	const TCHAR* Ret = nullptr;
	ECompressionLevelToString(InLevel,&Ret);
	return Ret;
}

CORE_API bool ECompressionLevelFromValue(int8 InValue, ECompressionLevel& OutLevel)
{
	if (InValue < OodleLZ_CompressionLevel_Min ||
		InValue > OodleLZ_CompressionLevel_Max)
	{
		return false;
	}

	OutLevel = (ECompressionLevel)InValue;
	return true;
}


static OodleLZ_Compressor CompressorToOodleLZ_Compressor(ECompressor Compressor)
{
	switch(Compressor)
	{
	case ECompressor::Selkie:
		return OodleLZ_Compressor_Selkie;
	case ECompressor::Mermaid:
		return OodleLZ_Compressor_Mermaid;
	case ECompressor::Kraken:
		return OodleLZ_Compressor_Kraken;
	case ECompressor::Leviathan:
		return OodleLZ_Compressor_Leviathan;
	case ECompressor::NotSet:
		return OodleLZ_Compressor_Invalid;
	default:
		UE_LOG(OodleDataCompression,Error,TEXT("Invalid Compressor: %d\n"),(int)Compressor);
		return OodleLZ_Compressor_Invalid;
	}
}

static OodleLZ_CompressionLevel CompressionLevelToOodleLZ_CompressionLevel(ECompressionLevel Level)
{
	int IntLevel = (int)Level;
	
	if ( IntLevel < (int)OodleLZ_CompressionLevel_Min || IntLevel > (int)OodleLZ_CompressionLevel_Max )
	{
		UE_LOG(OodleDataCompression,Error,TEXT("Invalid Level: %d\n"),IntLevel);
		return OodleLZ_CompressionLevel_Invalid;
	}

	return (OodleLZ_CompressionLevel) IntLevel;
}

ECompressionCommonUsage CORE_API GetCommonUsageFromLegacyCompressionFlags(ECompressionFlags Flags)
{
	switch(Flags)
	{
		case 0:
			return ECompressionCommonUsage::Default;
		case COMPRESS_BiasSpeed:
			return ECompressionCommonUsage::FastRealtimeEncode;
		case COMPRESS_BiasSize:
			return ECompressionCommonUsage::SlowerSmallerEncode;
		case COMPRESS_ForPackaging:
			return ECompressionCommonUsage::SlowestOfflineDistributionEncode;

		default:
			UE_LOG(OodleDataCompression,Error,TEXT("Invalid ECompressionFlags : %04X\n"),Flags);
			return ECompressionCommonUsage::Default;
	}
}

void CORE_API GetCompressorAndLevelForCommonUsage(ECompressionCommonUsage Usage,ECompressor & OutCompressor,ECompressionLevel & OutLevel)
{
	switch(Usage)
	{
		case ECompressionCommonUsage::Default:
			OutCompressor = ECompressor::Kraken;
			OutLevel = ECompressionLevel::Fast;
			break;
		case ECompressionCommonUsage::FastRealtimeEncode:
			OutCompressor = ECompressor::Mermaid;
			OutLevel = ECompressionLevel::HyperFast2;
			break;
		case ECompressionCommonUsage::SlowerSmallerEncode:
			OutCompressor = ECompressor::Kraken;
			OutLevel = ECompressionLevel::Normal;
			break;
		case ECompressionCommonUsage::SlowestOfflineDistributionEncode:
			OutCompressor = ECompressor::Kraken;
			OutLevel = ECompressionLevel::Optimal2;
			break;
		default:
			UE_LOG(OodleDataCompression,Error,TEXT("Invalid ECompressionFlags : %d\n"),(int)Usage);
			OutCompressor = ECompressor::Selkie;
			OutLevel = ECompressionLevel::None;
			return;
	}
}


int64 CORE_API CompressedBufferSizeNeeded(int64 InUncompressedSize)
{
	// size needed is the same for all newlz's
	//	so don't bother with a compressor arg here
	return OodleLZ_GetCompressedBufferSizeNeeded(OodleLZ_Compressor_Kraken, IntCastChecked<OO_SINTa>(InUncompressedSize));
}

int64 CORE_API GetMaximumCompressedSize(int64 InUncompressedSize)
{
	int64 NumBlocks = (InUncompressedSize+ OODLELZ_BLOCK_LEN-1)/OODLELZ_BLOCK_LEN;
	int64 MaxCompressedSize = InUncompressedSize + NumBlocks * OODLELZ_BLOCK_MAXIMUM_EXPANSION;
	return MaxCompressedSize;
}

struct OodleScratchBuffers
{
	OO_SINTa OodleScratchMemorySize = 0;
	int32 OodleScratchBufferCount = 0;

	struct alignas(PLATFORM_CACHE_LINE_SIZE) OodleScratchBuffer
	{
		FCriticalSection OodleScratchMemoryMutex;
		void* OodleScratchMemory = nullptr;
	};

	OodleScratchBuffer* OodleScratches = nullptr;

	OodleScratchBuffers()
	{
		// enough decoder scratch for any compressor & buffer size.
		// note "InCompressor" is what we want to Encode with but we may be asked to decode other compressors!
		OO_SINTa DecoderMemorySizeNeeded = OodleLZDecoder_MemorySizeNeeded(OodleLZ_Compressor_Invalid, -1);

		int32 BufferCount;
		
		if ( FPlatformProperties::RequiresCookedData() )
		{
			// runtime game or similar environment

			OO_SINTa EncoderMemorySizeNeeded = OodleLZ_GetCompressScratchMemBound(OodleLZ_Compressor_Mermaid, OodleLZ_CompressionLevel_VeryFast, 64*1024, nullptr);
			// DecoderMemorySizeNeeded is ~ 450000 , EncoderMemorySizeNeeded is ~ 470000
			OodleScratchMemorySize = DecoderMemorySizeNeeded > EncoderMemorySizeNeeded ? DecoderMemorySizeNeeded : EncoderMemorySizeNeeded;

			BufferCount = 2;

			// "PreallocatedBufferCount" is not a great name
			//	it's actually the max number of static buffers
			//	 they will only be allocated as needed

			// be wary of possible init order problem
			//  if we init Oodle before config might not exist yet?
			//  can we just check GConfig vs nullptr ?
			//  (eg. if Oodle is used to unpak ini filed?)
			if ( GConfig )
			{
				GConfig->GetInt(TEXT("OodleDataCompressionFormat"), TEXT("PreallocatedBufferCount"), BufferCount, GEngineIni);
				if (BufferCount < 0)
				{
					// negative means one per logical core
					BufferCount = FPlatformMisc::NumberOfCoresIncludingHyperthreads();
				}
			}
		}
		else
		{
			// tools, like UnrealPak or DDC commandlets

			OO_SINTa EncoderMemorySizeNeeded = OodleLZ_GetCompressScratchMemBound(OodleLZ_Compressor_Mermaid, OodleLZ_CompressionLevel_VeryFast, 256*1024, nullptr);
			// DecoderMemorySizeNeeded is ~ 450000 , EncoderMemorySizeNeeded is ~ 1200000
			OodleScratchMemorySize = DecoderMemorySizeNeeded > EncoderMemorySizeNeeded ? DecoderMemorySizeNeeded : EncoderMemorySizeNeeded;

			// allow one scratch buffer per logical core
			// they will only be allocated on demand if we actually reach that level of parallelism
			//	 so commandlets that don't use parallel compression don't waste memory
			BufferCount = FPlatformMisc::NumberOfCoresIncludingHyperthreads();
		}

		OodleScratchBufferCount = BufferCount;
		if (OodleScratchBufferCount)
		{
			LLM_SCOPE_BYTAG(OodleData);
			OodleScratches = new OodleScratchBuffer[OodleScratchBufferCount];
		}
		
		UE_LOG(OodleDataCompression, Verbose, TEXT("OodleScratchMemorySize=%d x %d"),(int)OodleScratchMemorySize,OodleScratchBufferCount);
	}
	
	~OodleScratchBuffers()
	{
		// NOTE: currently never freed

		if (OodleScratchBufferCount)
		{
			for (int i = 0; i < OodleScratchBufferCount; ++i)
			{
				if (OodleScratches[i].OodleScratchMemoryMutex.TryLock())
				{
					FMemory::Free(OodleScratches[i].OodleScratchMemory);
					OodleScratches[i].OodleScratchMemory = nullptr;
					OodleScratches[i].OodleScratchMemoryMutex.Unlock();
				}
				else
				{
					UE_LOG(OodleDataCompression, Error, TEXT("OodleDataCompression - shutting down while in use?"));
				}
			}

			delete [] OodleScratches;
		}
	}

	
	int64 OodleDecode(const void * InCompBuf, int64 InCompBufSize64, void * OutRawBuf, int64 InRawLen64) 
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Oodle.Decode);
		
		UE_LOG(OodleDataCompression, VeryVerbose, TEXT("OodleDecode: %lld -> %lld"),InCompBufSize64,InRawLen64);

		OO_SINTa InCompBufSize = IntCastChecked<OO_SINTa>(InCompBufSize64);
		OO_SINTa InRawLen = IntCastChecked<OO_SINTa>(InRawLen64);

		if ( InCompBufSize <= 0 )
		{
			UE_LOG(OodleDataCompression, Error, TEXT("OodleDecode: no compressed buffer size to decode!"));
			return OODLELZ_FAILED;
		}
		else if ( static_cast<const OO_U8 *>(InCompBuf)[0] == 0 )
		{
			UE_LOG(OodleDataCompression, Error, TEXT("OodleDecode: compressed buffer starts with zero byte; invalid or corrupt compressed stream."));
			return OODLELZ_FAILED;
		}
					
		// try to take a mutex for one of the pre-allocated decode buffers
		for (int i = 0; i < OodleScratchBufferCount; ++i)
		{
			if (OodleScratches[i].OodleScratchMemoryMutex.TryLock()) 
			{
				if (OodleScratches[i].OodleScratchMemory == nullptr)
				{
					LLM_SCOPE_BYTAG(OodleData);
					// allocate on first use
					OodleScratches[i].OodleScratchMemory = FMemory::Malloc(OodleScratchMemorySize);
					if (OodleScratches[i].OodleScratchMemory == nullptr)
					{
						UE_LOG(OodleDataCompression, Error, TEXT("OodleDecode: Failed to allocate scratch buffer %d bytes!"), OodleScratchMemorySize);
						return OODLELZ_FAILED;
					}
				}

				OO_SINTa Result = OodleLZ_Decompress(InCompBuf, InCompBufSize, OutRawBuf, InRawLen,
					OodleLZ_FuzzSafe_Yes, OodleLZ_CheckCRC_Yes, OodleLZ_Verbosity_None,
					NULL, 0, NULL, NULL,
					OodleScratches[i].OodleScratchMemory, OodleScratchMemorySize);

				OodleScratches[i].OodleScratchMemoryMutex.Unlock();

				return (int64) Result;
			}
		}
		
		// couldn't lock a shared scratch buffer
		//  allocate decoder mem on the heap to avoid waiting
		 
		//UE_LOG(OodleDataCompression, Display, TEXT("Decode with malloc : %d -> %d"),InCompBufSize,InRawLen );
		
		// find the minimum size needed for this decode, OodleScratchMemorySize may be larger
		OodleLZ_Compressor CurCompressor = OodleLZ_GetChunkCompressor(InCompBuf, InCompBufSize, NULL);
		if( CurCompressor == OodleLZ_Compressor_Invalid )
		{
			UE_LOG(OodleDataCompression, Error, TEXT("OodleDecode : no Oodle compressor found!; likely an invalid or corrupt compressed stream."));
			return OODLELZ_FAILED;
		}

		OO_SINTa DecoderMemorySize = OodleLZDecoder_MemorySizeNeeded(CurCompressor, InRawLen);
		if ( DecoderMemorySize <= 0 )
		{
			UE_LOG(OodleDataCompression, Error, TEXT("OodleDecode : MemorySizeNeeded invalid; likely an invalid or corrupt compressed stream."));
			return OODLELZ_FAILED;
		}

		LLM_SCOPE_BYTAG(OodleData);

		// allocate memory for the decoder so that Oodle doesn't allocate anything internally
		void * DecoderMemory = FMemory::Malloc(DecoderMemorySize);
		if (DecoderMemory == NULL) 
		{
			UE_LOG(OodleDataCompression, Error, TEXT("OodleDecode - Failed to allocate %d!"), (int)DecoderMemorySize);
			return OODLELZ_FAILED;
		}

		OO_SINTa Result = OodleLZ_Decompress(InCompBuf, InCompBufSize, OutRawBuf, InRawLen, 
			OodleLZ_FuzzSafe_Yes,OodleLZ_CheckCRC_Yes,OodleLZ_Verbosity_None,
			NULL, 0, NULL, NULL,
			DecoderMemory, DecoderMemorySize);

		FMemory::Free(DecoderMemory);

		return (int64) Result;
	}

	
	int64 OodleEncode( void * OutCompressedData, int64 InCompressedBufferSize,
								const void * InUncompressedData, int64 InUncompressedSize,
								ECompressor Compressor,
								ECompressionLevel Level)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Oodle.Encode);

		OodleLZ_Compressor LZCompressor = CompressorToOodleLZ_Compressor(Compressor);
		OodleLZ_CompressionLevel LZLevel = CompressionLevelToOodleLZ_CompressionLevel(Level);
		
		if ( LZCompressor == OodleLZ_Compressor_Invalid || LZLevel == OodleLZ_CompressionLevel_Invalid )
		{
			UE_LOG(OodleDataCompression,Error,TEXT("OodleEncode: Compressor or Level Invalid\n"));		
			return OODLELZ_FAILED;
		}
		if ( InCompressedBufferSize < (int64) OodleLZ_GetCompressedBufferSizeNeeded(LZCompressor,IntCastChecked<OO_SINTa>(InUncompressedSize)) )
		{
			UE_LOG(OodleDataCompression,Error,TEXT("OodleEncode: OutCompressedSize too small\n"));		
			return OODLELZ_FAILED;
		}
		
		// try to take a mutex for one of the pre-allocated decode buffers
		for (int i = 0; i < OodleScratchBufferCount; ++i)
		{
			if (OodleScratches[i].OodleScratchMemoryMutex.TryLock()) 
			{
				if (OodleScratches[i].OodleScratchMemory == nullptr)
				{
					LLM_SCOPE_BYTAG(OodleData);
					// allocate on first use
					OodleScratches[i].OodleScratchMemory = FMemory::Malloc(OodleScratchMemorySize);
					if (OodleScratches[i].OodleScratchMemory == nullptr)
					{
						UE_LOG(OodleDataCompression, Error, TEXT("OodleEncode: Failed to allocate scratch buffer %d bytes!"), OodleScratchMemorySize);
						return OODLELZ_FAILED;
					}
				}

				void * scratchMem = OodleScratches[i].OodleScratchMemory;
				OO_SINTa scratchSize = OodleScratchMemorySize;

				OO_SINTa Result = OodleLZ_Compress(LZCompressor,InUncompressedData,InUncompressedSize,OutCompressedData,LZLevel,
															NULL,NULL,NULL,
															scratchMem,scratchSize);

				OodleScratches[i].OodleScratchMemoryMutex.Unlock();

				
				UE_LOG(OodleDataCompression, VeryVerbose, TEXT("OodleEncode: %s (%s): %lld -> %lld"), \
					ECompressorToString(Compressor),ECompressionLevelToString(Level),InUncompressedSize,Result);

				return (int64) Result;
			}
		}

		// OodleLZ_Compress will alloc internally using installed CorePlugins (FMemory)

		void * scratchMem = nullptr;
		OO_SINTa scratchSize = 0;

		OO_SINTa Result = OodleLZ_Compress(LZCompressor,InUncompressedData,InUncompressedSize,OutCompressedData,LZLevel,
													NULL,NULL,NULL,
													scratchMem,scratchSize);
													
		UE_LOG(OodleDataCompression, VeryVerbose, TEXT("OodleEncode: %s (%s): %lld -> %lld"), \
			ECompressorToString(Compressor),ECompressionLevelToString(Level),InUncompressedSize,Result);

		return (int64) Result;
	}
};

static OodleScratchBuffers * GetGlobalOodleScratchBuffers()
{
	static OodleScratchBuffers GlobalOodleScratchBuffers;
	// init on first use, never freed
	return &GlobalOodleScratchBuffers;
}


static ICompressionFormat * GlobalOodleDataCompressionFormat = nullptr;

void CORE_API CompressionFormatInitOnFirstUseFromLock()
{
	// called from inside a critical section lock 
	//	from Compression.cpp / GetCompressionFormat
	if ( GlobalOodleDataCompressionFormat != nullptr )
		return;

	GlobalOodleDataCompressionFormat = CreateOodleDataCompressionFormat();
}


int64 CORE_API Compress(
							void * OutCompressedData, int64 InCompressedBufferSize,
							const void * InUncompressedData, int64 InUncompressedSize,
							ECompressor Compressor,
							ECompressionLevel Level)
{
	OodleScratchBuffers * Scratches = GetGlobalOodleScratchBuffers();

	int64 EncodedSize = Scratches->OodleEncode(OutCompressedData,InCompressedBufferSize,
							InUncompressedData,InUncompressedSize,
							Compressor,Level);
						
	return EncodedSize;
}

bool CORE_API Decompress(
						void * OutUncompressedData, int64 InUncompressedSize,
						const void * InCompressedData, int64 InCompressedSize)
{
	OodleScratchBuffers * Scratches = GetGlobalOodleScratchBuffers();

	int64 DecodeSize = Scratches->OodleDecode(InCompressedData,InCompressedSize,OutUncompressedData,InUncompressedSize);

	if ( DecodeSize == OODLELZ_FAILED )
	{
		return false;
	}

	check( DecodeSize == InUncompressedSize );
	return true;
}
					
static void* OODLE_CALLBACK OodleAlloc(OO_SINTa Size, OO_S32 Alignment)
{
	LLM_SCOPE_BYTAG(OodleData);
	return FMemory::Malloc(SIZE_T(Size), uint32(Alignment));
}

static void OODLE_CALLBACK OodleFree(void* Ptr)
{
	FMemory::Free(Ptr);
}

void CORE_API StartupPreInit(void)
{
	// called from LaunchEngineLoop at "PreInit" time
	// not all Engine services may be set up yet, be careful what you use
	
	// OodleConfig set global options for Oodle :
	OodleConfigValues OodleConfig = { };
	Oodle_GetConfigValues(&OodleConfig);
	// UE5 will always read/write Oodle v9 binary data :
	OodleConfig.m_OodleLZ_BackwardsCompatible_MajorVersion = 9;
	Oodle_SetConfigValues(&OodleConfig);
	
	// Oodle install CorePlugins here for log/alloc/etc.
	// 
	// install FMemory for Oodle allocators :
	OodleCore_Plugins_SetAllocators(OodleAlloc, OodleFree);
}

};

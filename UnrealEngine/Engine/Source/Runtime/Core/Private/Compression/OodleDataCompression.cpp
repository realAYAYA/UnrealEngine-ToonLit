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
#include "Async/ParallelFor.h"
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

CORE_API bool ECompressionLevelFromString(const TCHAR* InName, ECompressionLevel& OutLevel)
{
	for (SIZE_T i = 0; i < sizeof(CompressionLevelNameMap) / sizeof(CompressionLevelNameMap[0]); i++)
	{
		if (FCString::Stricmp(CompressionLevelNameMap[i].Name, InName) == 0)
		{
			OutLevel = CompressionLevelNameMap[i].Level;
			return true;
		}
	}
	// Since 0 is a valid compression level, we can't just atoi it or we'd always succeed.
	if ((InName[0] == '-' && FChar::IsDigit(InName[1]))
		|| FChar::IsDigit(InName[0]) )
	{
		int32 PossibleCompressionLevel = FCString::Atoi(InName);
		if (PossibleCompressionLevel >= OodleLZ_CompressionLevel_Min &&
			PossibleCompressionLevel <= OodleLZ_CompressionLevel_Max)
		{
			OutLevel = (ECompressionLevel)PossibleCompressionLevel;
			return true;
		}
	}
	return false;
}

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
			// allocate scratch space for Kraken Normal, that should get us all the usual cases.
			// (Mermaid and lower compression levels need less.)
			OO_SINTa EncoderMemorySizeNeeded = OodleLZ_GetCompressScratchMemBound(OodleLZ_Compressor_Kraken, OodleLZ_CompressionLevel_Normal, 256*1024, nullptr);
			// DecoderMemorySizeNeeded is ~ 450k , EncoderMemorySizeNeeded is ~ 2160k
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
								ECompressionLevel Level,
								bool CompressIndependentChunks,
								int64 DictionaryBackup)
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
		
		OodleLZ_CompressOptions LZOptions = * OodleLZ_CompressOptions_GetDefault(LZCompressor,LZLevel);
		if ( CompressIndependentChunks )
		{
			LZOptions.seekChunkReset = true;
			LZOptions.seekChunkLen = OODLELZ_BLOCK_LEN;

			check( DictionaryBackup == 0 ); // seek chunk and DictionaryBackup are mutually exclusive
		}

		const uint8 * DictionaryStart = (const uint8 *)InUncompressedData - DictionaryBackup;

		// try to take a mutex for one of the pre-allocated scratch buffers
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
															&LZOptions,DictionaryStart,nullptr,
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
													&LZOptions,DictionaryStart,nullptr,
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
							ECompressionLevel Level,
							bool CompressIndependentChunks,
							int64 DictionaryBackup)
{
	OodleScratchBuffers * Scratches = GetGlobalOodleScratchBuffers();

	int64 EncodedSize = Scratches->OodleEncode(OutCompressedData,InCompressedBufferSize,
							InUncompressedData,InUncompressedSize,
							Compressor,Level,CompressIndependentChunks,DictionaryBackup);
						
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

static void OODLE_CALLBACK OodlePrintf(int verboseLevel, const char* file, int line, const char* fmt, ...)
{
	TAnsiStringBuilder<256> OodleOutput;

	va_list Args;
	va_start(Args, fmt);
	OodleOutput.AppendV(fmt, Args);
	va_end(Args);
	
	UE_LOG(OodleDataCompression, Display, TEXT("Oodle: %hs"), *OodleOutput);
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
	OodleCore_Plugins_SetPrintf(OodlePrintf);
}



int64 CompressParallelSub(
							TArray64< TArray64<uint8> > & OutChunkCompressedData,
							const void * InUncompressedData, int64 UncompressedSize,
							ECompressor Compressor,	ECompressionLevel Level,
							bool CompressIndependentChunks)
{
	// beware: doing the split based on NumWorkers means that compressed output depends on the machine (if ! CompressIndependentChunks)
	//int32 NumWorkers = FTaskGraphInterface::Get().GetNumWorkerThreads();
	int32 NumWorkers = 128; // hard coded so that output is the same on all machines
	int64 ChunkLen = OodleLZ_MakeSeekChunkLen(UncompressedSize,NumWorkers);
	int64 NumChunks = (UncompressedSize + ChunkLen-1)/ChunkLen;

	OutChunkCompressedData.SetNum( NumChunks );
	
	// heuristic; don't backup more than chunklen ,or 16 MB max :
	//	this does hurt ratio in some cases (vs non-parallel encoding, or unlimited dictionary backup)
	const int64 MaxDictionaryBackup = FMath::Min( 16*1024*1024 , ChunkLen );

	// ParallelFor will run one of the chunks on the calling thread
	//	and will also fast path for NumChunks == 1

	ParallelFor( TEXT("OodleCompressParallel.PF"),(int32)NumChunks,1, [&](int32 Index)
	{
		TArray64<uint8> & OutCompressedArray = OutChunkCompressedData[Index];
		int64 ChunkStartPos = Index * ChunkLen;
		int64 ChunkUncompressedSize = FMath::Min( ChunkLen, (UncompressedSize - ChunkStartPos) );
		
		int64 Reserve = CompressedBufferSizeNeeded(ChunkUncompressedSize);
		OutCompressedArray.SetNum( Reserve );

		// if ! CompressIndependentChunks, do dictionary backup
		int64 DictionaryBackup = CompressIndependentChunks ? 0 : FMath::Min( ChunkStartPos , MaxDictionaryBackup );

		const uint8 * ChunkUncompressedData = (const uint8 *)InUncompressedData + ChunkStartPos;

		int64 Ret = Compress(&OutCompressedArray[0],Reserve,
			ChunkUncompressedData,ChunkUncompressedSize,
			Compressor,Level,CompressIndependentChunks,DictionaryBackup);

		// zero size for error
		check( OODLELZ_FAILED == 0 );
		check( Ret >= 0 );

		OutCompressedArray.SetNum(Ret, EAllowShrinking::No);
	} , EParallelForFlags::Unbalanced );

	int64 Total = 0;
	for(int64 i=0;i<NumChunks;i++)
	{
		int64 Len = OutChunkCompressedData[i].Num();
		if ( Len == 0 )
		{
			return OODLELZ_FAILED;
		}
		Total += Len;
	}

	return Total;
}

int64 CompressParallel(
							void * OutCompressedData, int64 CompressedBufferSize,
							const void * InUncompressedData, int64 UncompressedSize,
							ECompressor Compressor,	ECompressionLevel Level,
							bool CompressIndependentChunks)
{
	if ( UncompressedSize <= OODLELZ_BLOCK_LEN )
	{
		return Compress(OutCompressedData,CompressedBufferSize,
			InUncompressedData,UncompressedSize,Compressor,Level,CompressIndependentChunks);
	}

	TArray64< TArray64<uint8> > Chunks;
	int64 TotalCompLen = CompressParallelSub(Chunks,InUncompressedData,UncompressedSize,Compressor,Level,CompressIndependentChunks);
	if ( TotalCompLen <= 0 )
	{
		UE_LOG(OodleDataCompression,Error,TEXT("CompressParallelSub failed\n"));

		return TotalCompLen;
	}

	if ( TotalCompLen > CompressedBufferSize )
	{
		UE_LOG(OodleDataCompression,Error,TEXT("CompressParallel buffer size insufficient\n"));
		return OODLELZ_FAILED;
	}

	uint8 * StartPtr = (uint8 *)OutCompressedData;
	uint8 * OutPtr = StartPtr; 
	for(int64 i=0;i<Chunks.Num();i++)
	{
		memcpy(OutPtr, &Chunks[i][0] , Chunks[i].Num() );
		OutPtr += Chunks[i].Num();
	}
	check( (OutPtr - StartPtr) == TotalCompLen );

	#if 0
	// for testing : verify decode
	{
		TArray64<uint8> Decomp;
		Decomp.SetNum(UncompressedSize);
		DecompressParallel(&Decomp[0],Decomp.Num(),OutCompressedData,TotalCompLen);

		check( memcmp(InUncompressedData,&Decomp[0],UncompressedSize) == 0 );
	}
	#endif

	return TotalCompLen;
}
							
int64 CompressParallel(
							TArray64<uint8> & OutCompressedArray,
							const void * InUncompressedData, int64 UncompressedSize,
							ECompressor Compressor,	ECompressionLevel Level,
							bool CompressIndependentChunks)
{
	// appends to OutCompressedArray
	int64 StartNum = OutCompressedArray.Num();

	if ( UncompressedSize <= OODLELZ_BLOCK_LEN )
	{
		int64 Reserve = CompressedBufferSizeNeeded(UncompressedSize);
		OutCompressedArray.SetNum( StartNum + Reserve );

		int64 TotalCompLen =  Compress(&OutCompressedArray[StartNum],Reserve,
			InUncompressedData,UncompressedSize,Compressor,Level,CompressIndependentChunks);

		if ( TotalCompLen <= 0 )
		{
			return TotalCompLen;
		}
	
		check( TotalCompLen <= Reserve );
		OutCompressedArray.SetNum( StartNum + TotalCompLen , EAllowShrinking::No);
		return TotalCompLen;
	}

	TArray64< TArray64<uint8> > Chunks;
	int64 TotalCompLen = CompressParallelSub(Chunks,InUncompressedData,UncompressedSize,Compressor,Level,CompressIndependentChunks);
	if ( TotalCompLen <= 0 )
	{
		UE_LOG(OodleDataCompression,Error,TEXT("CompressParallelSub failed\n"));

		return TotalCompLen;
	}

	OutCompressedArray.SetNum( StartNum + TotalCompLen );

	uint8 * StartPtr = &OutCompressedArray[StartNum];
	uint8 * OutPtr = StartPtr; 
	for(int64 i=0;i<Chunks.Num();i++)
	{
		memcpy(OutPtr, &Chunks[i][0] , Chunks[i].Num() );
		OutPtr += Chunks[i].Num();
	}
	check( (OutPtr - StartPtr) == TotalCompLen );

	return TotalCompLen;
}

bool DecompressParallel(
						void * OutUncompressedData, int64 UncompressedSize,
						const void * InCompressedData, int64 CompressedSize
						)
{
	if ( UncompressedSize <= 2*OODLELZ_BLOCK_LEN )
	{
		// small buffer, just early out to a synchronous decode

		return Decompress(OutUncompressedData,UncompressedSize,InCompressedData,CompressedSize);
	}
	
	// this function assumes you either have no  seek resets at all
	//	or seek resets at every OODLELZ_BLOCK_LEN
	// therefore we are free to choose our decode seek chunk len here freely
	// if that was not the case
	//	(eg. say if you did seek resets at OODLELZ_BLOCK_LEN*4)
	// then we would have to change this logic to instead scan the comp data
	//	and find the seek resets, using OodleLZ_GetCompressedStepForRawStep

	int32 NumWorkers = FTaskGraphInterface::Get().GetNumWorkerThreads();
	int32 ChunkLen = OodleLZ_MakeSeekChunkLen(UncompressedSize,NumWorkers*3);
	int64 NumChunks = (UncompressedSize + ChunkLen-1)/ChunkLen;

	int64 SeekTableMemSize = OodleLZ_GetSeekTableMemorySizeNeeded((int32)NumChunks,OodleLZSeekTable_Flags_None);
	void * SeekTableMem = FMemory::Malloc(SeekTableMemSize);
	OodleLZ_SeekTable * SeekTable = (OodleLZ_SeekTable *) SeekTableMem;

	if ( ! OodleLZ_FillSeekTable(SeekTable,OodleLZSeekTable_Flags_None,ChunkLen,OutUncompressedData,UncompressedSize,InCompressedData,CompressedSize) )
	{
		UE_LOG(OodleDataCompression,Error,TEXT("OodleLZ_FillSeekTable failed\n"));
		return false;
	}

	if ( ! SeekTable->seekChunksIndependent )
	{
		// CompressIndependentChunks was not used in the encoder, cannot decode in parallel
		
		FMemory::Free(SeekTableMem);

		return Decompress(OutUncompressedData,UncompressedSize,InCompressedData,CompressedSize);
	}

	TArray64<int64> SeekCompressedPos;
	SeekCompressedPos.SetNum(NumChunks+1);
	SeekCompressedPos[0] = 0;
	for(int64 i=0;i<NumChunks;i++)
	{
		SeekCompressedPos[i+1] = SeekCompressedPos[i] + SeekTable->seekChunkCompLens[i];
	}
	
	bool AnyFailed = false;

	ParallelFor( TEXT("OodleDecompressParallel.PF"),(int32)NumChunks,1, [&](int32 Index)
	{
		int64 RawPos = Index * ChunkLen;
		int64 RawLen = FMath::Min( ChunkLen, (UncompressedSize - RawPos) );
		int64 CompPos = SeekCompressedPos[Index];
		int64 CompLen = SeekTable->seekChunkCompLens[Index];

		if ( ! Decompress( (uint8 *)OutUncompressedData + RawPos,RawLen,(const uint8 *)InCompressedData + CompPos,CompLen) )
		{
			// no need for atomics :
			AnyFailed = true;
		}
	}, EParallelForFlags::Unbalanced );

	FMemory::Free(SeekTableMem);

	return !AnyFailed;
}

}; // FOodleDataCompression

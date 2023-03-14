// Copyright Epic Games, Inc. All Rights Reserved.
#include "AudioDerivedData.h"
#include "Interfaces/IAudioFormat.h"
#include "Misc/CommandLine.h"
#include "Stats/Stats.h"
#include "Async/AsyncWork.h"
#include "Serialization/BulkData.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/EditorBulkData.h"
#include "Misc/ScopedSlowTask.h"
#include "Audio.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Sound/SoundWave.h"
#include "Async/Async.h"
#include "SoundWaveCompiler.h"
#include "Sound/SoundEffectBase.h"
#include "DerivedDataCacheInterface.h"
#include "ProfilingDebugging/CookStats.h"
#include "AudioResampler.h"
#include "AudioCompressionSettingsUtils.h"
#include "Sound/SoundSourceBus.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundWaveProcedural.h"
#include "IWaveformTransformation.h"
#include "DSP/FloatArrayMath.h"
#include "DSP/BufferVectorOperations.h"
#include "DSP/MultichannelBuffer.h"

DEFINE_LOG_CATEGORY_STATIC(LogAudioDerivedData, Log, All);

static int32 AllowAsyncCompression = 1;
FAutoConsoleVariableRef CVarAllowAsyncCompression(
	TEXT("au.compression.AsyncCompression"),
	AllowAsyncCompression,
	TEXT("1: Allow async compression of USoundWave when supported by the codec.\n")
	TEXT("0: Disable async compression."),
	ECVF_Default);

#define FORCE_RESAMPLE 0

#if ENABLE_COOK_STATS
namespace AudioCookStats
{
	static FCookStats::FDDCResourceUsageStats UsageStats;
	static FCookStats::FDDCResourceUsageStats StreamingChunkUsageStats;
	static FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
	{
		UsageStats.LogStats(AddStat, TEXT("Audio.Usage"), TEXT("Inline"));
		StreamingChunkUsageStats.LogStats(AddStat, TEXT("Audio.Usage"), TEXT("Streaming"));
	});
}
#endif

#if WITH_EDITORONLY_DATA

namespace AudioDerivedDataPrivate
{
	constexpr float FloatToPcm16Scalar = 32767.f;

	// This function for converting pcm16 to float is used so that existing 
	// assets cook to the exact same bit-wise result. While it would be nice to 
	// replace the divide operator with a multiply operator in the for loop, this 
	// should not be done as it produces different results.
	void ArrayPcm16ToFloat(TArrayView<const int16> InView, TArrayView<float> OutView)
	{
		check(InView.Num() == OutView.Num());

		const int16* InData = InView.GetData();
		float* OutData = OutView.GetData();
		const uint32 Num = InView.Num();

		for (uint32 i = 0; i < Num; i++)
		{
			OutData[i] = (float)InData[i] / FloatToPcm16Scalar;
		}
	}

	void ArrayFloatToPcm16(TArrayView<const float> InView, TArrayView<int16> OutView)
	{
		check(InView.Num() == OutView.Num());

		const float* InData = InView.GetData();
		int16* OutData = OutView.GetData();
		const uint32 Num = InView.Num();

		for (uint32 i = 0; i < Num; i++)
		{
			OutData[i] = (int16)(InData[i] * FloatToPcm16Scalar);
		}
	}
}

// Any thread implicated in the streamed audio platform data build must have a valid scope to be granted access
// to properties being modified by the build itself without triggering a FinishCache.
// Any other thread that is a consumer of the FStreamedAudioPlatformData will trigger a FinishCache when accessing
// incomplete properties which will wait until the builder thread has finished before returning property that is ready to be read.
class FStreamedAudioBuildScope
{
public:
	FStreamedAudioBuildScope(const FStreamedAudioPlatformData* PlatformData)
	{
		PreviousScope = PlatformDataBeingAsyncCompiled;
		PlatformDataBeingAsyncCompiled = PlatformData;
	}

	~FStreamedAudioBuildScope()
	{
		check(PlatformDataBeingAsyncCompiled);
		PlatformDataBeingAsyncCompiled = PreviousScope;
	}

	static bool ShouldWaitOnIncompleteProperties(const FStreamedAudioPlatformData* PlatformData)
	{
		return PlatformDataBeingAsyncCompiled != PlatformData;
	}

private:
	const FStreamedAudioPlatformData* PreviousScope = nullptr;
	// Only the thread(s) compiling this platform data will have full access to incomplete properties without causing any stalls.
	static thread_local const FStreamedAudioPlatformData* PlatformDataBeingAsyncCompiled;
};

thread_local const FStreamedAudioPlatformData* FStreamedAudioBuildScope::PlatformDataBeingAsyncCompiled = nullptr;

#endif  // #ifdef WITH_EDITORONLY_DATA

TIndirectArray<struct FStreamedAudioChunk>& FStreamedAudioPlatformData::GetChunks() const
{
#if WITH_EDITORONLY_DATA
	if (FStreamedAudioBuildScope::ShouldWaitOnIncompleteProperties(this))
	{
		// For the chunks to be available, any async task need to complete first.
		const_cast<FStreamedAudioPlatformData*>(this)->FinishCache();
	}
#endif
	return const_cast<FStreamedAudioPlatformData*>(this)->Chunks;
}

int32 FStreamedAudioPlatformData::GetNumChunks() const
{
#if WITH_EDITORONLY_DATA
	if (FStreamedAudioBuildScope::ShouldWaitOnIncompleteProperties(this))
	{
		// NumChunks is written by the caching process, any async task need to complete before we can read it.
		const_cast<FStreamedAudioPlatformData*>(this)->FinishCache();
	}
#endif

	return Chunks.Num();
}

FName FStreamedAudioPlatformData::GetAudioFormat() const
{
#if WITH_EDITORONLY_DATA
	if (FStreamedAudioBuildScope::ShouldWaitOnIncompleteProperties(this))
	{
		// AudioFormat is written by the caching process, any async task need to complete before we can read it.
		const_cast<FStreamedAudioPlatformData*>(this)->FinishCache();
	}
#endif

	return AudioFormat;
}

#if WITH_EDITORONLY_DATA

/*------------------------------------------------------------------------------
Derived data key generation.
------------------------------------------------------------------------------*/

// If you want to bump this version, generate a new guid using
// VS->Tools->Create GUID and paste it here. https://www.guidgen.com works too.
#define STREAMEDAUDIO_DERIVEDDATA_VER		TEXT("BC6E92FBBD314E3B9B9EC6778749EB5E")

/**
 * Computes the derived data key suffix for a SoundWave's Streamed Audio.
 * @param SoundWave - The SoundWave for which to compute the derived data key.
 * @param AudioFormatName - The audio format we're creating the key for
 * @param OutKeySuffix - The derived data key suffix.
 */
static void GetStreamedAudioDerivedDataKeySuffix(
	const USoundWave& SoundWave,
	FName AudioFormatName,
	const FPlatformAudioCookOverrides* CompressionOverrides,
	FString& OutKeySuffix
	)
{
	uint16 Version = 0;

	// get the version for this soundwave's platform format
	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	if (TPM)
	{
		const IAudioFormat* AudioFormat = TPM->FindAudioFormat(AudioFormatName);
		if (AudioFormat)
		{
			Version = AudioFormat->GetVersion(AudioFormatName);
		}
	}

	FString AudioFormatNameString = AudioFormatName.ToString();

	// If we have compression overrides for this target platform, append them to this string.
	if (CompressionOverrides)
	{
		FPlatformAudioCookOverrides::GetHashSuffix(CompressionOverrides, AudioFormatNameString);
	}

	// build the key
	OutKeySuffix = FString::Printf(TEXT("%s_%d_%s"),
		*AudioFormatNameString,
		Version,
		*SoundWave.CompressedDataGuid.ToString()
		);

#if PLATFORM_CPU_ARM_FAMILY
	// Separate out arm keys as x64 and arm64 clang do not generate the same data for a given
	// input. Add the arm specifically so that a) we avoid rebuilding the current DDC and
	// b) we can remove it once we get arm64 to be consistent.
	OutKeySuffix.Append(TEXT("_arm64"));
#endif
}

/**
 * Constructs a derived data key from the key suffix.
 * @param KeySuffix - The key suffix.
 * @param OutKey - The full derived data key.
 */
static void GetStreamedAudioDerivedDataKeyFromSuffix(const FString& KeySuffix, FString& OutKey)
{
	OutKey = FDerivedDataCacheInterface::BuildCacheKey(
		TEXT("STREAMEDAUDIO"),
		STREAMEDAUDIO_DERIVEDDATA_VER,
		*KeySuffix
		);
}

/**
 * Constructs the derived data key for an individual audio chunk.
 * @param KeySuffix - The key suffix.
 * @param ChunkIndex - The chunk index.
 * @param OutKey - The full derived data key for the audio chunk.
 */
static void GetStreamedAudioDerivedChunkKey(
	int32 ChunkIndex,
	const FStreamedAudioChunk& Chunk,
	const FString& KeySuffix,
	FString& OutKey
	)
{
	OutKey = FDerivedDataCacheInterface::BuildCacheKey(
		TEXT("STREAMEDAUDIO"),
		STREAMEDAUDIO_DERIVEDDATA_VER,
		*FString::Printf(TEXT("%s_CHUNK%u_%d"), *KeySuffix, ChunkIndex, Chunk.DataSize)
		);
}

/**
 * Computes the derived data key for Streamed Audio.
 * @param SoundWave - The soundwave for which to compute the derived data key.
 * @param AudioFormatName - The audio format we're creating the key for
 * @param OutKey - The derived data key.
 */
static void GetStreamedAudioDerivedDataKey(
	const USoundWave& SoundWave,
	FName AudioFormatName,
	const FPlatformAudioCookOverrides* CompressionOverrides,
	FString& OutKey
	)
{
	FString KeySuffix;
	GetStreamedAudioDerivedDataKeySuffix(SoundWave, AudioFormatName, CompressionOverrides, KeySuffix);
	GetStreamedAudioDerivedDataKeyFromSuffix(KeySuffix, OutKey);
}

static ITargetPlatform* GetRunningTargetPlatform(ITargetPlatformManagerModule* TPM)
{
	ITargetPlatform* CurrentPlatform = NULL;
	const TArray<ITargetPlatform*>& Platforms = TPM->GetActiveTargetPlatforms();

	check(Platforms.Num());

	CurrentPlatform = Platforms[0];

	for (int32 Index = 1; Index < Platforms.Num(); Index++)
	{
		if (Platforms[Index]->IsRunningPlatform())
		{
			CurrentPlatform = Platforms[Index];
			break;
		}
	}

	check(CurrentPlatform != NULL);
	return CurrentPlatform;
}

/**
 * Gets Wave format for a SoundWave on the current running platform
 * @param SoundWave - The SoundWave to get format for.
 */
static FName GetWaveFormatForRunningPlatform(USoundWave& SoundWave)
{
	// Compress to whatever format the active target platform wants
	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	if (TPM)
	{
		ITargetPlatform* CurrentPlatform = GetRunningTargetPlatform(TPM);


		return CurrentPlatform->GetWaveFormat(&SoundWave);
	}

	return NAME_None;
}

static const FPlatformAudioCookOverrides* GetCookOverridesForRunningPlatform()
{
	return FPlatformCompressionUtilities::GetCookOverrides(nullptr);
}

/**
 * Stores derived data in the DDC.
 * After this returns, all bulk data from streaming chunks will be sent separately to the DDC and the BulkData for those chunks removed.
 * @param DerivedData - The data to store in the DDC.
 * @param DerivedDataKeySuffix - The key suffix at which to store derived data.
 * @return number of bytes put to the DDC (total, including all chunks)
 */
static uint32 PutDerivedDataInCache(
	FStreamedAudioPlatformData* DerivedData,
	const FString& DerivedDataKeySuffix,
	const FStringView& SoundWaveName
	)
{
	TArray<uint8> RawDerivedData;
	FString DerivedDataKey;
	uint32 TotalBytesPut = 0;

	// Build the key with which to cache derived data.
	GetStreamedAudioDerivedDataKeyFromSuffix(DerivedDataKeySuffix, DerivedDataKey);

	FString LogString;
	if (UE_LOG_ACTIVE(LogAudio,Verbose))
	{
		LogString = FString::Printf(
			TEXT("Storing Streamed Audio in DDC:\n  Key: %s\n  Format: %s\n"),
			*DerivedDataKey,
			*DerivedData->AudioFormat.ToString()
			);
	}

	// Write out individual chunks to the derived data cache.
	const int32 ChunkCount = DerivedData->Chunks.Num();
	for (int32 ChunkIndex = 0; ChunkIndex < ChunkCount; ++ChunkIndex)
	{
		FString ChunkDerivedDataKey;
		FStreamedAudioChunk& Chunk = DerivedData->Chunks[ChunkIndex];
		GetStreamedAudioDerivedChunkKey(ChunkIndex, Chunk, DerivedDataKeySuffix, ChunkDerivedDataKey);

		if (UE_LOG_ACTIVE(LogAudio,Verbose))
		{
			LogString += FString::Printf(TEXT("  Chunk%d %d bytes %s\n"),
				ChunkIndex,
				Chunk.BulkData.GetBulkDataSize(),
				*ChunkDerivedDataKey
				);
		}

		TotalBytesPut += Chunk.StoreInDerivedDataCache(ChunkDerivedDataKey, SoundWaveName);
	}

	// Store derived data.
	// At this point we've stored all the non-inline data in the DDC, so this will only serialize and store the metadata and any inline chunks
	FMemoryWriter Ar(RawDerivedData, /*bIsPersistent=*/ true);
	DerivedData->Serialize(Ar, NULL);
	GetDerivedDataCacheRef().Put(*DerivedDataKey, RawDerivedData, SoundWaveName);
	TotalBytesPut += RawDerivedData.Num();
	UE_LOG(LogAudio, Verbose, TEXT("%s  Derived Data: %d bytes"), *LogString, RawDerivedData.Num());
	return TotalBytesPut;
}

namespace EStreamedAudioCacheFlags
{
	enum Type
	{
		None			= 0x0,
		Async			= 0x1,
		ForceRebuild	= 0x2,
		InlineChunks	= 0x4,
		AllowAsyncBuild	= 0x8,
		ForDDCBuild		= 0x10,
	};
};

/**
 * Worker used to cache streamed audio derived data.
 */
class FStreamedAudioCacheDerivedDataWorker : public FNonAbandonableTask
{
	/** Where to store derived data. */
	FStreamedAudioPlatformData* DerivedData = nullptr;
	/** Path of the SoundWave being cached for logging purpose. */
	FString SoundWavePath;
	/** Full name of the SoundWave being cached for logging purpose. */
	FString SoundWaveFullName;
	/** Audio Format Name */
	FName AudioFormatName;
	/** Derived data key suffix. */
	FString KeySuffix;
	/** Streamed Audio cache flags. */
	uint32 CacheFlags = 0;
	/** Have many bytes were loaded from DDC or built (for telemetry) */
	uint32 BytesCached = 0;
	/** Sample rate override specified for this sound wave. */
	const FPlatformAudioCookOverrides* CompressionOverrides = nullptr;
	/** true if caching has succeeded. */
	bool bSucceeded = false;
	/** true if the derived data was pulled from DDC */
	bool bLoadedFromDDC = false;
	/** Already tried to built once */
	bool bHasBeenBuilt = false;
	/** Handle for retrieving compressed audio for chunking */
	uint32 CompressedAudioHandle = 0;
	/** If the wave file is procedural */
	bool bIsProcedural = false;
	/** If the wave file is streaming */
	bool bIsStreaming = false;
	/** Initial chunk size */
	int32 InitialChunkSize = 0;

	bool GetCompressedData(TArray<uint8>& OutData)
	{
		if (CompressedAudioHandle)
		{
			GetDerivedDataCacheRef().WaitAsynchronousCompletion(CompressedAudioHandle);
			bool bResult = GetDerivedDataCacheRef().GetAsynchronousResults(CompressedAudioHandle, OutData);
			CompressedAudioHandle = 0;
			return bResult;
		}

		return false;
	}

	/** Build the streamed audio. This function is safe to call from any thread. */
	void BuildStreamedAudio()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BuildStreamedAudio);

		if (bIsProcedural)
		{
			return;
		}

		DerivedData->Chunks.Empty();

		ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
		const IAudioFormat* AudioFormat = NULL;
		if (TPM)
		{
			AudioFormat = TPM->FindAudioFormat(AudioFormatName);
		}

		if (AudioFormat)
		{
			DerivedData->AudioFormat = AudioFormatName;

			TArray<uint8> CompressedBuffer;
			if (GetCompressedData(CompressedBuffer))
			{
				TArray<TArray<uint8>> ChunkBuffers;

				// Set the ideal chunk size to be 256k to optimize for data reads on console.
				int32 MaxChunkSizeForCurrentWave = 256 * 1024;
				
				// By default, the first chunk's max size is the same as the other chunks.
				int32 FirstChunkSize = MaxChunkSizeForCurrentWave;

				const int32 MinimumChunkSize = AudioFormat->GetMinimumSizeForInitialChunk(AudioFormatName, CompressedBuffer);
				const bool bForceLegacyStreamChunking = bIsStreaming && CompressionOverrides && CompressionOverrides->StreamCachingSettings.bForceLegacyStreamChunking;

				// If the initial chunk  for this sound wave was overridden, use that:
				if (InitialChunkSize > 0)
				{
					FirstChunkSize = FMath::Max(MinimumChunkSize, InitialChunkSize);
				}
				else
				{
					// Ensure that the minimum chunk size is nonzero if our compressed buffer is not empty.
					checkf(CompressedBuffer.Num() == 0 || MinimumChunkSize != 0, TEXT("To use Load On Demand, please override GetMinimumSizeForInitialChunk"));

					//
					if (bForceLegacyStreamChunking)
					{
						int32 LegacyZerothChunkSize = CompressionOverrides->StreamCachingSettings.ZerothChunkSizeForLegacyStreamChunkingKB * 1024;
						if (LegacyZerothChunkSize == 0)
						{
							LegacyZerothChunkSize = MaxChunkSizeForCurrentWave;
						}

						FirstChunkSize = LegacyZerothChunkSize;
					}
					else
					{
						// Otherwise if we're using Audio Stream Caching, the first chunk should be as small as possible:
						FirstChunkSize = MinimumChunkSize;
					}
				}

				if (!bForceLegacyStreamChunking)
				{
					// Use the chunk size for this duration:
					MaxChunkSizeForCurrentWave = FPlatformCompressionUtilities::GetMaxChunkSizeForCookOverrides(CompressionOverrides);

					// observe the override chunk size now that we have set the FirstChunkSize
					const int32 MaxChunkSizeOverrideBytes = CompressionOverrides->StreamCachingSettings.MaxChunkSizeOverrideKB * 1024;
					if (MaxChunkSizeOverrideBytes > 0)
					{
						MaxChunkSizeForCurrentWave = FMath::Min(MaxChunkSizeOverrideBytes, MaxChunkSizeForCurrentWave);
					}

					UE_LOG(LogAudio, Display, TEXT("Chunk size for %s: %d"), *SoundWaveFullName, MaxChunkSizeForCurrentWave);
				}
				
				check(FirstChunkSize != 0 && MaxChunkSizeForCurrentWave != 0);

				if (AudioFormat->SplitDataForStreaming(CompressedBuffer, ChunkBuffers, FirstChunkSize, MaxChunkSizeForCurrentWave))
				{
					if (ChunkBuffers.Num() > 32)
					{
						UE_LOG(LogAudio, Display, TEXT("Sound Wave %s is very large, requiring %d chunks."), *SoundWaveFullName, ChunkBuffers.Num());
					}

					if (ChunkBuffers.Num() > 0)
					{
						// The zeroth chunk should not be zero-padded.
						const int32 AudioDataSize = ChunkBuffers[0].Num();

						//FStreamedAudioChunk* NewChunk = new(DerivedData->Chunks) FStreamedAudioChunk();
						int32 ChunkIndex = DerivedData->Chunks.Add(new FStreamedAudioChunk());
						FStreamedAudioChunk* NewChunk = &(DerivedData->Chunks[ChunkIndex]);
						// Store both the audio data size and the data size so decoders will know what portion of the bulk data is real audio
						NewChunk->AudioDataSize = AudioDataSize;
						NewChunk->DataSize = AudioDataSize;

						if (NewChunk->BulkData.IsLocked())
						{
							UE_LOG(LogAudioDerivedData, Warning, TEXT("While building split chunk for streaming: Raw PCM data already being written to. Chunk Index: 0 SoundWave: %s "), *SoundWaveFullName);
						}

						NewChunk->BulkData.Lock(LOCK_READ_WRITE);
						void* NewChunkData = NewChunk->BulkData.Realloc(NewChunk->AudioDataSize);
						FMemory::Memcpy(NewChunkData, ChunkBuffers[0].GetData(), AudioDataSize);
						NewChunk->BulkData.Unlock();
					}

					// Zero-pad the rest of the chunks here:
					for (int32 ChunkIndex = 1; ChunkIndex < ChunkBuffers.Num(); ++ChunkIndex)
					{
						// Zero pad the reallocation if the chunk isn't precisely the max chunk size to keep the reads aligned to MaxChunkSize
						const int32 AudioDataSize = ChunkBuffers[ChunkIndex].Num();
						check(AudioDataSize != 0 && AudioDataSize <= MaxChunkSizeForCurrentWave);

						int32 ZeroPadBytes = 0;

						if (bForceLegacyStreamChunking)
						{
							// padding when stream caching is enabled will significantly bloat the amount of space soundwaves take up on disk.
							ZeroPadBytes = FMath::Max(MaxChunkSizeForCurrentWave - AudioDataSize, 0);
						}

						FStreamedAudioChunk* NewChunk = new FStreamedAudioChunk();
						DerivedData->Chunks.Add(NewChunk);

						// Store both the audio data size and the data size so decoders will know what portion of the bulk data is real audio
						NewChunk->AudioDataSize = AudioDataSize;
						NewChunk->DataSize = AudioDataSize + ZeroPadBytes;

						if (NewChunk->BulkData.IsLocked())
						{
							UE_LOG(LogAudioDerivedData, Warning, TEXT("While building split chunk for streaming: Raw PCM data already being written to. Chunk Index: %d SoundWave: %s "), ChunkIndex, *SoundWaveFullName);
						}

						NewChunk->BulkData.Lock(LOCK_READ_WRITE);

						void* NewChunkData = NewChunk->BulkData.Realloc(NewChunk->DataSize);
						FMemory::Memcpy(NewChunkData, ChunkBuffers[ChunkIndex].GetData(), AudioDataSize);

						// if we are padding,
						if (ZeroPadBytes > 0)
						{
							// zero out the end of ChunkData (after the audio data ends).
							FMemory::Memzero((uint8*)NewChunkData + AudioDataSize, ZeroPadBytes);
						}

						NewChunk->BulkData.Unlock();
					}
				}
				else
				{
					// Could not split so copy compressed data into a single chunk
					FStreamedAudioChunk* NewChunk = new FStreamedAudioChunk();
					DerivedData->Chunks.Add(NewChunk);
					NewChunk->DataSize = CompressedBuffer.Num();
					NewChunk->AudioDataSize = NewChunk->DataSize;

					if (NewChunk->BulkData.IsLocked())
					{
						UE_LOG(LogAudioDerivedData, Warning, TEXT("While building single-chunk streaming SoundWave: Raw PCM data already being written to. SoundWave: %s "), *SoundWaveFullName);
					}

					NewChunk->BulkData.Lock(LOCK_READ_WRITE);
					void* NewChunkData = NewChunk->BulkData.Realloc(CompressedBuffer.Num());
					FMemory::Memcpy(NewChunkData, CompressedBuffer.GetData(), CompressedBuffer.Num());
					NewChunk->BulkData.Unlock();
				}

				// Store it in the cache.
				// @todo: This will remove the streaming bulk data, which we immediately reload below!
				// Should ideally avoid this redundant work, but it only happens when we actually have
				// to build the compressed audio, which should only ever be once.
				this->BytesCached = PutDerivedDataInCache(DerivedData, KeySuffix, SoundWavePath);

				check(this->BytesCached != 0);
			}
			else
			{
				UE_LOG(LogAudio, Display, TEXT("Failed to retrieve compressed data for format %s and soundwave %s."),
					   *AudioFormatName.GetPlainNameString(),
					   *SoundWavePath
					);
			}
		}

		if (DerivedData->Chunks.Num())
		{
			bool bInlineChunks = (CacheFlags & EStreamedAudioCacheFlags::InlineChunks) != 0;
			bSucceeded = !bInlineChunks || DerivedData->TryInlineChunkData();
		}
		else
		{
			UE_LOG(LogAudio, Display, TEXT("Failed to build %s derived data for %s"),
				*AudioFormatName.GetPlainNameString(),
				*SoundWavePath
				);
		}
	}

public:
	/** Initialization constructor. */
	FStreamedAudioCacheDerivedDataWorker(
		FStreamedAudioPlatformData* InDerivedData,
		USoundWave* InSoundWave,
		const FPlatformAudioCookOverrides* InCompressionOverrides,
		FName InAudioFormatName,
		uint32 InCacheFlags
		)
		: DerivedData(InDerivedData)
		, SoundWavePath(InSoundWave->GetPathName())
		, SoundWaveFullName(InSoundWave->GetFullName())
		, AudioFormatName(InAudioFormatName)
		, CacheFlags(InCacheFlags)
		, CompressionOverrides(InCompressionOverrides)
		, bIsProcedural(InSoundWave->IsA<USoundWaveProcedural>())
		, bIsStreaming(InSoundWave->bStreaming)
		, InitialChunkSize(InSoundWave->InitialChunkSize)
	{
		// Gather all USoundWave object inputs to avoid race-conditions that could result when touching the UObject from another thread.
		GetStreamedAudioDerivedDataKeySuffix(*InSoundWave, AudioFormatName, CompressionOverrides, KeySuffix);
		FName PlatformSpecificFormat = InSoundWave->GetPlatformSpecificFormat(InAudioFormatName, CompressionOverrides);

 		// Fetch compressed data directly from the DDC to ensure thread-safety. Will be async if the compressor is thread-safe.
		FDerivedAudioDataCompressor* DeriveAudioData = new FDerivedAudioDataCompressor(InSoundWave, InAudioFormatName, PlatformSpecificFormat, CompressionOverrides);
		CompressedAudioHandle = GetDerivedDataCacheRef().GetAsynchronous(DeriveAudioData);
	}

	/** Does the work to cache derived data. Safe to call from any thread. */
	void DoWork()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FStreamedAudioCacheDerivedDataWorker::DoWork);

		// This scope will let us access any incomplete properties since we are the producer of those properties and
		// we can't wait on ourself without causing a deadlock.
		FStreamedAudioBuildScope StreamedAudioBuildScope(DerivedData);

		TArray<uint8> RawDerivedData;
		bool bForceRebuild = (CacheFlags & EStreamedAudioCacheFlags::ForceRebuild) != 0;
		bool bInlineChunks = (CacheFlags & EStreamedAudioCacheFlags::InlineChunks) != 0;
		bool bForDDC = (CacheFlags & EStreamedAudioCacheFlags::ForDDCBuild) != 0;
		bool bAllowAsyncBuild = (CacheFlags & EStreamedAudioCacheFlags::AllowAsyncBuild) != 0;

		if (!bForceRebuild && GetDerivedDataCacheRef().GetSynchronous(*DerivedData->DerivedDataKey, RawDerivedData, SoundWavePath))
		{
			BytesCached = RawDerivedData.Num();
			FMemoryReader Ar(RawDerivedData, /*bIsPersistent=*/ true);
			DerivedData->Serialize(Ar, NULL);
			bSucceeded = true;
			// Load any streaming (not inline) chunks that are necessary for our platform.
			if (bForDDC)
			{
				for (int32 Index = 0; Index < DerivedData->Chunks.Num(); ++Index)
				{
					if (!DerivedData->GetChunkFromDDC(Index, NULL))
					{
						bSucceeded = false;
						break;
					}
				}
			}
			else if (bInlineChunks)
			{
				bSucceeded = DerivedData->TryInlineChunkData();
			}
			else
			{
				bSucceeded = DerivedData->AreDerivedChunksAvailable(SoundWavePath);
			}
			bLoadedFromDDC = true;
		}

		// Let us try to build asynchronously if allowed to after a DDC fetch failure instead of relying solely on the
		// synchronous finalize to perform all the work.
		if (!bSucceeded && bAllowAsyncBuild)
		{
			// Let Finalize know that we've already tried to build in case we didn't succeed, don't try a second time for nothing.
			bHasBeenBuilt = true;
			BuildStreamedAudio();
		}
	}

	/** Finalize work. Must be called ONLY by the thread that started this task! */
	bool Finalize()
	{
		// if we couldn't get from the DDC or didn't build synchronously, then we have to build now.
		// This is a super edge case that should rarely happen.
		if (!bSucceeded && !bHasBeenBuilt)
		{
			BuildStreamedAudio();
		}

		// Cleanup the async DDC query if needed
		TArray<uint8> Dummy;
		GetCompressedData(Dummy);

		return bLoadedFromDDC;
	}

	/** Expose bytes cached for telemetry. */
	uint32 GetBytesCached() const
	{
		return BytesCached;
	}

	/** Expose how the resource was returned for telemetry. */
	bool WasLoadedFromDDC() const
	{
		return bLoadedFromDDC;
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FStreamedAudioCacheDerivedDataWorker, STATGROUP_ThreadPoolAsyncTasks);
	}
};

struct FStreamedAudioAsyncCacheDerivedDataTask : public FAsyncTask<FStreamedAudioCacheDerivedDataWorker>
{
	FStreamedAudioAsyncCacheDerivedDataTask(
		FStreamedAudioPlatformData* InDerivedData,
		USoundWave* InSoundWave,
		const FPlatformAudioCookOverrides* CompressionSettings,
		FName InAudioFormatName,
		uint32 InCacheFlags
		)
		: FAsyncTask<FStreamedAudioCacheDerivedDataWorker>(
			InDerivedData,
			InSoundWave,
			CompressionSettings,
			InAudioFormatName,
			InCacheFlags
			)
	{
	}
};

void FStreamedAudioPlatformData::Cache(USoundWave& InSoundWave, const FPlatformAudioCookOverrides* CompressionOverrides, FName AudioFormatName,  uint32 InFlags)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FStreamedAudioPlatformData::Cache);

	// Flush any existing async task and ignore results.
	FinishCache();

	uint32 Flags = InFlags;

	static bool bForDDC = FString(FCommandLine::Get()).Contains(TEXT("Run=DerivedDataCache"));
	if (bForDDC)
	{
		Flags |= EStreamedAudioCacheFlags::ForDDCBuild;
	}

	bool bForceRebuild = (Flags & EStreamedAudioCacheFlags::ForceRebuild) != 0;
	bool bAsync = (Flags & EStreamedAudioCacheFlags::Async) != 0;
	GetStreamedAudioDerivedDataKey(InSoundWave, AudioFormatName, CompressionOverrides, DerivedDataKey);

	if (bAsync && !bForceRebuild && FSoundWaveCompilingManager::Get().IsAsyncCompilationAllowed(&InSoundWave))
	{
		FQueuedThreadPool* SoundWaveThreadPool = FSoundWaveCompilingManager::Get().GetThreadPool();
		EQueuedWorkPriority BasePriority = FSoundWaveCompilingManager::Get().GetBasePriority(&InSoundWave);

		{
			FWriteScopeLock AsyncTaskScope(AsyncTaskLock.Get());
			check(AsyncTask == nullptr);
			AsyncTask = new FStreamedAudioAsyncCacheDerivedDataTask(this, &InSoundWave, CompressionOverrides, AudioFormatName, Flags);
			AsyncTask->StartBackgroundTask(SoundWaveThreadPool, BasePriority, EQueuedWorkFlags::DoNotRunInsideBusyWait);
		}

		if (IsInAudioThread())
		{
			TWeakObjectPtr<USoundWave> WeakSoundWavePtr(&InSoundWave);
			FAudioThread::RunCommandOnGameThread([WeakSoundWavePtr]()
			{
				if (USoundWave* SoundWave = WeakSoundWavePtr.Get())
				{
					FSoundWaveCompilingManager::Get().AddSoundWaves({SoundWave});
				}
			});
		}
		else
		{
			FSoundWaveCompilingManager::Get().AddSoundWaves({&InSoundWave});
		}
	}
	else
	{
		FStreamedAudioCacheDerivedDataWorker Worker(this, &InSoundWave, CompressionOverrides, AudioFormatName, Flags);
		{
			COOK_STAT(auto Timer = AudioCookStats::UsageStats.TimeSyncWork());
			Worker.DoWork();
			Worker.Finalize();
			COOK_STAT(Timer.AddHitOrMiss(Worker.WasLoadedFromDDC() ? FCookStats::CallStats::EHitOrMiss::Hit : FCookStats::CallStats::EHitOrMiss::Miss, Worker.GetBytesCached()));
		}
	}
}

bool FStreamedAudioPlatformData::IsCompiling() const
{
	FReadScopeLock AsyncTaskScope(AsyncTaskLock.Get());
	return AsyncTask != nullptr;
}

bool FStreamedAudioPlatformData::IsAsyncWorkComplete() const
{
	FReadScopeLock AsyncTaskScope(AsyncTaskLock.Get());
	return AsyncTask == nullptr || AsyncTask->IsWorkDone();
}

bool FStreamedAudioPlatformData::IsFinishedCache() const
{
	FReadScopeLock AsyncTaskScope(AsyncTaskLock.Get());
	return AsyncTask == nullptr ? true : false;
}

void FStreamedAudioPlatformData::FinishCache()
{
	FWriteScopeLock AsyncTaskScope(AsyncTaskLock.Get());
	if (AsyncTask)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FStreamedAudioPlatformData::FinishCache);
		{
			COOK_STAT(auto Timer = AudioCookStats::UsageStats.TimeAsyncWait());
			AsyncTask->EnsureCompletion();
			FStreamedAudioCacheDerivedDataWorker& Worker = AsyncTask->GetTask();
			Worker.Finalize();
			COOK_STAT(Timer.AddHitOrMiss(Worker.WasLoadedFromDDC() ? FCookStats::CallStats::EHitOrMiss::Hit : FCookStats::CallStats::EHitOrMiss::Miss, Worker.GetBytesCached()));
		}
		delete AsyncTask;
		AsyncTask = nullptr;
	}
}

bool FStreamedAudioPlatformData::RescheduleAsyncTask(FQueuedThreadPool* InThreadPool, EQueuedWorkPriority InPriority)
{
	FReadScopeLock AsyncTaskScope(AsyncTaskLock.Get());
	return AsyncTask ? AsyncTask->Reschedule(InThreadPool, InPriority) : false;
}

bool FStreamedAudioPlatformData::WaitAsyncTaskWithTimeout(float InTimeoutInSeconds)
{
	FReadScopeLock AsyncTaskScope(AsyncTaskLock.Get());
	return AsyncTask ? AsyncTask->WaitCompletionWithTimeout(InTimeoutInSeconds) : true;
}

/**
 * Executes async DDC gets for chunks stored in the derived data cache.
 * @param Chunks - Chunks to retrieve.
 * @param FirstChunkToLoad - Index of the first chunk to retrieve.
 * @param OutHandles - Handles to the asynchronous DDC gets.
 */
static void BeginLoadDerivedChunks(TIndirectArray<FStreamedAudioChunk>& Chunks, int32 FirstChunkToLoad, TArray<uint32>& OutHandles)
{
	FDerivedDataCacheInterface& DDC = GetDerivedDataCacheRef();
	OutHandles.AddZeroed(Chunks.Num());
	for (int32 ChunkIndex = FirstChunkToLoad; ChunkIndex < Chunks.Num(); ++ChunkIndex)
	{
		const FStreamedAudioChunk& Chunk = Chunks[ChunkIndex];
		if (!Chunk.DerivedDataKey.IsEmpty())
		{
			OutHandles[ChunkIndex] = DDC.GetAsynchronous(*Chunk.DerivedDataKey, TEXTVIEW("Unknown SoundWave"));
		}
	}
}

bool FStreamedAudioPlatformData::TryInlineChunkData()
{
	TArray<uint32> AsyncHandles;
	TArray<uint8> TempData;
	FDerivedDataCacheInterface& DDC = GetDerivedDataCacheRef();

	BeginLoadDerivedChunks(Chunks, 0, AsyncHandles);
	for (int32 ChunkIndex = 0; ChunkIndex < Chunks.Num(); ++ChunkIndex)
	{
		FStreamedAudioChunk& Chunk = Chunks[ChunkIndex];
		if (Chunk.DerivedDataKey.IsEmpty() == false)
		{
			uint32 AsyncHandle = AsyncHandles[ChunkIndex];
			bool bLoadedFromDDC = false;
			COOK_STAT(auto Timer = AudioCookStats::StreamingChunkUsageStats.TimeAsyncWait());
			DDC.WaitAsynchronousCompletion(AsyncHandle);
			bLoadedFromDDC = DDC.GetAsynchronousResults(AsyncHandle, TempData);
			COOK_STAT(Timer.AddHitOrMiss(bLoadedFromDDC ? FCookStats::CallStats::EHitOrMiss::Hit : FCookStats::CallStats::EHitOrMiss::Miss, TempData.Num()));
			if (bLoadedFromDDC)
			{
				int32 ChunkSize = 0;
				int32 AudioDataSize = 0;
				FMemoryReader Ar(TempData, /*bIsPersistent=*/ true);
				Ar << ChunkSize;
				Ar << AudioDataSize; // Unused for the purposes of this function.

				if (Chunk.BulkData.IsLocked())
				{
					UE_LOG(LogAudioDerivedData, Warning, TEXT("In TryInlineChunkData: Raw PCM data already being written to. Chunk: %d DDC Key: %s "), ChunkIndex, *DerivedDataKey);
				}

				Chunk.BulkData.Lock(LOCK_READ_WRITE);
				void* ChunkData = Chunk.BulkData.Realloc(ChunkSize);
				Ar.Serialize(ChunkData, ChunkSize);
				Chunk.BulkData.Unlock();
				Chunk.DerivedDataKey.Empty();
			}
			else
			{
				return false;
			}
			TempData.Reset();
		}
	}
	return true;
}

#endif //WITH_EDITORONLY_DATA

FStreamedAudioPlatformData::FStreamedAudioPlatformData()
#if WITH_EDITORONLY_DATA
	: AsyncTask(nullptr)
#endif // #if WITH_EDITORONLY_DATA
{
}

FStreamedAudioPlatformData::~FStreamedAudioPlatformData()
{
#if WITH_EDITORONLY_DATA
	FWriteScopeLock AsyncTaskScope(AsyncTaskLock.Get());
	if (AsyncTask)
	{
		AsyncTask->EnsureCompletion();
		delete AsyncTask;
		AsyncTask = nullptr;
	}
#endif
}

int32 FStreamedAudioPlatformData::DeserializeChunkFromDDC(TArray<uint8> TempData, FStreamedAudioChunk &Chunk, int32 ChunkIndex, uint8** &OutChunkData)
{
	int32 ChunkSize = 0;
	FMemoryReader Ar(TempData, /*bIsPersistent=*/ true);
	int32 AudioDataSize = 0;
	Ar << ChunkSize;
	Ar << AudioDataSize;

#if WITH_EDITORONLY_DATA
	ensureAlwaysMsgf((ChunkSize == Chunk.DataSize && AudioDataSize == Chunk.AudioDataSize),
		TEXT("Chunk %d of %s SoundWave has invalid data in the DDC. Got %d bytes, expected %d. Audio Data was %d bytes but we expected %d bytes. Key=%s"),
		ChunkIndex,
		*AudioFormat.ToString(),
		ChunkSize,
		Chunk.DataSize,
		AudioDataSize,
		Chunk.AudioDataSize,
		*Chunk.DerivedDataKey
	);
#endif

	if (OutChunkData)
	{
		if (*OutChunkData == NULL)
		{
			*OutChunkData = static_cast<uint8*>(FMemory::Malloc(ChunkSize));
		}
		Ar.Serialize(*OutChunkData, ChunkSize);
	}

	if (FPlatformCompressionUtilities::IsCurrentPlatformUsingStreamCaching())
	{
		return AudioDataSize;
	}
	else
	{
		return ChunkSize;
	}
}

int32 FStreamedAudioPlatformData::GetChunkFromDDC(int32 ChunkIndex, uint8** OutChunkData, bool bMakeSureChunkIsLoaded /* = false */)
{
	if (GetNumChunks() == 0)
	{
		UE_LOG(LogAudioDerivedData, Display, TEXT("No streamed audio chunks found!"));
		return 0;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FStreamedAudioPlatformData::GetChunkFromDDC);

	// if bMakeSureChunkIsLoaded is true, we don't actually know the size of the chunk's bulk data,
	// so it will need to be allocated in GetCopy.
	check(!bMakeSureChunkIsLoaded || (OutChunkData && (*OutChunkData == nullptr)));

	bool bCachedChunk = false;
	check(ChunkIndex < GetNumChunks());
	FStreamedAudioChunk& Chunk = GetChunks()[ChunkIndex];

	int32 ChunkDataSize = 0;

#if WITH_EDITORONLY_DATA
	TArray<uint8> TempData;

	// Begin async DDC retrieval
	FDerivedDataCacheInterface& DDC = GetDerivedDataCacheRef();
	uint32 AsyncHandle = 0;
	if (!Chunk.DerivedDataKey.IsEmpty())
	{
		if (bMakeSureChunkIsLoaded)
		{
			if (DDC.GetSynchronous(*Chunk.DerivedDataKey, TempData, TEXTVIEW("Unknown SoundWave")))
			{
				ChunkDataSize = DeserializeChunkFromDDC(TempData, Chunk, ChunkIndex, OutChunkData);
			}
		}
		else
		{
			AsyncHandle = DDC.GetAsynchronous(*Chunk.DerivedDataKey, TEXTVIEW("Unknown SoundWave"));
		}
	}
	else if (Chunk.bLoadedFromCookedPackage)
	{
		if (Chunk.BulkData.IsBulkDataLoaded() || bMakeSureChunkIsLoaded)
		{
			if (OutChunkData)
			{
				ChunkDataSize = Chunk.BulkData.GetBulkDataSize();
				Chunk.GetCopy((void**)OutChunkData);
			}
		}
	}

	// Wait for async DDC to complete
	// Necessary otherwise we will return a ChunkDataSize of 0 which is considered a failure by most callers and will trigger rebuild. 
	if (Chunk.DerivedDataKey.IsEmpty() == false && AsyncHandle)
	{
		DDC.WaitAsynchronousCompletion(AsyncHandle);
		if (DDC.GetAsynchronousResults(AsyncHandle, TempData))
		{
			ChunkDataSize = DeserializeChunkFromDDC(TempData, Chunk, ChunkIndex, OutChunkData);
		}
	}
#else // #if WITH_EDITORONLY_DATA
	// Load chunk from bulk data if available. If the chunk is not loaded, GetCopy will load it synchronously.
	if (Chunk.BulkData.IsBulkDataLoaded() || bMakeSureChunkIsLoaded)
	{
		if (OutChunkData)
		{
			ChunkDataSize = Chunk.BulkData.GetBulkDataSize();
			Chunk.GetCopy((void**)OutChunkData);
		}
	}
#endif // #if WITH_EDITORONLY_DATA
	return ChunkDataSize;
}

#if WITH_EDITORONLY_DATA
bool FStreamedAudioPlatformData::AreDerivedChunksAvailable(FStringView InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FStreamedAudioPlatformData::AreDerivedChunksAvailable);

	TArray<FString> ChunkKeys;
	for (const FStreamedAudioChunk& Chunk : Chunks)
	{
		if (!Chunk.DerivedDataKey.IsEmpty())
		{
			ChunkKeys.Add(Chunk.DerivedDataKey);
		}
	}
	
	const bool bAllCachedDataProbablyExists = GetDerivedDataCacheRef().AllCachedDataProbablyExists(ChunkKeys);
	
	if (bAllCachedDataProbablyExists)
	{
		// If this is called from the game thread, try to prefetch chunks locally on background thread
		// to avoid doing high latency remote calls every time we reload this data.
		if (IsInGameThread())
		{
			if (GIOThreadPool)
			{
				FString Context{ InContext };
				AsyncPool(
					*GIOThreadPool,
					[ChunkKeys, Context]()
					{
						TRACE_CPUPROFILER_EVENT_SCOPE(PrefetchAudioChunks);
						GetDerivedDataCacheRef().TryToPrefetch(ChunkKeys, Context);
					},
					nullptr,
					EQueuedWorkPriority::Low
				);
			}
		}
		else
		{
			// Not on the game-thread, prefetch synchronously
			TRACE_CPUPROFILER_EVENT_SCOPE(PrefetchAudioChunks);
			GetDerivedDataCacheRef().TryToPrefetch(ChunkKeys, InContext);
		}
	}

	return bAllCachedDataProbablyExists;
}

bool FStreamedAudioPlatformData::AreDerivedChunksAvailable() const
{
	return AreDerivedChunksAvailable(TEXTVIEW("DerivedAudioChunks"));
}

#endif // #if WITH_EDITORONLY_DATA

void FStreamedAudioPlatformData::Serialize(FArchive& Ar, USoundWave* Owner)
{
#if WITH_EDITORONLY_DATA
	if (Owner)
	{
		Owner->RawDataCriticalSection.Lock();
	}
#endif

	int32 NumChunks = 0;
	if (Ar.IsSaving())
	{
		NumChunks = Chunks.Num();
	}

	Ar << NumChunks;
	Ar << AudioFormat;

	if (Ar.IsLoading())
	{
		check(NumChunks >= 0);
		Chunks.Empty(NumChunks);
		for (int32 ChunkIndex = 0; ChunkIndex < NumChunks; ++ChunkIndex)
		{
			Chunks.Add(new FStreamedAudioChunk());
		}
	}

	for (int32 ChunkIndex = 0; ChunkIndex < Chunks.Num(); ++ChunkIndex)
	{
		Chunks[ChunkIndex].Serialize(Ar, Owner, ChunkIndex);
	}
	

#if WITH_EDITORONLY_DATA
	if (Owner)
	{
		Owner->RawDataCriticalSection.Unlock();
	}
#endif
}

/**
 * Helper class to display a status update message in the editor.
 */
class FAudioStatusMessageContext : FScopedSlowTask
{
public:

	/**
	 * Updates the status message displayed to the user.
	 */
	explicit FAudioStatusMessageContext( const FText& InMessage )
	 : FScopedSlowTask(1, InMessage, GIsEditor && !IsRunningCommandlet())
	{
		UE_LOG(LogAudioDerivedData, Display, TEXT("%s"), *InMessage.ToString());
	}
};

/**
* Function used for resampling a USoundWave's WaveData, which is assumed to be int16 here:
*/
static void ResampleWaveData(Audio::FAlignedFloatBuffer& WaveData, int32 NumChannels, float SourceSampleRate, float DestinationSampleRate)
{
	double StartTime = FPlatformTime::Seconds();

	// Set up temporary output buffers:
	Audio::FAlignedFloatBuffer ResamplerOutputData;

	int32 NumSamples = WaveData.Num();
	
	// set up converter input params:
	Audio::FResamplingParameters ResamplerParams = {
		Audio::EResamplingMethod::BestSinc,
		NumChannels,
		SourceSampleRate,
		DestinationSampleRate,
		WaveData
	};

	// Allocate enough space in output buffer for the resulting audio:
	ResamplerOutputData.AddUninitialized(Audio::GetOutputBufferSize(ResamplerParams));
	Audio::FResamplerResults ResamplerResults;
	ResamplerResults.OutBuffer = &ResamplerOutputData;

	// Resample:
	if (Audio::Resample(ResamplerParams, ResamplerResults))
	{
		// resize WaveData buffer and convert back to int16:
		int32 NumSamplesGenerated = ResamplerResults.OutputFramesGenerated * NumChannels;

		WaveData.SetNum(NumSamplesGenerated);
		FMemory::Memcpy(WaveData.GetData(), ResamplerOutputData.GetData(), NumSamplesGenerated * sizeof(float));
	}
	else
	{
		UE_LOG(LogAudioDerivedData, Error, TEXT("Resampling operation failed."));
	}

	double TimeDelta = FPlatformTime::Seconds() - StartTime;
	UE_LOG(LogAudioDerivedData, Display, TEXT("Resampling file from %f to %f took %f seconds."), SourceSampleRate, DestinationSampleRate, TimeDelta);
}

struct FAudioCookInputs
{
	FString                     SoundName;
	FName                       BaseFormat;
	FName                       HashedFormat;
	const IAudioFormat*         Compressor;
	FGuid                       CompressedDataGuid;
#if WITH_EDITORONLY_DATA
	FString                     SoundFullName;
	TArray<int32>               ChannelOffsets;
	TArray<int32>               ChannelSizes;
	bool                        bIsASourceBus;
	bool                        bIsSoundWaveProcedural;
	int32                       CompressionQuality;
	float                       SampleRateOverride = -1.0f;
	bool                        bIsStreaming;
	float                       CompressionQualityModifier;
	
	TArray<Audio::FTransformationPtr> WaveTransformations;

	// Those are the only refs we keep on the actual USoundWave until
	// we have a mechanism in place to reference a bulkdata using a 
	// copy-on-write mechanism that doesn't require us to make
	// a copy in memory to get immutability.
	FCriticalSection& BulkDataCriticalSection;
	UE::Serialization::FEditorBulkData& BulkData;
#endif

	FAudioCookInputs(USoundWave* InSoundWave, FName InBaseFormat, FName InHashFormat, const FPlatformAudioCookOverrides* InCookOverrides)
		: SoundName(InSoundWave->GetName())
		, BaseFormat(InBaseFormat)
		, HashedFormat(InHashFormat)
		, CompressedDataGuid(InSoundWave->CompressedDataGuid)
#if WITH_EDITORONLY_DATA
		, SoundFullName(InSoundWave->GetFullName())
		, ChannelOffsets(InSoundWave->ChannelOffsets)
		, ChannelSizes(InSoundWave->ChannelSizes)
		, bIsASourceBus(InSoundWave->IsA<USoundSourceBus>())
		, bIsSoundWaveProcedural(InSoundWave->IsA<USoundWaveProcedural>())
		, CompressionQuality(InSoundWave->GetCompressionQuality())
		, CompressionQualityModifier(InCookOverrides ? InCookOverrides->CompressionQualityModifier : 1.0f)
		, WaveTransformations(InSoundWave->CreateTransformations())
		, BulkDataCriticalSection(InSoundWave->RawDataCriticalSection)
		, BulkData(InSoundWave->RawData)
#endif
	{
#if WITH_EDITORONLY_DATA
		checkf(IsInGameThread() || IsInAudioThread(), TEXT("FAudioCookInputs creation must happen on the game-thread or audio-thread as it reads from many non-thread safe properties of USoundWave"));

#if FORCE_RESAMPLE
		FPlatformAudioCookOverrides NewCompressionOverrides = FPlatformAudioCookOverrides();
		NewCompressionOverrides.bResampleForDevice = true;
		if (InCookOverrides == nullptr)
		{
			InCookOverrides = &NewCompressionOverrides;
		}
#endif //FORCE_RESAMPLE

		if (InCookOverrides && InCookOverrides->bResampleForDevice)
		{
			SampleRateOverride = InSoundWave->GetSampleRateForCompressionOverrides(InCookOverrides);
		}

		// without overrides, we don't know the target platform's name to be able to look up, and passing nullptr will use editor platform's settings, which could be wrong
		// @todo: Pass in TargetPlatform/PlatformName maybe?
		bIsStreaming = InSoundWave->IsStreaming(InCookOverrides ? *InCookOverrides : FPlatformAudioCookOverrides());
#endif // WITH_EDITORONLY_DATA

		ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
		if (TPM)
		{
			Compressor = TPM->FindAudioFormat(InBaseFormat);
		}
	}
};

#if WITH_EDITORONLY_DATA

/**
 * Cook a simple mono or stereo wave
 */
static void CookSimpleWave(const FAudioCookInputs& Inputs, TArray<uint8>& OutputBuffer)
{
	// Warning: Existing released assets should maintain bitwise exact encoded audio
	// in order to minimize patch sizes. Changing anything in this function can 
	// change the final encoded values and result in large unintended patches. 
	
	TRACE_CPUPROFILER_EVENT_SCOPE(CookSimpleWave);

	FWaveModInfo WaveInfo;
	TArray<uint8> Input;
	check(!OutputBuffer.Num());

	bool bWasLocked = false;

	FScopeLock ScopeLock(&Inputs.BulkDataCriticalSection);

	TFuture<FSharedBuffer> FutureBuffer = Inputs.BulkData.GetPayload();
	
	const uint8* RawWaveData = (const uint8*)FutureBuffer.Get().GetData(); // Will block 
	int32 RawDataSize = FutureBuffer.Get().GetSize();

	if (!Inputs.BulkData.HasPayloadData())
	{
		UE_LOG(LogAudioDerivedData, Warning, TEXT("LPCM data failed to load for sound %s"), *Inputs.SoundFullName);
	}
	else if (!WaveInfo.ReadWaveHeader(RawWaveData, RawDataSize, 0))
	{
		// If we failed to parse the wave header, it's either because of an invalid bitdepth or channel configuration.
		UE_LOG(LogAudioDerivedData, Warning, TEXT("Only mono or stereo 16 bit waves allowed: %s (%d bytes)"), *Inputs.SoundFullName, RawDataSize);
	}
	else
	{
		Input.AddUninitialized(WaveInfo.SampleDataSize);
		FMemory::Memcpy(Input.GetData(), WaveInfo.SampleDataStart, WaveInfo.SampleDataSize);
	}

	if(!Input.Num())
	{
		UE_LOG(LogAudioDerivedData, Warning, TEXT( "Can't cook %s because there is no source LPCM data" ), *Inputs.SoundFullName );
		return;
	}
	
	int32 WaveSampleRate = *WaveInfo.pSamplesPerSec;
	int32 NumChannels = *WaveInfo.pChannels;
	int32 NumBytes = Input.Num();
	int32 NumSamples = NumBytes / sizeof(int16);

	const bool bNeedsResample = (Inputs.SampleRateOverride > 0 && Inputs.SampleRateOverride != (float)WaveSampleRate);
	const bool bNeedsToApplyWaveTransformation = (Inputs.WaveTransformations.Num() > 0);

	// Only convert PCM wave data to float if needed. The conversion alters the sample
	// values enough to produce different results in the final encoded data. 
	const bool bNeedsFloatConversion = bNeedsResample || bNeedsToApplyWaveTransformation;

	if (bNeedsFloatConversion)
	{
		// To float for processing
		Audio::FAlignedFloatBuffer InputFloatBuffer;
		InputFloatBuffer.SetNumUninitialized(NumSamples);
		
		AudioDerivedDataPrivate::ArrayPcm16ToFloat(MakeArrayView((int16*)Input.GetData(), NumSamples), InputFloatBuffer);

		// Run any transformations
		if(bNeedsToApplyWaveTransformation)
		{
			Audio::FWaveformTransformationWaveInfo TransformationInfo;

			TransformationInfo.Audio = &InputFloatBuffer;
			TransformationInfo.NumChannels = NumChannels;
			TransformationInfo.SampleRate = WaveSampleRate;
			
			for(const Audio::FTransformationPtr& Transformation : Inputs.WaveTransformations)
			{
				Transformation->ProcessAudio(TransformationInfo);
			}

			UE_CLOG(WaveSampleRate != TransformationInfo.SampleRate, LogAudioDerivedData, Warning, TEXT("Wave transformations which alter the sample rate are not supported. Cooked audio for %s may be incorrect"), *Inputs.SoundFullName);
			WaveSampleRate = TransformationInfo.SampleRate;

			UE_CLOG(NumChannels != TransformationInfo.NumChannels, LogAudioDerivedData, Error, TEXT("Wave transformations which alter number of channels are not supported. Cooked audio for %s may be incorrect"), *Inputs.SoundFullName);
			NumChannels = TransformationInfo.NumChannels;

			NumSamples = InputFloatBuffer.Num();
		}
		
		// Resample if necessary
		if (bNeedsResample)
		{
			ResampleWaveData(InputFloatBuffer, NumChannels, WaveSampleRate, Inputs.SampleRateOverride);
			
			WaveSampleRate = Inputs.SampleRateOverride;
			NumSamples = InputFloatBuffer.Num();
		}

		// Clip Normalize
		const float MaxValue = Audio::ArrayMaxAbsValue(InputFloatBuffer);
		if (MaxValue > 1.0f)
		{
			UE_LOG(LogAudioDerivedData, Display, TEXT("Audio clipped during cook: This asset will be normalized by a factor of 1/%f. Consider attenuating the above asset."), MaxValue);

			Audio::ArrayMultiplyByConstantInPlace(InputFloatBuffer, 1.f / MaxValue);
		}

		// back to PCM
		NumBytes = NumSamples * sizeof(int16);
		Input.SetNumUninitialized(NumBytes);
		
		AudioDerivedDataPrivate::ArrayFloatToPcm16(InputFloatBuffer, MakeArrayView((int16*)Input.GetData(), NumSamples));
	}

	// Compression Quality
	FSoundQualityInfo QualityInfo = { 0 };
	float ModifiedCompressionQuality = (float)Inputs.CompressionQuality * Inputs.CompressionQualityModifier;
	if (ModifiedCompressionQuality >= 1.0f)
	{
		QualityInfo.Quality = FMath::FloorToInt(ModifiedCompressionQuality);
		UE_CLOG(Inputs.CompressionQuality != QualityInfo.Quality, LogAudioDerivedData, Display, TEXT("Compression Quality for %s will be modified from %d to %d."), *Inputs.SoundFullName, Inputs.CompressionQuality, QualityInfo.Quality);
	}
	else
	{
		QualityInfo.Quality = Inputs.CompressionQuality;
	}

	QualityInfo.NumChannels = NumChannels;
	QualityInfo.SampleRate = WaveSampleRate;
	QualityInfo.SampleDataSize = NumBytes;

	QualityInfo.bStreaming = Inputs.bIsStreaming;
	QualityInfo.DebugName = Inputs.SoundFullName;

	static const FName NAME_BINKA(TEXT("BINKA"));
	if (WaveSampleRate > 48000 &&
		Inputs.BaseFormat == NAME_BINKA)
	{
		// We have to do this here because we don't know the name of the wave inside the codec.
		UE_LOG(LogAudioDerivedData, Warning, TEXT("[%s] High sample rate wave (%d) with Bink Audio - perf waste - high frequencies are discarded by Bink Audio (like most perceptual codecs)."), *Inputs.SoundFullName, WaveSampleRate);
	}

	// Cook the data.
	if (!Inputs.Compressor->Cook(Inputs.BaseFormat, Input, QualityInfo, OutputBuffer))
	{
		UE_LOG(LogAudioDerivedData, Warning, TEXT("Cooking sound failed: %s"), *Inputs.SoundFullName);
	}
}

/**
 * Cook a multistream (normally 5.1) wave
 */
static void CookSurroundWave(const FAudioCookInputs& Inputs,  TArray<uint8>& OutputBuffer)
{
	// Warning: Existing released assets should maintain bitwise exact encoded audio
	// in order to minimize patch sizes. Changing anything in this function can 
	// change the final encoded values and result in large unintended patches. 

	TRACE_CPUPROFILER_EVENT_SCOPE(CookSurroundWave);

	check(!OutputBuffer.Num());

	int32					i;
	size_t					SampleDataSize = 0;
	FWaveModInfo			WaveInfo;
	TArray<TArray<uint8> >	SourceBuffers;
	TArray<int32>			RequiredChannels;

	FScopeLock ScopeLock(&Inputs.BulkDataCriticalSection);
	// Lock raw wave data.
	TFuture<FSharedBuffer> FutureBuffer = Inputs.BulkData.GetPayload();
	const uint8* RawWaveData = (const uint8*)FutureBuffer.Get().GetData(); // Will block 
	int32 RawDataSize = FutureBuffer.Get().GetSize();

	if (!RawWaveData || RawDataSize <=0 )
	{
		UE_LOG(LogAudioDerivedData, Warning, TEXT("Cooking surround sound failed: %s, Failed to load virtualized bulkdata payload"), *Inputs.SoundFullName);
		return;
	}

	// Front left channel is the master
	static_assert(SPEAKER_FrontLeft == 0, "Front-left speaker must be first.");

	// loop through channels to find which have data and which are required
	for (i = 0; i < SPEAKER_Count; i++)
	{
		FWaveModInfo WaveInfoInner;

		// Only mono files allowed
		if (WaveInfoInner.ReadWaveHeader(RawWaveData, Inputs.ChannelSizes[i], Inputs.ChannelOffsets[i])
			&& *WaveInfoInner.pChannels == 1)
		{
			if (SampleDataSize == 0)
			{
				// keep wave info/size of first channel data we find
				WaveInfo = WaveInfoInner;
				SampleDataSize = WaveInfo.SampleDataSize;
			}
			switch (i)
			{
				case SPEAKER_FrontLeft:
				case SPEAKER_FrontRight:
				case SPEAKER_LeftSurround:
				case SPEAKER_RightSurround:
					// Must have quadraphonic surround channels
					RequiredChannels.AddUnique(SPEAKER_FrontLeft);
					RequiredChannels.AddUnique(SPEAKER_FrontRight);
					RequiredChannels.AddUnique(SPEAKER_LeftSurround);
					RequiredChannels.AddUnique(SPEAKER_RightSurround);
					break;
				case SPEAKER_FrontCenter:
				case SPEAKER_LowFrequency:
					// Must have 5.1 surround channels
					for (int32 Channel = SPEAKER_FrontLeft; Channel <= SPEAKER_RightSurround; Channel++)
					{
						RequiredChannels.AddUnique(Channel);
					}
					break;
				case SPEAKER_LeftBack:
				case SPEAKER_RightBack:
					// Must have all previous channels
					for (int32 Channel = 0; Channel < i; Channel++)
					{
						RequiredChannels.AddUnique(Channel);
					}
					break;
				default:
					// unsupported channel count
					break;
			}
		}
	}

	if (SampleDataSize == 0)
	{
		UE_LOG(LogAudioDerivedData, Warning, TEXT( "Cooking surround sound failed: %s" ), *Inputs.SoundFullName );
		return;
	}

	TArray<FWaveModInfo, TInlineAllocator<SPEAKER_Count>> ChannelInfos;
	
	int32 ChannelCount = 0;
	// Extract all the info for channels
	for( i = 0; i < SPEAKER_Count; i++ )
	{
		FWaveModInfo WaveInfoInner;
		if( WaveInfoInner.ReadWaveHeader( RawWaveData, Inputs.ChannelSizes[ i ], Inputs.ChannelOffsets[ i ] )
			&& *WaveInfoInner.pChannels == 1 )
		{
			ChannelCount++;
			ChannelInfos.Add(WaveInfoInner);

			SampleDataSize = FMath::Max<uint32>(WaveInfoInner.SampleDataSize, SampleDataSize);
		}
		else if (RequiredChannels.Contains(i))
		{
			// Add an empty channel for cooking
			ChannelCount++;
			WaveInfoInner.SampleDataSize = 0;

			ChannelInfos.Add(WaveInfoInner);
		}
	}

	// Only allow the formats that can be played back through
	const bool bChannelCountValidForPlayback = ChannelCount == 4 || ChannelCount == 6 || ChannelCount == 7 || ChannelCount == 8;
	
	if( bChannelCountValidForPlayback == false )
	{
		UE_LOG(LogAudioDerivedData, Warning, TEXT( "No format available for a %d channel surround sound: %s" ), ChannelCount, *Inputs.SoundFullName );
		return;
	}

	// copy channels we need, ensuring all channels are the same size
	for(const FWaveModInfo& ChannelInfo : ChannelInfos)
	{
		TArray<uint8>& Input = *new (SourceBuffers) TArray<uint8>;
		Input.AddZeroed(SampleDataSize);

		if(ChannelInfo.SampleDataSize > 0)
		{
			FMemory::Memcpy(Input.GetData(), ChannelInfo.SampleDataStart, ChannelInfo.SampleDataSize);
		}
	}
	
	int32 WaveSampleRate = *WaveInfo.pSamplesPerSec;
	int32 NumFrames = SampleDataSize / sizeof(int16);

	// bNeedsResample could change if a transformation changes the sample rate
	bool bNeedsResample = Inputs.SampleRateOverride > 0 && Inputs.SampleRateOverride != (float)WaveSampleRate;
	
	const bool bContainsTransformations = Inputs.WaveTransformations.Num() > 0;
	const bool bNeedsDeinterleave = bNeedsResample || bContainsTransformations;

	if(bNeedsDeinterleave)
	{
		// multichannel wav's are stored deinterleaved, but our dsp assumes interleaved
		Audio::FAlignedFloatBuffer InterleavedFloatBuffer;

		Audio::FMultichannelBuffer InputMultichannelBuffer;

		Audio::SetMultichannelBufferSize(ChannelCount, NumFrames, InputMultichannelBuffer);
		
		// convert to float
		for (int32 ChannelIndex = 0; ChannelIndex < ChannelCount; ChannelIndex++)
		{
			AudioDerivedDataPrivate::ArrayPcm16ToFloat(MakeArrayView((const int16*)(SourceBuffers[ChannelIndex].GetData()), NumFrames), InputMultichannelBuffer[ChannelIndex]);
		}
	
		Audio::ArrayInterleave(InputMultichannelBuffer, InterleavedFloatBuffer);

		// run transformations
		if(bContainsTransformations)
		{
			Audio::FWaveformTransformationWaveInfo TransformationInfo;

			TransformationInfo.Audio = &InterleavedFloatBuffer;
			TransformationInfo.NumChannels = ChannelCount;
			TransformationInfo.SampleRate = WaveSampleRate;
		
			for(const Audio::FTransformationPtr& Transformation : Inputs.WaveTransformations)
			{
				Transformation->ProcessAudio(TransformationInfo);
			}

			UE_CLOG(WaveSampleRate != TransformationInfo.SampleRate, LogAudioDerivedData, Warning, TEXT("Wave transformations which alter the sample rate are not supported. Cooked audio for %s may be incorrect"), *Inputs.SoundFullName);
			WaveSampleRate = TransformationInfo.SampleRate;
			bNeedsResample = Inputs.SampleRateOverride > 0 && Inputs.SampleRateOverride != (float)WaveSampleRate;
			
			UE_CLOG(ChannelCount != TransformationInfo.NumChannels, LogAudioDerivedData, Error, TEXT("Wave transformations which alter number of channels are not supported. Cooked audio for %s may be incorrect"), *Inputs.SoundFullName);
			ChannelCount = TransformationInfo.NumChannels;

			NumFrames = InterleavedFloatBuffer.Num() / ChannelCount;
		}

		if (bNeedsResample)
		{
			ResampleWaveData(InterleavedFloatBuffer, ChannelCount, WaveSampleRate, Inputs.SampleRateOverride);
			
			WaveSampleRate = Inputs.SampleRateOverride;
			NumFrames = InterleavedFloatBuffer.Num() / ChannelCount;
		}

		// clip normalize
		const float MaxValue = Audio::ArrayMaxAbsValue(InterleavedFloatBuffer);
		if (MaxValue > 1.0f)
		{
			UE_LOG(LogAudioDerivedData, Display, TEXT("Audio clipped during cook: This asset will be normalized by a factor of 1/%f. Consider attenuating the above asset."), MaxValue);

			Audio::ArrayMultiplyByConstantInPlace(InterleavedFloatBuffer, 1.f / MaxValue);
		}

		Audio::ArrayDeinterleave(InterleavedFloatBuffer, InputMultichannelBuffer, ChannelCount);

		SampleDataSize = NumFrames * sizeof(int16);

		// back to PCM
		for (int32 ChannelIndex = 0; ChannelIndex < ChannelCount; ChannelIndex++)
		{
			TArray<uint8>& PcmBuffer = SourceBuffers[ChannelIndex];
				
			PcmBuffer.SetNum(SampleDataSize);
			
			AudioDerivedDataPrivate::ArrayFloatToPcm16(InputMultichannelBuffer[ChannelIndex], MakeArrayView((int16*)PcmBuffer.GetData(), NumFrames));
		}
	}

	UE_LOG(LogAudioDerivedData, Display, TEXT("Cooking %d channels for: %s"), ChannelCount, *Inputs.SoundFullName);

	FSoundQualityInfo QualityInfo = { 0 };

	float ModifiedCompressionQuality = (float)Inputs.CompressionQuality;

	if (!FMath::IsNearlyEqual(Inputs.CompressionQualityModifier, 1.0f))
	{
		ModifiedCompressionQuality = (float)Inputs.CompressionQuality * Inputs.CompressionQualityModifier;
	}
	
	if (ModifiedCompressionQuality >= 1.0f)
	{
		QualityInfo.Quality = FMath::FloorToInt(ModifiedCompressionQuality);
	}
	else
	{
		QualityInfo.Quality = Inputs.CompressionQuality;
	}

	QualityInfo.NumChannels = ChannelCount;
	QualityInfo.SampleRate = WaveSampleRate;
	QualityInfo.SampleDataSize = SampleDataSize;
	QualityInfo.bStreaming = Inputs.bIsStreaming;
	QualityInfo.DebugName = Inputs.SoundFullName;

	static const FName NAME_BINKA(TEXT("BINKA"));
	if (WaveSampleRate > 48000 &&
		Inputs.BaseFormat == NAME_BINKA)
	{
		// We have to do this here because we don't know the name of the wave inside the codec.
		UE_LOG(LogAudioDerivedData, Warning, TEXT("[%s] High sample rate wave (%d) with Bink Audio - perf waste - high frequencies are discarded by Bink Audio (like most perceptual codecs)."), *Inputs.SoundFullName, WaveSampleRate);
	}

	//@todo tighten up the checking for empty results here
	if (!Inputs.Compressor->CookSurround(Inputs.BaseFormat, SourceBuffers, QualityInfo, OutputBuffer))
	{
		UE_LOG(LogAudioDerivedData, Warning, TEXT("Cooking surround sound failed: %s"), *Inputs.SoundFullName);
	}
}

#endif // WITH_EDITORONLY_DATA

FDerivedAudioDataCompressor::FDerivedAudioDataCompressor(USoundWave* InSoundNode, FName InBaseFormat, FName InHashedFormat, const FPlatformAudioCookOverrides* InCompressionOverrides)
	: CookInputs(MakeUnique<FAudioCookInputs>(InSoundNode, InBaseFormat, InHashedFormat, InCompressionOverrides))
{
}

FString FDerivedAudioDataCompressor::GetPluginSpecificCacheKeySuffix() const
{
	int32 FormatVersion = 0xffff; // if the compressor is NULL, this will be used as the version...and in that case we expect everything to fail anyway
	if (CookInputs->Compressor)
	{
		FormatVersion = (int32)CookInputs->Compressor->GetVersion(CookInputs->BaseFormat);
	}

	check(CookInputs->CompressedDataGuid.IsValid());
	FString FormatHash = CookInputs->HashedFormat.ToString().ToUpper();
	return FString::Printf(TEXT("%s_%04X_%s"), *FormatHash, FormatVersion, *CookInputs->CompressedDataGuid.ToString());
}

bool FDerivedAudioDataCompressor::IsBuildThreadsafe() const
{
	return AllowAsyncCompression && CookInputs->Compressor ? CookInputs->Compressor->AllowParallelBuild() : false;
}

bool FDerivedAudioDataCompressor::Build(TArray<uint8>& OutData)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FDerivedAudioDataCompressor::Build);

#if WITH_EDITORONLY_DATA
	if (!CookInputs->Compressor)
	{
		UE_LOG(LogAudioDerivedData, Warning, TEXT("Could not find audio format to cook: %s"), *CookInputs->BaseFormat.ToString());
		return false;
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("AudioFormat"), FText::FromName(CookInputs->BaseFormat));
	Args.Add(TEXT("Hash"), FText::FromName(CookInputs->HashedFormat));
	Args.Add(TEXT("SoundNodeName"), FText::FromString(CookInputs->SoundName));
	FAudioStatusMessageContext StatusMessage(FText::Format(NSLOCTEXT("Engine", "BuildingCompressedAudioTaskStatus", "Building compressed audio format {AudioFormat} hash {Hash} wave {SoundNodeName}..."), Args));

	// these types of sounds do not need cooked data
	if(CookInputs->bIsASourceBus || CookInputs->bIsSoundWaveProcedural)
	{
		return false;
	}
	
	if (!CookInputs->ChannelSizes.Num())
	{
		check(!CookInputs->ChannelOffsets.Num());
		CookSimpleWave(*CookInputs, OutData);
	}
	else
	{
		check(CookInputs->ChannelOffsets.Num() == SPEAKER_Count);
		check(CookInputs->ChannelSizes.Num() == SPEAKER_Count);
		CookSurroundWave(*CookInputs, OutData);
	}

#endif
	return OutData.Num() > 0;
}

/*---------------------------------------
	USoundWave Derived Data functions
---------------------------------------*/

void USoundWave::CleanupCachedRunningPlatformData()
{
	check(SoundWaveDataPtr);
	SoundWaveDataPtr->RunningPlatformData = FStreamedAudioPlatformData();
}


void USoundWave::SerializeCookedPlatformData(FArchive& Ar)
{
	if (IsTemplate())
	{
		return;
	}

	DECLARE_SCOPE_CYCLE_COUNTER( TEXT("USoundWave::SerializeCookedPlatformData"), STAT_SoundWave_SerializeCookedPlatformData, STATGROUP_LoadTime );

#if WITH_EDITORONLY_DATA
	if (Ar.IsCooking() && Ar.IsPersistent())
	{
		check(Ar.CookingTarget()->AllowAudioVisualData());

		FName PlatformFormat = Ar.CookingTarget()->GetWaveFormat(this);
		const FPlatformAudioCookOverrides* CompressionOverrides = FPlatformCompressionUtilities::GetCookOverrides(*Ar.CookingTarget()->IniPlatformName());
		FString DerivedDataKey;

		GetStreamedAudioDerivedDataKeySuffix(*this, PlatformFormat, CompressionOverrides, DerivedDataKey);

		FStreamedAudioPlatformData *PlatformDataToSave = CookedPlatformData.FindRef(DerivedDataKey);

		if (PlatformDataToSave == NULL)
		{
			PlatformDataToSave = new FStreamedAudioPlatformData();
			PlatformDataToSave->Cache(*this, CompressionOverrides, PlatformFormat, EStreamedAudioCacheFlags::InlineChunks | EStreamedAudioCacheFlags::Async);

			CookedPlatformData.Add(DerivedDataKey, PlatformDataToSave);
		}

		PlatformDataToSave->FinishCache();
		PlatformDataToSave->Serialize(Ar, this);
	}
	else
#endif // #if WITH_EDITORONLY_DATA
	{
		check(!FPlatformProperties::IsServerOnly());
		check(SoundWaveDataPtr);

		CleanupCachedRunningPlatformData();

		// Don't serialize streaming data on servers, even if this platform supports streaming in theory
		SoundWaveDataPtr->RunningPlatformData.Serialize(Ar, this);
	}
}

#if WITH_EDITORONLY_DATA
void USoundWave::CachePlatformData(bool bAsyncCache)
{
	check(SoundWaveDataPtr);

	// don't interact with the DDC if we were loaded from cooked data in editor
	if (bLoadedFromCookedData)
	{
		return;
	}

	FString DerivedDataKey;
	FName AudioFormat = GetWaveFormatForRunningPlatform(*this);
	const FPlatformAudioCookOverrides* CompressionOverrides = GetCookOverridesForRunningPlatform();
	GetStreamedAudioDerivedDataKey(*this, AudioFormat, CompressionOverrides, DerivedDataKey);

	if (SoundWaveDataPtr->RunningPlatformData.DerivedDataKey != DerivedDataKey)
	{
		const uint32 CacheFlags = bAsyncCache ? (EStreamedAudioCacheFlags::Async | EStreamedAudioCacheFlags::AllowAsyncBuild) : EStreamedAudioCacheFlags::None;
		SoundWaveDataPtr->RunningPlatformData.Cache(*this, CompressionOverrides, AudioFormat, CacheFlags);
	}
}

void USoundWave::BeginCachePlatformData()
{
	CachePlatformData(true);

#if WITH_EDITOR
	// enable caching in postload for derived data cache commandlet and cook by the book
	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	if (TPM && (TPM->RestrictFormatsToRuntimeOnly() == false))
	{
		TArray<ITargetPlatform*> Platforms = TPM->GetActiveTargetPlatforms();
		// Cache for all the audio formats that the cooking target requires
		for (int32 FormatIndex = 0; FormatIndex < Platforms.Num(); FormatIndex++)
		{
			BeginCacheForCookedPlatformData(Platforms[FormatIndex]);
		}
	}
#endif
}
#if WITH_EDITOR

void USoundWave::BeginCacheForCookedPlatformData(const ITargetPlatform *TargetPlatform)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(USoundWave::BeginCacheForCookedPlatformData);

	const FPlatformAudioCookOverrides* CompressionOverrides = FPlatformCompressionUtilities::GetCookOverrides(*TargetPlatform->IniPlatformName());

	if (TargetPlatform->AllowAudioVisualData())
	{
		// Retrieve format to cache for targetplatform.
		FName PlatformFormat = TargetPlatform->GetWaveFormat(this);

		if (TargetPlatform->SupportsFeature(ETargetPlatformFeatures::AudioStreaming) && IsStreaming(*CompressionOverrides))
		{
			// Always allow the build to be performed asynchronously as it is now thread-safe by fetching compressed data directly from the DDC.
			uint32 CacheFlags = EStreamedAudioCacheFlags::Async | EStreamedAudioCacheFlags::InlineChunks | EStreamedAudioCacheFlags::AllowAsyncBuild;

			// find format data by comparing derived data keys.
			FString DerivedDataKey;
			GetStreamedAudioDerivedDataKeySuffix(*this, PlatformFormat, CompressionOverrides, DerivedDataKey);

			FStreamedAudioPlatformData *PlatformData = CookedPlatformData.FindRef(DerivedDataKey);

			if (PlatformData == nullptr)
			{
				PlatformData = new FStreamedAudioPlatformData();
				PlatformData->Cache(
					*this,
					CompressionOverrides,
					PlatformFormat,
					CacheFlags
					);
				CookedPlatformData.Add(DerivedDataKey, PlatformData);
			}
		}
		else
		{
			BeginGetCompressedData(PlatformFormat, CompressionOverrides);
		}
	}

	Super::BeginCacheForCookedPlatformData(TargetPlatform);
}

bool USoundWave::IsCachedCookedPlatformDataLoaded( const ITargetPlatform* TargetPlatform )
{
	TRACE_CPUPROFILER_EVENT_SCOPE(USoundWave::IsCachedCookedPlatformDataLoaded);

	const FPlatformAudioCookOverrides* CompressionOverrides = FPlatformCompressionUtilities::GetCookOverrides(*TargetPlatform->IniPlatformName());

	if (TargetPlatform->AllowAudioVisualData())
	{
		// Retrieve format to cache for targetplatform.
		FName PlatformFormat = TargetPlatform->GetWaveFormat(this);

		if (TargetPlatform->SupportsFeature(ETargetPlatformFeatures::AudioStreaming) && IsStreaming(*CompressionOverrides))
		{
			// find format data by comparing derived data keys.
			FString DerivedDataKey;
			GetStreamedAudioDerivedDataKeySuffix(*this, PlatformFormat, CompressionOverrides, DerivedDataKey);

			FStreamedAudioPlatformData *PlatformData = CookedPlatformData.FindRef(DerivedDataKey);
			if (PlatformData == nullptr)
			{
				// we haven't called begincache
				return false;
			}

			if (PlatformData->IsAsyncWorkComplete())
			{
				PlatformData->FinishCache();
			}

			return PlatformData->IsFinishedCache();
		}
		else
		{
			return IsCompressedDataReady(PlatformFormat, CompressionOverrides);
		}
	}

	return true;
}


/**
* Clear all the cached cooked platform data which we have accumulated with BeginCacheForCookedPlatformData calls
* The data can still be cached again using BeginCacheForCookedPlatformData again
*/
void USoundWave::ClearAllCachedCookedPlatformData()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(USoundWave::ClearAllCachedCookedPlatformData);

	Super::ClearAllCachedCookedPlatformData();

	for (auto It : CookedPlatformData)
	{
		delete It.Value;
	}

	CookedPlatformData.Empty();
}

void USoundWave::ClearCachedCookedPlatformData( const ITargetPlatform* TargetPlatform )
{
	TRACE_CPUPROFILER_EVENT_SCOPE(USoundWave::ClearCachedCookedPlatformData);

	Super::ClearCachedCookedPlatformData(TargetPlatform);

	const FPlatformAudioCookOverrides* CompressionOverrides = FPlatformCompressionUtilities::GetCookOverrides(*TargetPlatform->IniPlatformName());

	if (TargetPlatform->SupportsFeature(ETargetPlatformFeatures::AudioStreaming) && IsStreaming(*CompressionOverrides))
	{
		// Retrieve format to cache for targetplatform.
		FName PlatformFormat = TargetPlatform->GetWaveFormat(this);

		// find format data by comparing derived data keys.
		FString DerivedDataKey;
		GetStreamedAudioDerivedDataKeySuffix(*this, PlatformFormat, CompressionOverrides, DerivedDataKey);


		if ( CookedPlatformData.Contains(DerivedDataKey) )
		{
			FStreamedAudioPlatformData *PlatformData = CookedPlatformData.FindAndRemoveChecked( DerivedDataKey );
			delete PlatformData;
		}
	}
}

void USoundWave::WillNeverCacheCookedPlatformDataAgain()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(USoundWave::WillNeverCacheCookedPlatformDataAgain);

	FinishCachePlatformData();

	// this is called after we have finished caching the platform data but before we have saved the data
	// so need to keep the cached platform data around
	Super::WillNeverCacheCookedPlatformDataAgain();

	check(SoundWaveDataPtr);
	SoundWaveDataPtr->CompressedFormatData.FlushData();
}
#endif

void USoundWave::FinishCachePlatformData()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(USoundWave::FinishCachePlatformData);

	check(SoundWaveDataPtr);

	// Removed the call to CachePlatformData here since the role of FinishCachePlatformData
	// should only be to finish any outstanding task. The only place that was relying on
	// FinishCachePlatformData to also call CachePlatformData was USoundWave::PostLoad
	// which has been modified to call CachePlatformData instead.
	// Furthermore, this function is called in WillNeverCacheCookedPlatformDataAgain, which we
	// obviously don't want to start performing new work, just finish the outstanding one.

	// Make sure async requests are finished
	SoundWaveDataPtr->RunningPlatformData.FinishCache();

#if DO_CHECK
	// If we're allowing cooked data to be loaded then the derived data key will not have been serialized, so won't match and that's fine
	if (!GAllowCookedDataInEditorBuilds && SoundWaveDataPtr->RunningPlatformData.GetNumChunks())
	{
		FString DerivedDataKey;
		FName AudioFormat = GetWaveFormatForRunningPlatform(*this);
		const FPlatformAudioCookOverrides* CompressionOverrides = GetCookOverridesForRunningPlatform();
		GetStreamedAudioDerivedDataKey(*this, AudioFormat, CompressionOverrides, DerivedDataKey);

		UE_CLOG(SoundWaveDataPtr->RunningPlatformData.DerivedDataKey != DerivedDataKey, LogAudio, Warning, TEXT("Audio was cooked with the DDC key %s but should've had the DDC key %s. the cook overrides/codec used may be incorrect."), *SoundWaveDataPtr->RunningPlatformData.DerivedDataKey, *DerivedDataKey);
	}
#endif
}

void USoundWave::ForceRebuildPlatformData()
{
	check(SoundWaveDataPtr);
	const FPlatformAudioCookOverrides* CompressionOverrides = GetCookOverridesForRunningPlatform();

	SoundWaveDataPtr->RunningPlatformData.Cache(
		*this,
		CompressionOverrides,
		GetWaveFormatForRunningPlatform(*this),
		EStreamedAudioCacheFlags::ForceRebuild
		);
}
#endif //WITH_EDITORONLY_DATA

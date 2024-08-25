// Copyright Epic Games, Inc. All Rights Reserved.


#include "AudioDecompress.h"
#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Hash/xxhash.h"
#include "Interfaces/IAudioFormat.h"
#include "AudioStreamingCache.h"
#include "Misc/CoreStats.h"
#include "Misc/ScopeRWLock.h"
#include "Stats/StatsTrace.h"
#include "Sound/StreamedAudioChunkSeekTable.h"

namespace AudioDecompressPrivate
{
#ifndef WITH_AUDIO_DECODER_DIAGNOSTICS
	// Optionally enable this manually with a flag. They use strings, so shouldn't be enabled by default.
	#define WITH_AUDIO_DECODER_DIAGNOSTICS (0)
#endif //WITH_AUDIO_DECODER_DIAGNOSTICS

#if WITH_AUDIO_DECODER_DIAGNOSTICS
	
	static FString ForceDecoderErrorOnWaveCVar;
	FAutoConsoleVariableRef CVarForceDecoderErrorOnWave(
		TEXT("au.debug.force_decoder_error_on_wave"),
		ForceDecoderErrorOnWaveCVar,
		TEXT("Force Decoder Error On Any decoding wave matching this string."),
		ECVF_Default);

	static FString ForceDecoderNegativeSamplesOnWaveCVar;
	FAutoConsoleVariableRef CVarForceDecoderNegativeSamplesOnWave(
		TEXT("au.debug.force_decoder_negative_samples_on_wave"),
		ForceDecoderNegativeSamplesOnWaveCVar,
		TEXT("Force Negative Samples on Decode call to simulate error" ),
		ECVF_Default);

	// Macro to string match against the debugging wave of choice.
	#define DECODER_MATCHES_WAVE(STR)\
		(!STR.IsEmpty() && StreamingSoundWave.IsValid() && StreamingSoundWave->GetFName().ToString().Contains(STR))
	
#else  //WITH_AUDIO_DECODER_DIAGNOSTICS
	#define DECODER_MATCHES_WAVE(STR) (false)
#endif //WITH_AUDIO_DECODER_DIAGNOSTICS
}

IStreamedCompressedInfo::IStreamedCompressedInfo()
	: bIsStreaming(false)
	, SrcBufferData(nullptr)
	, SrcBufferDataSize(0)
	, SrcBufferOffset(0)
	, AudioDataOffset(0)
	, AudioDataChunkIndex(0)
	, TrueSampleCount(0)
	, CurrentSampleCount(0)
	, NumChannels(0)
	, MaxFrameSizeSamples(0)
	, SampleStride(0)
	, LastPCMByteSize(0)
	, LastPCMOffset(0)
	, bStoringEndOfFile(false)
	, CurrentChunkIndex(0)
	, SrcBufferPadding(0)
	, StreamSeekBlockIndex(INDEX_NONE)
	, StreamSeekBlockOffset(0)
{
	StartTimeInCycles = FPlatformTime::Cycles64();
}

uint32 IStreamedCompressedInfo::Read(void *OutBuffer, uint32 DataSize)
{
	uint32 BytesToRead = FMath::Min(DataSize, SrcBufferDataSize - SrcBufferOffset);
	if (BytesToRead > 0)
	{
		FMemory::Memcpy(OutBuffer, SrcBufferData + SrcBufferOffset, BytesToRead);
		SrcBufferOffset += BytesToRead;
	}
	return BytesToRead;
}

bool IStreamedCompressedInfo::ReadCompressedInfo(const uint8* InSrcBufferData, uint32 InSrcBufferDataSize, FSoundQualityInfo* QualityInfo)
{
	check(!SrcBufferData);

	SCOPE_CYCLE_COUNTER(STAT_AudioStreamedDecompressTime);

	// Parse the format header, this is done different for each format
	if (!ParseHeader(InSrcBufferData, InSrcBufferDataSize, QualityInfo))
	{
		return false;
	}

	// After parsing the header, the SrcBufferData should be none-null
	check(SrcBufferData != nullptr);

	// Sample Stride is 
	SampleStride = NumChannels * MONO_PCM_SAMPLE_SIZE;

	MaxFrameSizeSamples = GetMaxFrameSizeSamples();
		
	LastDecodedPCM.Empty(MaxFrameSizeSamples * SampleStride);
	LastDecodedPCM.AddUninitialized(MaxFrameSizeSamples * SampleStride);

	return CreateDecoder();
}

bool IStreamedCompressedInfo::ReadCompressedData(uint8* Destination, bool bLooping, uint32 BufferSize)
{
	check(Destination);

	SCOPE_CYCLE_COUNTER(STAT_AudioStreamedDecompressTime);

	bool bFinished = false;
	uint32 TotalBytesDecoded = 0;

	while (TotalBytesDecoded < BufferSize)
	{
		const uint8* EncodedSrcPtr = SrcBufferData + SrcBufferOffset;
		const uint32 RemainingEncodedSrcSize = SrcBufferDataSize - SrcBufferOffset;

		const FDecodeResult DecodeResult = Decode(EncodedSrcPtr, RemainingEncodedSrcSize, Destination, BufferSize - TotalBytesDecoded);
		if (!DecodeResult.NumPcmBytesProduced)
		{
			bFinished = true;

			if (bLooping)
			{
				SrcBufferOffset = AudioDataOffset;
				CurrentSampleCount = 0;

				PrepareToLoop();
			}
			else
			{
				// Zero the remainder of the buffer
				FMemory::Memzero(Destination, BufferSize - TotalBytesDecoded);
				break;
			}
		}
		else if (DecodeResult.NumPcmBytesProduced < 0)
		{
			// Zero the remainder of the buffer
			FMemory::Memzero(Destination, BufferSize - TotalBytesDecoded);
			return true;
		}

		TotalBytesDecoded += DecodeResult.NumPcmBytesProduced;
		SrcBufferOffset += DecodeResult.NumCompressedBytesConsumed;
		Destination += DecodeResult.NumPcmBytesProduced;
	}

	return bFinished;
}

void IStreamedCompressedInfo::SeekToTime(const float InSeekTimeSeconds)
{
	if (StreamingSoundWave.IsValid())
	{
		const FSoundWavePtr WaveData = StreamingSoundWave->GetSoundWaveData();
		if (WaveData.IsValid())
		{
			// Negative time will seek to start.
			int64 SeekTimeAudioFrames = 0;
			if (InSeekTimeSeconds > 0)
			{
				SeekTimeAudioFrames = FMath::FloorToInt64(WaveData->GetSampleRate() * InSeekTimeSeconds);
				if (!IntFitsIn<uint32>(SeekTimeAudioFrames))
				{
					UE_LOG(LogAudio, Warning, TEXT("Seek too large (%.2f seconds), ignoring..."), InSeekTimeSeconds);
					return;
				}
			}

			// If we have a chunk setup to contain a seek-table it will return a value other than INDEX_NONE here.
			const int32 ChunkIndexToSeekTo = WaveData->FindChunkIndexForSeeking(IntCastChecked<uint32>(SeekTimeAudioFrames));
			if (ChunkIndexToSeekTo >= 0)
			{
				StreamSeekBlockIndex = ChunkIndexToSeekTo;
				StreamSeekBlockOffset = 0;								// We don't know this until the seek-table is loaded, so we leave it 0 for now.
				StreamSeekToAudioFrames = SeekTimeAudioFrames;			// Store the time in samples so when we load the chunks table loads we can find the offset.
			}
		}
	}
}

void IStreamedCompressedInfo::SeekToFrame(const uint32 InSeekTimeFrames)
{
	if (StreamingSoundWave.IsValid())
	{
		const FSoundWavePtr WaveData = StreamingSoundWave->GetSoundWaveData();
		if (WaveData.IsValid())
		{
			// If we have a chunk setup to contain a seek-table it will return a value other than INDEX_NONE here.
			const int32 ChunkIndexToSeekTo = WaveData->FindChunkIndexForSeeking(IntCastChecked<uint32>(InSeekTimeFrames));
			if (ChunkIndexToSeekTo >= 0)
			{
				StreamSeekBlockIndex = ChunkIndexToSeekTo;
				StreamSeekBlockOffset = 0;								// We don't know this until the seek-table is loaded, so we leave it 0 for now.
				StreamSeekToAudioFrames = InSeekTimeFrames;			// Store the time in samples so when we load the chunks table loads we can find the offset.
			}
		}
	}
}

void IStreamedCompressedInfo::ExpandFile(uint8* DstBuffer, struct FSoundQualityInfo* QualityInfo)
{
	check(DstBuffer);
	check(QualityInfo);

	// Ensure we're at the start of the audio data
	SrcBufferOffset = AudioDataOffset;

	uint32 RawPCMOffset = 0;

	while (RawPCMOffset < QualityInfo->SampleDataSize)
	{
		const int32 DecodedFrames = DecompressToPCMBuffer( /*Unused*/ 0);

		if (DecodedFrames < 0)
		{
			RawPCMOffset += ZeroBuffer(DstBuffer + RawPCMOffset, QualityInfo->SampleDataSize - RawPCMOffset);
		}
		else
		{
			LastPCMByteSize = IncrementCurrentSampleCount(DecodedFrames * NumChannels) * MONO_PCM_SAMPLE_SIZE;
			RawPCMOffset += WriteFromDecodedPCM(DstBuffer + RawPCMOffset, QualityInfo->SampleDataSize - RawPCMOffset);
		}
	}
}

bool IStreamedCompressedInfo::StreamCompressedInfoInternal(const FSoundWaveProxyPtr& InWaveProxy, struct FSoundQualityInfo* QualityInfo)
{
	// we have already cached the wave, confirm it is valid and the same
	if (!(ensureAlways(StreamingSoundWave.IsValid() && (StreamingSoundWave == InWaveProxy))))
	{
		return false;
	}

	// Get the zeroth chunk of data (should always be loaded)
	CurrentChunkIndex = 0;
	uint32 ChunkSize = 0;
	const uint8* FirstChunk = GetLoadedChunk(StreamingSoundWave, CurrentChunkIndex, ChunkSize);

	// If there is a seek-table present in the chunk, we'll need to parse that first.
	if (InWaveProxy->GetSoundWaveData()->HasChunkSeekTable(CurrentChunkIndex))
	{
		uint32 SeekTableEnd = 0;
		if (ensureMsgf(FStreamedAudioChunkSeekTable::Parse(FirstChunk, ChunkSize, SeekTableEnd, GetCurrentSeekTable()),
			TEXT("Failed to parse seektable in '%s' chunk=%d"), *StreamingSoundWave->GetFName().ToString(), CurrentChunkIndex))
		{
			ChunkSize -= SeekTableEnd;
			FirstChunk += SeekTableEnd;
		}
	}
	
	bIsStreaming = true;
	if (FirstChunk)
	{
		bool HeaderReadResult = ReadCompressedInfo(FirstChunk, ChunkSize, QualityInfo);
		
		// If we've read through the entirety of the zeroth chunk while parsing the header, move onto the next chunk.
		if (SrcBufferOffset >= ChunkSize)
		{
			CurrentChunkIndex++;
			SrcBufferData = NULL;
			SrcBufferDataSize = 0;

			AudioDataChunkIndex = CurrentChunkIndex;
			AudioDataOffset -= ChunkSize;
		}

		return HeaderReadResult;
	}

	return false;
}

bool ICompressedAudioInfo::HasError() const
{
	return bHasError || DECODER_MATCHES_WAVE(AudioDecompressPrivate::ForceDecoderErrorOnWaveCVar);
}

bool IStreamedCompressedInfo::StreamCompressedData(uint8* Destination, bool bLooping, uint32 BufferSize, int32& OutNumBytesStreamed)
{
	check(Destination);
	SCOPED_NAMED_EVENT(IStreamedCompressedInfo_StreamCompressedData, FColor::Blue);

	SCOPE_CYCLE_COUNTER(STAT_AudioStreamedDecompressTime);
	
	UE_LOG(LogAudio, VeryVerbose, TEXT("Streaming compressed data from SoundWave'%s' - Chunk=%d\tCurrentSampleCount=%d\tTrueSampleCount=%d\tNumChunks=%d\tOffset=%d\tChunkSize=%d\tLooping=%s\tLastPCMOffset=%d\tContainsEOF=%s" ), 
		*StreamingSoundWave->GetFName().ToString(), 	
		CurrentChunkIndex, 
		CurrentSampleCount, 
		TrueSampleCount, 
		StreamingSoundWave->GetNumChunks(),
		SrcBufferOffset, 
		SrcBufferDataSize,
		bLooping ? TEXT("YES") : TEXT("NO"),
		LastPCMOffset,	
		bStoringEndOfFile ? TEXT("YES") : TEXT("NO")
	);

	// If we have a pending next chunk from seeking, move to it now.
	if (StreamSeekBlockIndex != INDEX_NONE)
	{
		uint32 ChunkSize = 0;
		const uint8* NewlySeekedChunk = GetLoadedChunk(StreamingSoundWave, StreamSeekBlockIndex, ChunkSize);
		UE_LOG(LogAudio, Log, TEXT("Seek request for (%s): (%s) Chunk=%d (%s), Offset=%d, OffsetInAudioFrames=%u"),
			*StreamingSoundWave->GetFName().ToString(),
			StreamSeekToAudioFrames != INDEX_NONE ? TEXT("Using streaming seek-tables") : TEXT("Using chunk/offset pair"),
			StreamSeekBlockIndex.load(),
			NewlySeekedChunk != nullptr ? TEXT("cache hit") : TEXT("cache miss"),
			StreamSeekBlockOffset,
			StreamSeekToAudioFrames
		);
		
		if (NewlySeekedChunk == nullptr)
		{
			// After a seek we're likely to need to wait a bit for the chunk to get in to memory.
			ZeroBuffer(Destination, BufferSize);
			return false;
		}	

		if (StreamSeekToAudioFrames == INDEX_NONE)
		{
			// Non-streaming-seek tables.
			// Commit the new chunk as the current.
			SrcBufferData = NewlySeekedChunk;
			CurrentChunkIndex = StreamSeekBlockIndex;
			SrcBufferDataSize = ChunkSize;
			SrcBufferOffset = StreamSeekBlockOffset;
		}
		else
		{
			// If we are using a chunked seek-table, we need to parse the table first.
			if (StreamingSoundWave->GetSoundWaveData()->HasChunkSeekTable(CurrentChunkIndex))
			{
				uint32 TableOffset = 0;
				if (ensureMsgf(FStreamedAudioChunkSeekTable::Parse(NewlySeekedChunk, ChunkSize, TableOffset, GetCurrentSeekTable()),
					TEXT("Failed to parse seektable in '%s', chunk=%d"), *StreamingSoundWave->GetFName().ToString(), CurrentChunkIndex))
				{
					if (StreamSeekToAudioFrames != INDEX_NONE)
					{
						const FStreamedAudioChunk& Chunk = StreamingSoundWave->GetSoundWaveData()->GetChunk(CurrentChunkIndex);
						int32 SeekOffsetInChunkSpace = StreamSeekToAudioFrames - Chunk.SeekOffsetInAudioFrames;
						uint32 Offset = GetCurrentSeekTable().FindOffset(StreamSeekToAudioFrames);
						if (Offset != INDEX_NONE)
						{
							// All looks good, commit this as our current chunk.
							CurrentChunkIndex = StreamSeekBlockIndex;
							SrcBufferDataSize = ChunkSize;
							SrcBufferData = NewlySeekedChunk;

							// Set our offset from the table.
							SrcBufferOffset = Offset + TableOffset;
							SrcBufferOffset += CurrentChunkIndex == 0 ? AudioDataOffset : 0;

							// Convert frames to samples and update "current sample count"
							CurrentSampleCount = StreamSeekToAudioFrames * NumChannels;
							LastPCMByteSize = 0;
							LastPCMOffset = 0;
							bStoringEndOfFile = false;
						}
						else // Seek failed (off the end of the chunk).
						{
							const float TimeInSeconds = (float)StreamSeekToAudioFrames / StreamingSoundWave->GetSampleRate();
							UE_LOG(LogAudio, Log, TEXT("Failed seeking to %2.2f seconds as it's off the end of the stream. Wave=%s"), TimeInSeconds, *StreamingSoundWave->GetFName().ToString());
						}
					}
				}
			}
		}

		// Seeking logic over, turn off seeking state
		StreamSeekBlockIndex = INDEX_NONE;
		StreamSeekToAudioFrames = INDEX_NONE;
		StreamSeekBlockOffset = INDEX_NONE;
	}
	
	// If next chunk wasn't loaded when last one finished reading, try to get it again now
	if (SrcBufferData == NULL)
	{
		uint32 ChunkSize = 0;
		SrcBufferData = GetLoadedChunk(StreamingSoundWave, CurrentChunkIndex, ChunkSize);
		if (SrcBufferData)
		{
			SrcBufferDataSize = ChunkSize;
			SrcBufferOffset = CurrentChunkIndex == 0 ? AudioDataOffset : 0;

			// If we are using a chunked seek-table, we need to parse the table first.
			if (StreamingSoundWave->GetSoundWaveData()->HasChunkSeekTable(CurrentChunkIndex))
			{
				ensureMsgf(FStreamedAudioChunkSeekTable::Parse(SrcBufferData, SrcBufferDataSize, SrcBufferOffset, GetCurrentSeekTable()), 
					TEXT("Failed to parse seektable in '%s' chunk=%d"), *StreamingSoundWave->GetFName().ToString(), CurrentChunkIndex );
			}
		}
		else
		{
			// Still not loaded, zero remainder of current buffer.
			// For Chunk 1
			//  For Load on Demand, this is expected, because it will likely wait for the first chunk to load.
			//  For Prime on Load, it could happen if the load is still pending for the First Chunk.
			//  For Retain on Load, it could also happen if the retained data has been purged for the cache. (or we're in the editor)
			// For All other Chunks other than > 1
			//  An underrun (starvation) has occured. Warn the user.
			const ESoundWaveLoadingBehavior Behavior = StreamingSoundWave->GetLoadingBehavior();
			const float TimeSoFar = FPlatformTime::ToSeconds64(FPlatformTime::Cycles64() - StartTimeInCycles);
			if (CurrentChunkIndex == 1)
			{
				switch(Behavior)
				{
				case ESoundWaveLoadingBehavior::RetainOnLoad:
				{	// We don't inline in the editor, so don't warn.
					const bool bIsEditor = GEngine ? GEngine->IsEditor() : false;
					UE_CLOG(!bIsEditor, LogAudioStreamCaching, Verbose, TEXT("First Audio Chunk (%s) not loaded and we are 'RetainOnLoad'. Likely because the stream cache has purged it. FailCount=%d"),
						*StreamingSoundWave->GetFName().ToString(), ++PrintChunkFailMessageCount);
					break;
				}
				case ESoundWaveLoadingBehavior::PrimeOnLoad:
				{
					UE_LOG(LogAudioStreamCaching, Verbose, TEXT("First Audio Chunk (%s) not loaded and we are 'PrimeOnLoad'. Likely because the stream cache is still loading it. Attempt=%d"),
						*StreamingSoundWave->GetFName().ToString(), ++PrintChunkFailMessageCount);
					break;
				}
				case ESoundWaveLoadingBehavior::LoadOnDemand:
				{
					UE_LOG(LogAudioStreamCaching, VeryVerbose, TEXT("First Audio Chunk (%s) not loaded and we are 'LoadOnDemand'. This is expected while we load the first chunk. Attempt=%d"),
						*StreamingSoundWave->GetFName().ToString(), ++PrintChunkFailMessageCount);
					break;
				}
				default:
					break;
				}
			}
			else // If we're mid play back, and we hit this point. We've underrun, log a warning.
			{
				UE_LOG(LogAudioStreamCaching, Warning, TEXT("Chunk %d not yet loaded for playing SoundWave '%s'. Attempt=%d, LoadBehavior=%s, Latency=%2.2f secs, LikelyReason: Not loaded fast enough, IO Saturation." ),
					CurrentChunkIndex, *StreamingSoundWave->GetFName().ToString(), ++PrintChunkFailMessageCount, EnumToString(Behavior), TimeSoFar);
			}
			
			ZeroBuffer(Destination, BufferSize);
			return false;
		}
	}

	bool bLooped = false;
	
	// Write out any PCM data that was decoded during the last request
	uint32 RawPCMOffset = WriteFromDecodedPCM(Destination, BufferSize);
	// immediately update the OutNumBytesStreamed to reflect what we just wrote
	OutNumBytesStreamed = RawPCMOffset;

	// if we were storing the end of the file and just now read to the end (LastPCMByteSize == 0), that means we looped
	if (bStoringEndOfFile && LastPCMByteSize == 0)
	{
		bLooped = true;
		bStoringEndOfFile = false;
	}
	
	while (RawPCMOffset < BufferSize)
	{
		if (HasError())
		{
			// If we've encountered an error, just always write zeroes and don't log anything because we already have
			// and we don't want to spam every tick.
			LastPCMByteSize = 0;
			ZeroBuffer(Destination + RawPCMOffset, BufferSize - RawPCMOffset);

			// We return true here to tell callers that we've completed decoding and should
			// be looped or otherwise terminated.
			return true;
		}

		// Decompress the next compression frame of audio (many samples) into the PCM buffer
		const int32 DecodedFrames = DecompressToPCMBuffer(/*Unused*/ 0);

		if (DecodedFrames < 0)
		{
			FXxHash64 Hash = FXxHash64::HashBuffer(SrcBufferData, SrcBufferDataSize);
			UE_LOG(LogAudioStreamCaching, Warning, TEXT("Decoder error! Zero padding and terminating... Chunk=%d, Wave=%s DecodedFrames=%d SrcBufferOffset=%d SrcBufferDataSize=%d ChunkXxHash64=0x%llx"),
				CurrentChunkIndex, *StreamingSoundWave->GetFName().ToString(), DecodedFrames, SrcBufferOffset, SrcBufferDataSize, Hash.Hash);

			// Flag that the decoder has an unrecoverable error, so we don't try again.
			bHasError = true;

			LastPCMByteSize = 0;
			ZeroBuffer(Destination + RawPCMOffset, BufferSize - RawPCMOffset);
			return false;
		}
		else
		{
			LastPCMByteSize = IncrementCurrentSampleCount(DecodedFrames * NumChannels) * MONO_PCM_SAMPLE_SIZE;

			// update OutNumBytesStreamed as we write out data
			RawPCMOffset += WriteFromDecodedPCM(Destination + RawPCMOffset, BufferSize - RawPCMOffset);
			OutNumBytesStreamed = RawPCMOffset;

			const int32 PreviousChunkIndex = CurrentChunkIndex;

			// Have we reached the end of buffer?
			if (SrcBufferOffset >= SrcBufferDataSize - SrcBufferPadding)
			{
				// Special case for the last chunk of audio
				if (CurrentChunkIndex == StreamingSoundWave->GetNumChunks() - 1)
				{
					// check whether all decoded PCM was written
					if (LastPCMByteSize == 0)
					{
						bLooped = true;
					}
					else
					{
						bStoringEndOfFile = true;
					}

					if (bLooping)
					{
						CurrentChunkIndex = AudioDataChunkIndex;
						SrcBufferOffset = AudioDataOffset;
						CurrentSampleCount = 0;

						// Prepare the decoder to begin looping.
						PrepareToLoop();
					}
					else
					{
						// this is zero padding, so don't include it in the OutNumBytesStreamed count
						ZeroBuffer(Destination + RawPCMOffset, BufferSize - RawPCMOffset);
						break;
					}
				}
				else
				{
					CurrentChunkIndex++;
					SrcBufferOffset = 0;
				}

				SrcBufferData = GetLoadedChunk(StreamingSoundWave, CurrentChunkIndex, SrcBufferDataSize);
				if (SrcBufferData)
				{
					// If we are using a chunked seek-table, we need to parse the table first.
					if (StreamingSoundWave->GetSoundWaveData()->HasChunkSeekTable(CurrentChunkIndex))
					{
						ensureMsgf(FStreamedAudioChunkSeekTable::Parse(SrcBufferData, SrcBufferDataSize, SrcBufferOffset, GetCurrentSeekTable()), 
							TEXT("Failed to parse seektable in '%s' chunk=%d"), *StreamingSoundWave->GetFName().ToString(), CurrentChunkIndex);
					}
					UE_CLOG(PreviousChunkIndex != CurrentChunkIndex, LogAudio, Log, TEXT("Changed current chunk '%s' from %d to %d, Offset %d"),
						*StreamingSoundWave->GetFName().ToString(), PreviousChunkIndex, CurrentChunkIndex, SrcBufferOffset);
				}
				else
				{
					UE_LOG(LogAudioStreamCaching, Warning, TEXT("Chunk %d not yet loaded for playing SoundWave '%s'. LikelyReason: Not loaded fast enough, IO Saturation."),
						CurrentChunkIndex, *StreamingSoundWave->GetFName().ToString());

					SrcBufferDataSize = 0;
					// this is zero padding, so don't include it in the OutNumBytesStreamed count
					ZeroBuffer(Destination + RawPCMOffset, BufferSize - RawPCMOffset);
					break;
				}
			}
		}
	}

	return bLooped;
}

int32 IStreamedCompressedInfo::DecompressToPCMBuffer(uint16 /*Unused*/ )
{
	// At the end of buffer? 
	if (SrcBufferOffset >= SrcBufferDataSize - SrcBufferPadding)
	{
		// Important we say that nothing was decoded and not an error.
		return 0;
	}
		
	// Ask for the frame size only after checking if there's space in the buffer (as the decoders will flag errors if there nothing to read).
	int32 FrameSize = GetFrameSize();
	if (FrameSize <= 0)
	{
		UE_LOG(LogAudio, Warning, TEXT("Decoder error: Frame size negative. (indicates packet error).: Wave='%s', FrameSize=%d, SrcBufferOffset=%u, SrcBufferDataSize=%u"),
			*GetStreamingSoundWave()->GetSoundWaveData()->GetFName().ToString(), FrameSize, SrcBufferOffset, SrcBufferDataSize);

		// Error.
		return -1;
	}
	if (SrcBufferOffset + FrameSize > SrcBufferDataSize)
	{
		UE_LOG(LogAudio, Warning, TEXT("Decoder error: Frame size too large (decoding it will take us out of bounds).: Wave='%s', FrameSize=%d, SrcBufferOffset=%u, SrcBufferDataSize=%u, Overby=%d"), 
			*GetStreamingSoundWave()->GetSoundWaveData()->GetFName().ToString(), FrameSize, SrcBufferOffset, SrcBufferDataSize, (int32)SrcBufferOffset+FrameSize-SrcBufferDataSize );
		
		// if frame size is too large, something has gone wrong
		return -1;
	}

	const uint8* SrcPtr = SrcBufferData + SrcBufferOffset;
	SrcBufferOffset += FrameSize;
	LastPCMOffset = 0;
	
	const FDecodeResult DecodeResult = Decode(SrcPtr, FrameSize, LastDecodedPCM.GetData(), LastDecodedPCM.Num());
	
	if (DecodeResult.NumCompressedBytesConsumed == INDEX_NONE || DECODER_MATCHES_WAVE(AudioDecompressPrivate::ForceDecoderNegativeSamplesOnWaveCVar))
	{
		UE_LOG(LogAudio, Warning, TEXT("Decoder error: Decode call returned INDEX_NONE which indicates an error. : Wave='%s', FrameSize=%d, SrcBufferOffset=%u, SrcBufferDataSize=%u"), 
			*GetStreamingSoundWave()->GetSoundWaveData()->GetFName().ToString(), FrameSize, SrcBufferOffset, SrcBufferDataSize);

		// Error.
		return -1;
	}

	SrcBufferOffset -= FrameSize;
	SrcBufferOffset += DecodeResult.NumCompressedBytesConsumed;
	return DecodeResult.NumAudioFramesProduced;
}

uint32 IStreamedCompressedInfo::IncrementCurrentSampleCount(uint32 NewSamples)
{
	if (CurrentSampleCount + NewSamples > TrueSampleCount)
	{
		NewSamples = TrueSampleCount - CurrentSampleCount;
		CurrentSampleCount = TrueSampleCount;
	}
	else
	{
		CurrentSampleCount += NewSamples;
	}
	return NewSamples;
}

uint32 IStreamedCompressedInfo::WriteFromDecodedPCM(uint8* Destination, uint32 BufferSize)
{
	// logical number of bytes we need to copy
	uint32 BytesToCopy = FMath::Min(BufferSize, LastPCMByteSize - LastPCMOffset);

	// make sure we aren't reading off the end of LastDecodedPCM
	BytesToCopy = FMath::Min(BytesToCopy, LastDecodedPCM.Num() - LastPCMOffset);

	if (BytesToCopy > 0)
	{
		check(BytesToCopy <= LastDecodedPCM.Num() - LastPCMOffset);
		FMemory::Memcpy(Destination, LastDecodedPCM.GetData() + LastPCMOffset, BytesToCopy);
		LastPCMOffset += BytesToCopy;
		if (LastPCMOffset >= LastPCMByteSize)
		{
			LastPCMOffset = 0;
			LastPCMByteSize = 0;
		}
	}
	return BytesToCopy;
}

uint32 IStreamedCompressedInfo::ZeroBuffer(uint8* Destination, uint32 BufferSize)
{
	check(Destination);

	if (BufferSize > 0)
	{
		FMemory::Memzero(Destination, BufferSize);
		return BufferSize;
	}
	return 0;
}


const uint8* IStreamedCompressedInfo::GetLoadedChunk(const FSoundWaveProxyPtr& InSoundWave, uint32 ChunkIndex, uint32& OutChunkSize)
{
	if (!ensure(InSoundWave.IsValid()))
	{
		OutChunkSize = 0;
		return nullptr;
	}
	else if (ChunkIndex >= InSoundWave->GetNumChunks())
	{
		OutChunkSize = 0;
		return nullptr;
	}
	else if (ChunkIndex == 0)
	{
		TArrayView<const uint8> ZerothChunk = FSoundWaveProxy::GetZerothChunk(InSoundWave, true);
		OutChunkSize = ZerothChunk.Num();
		return ZerothChunk.GetData();
	}
	else
	{
		CurCompressedChunkHandle = IStreamingManager::Get().GetAudioStreamingManager().GetLoadedChunk(InSoundWave, ChunkIndex, false, true);
		OutChunkSize = CurCompressedChunkHandle.Num();
		return CurCompressedChunkHandle.GetData();
	}
}

FStreamedAudioChunkSeekTable& IStreamedCompressedInfo::GetCurrentSeekTable()
{
	if (!CurrentChunkSeekTable.IsValid())
	{
		CurrentChunkSeekTable = MakePimpl<FStreamedAudioChunkSeekTable>();
	}
	return *CurrentChunkSeekTable;
}

const FStreamedAudioChunkSeekTable& IStreamedCompressedInfo::GetCurrentSeekTable() const
{
	return const_cast<IStreamedCompressedInfo*>(this)->GetCurrentSeekTable();
}

/**
 * Worker for decompression on a separate thread
 */
FAsyncAudioDecompressWorker::FAsyncAudioDecompressWorker(USoundWave* InWave, int32 InPrecacheBufferNumFrames, FAudioDevice* InAudioDevice)
	: Wave(InWave)
	, AudioInfo(nullptr)
	, NumPrecacheFrames(InPrecacheBufferNumFrames)
{
	check(NumPrecacheFrames > 0);


	AudioInfo = IAudioInfoFactoryRegistry::Get().Create(InWave->GetRuntimeFormat());
}

void FAsyncAudioDecompressWorker::DoWork()
{
	LLM_SCOPE(ELLMTag::AudioDecompress);

	if (AudioInfo)
	{
		FSoundQualityInfo QualityInfo = { 0 };

		// Parse the audio header for the relevant information
		if (AudioInfo->ReadCompressedInfo(Wave->GetResourceData(), Wave->GetResourceSize(), &QualityInfo))
		{
			FScopeCycleCounterUObject WaveObject( Wave );

#if PLATFORM_ANDROID
			// Handle resampling
			if (QualityInfo.SampleRate > 48000)
			{
				UE_LOG( LogAudio, Warning, TEXT("Resampling file %s from %d"), *(Wave->GetName()), QualityInfo.SampleRate);
				UE_LOG( LogAudio, Warning, TEXT("  Size %d"), QualityInfo.SampleDataSize);
				uint32 SampleCount = QualityInfo.SampleDataSize / (QualityInfo.NumChannels * sizeof(uint16));
				QualityInfo.SampleRate = QualityInfo.SampleRate / 2;
				SampleCount /= 2;
				QualityInfo.SampleDataSize = SampleCount * QualityInfo.NumChannels * sizeof(uint16);
				AudioInfo->EnableHalfRate(true);
			}
#endif

			Wave->SetSampleRate(QualityInfo.SampleRate);
			Wave->NumChannels = QualityInfo.NumChannels;
			if (QualityInfo.Duration > 0.0f)
			{ 
				Wave->Duration = QualityInfo.Duration;
			}

			if (Wave->DecompressionType == DTYPE_RealTime)
			{
				LLM_SCOPE(ELLMTag::AudioRealtimePrecache);
#if PLATFORM_NUM_AUDIODECOMPRESSION_PRECACHE_BUFFERS > 0
				const uint32 PCMBufferSize = NumPrecacheFrames * MONO_PCM_SAMPLE_SIZE * Wave->NumChannels * PLATFORM_NUM_AUDIODECOMPRESSION_PRECACHE_BUFFERS;
				Wave->NumPrecacheFrames = NumPrecacheFrames;
				if (Wave->CachedRealtimeFirstBuffer == nullptr)
				{
					Wave->CachedRealtimeFirstBuffer = (uint8*)FMemory::Malloc(PCMBufferSize);
					AudioInfo->ReadCompressedData(Wave->CachedRealtimeFirstBuffer, Wave->bLooping, PCMBufferSize);
				}
				else if (Wave->GetPrecacheState() == ESoundWavePrecacheState::Done)
				{
					UE_LOG(LogAudio, Warning, TEXT("Attempted to precache decoded audio multiple times."));
				}
#endif
			}
			else
			{
				LLM_SCOPE(ELLMTag::AudioFullDecompress);
				check(Wave->DecompressionType == DTYPE_Native || Wave->DecompressionType == DTYPE_Procedural);

				Wave->RawPCMDataSize = QualityInfo.SampleDataSize;
				check(Wave->RawPCMData == nullptr);
				Wave->RawPCMData = ( uint8* )FMemory::Malloc( Wave->RawPCMDataSize );

				// Decompress all the sample data into preallocated memory
				AudioInfo->ExpandFile(Wave->RawPCMData, &QualityInfo);

				// Only track the RawPCMDataSize at this point since we haven't yet
				// removed the compressed asset from memory and GetResourceSize() would add that at this point
				Wave->TrackedMemoryUsage += Wave->RawPCMDataSize;
				INC_DWORD_STAT_BY(STAT_AudioMemorySize, Wave->RawPCMDataSize);
				INC_DWORD_STAT_BY(STAT_AudioMemory, Wave->RawPCMDataSize);
			}
		}
		else if (Wave->DecompressionType == DTYPE_RealTime)
		{
			Wave->DecompressionType = DTYPE_Invalid;
			Wave->NumChannels = 0;

			Wave->RemoveAudioResource();
		}

		if (Wave->DecompressionType == DTYPE_Native)
		{
			// todo: this code is stale and needs to be gutted since going all-in on stream caching,
			// but for now, GetOwnedBulkData() is being removed as a function.
			// FOwnedBulkDataPtr* BulkDataPtr = Wave->GetOwnedBulkData();
			FOwnedBulkDataPtr* BulkDataPtr = nullptr;
			UE_CLOG(BulkDataPtr && BulkDataPtr->GetMappedRegion(), LogAudio, Warning, TEXT("Mapped audio (%s) was discarded after decompression. This is not ideal as it takes more load time and doesn't save memory."), *Wave->GetName());

			// Delete the compressed data
			Wave->RemoveAudioResource();
		}

		delete AudioInfo;
	}
}

static TAutoConsoleVariable<int32> CVarShouldUseBackgroundPoolFor_FAsyncRealtimeAudioTask(
	TEXT("AudioThread.UseBackgroundThreadPool"),
	1,
	TEXT("If true, use the background thread pool for realtime audio decompression."));

bool ShouldUseBackgroundPoolFor_FAsyncRealtimeAudioTask()
{
	return !!CVarShouldUseBackgroundPoolFor_FAsyncRealtimeAudioTask.GetValueOnAnyThread();
}

// end

bool ICompressedAudioInfo::StreamCompressedInfo(USoundWave* Wave, FSoundQualityInfo* QualityInfo)
{
	if (!Wave)
	{
		return false;
	}

	// Create and cache proxy object
	StreamingSoundWave = Wave->CreateSoundWaveProxy();
	if (!StreamingSoundWave.IsValid())
	{
		return false;
	}

	return StreamCompressedInfoInternal(StreamingSoundWave, QualityInfo);
}

bool ICompressedAudioInfo::StreamCompressedInfo(const FSoundWaveProxyPtr& Wave, FSoundQualityInfo* QualityInfo)
{
	// Create our own copy of the proxy object
	StreamingSoundWave = Wave;
	return StreamCompressedInfoInternal(StreamingSoundWave, QualityInfo);
}

IAudioInfoFactoryRegistry& IAudioInfoFactoryRegistry::Get()
{
	static struct FConcreteRegistry : IAudioInfoFactoryRegistry
	{
		mutable FRWLock FactoriesRWLock;
		TMap<FName, IAudioInfoFactory*> Factories;

		void Register(IAudioInfoFactory* InFactory, FName InFormatName) override
		{
			FRWScopeLock Lock(FactoriesRWLock, FRWScopeLockType::SLT_Write);
			UE_LOG(LogAudio, Display, TEXT("AudioInfo: '%s' Registered"), *InFormatName.ToString());
			check(!Factories.Contains(InFormatName));
			Factories.Add(InFormatName) = InFactory;
		}
		void Unregister(IAudioInfoFactory* InFactory, FName InFormatName) override
		{
			FRWScopeLock Lock(FactoriesRWLock, FRWScopeLockType::SLT_Write);

			Factories.Remove(InFormatName);
		}
		IAudioInfoFactory* Find(FName InFormat) const override
		{
			FRWScopeLock Lock(FactoriesRWLock, FRWScopeLockType::SLT_ReadOnly);
			if( IAudioInfoFactory* const* Factory = Factories.Find(InFormat))
			{
				return *Factory;
			}
			return nullptr;
		}
	} sInstance;
	return sInstance;
}

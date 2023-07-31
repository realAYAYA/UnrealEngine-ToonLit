// Copyright Epic Games, Inc. All Rights Reserved.


#include "AudioDecompress.h"
#include "AudioDevice.h"
#include "Interfaces/IAudioFormat.h"
#include "ContentStreaming.h"
#include "HAL/LowLevelMemTracker.h"
#include "ADPCMAudioInfo.h"

IStreamedCompressedInfo::IStreamedCompressedInfo()
	: bIsStreaming(false)
	,SrcBufferData(nullptr)
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
	, bPrintChunkFailMessage(true)
	, SrcBufferPadding(0)
	, StreamSeekBlockIndex(INDEX_NONE)
	, StreamSeekBlockOffset(0)
{
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
	SampleStride = NumChannels * sizeof(int16);

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

void IStreamedCompressedInfo::ExpandFile(uint8* DstBuffer, struct FSoundQualityInfo* QualityInfo)
{
	check(DstBuffer);
	check(QualityInfo);

	// Ensure we're at the start of the audio data
	SrcBufferOffset = AudioDataOffset;

	uint32 RawPCMOffset = 0;

	while (RawPCMOffset < QualityInfo->SampleDataSize)
	{
		int32 DecodedSamples = DecompressToPCMBuffer( /*Unused*/ 0);

		if (DecodedSamples < 0)
		{
			RawPCMOffset += ZeroBuffer(DstBuffer + RawPCMOffset, QualityInfo->SampleDataSize - RawPCMOffset);
		}
		else
		{
			LastPCMByteSize = IncrementCurrentSampleCount(DecodedSamples) * SampleStride;
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

	// Get the first chunk of audio data (should always be loaded)
	CurrentChunkIndex = 0;
	uint32 ChunkSize = 0;
	const uint8* FirstChunk = GetLoadedChunk(StreamingSoundWave, CurrentChunkIndex, ChunkSize);

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

bool IStreamedCompressedInfo::StreamCompressedData(uint8* Destination, bool bLooping, uint32 BufferSize, int32& OutNumBytesStreamed)
{
	check(Destination);
	SCOPED_NAMED_EVENT(IStreamedCompressedInfo_StreamCompressedData, FColor::Blue);

	SCOPE_CYCLE_COUNTER(STAT_AudioStreamedDecompressTime);


	UE_LOG(LogAudio, Log, TEXT("Streaming compressed data from SoundWave'%s' - Chunk=%d\tNumChunks=%d\tOffset=%d\tChunkSize=%d\tLooping=%s\tLastPCMOffset=%d\tContainsEOF=%s" ), 
		*StreamingSoundWave->GetFName().ToString(), 	
		CurrentChunkIndex, 
		StreamingSoundWave->GetNumChunks(),
		SrcBufferOffset, 
		SrcBufferDataSize,
		bLooping ? TEXT("YES") : TEXT("NO"),
		LastPCMOffset,		
		bStoringEndOfFile ? TEXT("YES") : TEXT("NO")
	);

	// Write out any PCM data that was decoded during the last request
	uint32 RawPCMOffset = WriteFromDecodedPCM(Destination, BufferSize);

	// If we have a pending next chunk from seeking, move to it now.
	if (StreamSeekBlockIndex != INDEX_NONE)
	{
		uint32 ChunkSize = 0;
		SrcBufferData = GetLoadedChunk(StreamingSoundWave, StreamSeekBlockIndex, ChunkSize);
		UE_LOG(LogAudio, Log, TEXT("Seek block request: %d / %d (%s)"), StreamSeekBlockIndex.load(), StreamSeekBlockOffset, SrcBufferData == nullptr ? TEXT("present") : TEXT("missing"));
		if (SrcBufferData == nullptr)
		{
			// After a seek we're likely to need to wait a bit for the chunk to get in to memory.
			ZeroBuffer(Destination + RawPCMOffset, BufferSize - RawPCMOffset);
			return false;
		}

		CurrentChunkIndex = StreamSeekBlockIndex;
		SrcBufferDataSize = ChunkSize;
		SrcBufferOffset = StreamSeekBlockOffset;
		StreamSeekBlockIndex = INDEX_NONE;
	}

	// If next chunk wasn't loaded when last one finished reading, try to get it again now
	if (SrcBufferData == NULL)
	{
		uint32 ChunkSize = 0;
		SrcBufferData = GetLoadedChunk(StreamingSoundWave, CurrentChunkIndex, ChunkSize);
		if (SrcBufferData)
		{
			bPrintChunkFailMessage = true;
			SrcBufferDataSize = ChunkSize;
			SrcBufferOffset = CurrentChunkIndex == 0 ? AudioDataOffset : 0;
		}
		else
		{
			// Still not loaded, zero remainder of current buffer
			if (bPrintChunkFailMessage)
			{
				UE_LOG(LogAudio, Verbose, TEXT("Chunk %d not loaded from streaming manager for SoundWave '%s'. Likely due to stall on game thread."), CurrentChunkIndex, *StreamingSoundWave->GetFName().ToString());
				bPrintChunkFailMessage = false;
			}
			ZeroBuffer(Destination + RawPCMOffset, BufferSize - RawPCMOffset);
			OutNumBytesStreamed = BufferSize - RawPCMOffset;
			return false;
		}
	}

	bool bLooped = false;

	if (bStoringEndOfFile && LastPCMByteSize > 0)
	{
		// delayed returning looped because we hadn't read the entire buffer
		bLooped = true;
		bStoringEndOfFile = false;
	}

	while (RawPCMOffset < BufferSize)
	{
		// Decompress the next compression frame of audio (many samples) into the PCM buffer
		int32 DecodedSamples = DecompressToPCMBuffer(/*Unused*/ 0);


		if (DecodedSamples < 0)
		{
			LastPCMByteSize = 0;
			ZeroBuffer(Destination + RawPCMOffset, BufferSize - RawPCMOffset);
			OutNumBytesStreamed = BufferSize - RawPCMOffset;
			return false;
		}
		else
		{
			LastPCMByteSize = IncrementCurrentSampleCount(DecodedSamples) * SampleStride;

			RawPCMOffset += WriteFromDecodedPCM(Destination + RawPCMOffset, BufferSize - RawPCMOffset);

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
						RawPCMOffset += ZeroBuffer(Destination + RawPCMOffset, BufferSize - RawPCMOffset);
						OutNumBytesStreamed = BufferSize - RawPCMOffset;
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
					UE_CLOG(PreviousChunkIndex != CurrentChunkIndex, LogAudio, Log, TEXT("Changed current chunk '%s' from %d to %d, Offset %d"), *StreamingSoundWave->GetFName().ToString(), PreviousChunkIndex, CurrentChunkIndex, SrcBufferOffset);
				}
				else
				{
					SrcBufferDataSize = 0;
					RawPCMOffset += ZeroBuffer(Destination + RawPCMOffset, BufferSize - RawPCMOffset);
				}
			}
		}
	}

	OutNumBytesStreamed = BufferSize;
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
		// Error.
		return -1;
	}
	if (SrcBufferOffset + FrameSize > SrcBufferDataSize)
	{
		// if frame size is too large, something has gone wrong
		return -1;
	}

	const uint8* SrcPtr = SrcBufferData + SrcBufferOffset;
	SrcBufferOffset += FrameSize;
	LastPCMOffset = 0;
	
	const FDecodeResult DecodeResult = Decode(SrcPtr, FrameSize, LastDecodedPCM.GetData(), LastDecodedPCM.Num());
	if (DecodeResult.NumCompressedBytesConsumed != INDEX_NONE )
	{
		SrcBufferOffset -= FrameSize;
		SrcBufferOffset += DecodeResult.NumCompressedBytesConsumed;
	}

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

//////////////////////////////////////////////////////////////////////////
// Copied from IOS - probably want to split and share
//////////////////////////////////////////////////////////////////////////
#define NUM_ADAPTATION_TABLE 16
#define NUM_ADAPTATION_COEFF 7
#define SOUND_SOURCE_FREE 0
#define SOUND_SOURCE_LOCKED 1

namespace
{
	template <typename T, uint32 B>
	inline T SignExtend(const T ValueToExtend)
	{
		struct { T ExtendedValue : B; } SignExtender;
		return SignExtender.ExtendedValue = ValueToExtend;
	}

	template <typename T>
	inline T ReadFromByteStream(const uint8* ByteStream, int32& ReadIndex, bool bLittleEndian = true)
	{
		T ValueRaw = 0;

		if (bLittleEndian)
		{
#if PLATFORM_LITTLE_ENDIAN
			for (int32 ByteIndex = 0; ByteIndex < sizeof(T); ++ByteIndex)
#else
			for (int32 ByteIndex = sizeof(T) - 1; ByteIndex >= 0; --ByteIndex)
#endif // PLATFORM_LITTLE_ENDIAN
			{
				ValueRaw |= ByteStream[ReadIndex++] << 8 * ByteIndex;
			}
		}
		else
		{
#if PLATFORM_LITTLE_ENDIAN
			for (int32 ByteIndex = sizeof(T) - 1; ByteIndex >= 0; --ByteIndex)
#else
			for (int32 ByteIndex = 0; ByteIndex < sizeof(T); ++ByteIndex)
#endif // PLATFORM_LITTLE_ENDIAN
			{
				ValueRaw |= ByteStream[ReadIndex++] << 8 * ByteIndex;
			}
		}

		return ValueRaw;
	}

	template <typename T>
	inline void WriteToByteStream(T Value, uint8* ByteStream, int32& WriteIndex, bool bLittleEndian = true)
	{
		if (bLittleEndian)
		{
#if PLATFORM_LITTLE_ENDIAN
			for (int32 ByteIndex = 0; ByteIndex < sizeof(T); ++ByteIndex)
#else
			for (int32 ByteIndex = sizeof(T) - 1; ByteIndex >= 0; --ByteIndex)
#endif // PLATFORM_LITTLE_ENDIAN
			{
				ByteStream[WriteIndex++] = (Value >> (8 * ByteIndex)) & 0xFF;
			}
		}
		else
		{
#if PLATFORM_LITTLE_ENDIAN
			for (int32 ByteIndex = sizeof(T) - 1; ByteIndex >= 0; --ByteIndex)
#else
			for (int32 ByteIndex = 0; ByteIndex < sizeof(T); ++ByteIndex)
#endif // PLATFORM_LITTLE_ENDIAN
			{
				ByteStream[WriteIndex++] = (Value >> (8 * ByteIndex)) & 0xFF;
			}
		}
	}

	template <typename T>
	inline T ReadFromArray(const T* ElementArray, int32& ReadIndex, int32 NumElements, int32 IndexStride = 1)
	{
		T OutputValue = 0;

		if (ReadIndex >= 0 && ReadIndex < NumElements)
		{
			OutputValue = ElementArray[ReadIndex];
			ReadIndex += IndexStride;
		}

		return OutputValue;
	}

	inline bool LockSourceChannel(int32* ChannelLock)
	{
		check(ChannelLock != NULL);
		return FPlatformAtomics::InterlockedCompareExchange(ChannelLock, SOUND_SOURCE_LOCKED, SOUND_SOURCE_FREE) == SOUND_SOURCE_FREE;
	}

	inline void UnlockSourceChannel(int32* ChannelLock)
	{
		check(ChannelLock != NULL);
		int32 Result = FPlatformAtomics::InterlockedCompareExchange(ChannelLock, SOUND_SOURCE_FREE, SOUND_SOURCE_LOCKED);

		check(Result == SOUND_SOURCE_LOCKED);
	}

} // end namespace

namespace ADPCM
{
	template <typename T>
	static void GetAdaptationTable(T(&OutAdaptationTable)[NUM_ADAPTATION_TABLE])
	{
		// Magic values as specified by standard
		static T AdaptationTable[] =
		{
			230, 230, 230, 230, 307, 409, 512, 614,
			768, 614, 512, 409, 307, 230, 230, 230
		};

		FMemory::Memcpy(&OutAdaptationTable, AdaptationTable, sizeof(AdaptationTable));
	}

	struct FAdaptationContext
	{
	public:
		// Adaptation constants
		int32 AdaptationTable[NUM_ADAPTATION_TABLE];
		int32 AdaptationCoefficient1[NUM_ADAPTATION_COEFF];
		int32 AdaptationCoefficient2[NUM_ADAPTATION_COEFF];

		int32 AdaptationDelta;
		int32 Coefficient1;
		int32 Coefficient2;
		int32 Sample1;
		int32 Sample2;

		FAdaptationContext() :
			AdaptationDelta(0),
			Coefficient1(0),
			Coefficient2(0),
			Sample1(0),
			Sample2(0)
		{
			GetAdaptationTable(AdaptationTable);
			GetAdaptationCoefficients(AdaptationCoefficient1, AdaptationCoefficient2);
		}
	};

	FORCEINLINE int16 DecodeNibble(FAdaptationContext& Context, uint8 EncodedNibble)
	{
		int32 PredictedSample = (Context.Sample1 * Context.Coefficient1 + Context.Sample2 * Context.Coefficient2) / 256;
		PredictedSample += SignExtend<int8, 4>(EncodedNibble) * Context.AdaptationDelta;
		PredictedSample = FMath::Clamp(PredictedSample, -32768, 32767);

		// Shuffle samples for the next iteration
		Context.Sample2 = Context.Sample1;
		Context.Sample1 = static_cast<int16>(PredictedSample);
		Context.AdaptationDelta = (Context.AdaptationDelta * Context.AdaptationTable[EncodedNibble]) / 256;
		Context.AdaptationDelta = FMath::Max(Context.AdaptationDelta, 16);

		return Context.Sample1;
	}

	void DecodeBlock(const uint8* EncodedADPCMBlock, int32 BlockSize, int16* DecodedPCMData)
	{
		FAdaptationContext Context;
		int32 ReadIndex = 0;
		int32 WriteIndex = 0;

		uint8 CoefficientIndex = ReadFromByteStream<uint8>(EncodedADPCMBlock, ReadIndex);
		Context.AdaptationDelta = ReadFromByteStream<int16>(EncodedADPCMBlock, ReadIndex);
		Context.Sample1 = ReadFromByteStream<int16>(EncodedADPCMBlock, ReadIndex);
		Context.Sample2 = ReadFromByteStream<int16>(EncodedADPCMBlock, ReadIndex);
		Context.Coefficient1 = Context.AdaptationCoefficient1[CoefficientIndex];
		Context.Coefficient2 = Context.AdaptationCoefficient2[CoefficientIndex];

		// The first two samples are sent directly to the output in reverse order, as per the standard
		DecodedPCMData[WriteIndex++] = Context.Sample2;
		DecodedPCMData[WriteIndex++] = Context.Sample1;

		uint8 EncodedNibblePair = 0;
		uint8 EncodedNibble = 0;
		while (ReadIndex < BlockSize)
		{
			// Read from the byte stream and advance the read head.
			EncodedNibblePair = ReadFromByteStream<uint8>(EncodedADPCMBlock, ReadIndex);

			EncodedNibble = (EncodedNibblePair >> 4) & 0x0F;
			DecodedPCMData[WriteIndex++] = DecodeNibble(Context, EncodedNibble);

			EncodedNibble = EncodedNibblePair & 0x0F;
			DecodedPCMData[WriteIndex++] = DecodeNibble(Context, EncodedNibble);
		}
	}

	// Decode two PCM streams and interleave as stereo data
	void DecodeBlockStereo(const uint8* EncodedADPCMBlockLeft, const uint8* EncodedADPCMBlockRight, int32 BlockSize, int16* DecodedPCMData)
	{
		FAdaptationContext ContextLeft;
		FAdaptationContext ContextRight;
		int32 ReadIndexLeft = 0;
		int32 ReadIndexRight = 0;
		int32 WriteIndex = 0;

		uint8 CoefficientIndexLeft = ReadFromByteStream<uint8>(EncodedADPCMBlockLeft, ReadIndexLeft);
		ContextLeft.AdaptationDelta = ReadFromByteStream<int16>(EncodedADPCMBlockLeft, ReadIndexLeft);
		ContextLeft.Sample1 = ReadFromByteStream<int16>(EncodedADPCMBlockLeft, ReadIndexLeft);
		ContextLeft.Sample2 = ReadFromByteStream<int16>(EncodedADPCMBlockLeft, ReadIndexLeft);
		ContextLeft.Coefficient1 = ContextLeft.AdaptationCoefficient1[CoefficientIndexLeft];
		ContextLeft.Coefficient2 = ContextLeft.AdaptationCoefficient2[CoefficientIndexLeft];

		uint8 CoefficientIndexRight = ReadFromByteStream<uint8>(EncodedADPCMBlockRight, ReadIndexRight);
		ContextRight.AdaptationDelta = ReadFromByteStream<int16>(EncodedADPCMBlockRight, ReadIndexRight);
		ContextRight.Sample1 = ReadFromByteStream<int16>(EncodedADPCMBlockRight, ReadIndexRight);
		ContextRight.Sample2 = ReadFromByteStream<int16>(EncodedADPCMBlockRight, ReadIndexRight);
		ContextRight.Coefficient1 = ContextRight.AdaptationCoefficient1[CoefficientIndexRight];
		ContextRight.Coefficient2 = ContextRight.AdaptationCoefficient2[CoefficientIndexRight];

		// The first two samples from each stream are sent directly to the output in reverse order, as per the standard
		DecodedPCMData[WriteIndex++] = ContextLeft.Sample2;
		DecodedPCMData[WriteIndex++] = ContextRight.Sample2;
		DecodedPCMData[WriteIndex++] = ContextLeft.Sample1;
		DecodedPCMData[WriteIndex++] = ContextRight.Sample1;

		uint8 EncodedNibblePairLeft = 0;
		uint8 EncodedNibbleLeft = 0;
		uint8 EncodedNibblePairRight = 0;
		uint8 EncodedNibbleRight = 0;

		while (ReadIndexLeft < BlockSize)
		{
			// Read from the byte stream and advance the read head.
			EncodedNibblePairLeft = ReadFromByteStream<uint8>(EncodedADPCMBlockLeft, ReadIndexLeft);
			EncodedNibblePairRight = ReadFromByteStream<uint8>(EncodedADPCMBlockRight, ReadIndexRight);

			EncodedNibbleLeft = (EncodedNibblePairLeft >> 4) & 0x0F;
			DecodedPCMData[WriteIndex++] = DecodeNibble(ContextLeft, EncodedNibbleLeft);

			EncodedNibbleRight = (EncodedNibblePairRight >> 4) & 0x0F;
			DecodedPCMData[WriteIndex++] = DecodeNibble(ContextRight, EncodedNibbleRight);

			EncodedNibbleLeft = EncodedNibblePairLeft & 0x0F;
			DecodedPCMData[WriteIndex++] = DecodeNibble(ContextLeft, EncodedNibbleLeft);

			EncodedNibbleRight = EncodedNibblePairRight & 0x0F;
			DecodedPCMData[WriteIndex++] = DecodeNibble(ContextRight, EncodedNibbleRight);
		}

	}
} // end namespace ADPCM

//////////////////////////////////////////////////////////////////////////
// End of copied section
//////////////////////////////////////////////////////////////////////////

/**
 * Worker for decompression on a separate thread
 */
FAsyncAudioDecompressWorker::FAsyncAudioDecompressWorker(USoundWave* InWave, int32 InPrecacheBufferNumFrames, FAudioDevice* InAudioDevice)
	: Wave(InWave)
	, AudioInfo(nullptr)
	, NumPrecacheFrames(InPrecacheBufferNumFrames)
{
	check(NumPrecacheFrames > 0);
	if (InAudioDevice)
	{
		AudioInfo = InAudioDevice->CreateCompressedAudioInfo(Wave);
	}
	else if (GEngine && GEngine->GetMainAudioDevice())
	{
		// alternatively, we could expose InWave::InternalProxy via public getter to avoid the proxy creation
		AudioInfo = GEngine->GetMainAudioDevice()->CreateCompressedAudioInfo(InWave->CreateSoundWaveProxy());
	}
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
				const uint32 PCMBufferSize = NumPrecacheFrames * sizeof(int16) * Wave->NumChannels * PLATFORM_NUM_AUDIODECOMPRESSION_PRECACHE_BUFFERS;
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

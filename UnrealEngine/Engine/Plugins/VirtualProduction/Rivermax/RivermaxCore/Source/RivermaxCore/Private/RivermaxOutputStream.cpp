// Copyright Epic Games, Inc. All Rights Reserved.

#include "RivermaxOutputStream.h"

#include "Async/Async.h"
#include "IRivermaxCoreModule.h"
#include "IRivermaxManager.h"
#include "Misc/ByteSwap.h"
#include "RivermaxLog.h"
#include "RivermaxPTPUtils.h"
#include "RivermaxTypes.h"
#include "RivermaxUtils.h"


namespace UE::RivermaxCore::Private
{
	static TAutoConsoleVariable<int32> CVarRivermaxWakeupOffset(
		TEXT("Rivermax.WakeupOffset"), 0,
		TEXT("Wakeup is done on alignment point. This offset will be substracted from it to wake up earlier. Units: nanoseconds"),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRivermaxScheduleOffset(
		TEXT("Rivermax.ScheduleOffset"), 0,
		TEXT("Scheduling is done at alignment point plus TRO. This offset will be added to it to delay or schedule earlier. Units: nanoseconds"),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRivermaxOutputShowStats(
		TEXT("Rivermax.ShowOutputStats"), 0,
		TEXT("Enable stats logging at fixed interval"),
		ECVF_Default);

	static TAutoConsoleVariable<float> CVarRivermaxOutputShowStatsInterval(
		TEXT("Rivermax.ShowStatsInterval"), 1.0,
		TEXT("Interval at which to show stats in seconds"),
		ECVF_Default);

	bool FindPayloadSize(const FRivermaxStreamOptions& InOptions, uint32 InBytesPerLine, const FVideoFormatInfo& FormatInfo, uint16& OutPayloadSize)
	{
		using namespace UE::RivermaxCore::Private::Utils;

		// For now, we only find a payload size that can be equal across one line
		// Support for last payload of a line being smaller is there but is causing issue
		// We fail to output if we receive a resolution for which we can't find an equal payload size
		int32 TestPoint = InBytesPerLine / MaxPayloadSize;
		if (TestPoint == 0)
		{
			if (InBytesPerLine > MinPayloadSize)
			{
				if (InBytesPerLine % FormatInfo.PixelGroupSize == 0)
				{
					OutPayloadSize = InBytesPerLine;
					return true;
				}
			}
			return false;
		}

		while (true)
		{
			const int32 TestSize = InBytesPerLine / TestPoint;
			if (TestSize < MinPayloadSize)
			{
				break;
			}

			if (TestSize <= MaxPayloadSize)
			{
				if ((TestSize % FormatInfo.PixelGroupSize) == 0 && (InBytesPerLine % TestPoint) == 0)
				{
					OutPayloadSize = TestSize;
					return true;
				}
			}

			++TestPoint;
		}

		return false;
	}

	uint32 FindChunksPerLine(const FRivermaxStreamOptions& InOptions)
	{
		uint32 ChunksPerLine = 1; //Will need to revisit the impact of that
		static constexpr uint32 MaxChunksPerLine = 10;
		while (ChunksPerLine <= MaxChunksPerLine)
		{
			if (InOptions.Resolution.Y % ChunksPerLine == 0)
			{
				return ChunksPerLine;
			}

			++ChunksPerLine;
		}

		return 0;
	}


	FRivermaxOutputStream::FRivermaxOutputStream()
		: bIsActive(false)
	{

	}

	FRivermaxOutputStream::~FRivermaxOutputStream()
	{
		Uninitialize();
	}

	bool FRivermaxOutputStream::Initialize(const FRivermaxStreamOptions& InOptions, IRivermaxOutputStreamListener& InListener)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FRivermaxOutputStream::Initialize);

		IRivermaxCoreModule& RivermaxModule = FModuleManager::LoadModuleChecked<IRivermaxCoreModule>(TEXT("RivermaxCore"));
		if (RivermaxModule.GetRivermaxManager()->IsInitialized() == false)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Can't create Rivermax Output Stream. Library isn't initialized."));
			return false;
		}

		Options = InOptions;
		Listener = &InListener;
		FormatInfo = FStandardVideoFormat::GetVideoFormatInfo(Options.PixelFormat);

		// Verify resolution for sampling type
		if (Options.AlignedResolution.X % FormatInfo.PixelGroupCoverage != 0)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Can't create Rivermax Output Stream. Aligned horizontal resolution of %d doesn't align with pixel group coverage of %d."), Options.AlignedResolution.X, FormatInfo.PixelGroupCoverage);
			return false;
		}

		// We do (try) to make gpu allocations here to let the capturer know if we require it or not.
		if (RivermaxModule.GetRivermaxManager()->IsGPUDirectSupported() && Options.bUseGPUDirect)
		{
			//bUseGPUDirect = AllocateGPUBuffers();
		}

		Async(EAsyncExecution::TaskGraph, [this]()
		{
			//Create event to wait on when no frames are available to send
			ReadyToSendEvent = FPlatformProcess::GetSynchEventFromPool();

			TAnsiStringBuilder<2048> SDPDescription;
			UE::RivermaxCore::Private::Utils::StreamOptionsToSDPDescription(Options, SDPDescription);

			// Initialize buffers in system memory if we're not using gpudirect
			if(bUseGPUDirect == false)
			{
				AllocateSystemBuffers();
			}

			// Initialize internal memory and rivermax configuration 
			if(InitializeStreamMemoryConfig())
			{
				// Initialize rivermax output stream with desired config
				const uint32 NumberPacketsPerFrame = StreamMemory.PacketsPerFrame;
				const uint32 MediaBlockIndex = 0;
				rmax_stream_id NewId;
				rmax_qos_attr QOSAttributes = { 0, 0 }; //todo

				rmax_buffer_attr BufferAttributes;
				FMemory::Memset(&BufferAttributes, 0, sizeof(BufferAttributes));
				BufferAttributes.chunk_size_in_strides = StreamMemory.ChunkSizeInStrides;
				BufferAttributes.data_stride_size = StreamMemory.PayloadSize; //Stride between chunks. 
				BufferAttributes.app_hdr_stride_size = StreamMemory.HeaderStrideSize;
				BufferAttributes.mem_block_array = StreamMemory.MemoryBlocks.GetData();
				BufferAttributes.mem_block_array_len = StreamMemory.MemoryBlocks.Num();
				BufferAttributes.attr_flags = RMAX_OUT_BUFFER_ATTR_FLAG_NONE;

				rmax_status_t Status = rmax_out_create_stream(SDPDescription.GetData(), &BufferAttributes, &QOSAttributes, NumberPacketsPerFrame, MediaBlockIndex, &NewId);
				if (Status == RMAX_OK)
				{
					struct sockaddr_in SourceAddress;
					struct sockaddr_in DestinationAddress;
					FMemory::Memset(&SourceAddress, 0, sizeof(SourceAddress));
					FMemory::Memset(&DestinationAddress, 0, sizeof(DestinationAddress));
					Status = rmax_out_query_address(NewId, MediaBlockIndex, &SourceAddress, &DestinationAddress);
					if (Status == RMAX_OK)
					{
						StreamId = NewId;

						StreamData.FrameFieldTimeIntervalNs = 1E9 / Options.FrameRate.AsDecimal();
						InitializeStreamTimingSettings();

						UE_LOG(LogRivermax, Display, TEXT("Output started to send %dx%d using %d payloads of size %d"), Options.AlignedResolution.X, Options.AlignedResolution.Y, StreamMemory.PacketsInLine, StreamMemory.PayloadSize);

						bIsActive = true;
						RivermaxThread.Reset(FRunnableThread::Create(this, TEXT("Rmax OutputStream Thread"), 128 * 1024, TPri_AboveNormal, FPlatformAffinity::GetPoolThreadMask()));
					}
				}
				else
				{
					UE_LOG(LogRivermax, Warning, TEXT("Failed to create Rivermax output stream. Status: %d"), Status);
				}
			}

			Listener->OnInitializationCompleted(bIsActive);
		});

		return true;
	}

	void FRivermaxOutputStream::Uninitialize()
	{
		if (RivermaxThread != nullptr)
		{
			Stop();
			
			ReadyToSendEvent->Trigger();
			RivermaxThread->Kill(true);
			
			RivermaxThread.Reset();


			FPlatformProcess::ReturnSynchEventToPool(ReadyToSendEvent);
			ReadyToSendEvent = nullptr;
			UE_LOG(LogRivermax, Log, TEXT("Rivermax Output stream has shutdown"));
		}
	}

	bool FRivermaxOutputStream::PushVideoFrame(const FRivermaxOutputVideoFrameInfo& NewFrame)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FRivermaxOutputStream::PushVideoFrame);

		if (TSharedPtr<FRivermaxOutputFrame> AvailableFrame = GetNextAvailableFrame(NewFrame.FrameIdentifier))
		{
			const int32 Stride = GetStride();
			FMemory::Memcpy(AvailableFrame->VideoBuffer, NewFrame.VideoBuffer, NewFrame.Height * Stride);

			AvailableFrame->bIsVideoBufferReady = true;

			//If Frame ready to be sent
			if(AvailableFrame->IsReadyToBeSent())
			{
				const FString TraceName = FString::Format(TEXT("FRivermaxOutputStream::PushFrame {0}"), { AvailableFrame->FrameIndex });
				TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*TraceName);

				FScopeLock Lock(&FrameCriticalSection);
				
				AvailableFrames.Remove(AvailableFrame);
				FramesToSend.Add(MoveTemp(AvailableFrame));
				ReadyToSendEvent->Trigger();
			}

			return true;
		}
		
		return false;
	}

	void FRivermaxOutputStream::Process_AnyThread()
	{
		// Not the best path but it seems to work without tearing for now
		// Wait for the next time a frame should be sent (based on frame interval)
		// If a frame had been sent, make it available again
		// This is to avoid making it available right after scheduling it. It's not sent yet and we start overwriting it
		// Get next available frame that was rendered (Wait if none are)
		// Send frame
		// Restart
		{
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FRivermaxOutputStream::Wait);
				WaitForNextRound();
			}

			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FRivermaxOutputStream::PrepareNextFrame);
				PrepareNextFrame();
			}

			

			if (bIsActive == false)
			{
				return;
			}

			const FString TraceName = FString::Format(TEXT("FRivermaxOutputStream::SendFrame {0}"), { CurrentFrame->FrameIndex });
			TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*TraceName);

			do 
			{
				if (bIsActive)
				{
					GetNextChunk();
				}

				if (bIsActive)
				{
					SetupRTPHeaders();
				}

				if (bIsActive)
				{
					CommitNextChunks();
				}

				//Update frame progress
				if (bIsActive)
				{
					Stats.TotalStrides += StreamMemory.ChunkSizeInStrides;
					++CurrentFrame->ChunkNumber;
				}

			} while (CurrentFrame->ChunkNumber < StreamMemory.ChunksPerMemoryBlock && bIsActive);
			

			Stats.MemoryBlockSentCounter++;
			Stats.TotalStrides += StreamMemory.ChunkSizeInStrides;
			StreamData.bHasFrameFirstChunkBeenFetched = false;
		}
	}

	void FRivermaxOutputStream::AllocateSystemBuffers()
	{
		const int32 Stride = GetStride();
		const int32 FrameAllocSize = Options.AlignedResolution.Y * Stride;
		AvailableFrames.Reserve(Options.NumberOfBuffers);
		for (int32 Index = 0; Index < Options.NumberOfBuffers; ++Index)
		{
			TSharedPtr<FRivermaxOutputFrame> Frame = MakeShared<FRivermaxOutputFrame>(Index);

			constexpr uint32 CacheLineSize = PLATFORM_CACHE_LINE_SIZE;
			Frame->VideoBuffer = static_cast<uint8*>(FMemory::Malloc(FrameAllocSize, CacheLineSize));
			AvailableFrames.Add(MoveTemp(Frame));
		}
	}

	bool FRivermaxOutputStream::InitializeStreamMemoryConfig()
	{
		using namespace UE::RivermaxCore::Private::Utils;

		//2110 stream type
		//We need to use the fullframe allocated size to compute the payload size.

		const int32 BytesPerLine = GetStride();
		const uint32 EffectiveBytesPerLine = BytesPerLine;

		const bool bFoundPayload = FindPayloadSize(Options, BytesPerLine, FormatInfo, StreamMemory.PayloadSize);
		if (bFoundPayload == false)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Could not find payload size for desired resolution %dx%d for desired pixel format"), Options.AlignedResolution.X, Options.AlignedResolution.Y);
			return false;
		}

		StreamMemory.PixelGroupPerPacket = StreamMemory.PayloadSize / FormatInfo.PixelGroupSize;
		StreamMemory.PixelsPerPacket = StreamMemory.PixelGroupPerPacket * FormatInfo.PixelGroupCoverage;
		StreamMemory.PacketsInLine = FMath::CeilToInt32((float)Options.AlignedResolution.X / StreamMemory.PixelsPerPacket);

		StreamMemory.LinesInChunk = FindChunksPerLine(Options);

		StreamMemory.ChunkSizeInStrides = StreamMemory.LinesInChunk * StreamMemory.PacketsInLine;

		StreamMemory.FramesFieldPerMemoryBlock = 1;
		StreamMemory.PacketsPerFrame =  StreamMemory.PacketsInLine * Options.Resolution.Y;
		StreamMemory.PacketsPerMemoryBlock = StreamMemory.PacketsPerFrame * StreamMemory.FramesFieldPerMemoryBlock;
		StreamMemory.ChunksPerFrameField = StreamMemory.PacketsPerFrame / StreamMemory.ChunkSizeInStrides;
		StreamMemory.ChunksPerMemoryBlock = StreamMemory.FramesFieldPerMemoryBlock * StreamMemory.ChunksPerFrameField;
		StreamMemory.MemoryBlockCount = Options.NumberOfBuffers;
		StreamMemory.StridesPerMemoryBlock = StreamMemory.ChunkSizeInStrides * StreamMemory.ChunksPerMemoryBlock;

		// Setup arrays with the right sizes so we can give pointers to rivermax
		StreamMemory.RTPHeaders.SetNumZeroed(StreamMemory.PacketsPerMemoryBlock);
		StreamMemory.PayloadSizes.SetNumUninitialized(StreamMemory.PacketsPerMemoryBlock);
		StreamMemory.HeaderSizes.SetNumUninitialized(StreamMemory.PacketsPerMemoryBlock);
		StreamMemory.HeaderStrideSize = RTPHeaderSize;
		for (int32 PayloadSizeIndex = 0; PayloadSizeIndex < StreamMemory.PayloadSizes.Num(); ++PayloadSizeIndex)
		{
			//Go through each chunk to have effective payload size to be sent (last one of each line could be smaller)
			if ((PayloadSizeIndex + 1) % StreamMemory.PacketsInLine == 0)
			{
				const uint32 LeftOver = EffectiveBytesPerLine - ((StreamMemory.PacketsInLine - 1) * StreamMemory.PayloadSize);
				StreamMemory.PayloadSizes[PayloadSizeIndex] = LeftOver;
				check(LeftOver % FormatInfo.PixelGroupSize == 0);
			}
			else
			{
				StreamMemory.PayloadSizes[PayloadSizeIndex] = StreamMemory.PayloadSize;
			}
			StreamMemory.HeaderSizes[PayloadSizeIndex] = StreamMemory.HeaderStrideSize;
		}
		StreamMemory.MemoryBlocks.SetNumZeroed(StreamMemory.MemoryBlockCount);
		for (uint32 BlockIndex = 0; BlockIndex < StreamMemory.MemoryBlockCount; ++BlockIndex)
		{
			rmax_mem_block& Block = StreamMemory.MemoryBlocks[BlockIndex];
			Block.chunks_num = StreamMemory.ChunksPerMemoryBlock;
			Block.app_hdr_size_arr = StreamMemory.HeaderSizes.GetData();
			Block.data_size_arr = StreamMemory.PayloadSizes.GetData();
			Block.data_ptr = AvailableFrames[BlockIndex]->VideoBuffer;
			Block.app_hdr_ptr = &StreamMemory.RTPHeaders[BlockIndex];
		}

		return true;
	}

	void FRivermaxOutputStream::InitializeNextFrame(const TSharedPtr<FRivermaxOutputFrame>& NextFrame)
	{
		NextFrame->LineNumber = 0;
		NextFrame->PacketCounter = 0;
		NextFrame->SRDOffset = 0;
		NextFrame->ChunkNumber = 0;
		NextFrame->PayloadPtr = nullptr;
		NextFrame->HeaderPtr = nullptr;
	}

	TSharedPtr<FRivermaxOutputFrame> FRivermaxOutputStream::GetNextFrameToSend()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FRivermaxOutputStream::GetNextFrameToSend);

		TSharedPtr<FRivermaxOutputFrame> FrameToSend;

		FScopeLock Lock(&FrameCriticalSection);
		if (FramesToSend.IsEmpty() == false)
		{
			// Pick oldest frame of the lot. 
			// Todo Sort array when modified to always pick first item.
			uint32 OldestIdentifier = TNumericLimits<uint32>::Max();
			TArray<TSharedPtr<FRivermaxOutputFrame>>::TIterator It = FramesToSend.CreateIterator();
			for (It; It; ++It)
			{
				TSharedPtr<FRivermaxOutputFrame>& ItFrame = *It;
				if (ItFrame->FrameIdentifier < OldestIdentifier)
				{
					FrameToSend = *It;
					OldestIdentifier = FrameToSend->FrameIdentifier;
				}
			}

			if (ensure(FrameToSend))
			{
				FramesToSend.Remove(FrameToSend);
				InitializeNextFrame(FrameToSend);
			}
		}

		return FrameToSend;
	}

	TSharedPtr<FRivermaxOutputFrame> FRivermaxOutputStream::GetNextAvailableFrame(uint32 InFrameIdentifier)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FRivermaxOutputStream::GetNextAvailableFrame);

		TSharedPtr<FRivermaxOutputFrame> NextFrame;

		{
			FScopeLock Lock(&FrameCriticalSection);

			//Find matching frame identifier
			if(TSharedPtr<FRivermaxOutputFrame>* MatchingFrame = AvailableFrames.FindByPredicate([InFrameIdentifier](const TSharedPtr<FRivermaxOutputFrame>& Other){ return Other->FrameIdentifier == InFrameIdentifier; }))
			{
				NextFrame = *MatchingFrame;
			}
			else if(TSharedPtr<FRivermaxOutputFrame>* EmptyFrame = AvailableFrames.FindByPredicate([](const TSharedPtr<FRivermaxOutputFrame>& Other){ return Other->FrameIdentifier == FRivermaxOutputFrame::InvalidIdentifier; })) //Find an empty frame
			{
				NextFrame = *EmptyFrame;
				NextFrame->FrameIdentifier = InFrameIdentifier;
			}
			else
			{
				// Could reuse a frame not yet ready to be sent
				// Could also look into stealing a frame ready to be sent bot not sent yet
			}
		}

		return NextFrame;
	}

	void FRivermaxOutputStream::BuildRTPHeader(FRTPHeader& OutHeader) const
	{
		// build RTP header - 12 bytes
		/*
		0                   1                   2                   3
		0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		| V |P|X|  CC   |M|     PT      |            SEQ                |
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|                           timestamp                           |
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|                           ssrc                                |
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+*/
		OutHeader.RawHeader[0] = 0x80;  // 10000000 - version2, no padding, no extension
		OutHeader.RawHeader[1] = 96; //todo payload type from sdp file
		OutHeader.RawHeader[2] = (StreamData.SequenceNumber >> 8) & 0xFF;  // sequence number
		OutHeader.RawHeader[3] = (StreamData.SequenceNumber) & 0xFF;  // sequence number

		const uint32 MediaTimestamp = GetTimestampFromTime(StreamData.NextAlignmentPointNanosec, MediaClockSampleRate);
		*(uint32*)&OutHeader.RawHeader[4] = ByteSwap(MediaTimestamp);

		//2110 specific header
		*(uint32*)&OutHeader.RawHeader[8] = 0x0eb51dbd;  // simulated ssrc

		if (StreamType == ERivermaxStreamType::VIDEO_2110_20_STREAM)
		{
			//SRD means Sample Row Data
			
			// build SRD header - 8-14 bytes
		   /* 0                   1                   2                   3
			0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
			+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			|    Extended Sequence Number   |           SRD Length          |
			+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			|F|     SRD Row Number          |C|         SRD Offset          |
			+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */
			OutHeader.RawHeader[12] = (StreamData.SequenceNumber >> 24) & 0xff;  // high 16 bit of seq Extended Sequence Number
			OutHeader.RawHeader[13] = (StreamData.SequenceNumber >> 16) & 0xff;  // high 16 bit of seq Extended Sequence Number

			const uint16 CurrentPayloadSize = StreamMemory.PayloadSizes[StreamData.SequenceNumber % StreamMemory.PacketsPerFrame];
			*(uint16*)&OutHeader.RawHeader[14] = ByteSwap(CurrentPayloadSize);  // SRD Length
		
			uint16 number_of_rows = Options.Resolution.Y; //todo divide by 2 if interlaced
			uint16 srd_row_number = (CurrentFrame->LineNumber % number_of_rows);
			*(uint16*)&OutHeader.RawHeader[16] = ByteSwap(srd_row_number);
			OutHeader.RawHeader[16] |= (0 << 7); //todo when fields are sent for interlace

			// we never have continuation

			// Write out current offset in pixels
			*(uint16*)&OutHeader.RawHeader[18] = ByteSwap(CurrentFrame->SRDOffset);  // SRD Offset

			// Update SRD offset in pixel for next round
			uint16 PixelsPerPacket = (uint16)((StreamMemory.PayloadSize * FormatInfo.PixelGroupCoverage) / FormatInfo.PixelGroupSize);
			CurrentFrame->SRDOffset = (CurrentFrame->SRDOffset + PixelsPerPacket) % (PixelsPerPacket * StreamMemory.PacketsInLine);
		}

		if (++CurrentFrame->PacketCounter == StreamMemory.PacketsPerFrame)
		{
			OutHeader.RawHeader[1] |= 0x80; // last packet in frame (Marker)

			// ST2210-20: the timestamp SHOULD be the same for each packet of the frame/field.
			const double ticks = (MediaClockSampleRate / (Options.FrameRate.AsDecimal()));
			//if (set.video_type != VIDEO_TYPE::PROGRESSIVE) {
			//	send_data.m_second_field = !send_data.m_second_field;
			//	ticks /= 2;
			//}
			//CurrentFrame->TimestampTicks += ticks;
		}
	}

	void FRivermaxOutputStream::DestroyStream()
	{
		rmax_status_t Status = rmax_out_cancel_unsent_chunks(StreamId);
		if (Status != RMAX_OK)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Could not cancel unsent chunks when destroying output stream. Status: %d"), Status);
		}

		do 
		{
			Status = rmax_out_destroy_stream(StreamId);
			if (RMAX_ERR_BUSY == Status) 
			{
				FPlatformProcess::SleepNoStats(0.01f);
			}
		} while (Status == RMAX_ERR_BUSY);
	}

	void FRivermaxOutputStream::WaitForNextRound()
	{
		IRivermaxCoreModule& RivermaxModule = FModuleManager::LoadModuleChecked<IRivermaxCoreModule>(TEXT("RivermaxCore"));
		const uint64 CurrentTimeNanosec = RivermaxModule.GetRivermaxManager()->GetTime();
		const double CurrentPlatformTime = FPlatformTime::Seconds();
		const uint64 CurrentFrameNumber = UE::RivermaxCore::GetFrameNumber(CurrentTimeNanosec, Options.FrameRate);

		// Frame number we will want to align with
		uint64 NextFrameNumber = CurrentFrameNumber;

		bool bHasValidTimings = true;
		if (StreamData.bHasValidNextFrameNumber == false)
		{
			// Number here is quite big to patch an issue found during testing.
			// Sometimes, first wait is not long enough and we end up scheduling 2 frames super close. 
			// Which causes rivermax hardware send queues to fill up and cause retries
			// which increases the time it takes to commit all chunks per frame
			// Will need to be improved. 
			NextFrameNumber = CurrentFrameNumber + 10;
		}
		else
		{
			// Case where we are back and frame number is the previous one. Depending on offsets, this could happen
			if (CurrentFrameNumber == StreamData.NextAlignmentPointFrameNumber - 1)
			{
				NextFrameNumber = StreamData.NextAlignmentPointFrameNumber + 1;
				UE_LOG(LogRivermax, Verbose, TEXT("Scheduling last frame was faster than expected. (CurrentFrame: '%llu' LastScheduled: '%llu') Scheduling for following expected one.")
				, CurrentFrameNumber
				, StreamData.NextAlignmentPointFrameNumber);
			}
			else
			{
				// We expect current frame number to be the one we scheduled for the last time or greater if something happened
				if (CurrentFrameNumber >= StreamData.NextAlignmentPointFrameNumber)
				{
					// If current frame is greater than last scheduled, we missed an alignment point. Shouldn't happen with continuous thread independent of engine.
					const uint64 DeltaFrames = CurrentFrameNumber - StreamData.NextAlignmentPointFrameNumber;
					if (DeltaFrames >= 1)
					{
						UE_LOG(LogRivermax, Warning, TEXT("Output missed %llu frames."), DeltaFrames);
						// For now, schedule for the following frame as normal but might need to revisit this behavior if it causes issues.
					}

					NextFrameNumber = CurrentFrameNumber + 1;
				}
				else
				{
					// This is not expected (going back in time) but we should be able to continue. Scheduling immediately
					ensure(false);
					bHasValidTimings = false;
				}
			}
		}

		// Get next alignment point based on the frame number we are aligning with
		const uint64 NextAlignmentNano = UE::RivermaxCore::GetAlignmentPointFromFrameNumber(NextFrameNumber, Options.FrameRate);

		// Add Tro offset to next alignment point and configurable offset
		StreamData.NextAlignmentPointNanosec = NextAlignmentNano;
		StreamData.NextScheduleTimeNanosec = NextAlignmentNano + TransmitOffsetNanosec + CVarRivermaxScheduleOffset.GetValueOnAnyThread();
		StreamData.NextAlignmentPointFrameNumber = NextFrameNumber;

		// Offset wakeup if desired to give more time for scheduling. 
		const uint64 WakeupTime = NextAlignmentNano - CVarRivermaxWakeupOffset.GetValueOnAnyThread();

		// Used to know if we schedule blindly in the future or based on previous data
		StreamData.bHasValidNextFrameNumber = bHasValidTimings;
		
		uint64 WaitTimeNanosec = WakeupTime - CurrentTimeNanosec;

		// Wakeup can be smaller than current time with controllable offset
		if (WakeupTime < CurrentTimeNanosec)
		{
			WaitTimeNanosec = 0;
		}

		static constexpr float SleepThresholdSec = 5.0f * (1.0f / 1000.0f);
		static constexpr float YieldTimeSec = 2.0f * (1.0f / 1000.0f);
		double WaitTimeSec = WaitTimeNanosec / 1E9;

		// Protect for erroneously long wait time
		if (!ensure(WaitTimeSec < 1.0))
		{
			WaitTimeSec = 1.0;
		}

		// Sleep for the largest chunk of time
		if (WaitTimeSec > SleepThresholdSec)
		{
			FPlatformProcess::SleepNoStats(WaitTimeSec - YieldTimeSec);
		}

		// Yield our thread time for the remainder of wait time. 
		// Should we spin for a smaller time to be more precise?
		// Should we use platform time instead of rivermax get PTP to avoid making calls to it?
		constexpr bool bUsePlatformTimeToSpin = true;
		if (bUsePlatformTimeToSpin == false)
		{
			while (RivermaxModule.GetRivermaxManager()->GetTime() < WakeupTime)
			{
				FPlatformProcess::SleepNoStats(0.f);
			}
		}
		else
		{
			while (FPlatformTime::Seconds() < (CurrentPlatformTime + WaitTimeSec))
			{
				FPlatformProcess::SleepNoStats(0.f);
			}
		}

		if (StreamData.bHasValidNextFrameNumber)
		{
			const uint64 AfterSleepTimeNanosec = RivermaxModule.GetRivermaxManager()->GetTime();
			UE_LOG(LogRivermax, Verbose, TEXT("Scheduling at %llu. CurrentTime %llu. NextAlign %llu. Waiting %0.9f"), StreamData.NextScheduleTimeNanosec, CurrentTimeNanosec, NextAlignmentNano, (double)WaitTimeNanosec / 1E9);
		}
	}

	void FRivermaxOutputStream::GetNextChunk()
	{
		rmax_status_t Status;
		do
		{
			Status = rmax_out_get_next_chunk(StreamId, &CurrentFrame->PayloadPtr, &CurrentFrame->HeaderPtr);
			if (Status == RMAX_OK)
			{
				if (StreamData.bHasFrameFirstChunkBeenFetched == false)
				{
					StreamData.bHasFrameFirstChunkBeenFetched = true;
					if (CurrentFrame->VideoBuffer != CurrentFrame->PayloadPtr)
					{
						//Debug code to track rivermax chunk processing
						UE_LOG(LogRivermax, Warning, TEXT("Frame being sent (%d) doesn't match chunks being processed."), CurrentFrame->FrameIndex);
					}
				}

				break;
			}
			else if (Status == RMAX_ERR_NO_FREE_CHUNK)
			{
				//We should not be here
				Stats.ChunkRetries++;
			}
			else
			{
				UE_LOG(LogRivermax, Error, TEXT("Invalid error happened while trying to get next chunks. Status: %d"), Status);
				Listener->OnStreamError();
				bIsActive = false;
			}
		} while (Status != RMAX_OK && bIsActive);
	}

	void FRivermaxOutputStream::SetupRTPHeaders()
	{
		uint8* HeaderRawPtr = reinterpret_cast<uint8*>(CurrentFrame->HeaderPtr);
		check(HeaderRawPtr);
		for (uint32 StrideIndex = 0; StrideIndex < StreamMemory.ChunkSizeInStrides && CurrentFrame->PacketCounter < StreamMemory.PacketsPerFrame; ++StrideIndex)
		{
			uint8* NextHeaderRawPtr = HeaderRawPtr + (StrideIndex * StreamMemory.HeaderStrideSize);
			BuildRTPHeader(*reinterpret_cast<FRTPHeader*>(NextHeaderRawPtr));
			//todo only for video
			if (!((StrideIndex + 1) % StreamMemory.PacketsInLine))
			{
				CurrentFrame->LineNumber = (CurrentFrame->LineNumber + 1) % Options.Resolution.Y; //preparing line number for next iteration
			}
			++StreamData.SequenceNumber;
		}
	}

	void FRivermaxOutputStream::CommitNextChunks()
	{
		rmax_status_t Status;
		do
		{
			//Only first chunk gets scheduled with a timestamp. Following chunks are queued after it using 0
			uint64 ScheduleTime = CurrentFrame->ChunkNumber == 0 ? StreamData.NextScheduleTimeNanosec : 0;
			const rmax_commit_flags_t CommitFlags{};
			if (ScheduleTime != 0)
			{
				IRivermaxCoreModule& RivermaxModule = FModuleManager::LoadModuleChecked<IRivermaxCoreModule>(TEXT("RivermaxCore"));
				const uint64 CurrentTimeNanosec = RivermaxModule.GetRivermaxManager()->GetTime();
				TRACE_BOOKMARK(TEXT("Sched A: %llu, C: %llu"), StreamData.NextAlignmentPointNanosec, CurrentTimeNanosec);
				if (ScheduleTime <= CurrentTimeNanosec)
				{
					ScheduleTime = 0;
					++Stats.CommitImmediate;
				}
			}

			Status = rmax_out_commit(StreamId, ScheduleTime, CommitFlags);

			if (Status == RMAX_OK)
			{
				break;
			}
			else if (Status == RMAX_ERR_HW_SEND_QUEUE_FULL)
			{
				Stats.CommitRetries++;
				TRACE_CPUPROFILER_EVENT_SCOPE(CommitNextChunks::QUEUEFULL);
			}
			else if(Status == RMAX_ERR_HW_COMPLETION_ISSUE)
			{
				UE_LOG(LogRivermax, Error, TEXT("Completion issue while trying to commit next round of chunks."));
				Listener->OnStreamError();
				bIsActive = false;
			}
			else
			{
				UE_LOG(LogRivermax, Error, TEXT("Unhandled error (%d) while trying to commit next round of chunks."), Status);
				Listener->OnStreamError();
				bIsActive = false;
			}

		} while (Status != RMAX_OK && bIsActive);
	}

	bool FRivermaxOutputStream::Init()
	{
		return true;
	}

	uint32 FRivermaxOutputStream::Run()
	{
		ReadyToSendEvent->Wait();
		while (bIsActive)
		{
			ShowStats();
			Process_AnyThread();
		}

		DestroyStream();

		return 0;
	}

	void FRivermaxOutputStream::Stop()
	{
		bIsActive = false;
	}

	void FRivermaxOutputStream::Exit()
	{

	}

	void FRivermaxOutputStream::PrepareNextFrame()
	{
		// We always want to send out a frame at the desired interval. If a new frame is not ready, reuse last one. 
		bool bHasFrameToSend = false;
		{
			FScopeLock Lock(&FrameCriticalSection);
			bHasFrameToSend = FramesToSend.IsEmpty() == false;
		}

		// First frame occurrence when none were produced yet so we wait for the first one to be available
		if (CurrentFrame.IsValid() == false && bHasFrameToSend == false)
		{
			//First iteration before first frame available
			ReadyToSendEvent->Wait();
		}

		// Here, we either have a frame available to send or the last one to resend
		{
			{
				FScopeLock Lock(&FrameCriticalSection);
				bHasFrameToSend = FramesToSend.IsEmpty() == false;
			}

			check(bHasFrameToSend || CurrentFrame.IsValid());

			if (bHasFrameToSend)
			{
				if (CurrentFrame.IsValid())
				{
					//Release last frame sent. We keep hold to avoid overwriting it as rivermax is sending it
					CurrentFrame->Reset();
					{
						FScopeLock Lock(&FrameCriticalSection);
						AvailableFrames.Add(MoveTemp(CurrentFrame));
						CurrentFrame.Reset();
					}
				}

				CurrentFrame = GetNextFrameToSend();
			}
			else
			{
				// No frame to send, keep last one and restarts its internal counters
				UE_LOG(LogRivermax, Verbose, TEXT("No frame to send. Reusing last frame '%d'"), CurrentFrame->FrameIndex);
				InitializeNextFrame(CurrentFrame);		
				
				// Since we want to resend last frame, we need to fast forward chunk pointer to re-point to the one we just sent
				rmax_status_t Status;
				do 
				{
					Status = rmax_out_skip_chunks(StreamId, StreamMemory.ChunksPerFrameField * (Options.NumberOfBuffers - 1));
					if (Status != RMAX_OK)
					{
						if (Status == RMAX_ERR_NO_FREE_CHUNK)
						{
							// Wait until there are enough free chunk to be skipped
							UE_LOG(LogRivermax, Warning, TEXT("No chunks ready to skip. Waiting"));
						}
						else
						{ 
							ensure(false);
							UE_LOG(LogRivermax, Error, TEXT("Invalid error happened while trying to skip chunks. Status: %d."), Status);
							Listener->OnStreamError();
							bIsActive = false;
						}
					}
				} while (Status != RMAX_OK && bIsActive);
			}
		}

		//TRACE_BOOKMARK(TEXT("Schedulin %llu"), GetTimestampFromTime(StreamData.NextAlignmentPointNanosec, MediaClockSampleRate));
	}

	void FRivermaxOutputStream::InitializeStreamTimingSettings()
	{
		using namespace UE::RivermaxCore::Private::Utils;

		double FrameIntervalNs = StreamData.FrameFieldTimeIntervalNs;
		const bool bIsProgressive = true;//todo MediaConfiguration.IsProgressive() 
		uint32 PacketsInFrameField = StreamMemory.PacketsPerFrame;
		if (bIsProgressive == false)
		{
			FrameIntervalNs *= 2;
			PacketsInFrameField *= 2;
		}

		// See https://ieeexplore.ieee.org/stamp/stamp.jsp?tp=&arnumber=8165971 for reference
		// Gapped PRS doesn't support non standard resolution. Linear PRS would but Rivermax doesn't support it.
		if (StreamType == ERivermaxStreamType::VIDEO_2110_20_STREAM)
		{
			double RActive;
			double TRODefaultMultiplier;
			if (bIsProgressive)
			{
				RActive = (1080.0 / 1125.0);
				if (Options.Resolution.Y >= FullHDHeight)
				{
					// As defined by SMPTE 2110-21 6.3.2
					TRODefaultMultiplier = (43.0 / 1125.0);
				}
				else
				{
					TRODefaultMultiplier = (28.0 / 750.0);
				}
			}
			else
			{
				if (Options.Resolution.Y >= FullHDHeight)
				{
					// As defined by SMPTE 2110-21 6.3.3
					RActive = (1080.0 / 1125.0);
					TRODefaultMultiplier = (22.0 / 1125.0);
				}
				else if (Options.Resolution.Y >= 576)
				{
					RActive = (576.0 / 625.0);
					TRODefaultMultiplier = (26.0 / 625.0);
				}
				else
				{
					RActive = (487.0 / 525.0);
					TRODefaultMultiplier = (20.0 / 525.0);
				}
			}

			// Need to reinvestigate the implication of this and possibly add cvar to control it runtime
			const double TRSNano = (FrameIntervalNs * RActive) / PacketsInFrameField;
			TransmitOffsetNanosec = (uint64)((TRODefaultMultiplier * FrameIntervalNs));
		}
	}

	uint32 FRivermaxOutputStream::GetTimestampFromTime(uint64 InTimeNanosec, double InMediaClockRate) const
	{
		// RTP timestamp is 32 bits and based on media clock (usually 90kHz).
		// Conversion based on rivermax samples

		const uint64 Nanoscale = 1E9;
		const uint64 Seconds = InTimeNanosec / Nanoscale;
		const uint64 Nanoseconds = InTimeNanosec % Nanoscale;
		const uint64 MediaFrameNumber = Seconds * InMediaClockRate;
		const uint64 MediaSubFrameNumber = Nanoseconds * InMediaClockRate / Nanoscale;
		const double Mask = 0x100000000;
		const double MediaTime = FMath::Fmod(MediaFrameNumber, Mask);
		const double MediaSubTime = FMath::Fmod(MediaSubFrameNumber, Mask);
		return MediaTime + MediaSubTime;
	}

	void FRivermaxOutputStream::ShowStats()
	{
		if (CVarRivermaxOutputShowStats.GetValueOnAnyThread() != 0)
		{
			const double CurrentTime = FPlatformTime::Seconds();
			if (CurrentTime - LastStatsShownTimestamp > CVarRivermaxOutputShowStatsInterval.GetValueOnAnyThread())
			{
				LastStatsShownTimestamp = CurrentTime;
				UE_LOG(LogRivermax, Log, TEXT("Stats: FrameSent: %d. CommitImmediate: %d. CommitRetries: %d. ChunkRetries: %d"), Stats.MemoryBlockSentCounter, Stats.CommitImmediate, Stats.CommitRetries, Stats.ChunkRetries);
			}
		}
	}

	bool FRivermaxOutputStream::PushGPUVideoFrame(const FRivermaxOutputVideoFrameInfo& NewFrame, FBufferRHIRef CapturedBuffer)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FRivermaxOutputStream::PushGPUVideoFrame);

		return false;
	}

	bool FRivermaxOutputStream::IsGPUDirectSupported() const
	{
		return bUseGPUDirect;
	}

	int32 FRivermaxOutputStream::GetStride() const
	{
		check(FormatInfo.PixelGroupCoverage != 0);
		return (Options.AlignedResolution.X / FormatInfo.PixelGroupCoverage) * FormatInfo.PixelGroupSize;
	}
}


// Copyright Epic Games, Inc. All Rights Reserved.

#include "RivermaxOutputStream.h"

#include "Async/Async.h"
#include "CudaModule.h"
#include "ID3D12DynamicRHI.h"
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

	static TAutoConsoleVariable<int32> CVarRivermaxOutputEnableMultiSRD(
		TEXT("Rivermax.Output.EnableMultiSRD"), 1,
		TEXT("When enabled, payload size will be constant across the frame except the last one.\n" 
		     "If disabled, a payload size that fits the line will be used causing some resolution to not be supported."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRivermaxOutputLinesPerChunk(
		TEXT("Rivermax.Output.LinesPerChunk"), 4,
		TEXT("Defines the number of lines to pack in a chunk. Higher number will increase latency"),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRivermaxOutputMaximizePacketSize(
		TEXT("Rivermax.Output.MaximizePacketSize"), 1,
		TEXT("Enables bigger packet sizes to maximize utilisation of potential UDP packet. If not enabled, packet size will be aligned with HD/4k sizes"),
		ECVF_Default);

	bool FindPayloadSize(const FRivermaxOutputStreamOptions& InOptions, uint32 InBytesPerLine, const FVideoFormatInfo& FormatInfo, uint16& OutPayloadSize)
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

	uint32 FindLinesPerChunk(const FRivermaxOutputStreamOptions& InOptions)
	{
		// More lines per chunks mean we will do more work prior to start sending a chunk. So, added 'latency' in terms of packet / parts of frame.
		// Less lines per chunk mean that sender thread might starve.
		// SDK sample uses 4 lines for UHD and 8 for HD. 
		return CVarRivermaxOutputLinesPerChunk.GetValueOnAnyThread();
	}

	uint16 GetPayloadSize(ESamplingType SamplingType)
	{
		const FVideoFormatInfo FormatInfo = FStandardVideoFormat::GetVideoFormatInfo(SamplingType);
		uint16 PayloadSize;
		switch (SamplingType)
		{

		case ESamplingType::YUV444_10bit:
		{
			// Passthrough
		}
		case ESamplingType::RGB_10bit:
		{
			PayloadSize = 1200;
			break;
		}
		case ESamplingType::YUV444_8bit:
		{
			// Passthrough
		}
		case ESamplingType::RGB_8bit:
		{
			PayloadSize = 1152;
			break;
		}
		case ESamplingType::YUV444_12bit:
		{
			// Passthrough
		}
		case ESamplingType::RGB_12bit:
		{
			PayloadSize = 1152;
			break;
		}
		case ESamplingType::YUV444_16bit:
		{
			// Passthrough
		}
		case ESamplingType::YUV444_16bitFloat:
		{
			// Passthrough
		}
		case ESamplingType::RGB_16bit:
		{
			// Passthrough
		}
		case ESamplingType::RGB_16bitFloat:
		{
			PayloadSize = 1152;
			break;
		}
		case ESamplingType::YUV422_8bit:
		{
			PayloadSize = 1280;
			break;
		}
		case ESamplingType::YUV422_10bit:
		{
			PayloadSize = 1200;
			break;
		}
		case ESamplingType::YUV422_12bit:
		{
			PayloadSize = 1152;
			break;
		}
		case ESamplingType::YUV422_16bit:
		{
			// Passthrough
		}
		case ESamplingType::YUV422_16bitFloat:
		{
			PayloadSize = 1280;
			break;
		}
		default:
		{
			checkNoEntry();
			PayloadSize = 1200;
			break;
		}
		}

		ensure(PayloadSize % FormatInfo.PixelGroupSize == 0);
		return PayloadSize;
	}

	/** 
	 * Returns a payload closer to the max value we can have for standard UDP size 
	 * RTPHeader can be bigger depending on configuration so we'll cap payload at 1400.
	 */
	uint16 GetMaximizedPayloadSize(ESamplingType SamplingType)
	{
		const FVideoFormatInfo FormatInfo = FStandardVideoFormat::GetVideoFormatInfo(SamplingType);
		uint16 PayloadSize;
		switch (SamplingType)
		{
		
		case ESamplingType::YUV444_10bit:
		{
			// Passthrough
		}
		case ESamplingType::RGB_10bit:
		{
			PayloadSize = 1395;
			break;
		}
		case ESamplingType::YUV444_8bit:
		{
			// Passthrough
		}
		case ESamplingType::RGB_8bit:
		{
			PayloadSize = 1398;	
			break;
		}
		case ESamplingType::YUV444_12bit:
		{
			// Passthrough
		}
		case ESamplingType::RGB_12bit:
		{
			PayloadSize = 1395;
			break;
		}
		case ESamplingType::YUV444_16bit:
		{
			// Passthrough
		}
		case ESamplingType::YUV444_16bitFloat:
		{
			// Passthrough
		}
		case ESamplingType::RGB_16bit:
		{
			// Passthrough
		}
		case ESamplingType::RGB_16bitFloat:
		{
			PayloadSize = 1398;
			break;
		}
		case ESamplingType::YUV422_8bit:
		{
			PayloadSize = 1400;
			break;
		}
		case ESamplingType::YUV422_10bit:
		{
			PayloadSize = 1400;
			break;
		}
		case ESamplingType::YUV422_12bit:
		{
			PayloadSize = 1398;
			break;
		}
		case ESamplingType::YUV422_16bit:
		{
			// Passthrough
		}
		case ESamplingType::YUV422_16bitFloat:
		{
			PayloadSize = 1400;
			break;
		}
		default:
		{
			checkNoEntry();
			PayloadSize = 1200;
			break;
		}
		}

		ensure(PayloadSize % FormatInfo.PixelGroupSize == 0);
		return PayloadSize;
	}


	FRivermaxOutputStream::FRivermaxOutputStream()
		: bIsActive(false)
	{

	}

	FRivermaxOutputStream::~FRivermaxOutputStream()
	{
		Uninitialize();
	}

	bool FRivermaxOutputStream::Initialize(const FRivermaxOutputStreamOptions& InOptions, IRivermaxOutputStreamListener& InListener)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FRivermaxOutputStream::Initialize);

		RivermaxModule = FModuleManager::GetModulePtr<IRivermaxCoreModule>(TEXT("RivermaxCore"));
		if (RivermaxModule->GetRivermaxManager()->IsLibraryInitialized() == false)
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
		if (RivermaxModule->GetRivermaxManager()->IsGPUDirectOutputSupported() && Options.bUseGPUDirect)
		{
			bUseGPUDirect = AllocateGPUBuffers();
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
				BufferAttributes.chunk_size_in_strides = StreamMemory.PacketsPerChunk;
				BufferAttributes.data_stride_size = StreamMemory.PayloadSize; 
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

						UE_LOG(LogRivermax, Display, TEXT("Output stream started sending on stream %s:%d on interface %s%s")
							, *Options.StreamAddress
							, Options.Port
							, *Options.InterfaceAddress
							, bUseGPUDirect ? TEXT(" using GPUDirect") : TEXT(""));

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

			DeallocateBuffers();
		}
	}

	bool FRivermaxOutputStream::PushVideoFrame(const FRivermaxOutputVideoFrameInfo& NewFrame)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FRivermaxOutputStream::PushVideoFrame);

		if (TSharedPtr<FRivermaxOutputFrame> AvailableFrame = GetNextAvailableFrame(NewFrame.FrameIdentifier))
		{
			const int32 Stride = GetStride();
			FMemory::Memcpy(AvailableFrame->VideoBuffer, NewFrame.VideoBuffer, Options.AlignedResolution.Y * Stride);

			AvailableFrame->bIsVideoBufferReady = true;

			//If Frame ready to be sent
			if(AvailableFrame->IsReadyToBeSent())
			{
				const FString TraceName = FString::Format(TEXT("FRivermaxOutputStream::PushFrame {0}"), { AvailableFrame->FrameIndex });
				TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*TraceName);
				
				MarkFrameToBeSent(MoveTemp(AvailableFrame));
			}

			return true;
		}
		
		return false;
	}

	void FRivermaxOutputStream::Process_AnyThread()
	{
		using namespace UE::RivermaxCore::Private::Utils;

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

			// At this point, if there is no frame to send, move on to wait for next round
			if(CurrentFrame.IsValid() == false || !bIsActive)
			{
				return;
			}

			StreamData.LastSendStartTimeNanoSec = RivermaxModule->GetRivermaxManager()->GetTime();
			
			// Add markup when we start pushing out a frame with its timestamp to track it across network
			if (StreamData.bHasFrameFirstChunkBeenFetched == false)
			{
				const uint32 MediaTimestamp = GetTimestampFromTime(StreamData.NextAlignmentPointNanosec, MediaClockSampleRate);
				const FString TraceName = FString::Format(TEXT("RmaxOutput::StartSending {0}|{1}"), { MediaTimestamp, CurrentFrame->FrameIdentifier });
				TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*TraceName);
				UE_LOG(LogRivermax, Verbose, TEXT("RmaxTX frame number %u with timestamp %u."), CurrentFrame->FrameIdentifier, MediaTimestamp);
			}

			const FString TraceName = FString::Format(TEXT("RmaxOutput::SendFrame {0}"), { CurrentFrame->FrameIndex });
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
					Stats.TotalStrides += StreamMemory.PacketsPerChunk;
					++CurrentFrame->ChunkNumber;
				}

			} while (CurrentFrame->ChunkNumber < StreamMemory.ChunksPerMemoryBlock && bIsActive);
			

			Stats.MemoryBlockSentCounter++;
			StreamData.bHasFrameFirstChunkBeenFetched = false;
		}
	}

	void FRivermaxOutputStream::AllocateSystemBuffers()
	{
		const int32 Stride = GetStride();
		const int32 FrameAllocSize = Options.AlignedResolution.Y * Stride;
		AvailableFrames.Reserve(Options.NumberOfBuffers);

		const auto SystemDeallocator = [](void* Buffer)
		{
			FMemory::Free(Buffer);
		};

		for (int32 Index = 0; Index < Options.NumberOfBuffers; ++Index)
		{
			TSharedPtr<FRivermaxOutputFrame> Frame = MakeShared<FRivermaxOutputFrame>(Index, SystemDeallocator);

			constexpr uint32 CacheLineSize = PLATFORM_CACHE_LINE_SIZE;
			Frame->VideoBuffer = FMemory::Malloc(FrameAllocSize, CacheLineSize);
			AvailableFrames.Add(MoveTemp(Frame));
		}
	}

	bool FRivermaxOutputStream::InitializeStreamMemoryConfig()
	{
		using namespace UE::RivermaxCore::Private::Utils;

		//2110 stream type
		//We need to use the fullframe allocated size to compute the payload size.

		const int32 BytesPerLine = GetStride();

		// Find out payload we want to use. Either we go the 'potential' multi SRD route or we keep the old way of finding a common payload
		// with more restrictions on resolution supported. Kept in place to be able to fallback in case there are issues with the multiSRD one.
		if (CVarRivermaxOutputEnableMultiSRD.GetValueOnAnyThread() >= 1)
		{
			if (CVarRivermaxOutputMaximizePacketSize.GetValueOnAnyThread() >= 1)
			{
				StreamMemory.PayloadSize = GetMaximizedPayloadSize(FormatInfo.Sampling);
			}
			else
			{
				StreamMemory.PayloadSize = GetPayloadSize(FormatInfo.Sampling);
			}
		}
		else
		{
			const bool bFoundPayload = FindPayloadSize(Options, BytesPerLine, FormatInfo, StreamMemory.PayloadSize);
			if (bFoundPayload == false)
			{
				UE_LOG(LogRivermax, Warning, TEXT("Could not find payload size for desired resolution %dx%d for desired pixel format"), Options.AlignedResolution.X, Options.AlignedResolution.Y);
				return false;
			}
		}

		// With payload size in hand, figure out how many packets we will need, how many chunks (group of packets) and configure descriptor arrays

		const uint32 PixelCount = Options.AlignedResolution.X * Options.AlignedResolution.Y;
		const uint64 FrameSize = PixelCount / FormatInfo.PixelGroupCoverage * FormatInfo.PixelGroupSize;

		StreamMemory.PixelGroupPerPacket = StreamMemory.PayloadSize / FormatInfo.PixelGroupSize;
		StreamMemory.PixelsPerPacket = StreamMemory.PixelGroupPerPacket * FormatInfo.PixelGroupCoverage;

		// We might need a smaller packet to complete the end of frame so ceil to the next value
		StreamMemory.PacketsPerFrame = FMath::CeilToInt32((float)PixelCount / StreamMemory.PixelsPerPacket);

		// Depending on resolution and payload size, last packet of a line might not be fully utilized but we need the remaining bytes so ceil to next value
		StreamMemory.PacketsInLine = FMath::CeilToInt32((float)StreamMemory.PacketsPerFrame / Options.AlignedResolution.Y);

		StreamMemory.LinesInChunk = FindLinesPerChunk(Options);
		StreamMemory.PacketsPerChunk = StreamMemory.LinesInChunk * StreamMemory.PacketsInLine;
		StreamMemory.FramesFieldPerMemoryBlock = 1;

		// Chunk count won't necessarily align with the number of packets required. We need an integer amount of chunks to initialize our stream
		// and calculate how many packets that represents. Rivermax will expect the payload/header array to be that size. It just means that
		// we will mark the extra packets as 0 size.
		StreamMemory.ChunksPerFrameField = FMath::CeilToInt32((float)StreamMemory.PacketsPerFrame / StreamMemory.PacketsPerChunk);
		StreamMemory.PacketsPerMemoryBlock = StreamMemory.ChunksPerFrameField * StreamMemory.PacketsPerChunk * StreamMemory.FramesFieldPerMemoryBlock;
		StreamMemory.ChunksPerMemoryBlock = StreamMemory.FramesFieldPerMemoryBlock * StreamMemory.ChunksPerFrameField;
		StreamMemory.MemoryBlockCount = Options.NumberOfBuffers;

		// Setup arrays with the right sizes so we can give pointers to rivermax
		StreamMemory.RTPHeaders.SetNumZeroed(StreamMemory.MemoryBlockCount);
		StreamMemory.PayloadSizes.SetNumUninitialized(StreamMemory.PacketsPerMemoryBlock);
		StreamMemory.HeaderSizes.SetNumUninitialized(StreamMemory.PacketsPerMemoryBlock);
		StreamMemory.HeaderStrideSize = sizeof(FRawRTPHeader);

		uint64 TotalSize = 0;
		uint64 LineSize = 0;
		for (int32 PayloadSizeIndex = 0; PayloadSizeIndex < StreamMemory.PayloadSizes.Num(); ++PayloadSizeIndex)
		{
			uint32 HeaderSize = FRawRTPHeader::OneSRDSize;
			uint32 ThisPayloadSize = StreamMemory.PayloadSize;
			if (TotalSize < FrameSize)
			{
				if ((LineSize + StreamMemory.PayloadSize) == BytesPerLine)
				{
					LineSize = 0;
				}
				else if ((LineSize + StreamMemory.PayloadSize) > BytesPerLine)
				{
					HeaderSize = FRawRTPHeader::TwoSRDSize;
					LineSize = StreamMemory.PayloadSize - (BytesPerLine - LineSize);
					if (LineSize > BytesPerLine)
					{
						UE_LOG(LogRivermax, Warning, TEXT("Unsupported small resolution, %dx%d, needing more than 2 SRD to express"), Options.AlignedResolution.X, Options.AlignedResolution.Y);
						return false;
					}
				}
				else
				{
					// Keep track of line size offset to know when to use TwoSRDs
					LineSize += StreamMemory.PayloadSize;
				}

				if ((TotalSize + StreamMemory.PayloadSize) > FrameSize)
				{
					HeaderSize = FRawRTPHeader::OneSRDSize;
				}
			}
			else
			{
				// Extra header/payload required for the chunk alignment are set to 0. Nothing has to be sent out the wire.
				HeaderSize = 0;
				ThisPayloadSize = 0;
			}
			
			StreamMemory.HeaderSizes[PayloadSizeIndex] = HeaderSize;
			StreamMemory.PayloadSizes[PayloadSizeIndex] = ThisPayloadSize;
			TotalSize += ThisPayloadSize;
		}
		StreamMemory.MemoryBlocks.SetNumZeroed(StreamMemory.MemoryBlockCount);
		for (uint32 BlockIndex = 0; BlockIndex < StreamMemory.MemoryBlockCount; ++BlockIndex)
		{
			rmax_mem_block& Block = StreamMemory.MemoryBlocks[BlockIndex];
			Block.chunks_num = StreamMemory.ChunksPerMemoryBlock;
			Block.app_hdr_size_arr = StreamMemory.HeaderSizes.GetData();
			Block.data_size_arr = StreamMemory.PayloadSizes.GetData();
			Block.data_ptr = AvailableFrames[BlockIndex]->VideoBuffer;
			
			StreamMemory.RTPHeaders[BlockIndex].SetNumZeroed(StreamMemory.PacketsPerFrame);
			Block.app_hdr_ptr = &StreamMemory.RTPHeaders[BlockIndex][0];
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

	void FRivermaxOutputStream::BuildRTPHeader(FRawRTPHeader& OutHeader) const
	{
		using namespace UE::RivermaxCore::Private::Utils;

		OutHeader = {};
		OutHeader.Version = 2;
		OutHeader.PaddingBit = 0;
		OutHeader.ExtensionBit = 0;
		OutHeader.PayloadType = 96; //Payload type should probably be infered from SDP
		OutHeader.SequenceNumber = ByteSwap((uint16)(StreamData.SequenceNumber & 0xFFFF));

		// For now, in order to be able to use a framelocked input, we pipe frame number in the timestamp for a UE-UE interaction
		// Follow up work to investigate adding this in RTP header
		uint64 InputTime = StreamData.NextAlignmentPointNanosec;
		if (Options.bDoFrameCounterTimestamping)
		{
			InputTime = UE::RivermaxCore::GetAlignmentPointFromFrameNumber(CurrentFrame->FrameIdentifier, Options.FrameRate);
		}

		const uint32 MediaTimestamp = GetTimestampFromTime(InputTime, MediaClockSampleRate);
		OutHeader.Timestamp = ByteSwap(MediaTimestamp);

		//2110 specific header
		OutHeader.SynchronizationSource = ByteSwap((uint32)0x0eb51dbd);  // Should Unreal has its own synch source ID

		if (StreamType == ERivermaxStreamType::VIDEO_2110_20_STREAM)
		{
			if (CurrentFrame->PacketCounter + 1 == StreamMemory.PacketsPerFrame)
			{
				OutHeader.MarkerBit = 1; // last packet in frame (Marker)
			}

			OutHeader.ExtendedSequenceNumber = ByteSwap((uint16)((StreamData.SequenceNumber >> 16) & 0xFFFF));

			// Verify if payload size exceeds line 
			const uint32 CurrentPayloadSize = StreamMemory.PayloadSizes[CurrentFrame->PacketCounter];

			const uint32 LineSizeOffset = ((CurrentFrame->SRDOffset / FormatInfo.PixelGroupCoverage) * FormatInfo.PixelGroupSize);
			const uint32 LineSize = ((Options.AlignedResolution.X / FormatInfo.PixelGroupCoverage) * FormatInfo.PixelGroupSize);

			const uint16 SRD1Length = FMath::Min(LineSize - LineSizeOffset, CurrentPayloadSize);
			const uint16 SRD1PixelCount = SRD1Length / FormatInfo.PixelGroupSize * FormatInfo.PixelGroupCoverage;
			uint16 SRD2Length = SRD1Length < CurrentPayloadSize ? CurrentPayloadSize - SRD1Length : 0;
			if (SRD2Length && CurrentFrame->LineNumber == ((uint32)Options.AlignedResolution.Y-1))
			{
				SRD2Length = 0;
			}

			OutHeader.SRD1Length = ByteSwap(SRD1Length);	
			OutHeader.SetSrd1RowNumber(CurrentFrame->LineNumber); //todo divide by 2 if interlaced
			OutHeader.FieldIdentification1 = 0; //todo when fields are sent for interlace
			OutHeader.SetSrd1Offset(CurrentFrame->SRDOffset); 

			CurrentFrame->SRDOffset += SRD1PixelCount;
			if (CurrentFrame->SRDOffset >= Options.AlignedResolution.X)
			{
				CurrentFrame->SRDOffset = 0;
				++CurrentFrame->LineNumber;
			}

			if (SRD2Length > 0)
			{
				OutHeader.SRD2Length = ByteSwap(SRD2Length);

				OutHeader.ContinuationBit1 = 1;
				OutHeader.FieldIdentification2 = 0;
				OutHeader.SetSrd2RowNumber(CurrentFrame->LineNumber);
				OutHeader.SetSrd2Offset(CurrentFrame->SRDOffset);

				const uint16 SRD2PixelCount = SRD2Length / FormatInfo.PixelGroupSize * FormatInfo.PixelGroupCoverage;
				CurrentFrame->SRDOffset += SRD2PixelCount;
				if (CurrentFrame->SRDOffset >= Options.AlignedResolution.X)
				{
					CurrentFrame->SRDOffset = 0;
					++CurrentFrame->LineNumber;
				}
			}
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
				FPlatformProcess::SleepNoStats(SleepTimeSeconds);
			}
		} while (Status == RMAX_ERR_BUSY);
	}

	void FRivermaxOutputStream::WaitForNextRound()
	{
		const uint64 CurrentTimeNanosec = RivermaxModule->GetRivermaxManager()->GetTime();
		const double CurrentPlatformTime = FPlatformTime::Seconds();
		const uint64 CurrentFrameNumber = UE::RivermaxCore::GetFrameNumber(CurrentTimeNanosec, Options.FrameRate);

		switch (Options.AlignmentMode)
		{
			case ERivermaxAlignmentMode::AlignmentPoint:
			{
				CalculateNextScheduleTime_AlignementPoints(CurrentTimeNanosec, CurrentFrameNumber);
				break;
			}
			case ERivermaxAlignmentMode::FrameCreation:
			{
				CalculateNextScheduleTime_FrameCreation(CurrentTimeNanosec, CurrentFrameNumber);
				break;
			}
			default:
			{
				checkNoEntry();
			}
		}

		// Offset wakeup if desired to give more time for scheduling. 
		const uint64 WakeupTime = StreamData.NextAlignmentPointNanosec - CVarRivermaxWakeupOffset.GetValueOnAnyThread();
		
		uint64 WaitTimeNanosec = WakeupTime - CurrentTimeNanosec;

		// Wakeup can be smaller than current time with controllable offset
		if (WakeupTime < CurrentTimeNanosec)
		{
			WaitTimeNanosec = 0;
		}

		static constexpr float SleepThresholdSec = 5.0f * (1.0f / 1000.0f);
		static constexpr float YieldTimeSec = 2.0f * (1.0f / 1000.0f);
		const double WaitTimeSec = FMath::Min(WaitTimeNanosec / 1E9, 1.0);

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
			while (RivermaxModule->GetRivermaxManager()->GetTime() < WakeupTime)
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
			const uint64 AfterSleepTimeNanosec = RivermaxModule->GetRivermaxManager()->GetTime();
			UE_LOG(LogRivermax, Verbose, TEXT("Scheduling at %llu. CurrentTime %llu. NextAlign %llu. Waiting %0.9f"), StreamData.NextScheduleTimeNanosec, CurrentTimeNanosec, StreamData.NextAlignmentPointNanosec, (double)WaitTimeNanosec / 1E9);
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
		FRawRTPHeader* HeaderRawPtr = reinterpret_cast<FRawRTPHeader*>(CurrentFrame->HeaderPtr);
		check(HeaderRawPtr);
		for (uint32 StrideIndex = 0; StrideIndex < StreamMemory.PacketsPerChunk && CurrentFrame->PacketCounter < StreamMemory.PacketsPerFrame; ++StrideIndex)
		{
			BuildRTPHeader(*HeaderRawPtr);
			++StreamData.SequenceNumber;
			CurrentFrame->BytesSent += StreamMemory.PayloadSizes[CurrentFrame->PacketCounter];
			++CurrentFrame->PacketCounter;
			++HeaderRawPtr;
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
				const uint64 CurrentTimeNanosec = RivermaxModule->GetRivermaxManager()->GetTime();
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
		// Initial wait for a frame to be produced
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FRivermaxOutputStream::InitialWait);
			ReadyToSendEvent->Wait();
		}

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

		const uint64 CurrentTimeNanosec = RivermaxModule->GetRivermaxManager()->GetTime();

		// Different scenarios here.
		// When aligning on frame creation, we will always wait for a frame to be available.
		// When aligning on alignment points, we either repeat the last one if none is available and continuous mode is on
		// or skip sending one if continuous mode is off.
		if (Options.AlignmentMode == ERivermaxAlignmentMode::FrameCreation)
		{
			if (!bHasFrameToSend)
			{
				// No frame in this mode means we are going to wait for it
				//First iteration before first frame available
				TRACE_CPUPROFILER_EVENT_SCOPE(FRivermaxOutputStream::WaitForNewFrame);
				do
				{
					FPlatformProcess::SleepNoStats(SleepTimeSeconds);

					// Early exit if stream isn't active anymore
					if (!bIsActive)
					{
						return;
					}

					{
						FScopeLock Lock(&FrameCriticalSection);
						bHasFrameToSend = FramesToSend.IsEmpty() == false;
					}

				} while (bHasFrameToSend == false);
			}
		}

		{
			{
				FScopeLock Lock(&FrameCriticalSection);
				bHasFrameToSend = FramesToSend.IsEmpty() == false;
			}

			// If we have a frame or we're not doing continuous output, we always release the last frame.
			if (!Options.bDoContinuousOutput || bHasFrameToSend)
			{
				// Not doing a continuous output and no frame to send, release the last one sent if any and move on waiting
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
				if (Options.AlignedResolution.Y >= FullHDHeight)
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
				if (Options.AlignedResolution.Y >= FullHDHeight)
				{
					// As defined by SMPTE 2110-21 6.3.3
					RActive = (1080.0 / 1125.0);
					TRODefaultMultiplier = (22.0 / 1125.0);
				}
				else if (Options.AlignedResolution.Y >= 576)
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

		const int32 Stride = GetStride();
		if (TSharedPtr<FRivermaxOutputFrame> AvailableFrame = GetNextAvailableFrame(NewFrame.FrameIdentifier))
		{
			const FString TraceName = FString::Format(TEXT("FRivermaxOutputStream::PushFrame {0}|{1}"), { AvailableFrame->FrameIndex, NewFrame.FrameIdentifier });
			TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*TraceName);

			FCUDAModule& CudaModule = FModuleManager::GetModuleChecked<FCUDAModule>("CUDA");
			CUresult Result = CudaModule.DriverAPI()->cuCtxPushCurrent(CudaModule.GetCudaContext());
			
			void* MappedPointer = GetMappedAddress(CapturedBuffer);
			if (MappedPointer == nullptr)
			{
				UE_LOG(LogRivermax, Error, TEXT("Failed to find a mapped memory address for captured buffer. Stopping capture."));
				Listener->OnStreamError();
				return false;
			}

			const CUdeviceptr CudaMemoryPointer = reinterpret_cast<CUdeviceptr>(MappedPointer);
			Result = CudaModule.DriverAPI()->cuMemcpyDtoDAsync(reinterpret_cast<CUdeviceptr>(AvailableFrame->VideoBuffer), CudaMemoryPointer, Options.AlignedResolution.Y * Stride, reinterpret_cast<CUstream>(GPUStream));
			if (Result != CUDA_SUCCESS)
			{
				UE_LOG(LogRivermax, Error, TEXT("Failed to copy captured bufer to cuda memory. Stopping capture. Error: %d"), Result);
				Listener->OnStreamError();
				return false;
			}


			// Callback called by Cuda when stream work has completed on cuda engine (MemCpy -> Callback)
			// Once Memcpy has been done, we know we can mark that memory as available to be sent. 
			
			auto CudaCallback = [](void* userData)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(CudaWorkDoneCallback);

				FRivermaxOutputStream* Stream = reinterpret_cast<FRivermaxOutputStream*>(userData);

				TOptional<uint32> TargetFrameIdentifier = Stream->PendingIdentifiers.Dequeue();
				if (TargetFrameIdentifier.IsSet())
				{
					if (TSharedPtr<FRivermaxOutputFrame> AvailableFrame = Stream->GetNextAvailableFrame(TargetFrameIdentifier.GetValue()))
					{
						AvailableFrame->bIsVideoBufferReady = true;
						if (AvailableFrame->IsReadyToBeSent())
						{
							const FString TraceName = FString::Format(TEXT("FRivermaxOutputStream::PushFrame {0}"), { AvailableFrame->FrameIndex });
							TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*TraceName);

							Stream->MarkFrameToBeSent(MoveTemp(AvailableFrame));
						}
					}
				}
			};
			
			// Add pending frame for cuda callback 
			PendingIdentifiers.Enqueue(AvailableFrame->FrameIdentifier);

			// Schedule a callback to make the frame available
			CudaModule.DriverAPI()->cuLaunchHostFunc(reinterpret_cast<CUstream>(GPUStream), CudaCallback, this);
			
			FCUDAModule::CUDA().cuCtxPopCurrent(nullptr);

			return true;
		}

		return false;
	}

	bool FRivermaxOutputStream::IsGPUDirectSupported() const
	{
		return bUseGPUDirect;
	}

	bool FRivermaxOutputStream::AllocateGPUBuffers()
	{
		// Allocate a single memory space that will contain all frame buffers
		
		TRACE_CPUPROFILER_EVENT_SCOPE(FRivermaxOutputStream::AllocateGPUBuffers);

		const ERHIInterfaceType RHIType = RHIGetInterfaceType();
		if (RHIType != ERHIInterfaceType::D3D12)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Can't initialize output to use GPUDirect. RHI is %d but only Dx12 is supported at the moment."), RHIType);
			return false;
		}

		const int32 Stride = GetStride();
		const int32 FrameAllocSize = Options.AlignedResolution.Y * Stride;

		FCUDAModule& CudaModule = FModuleManager::GetModuleChecked<FCUDAModule>("CUDA");
		CudaModule.DriverAPI()->cuCtxPushCurrent(CudaModule.GetCudaContext());

		// Todo: Add support for mgpu. For now, this will not work unless the memcpy does implicitely a cross gpu transfer.
		const int GPUIndex = CudaModule.GetCudaDeviceIndex();
		CUdevice CudaDevice;
		CUresult Status = CudaModule.DriverAPI()->cuDeviceGet(&CudaDevice, GPUIndex);
		if(Status != CUDA_SUCCESS)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Can't initialize output to use GPUDirect. Failed to get a Cuda device for GPU %d. Status: %d"), GPUIndex, Status);
			return false;
		}

		CUmemAllocationProp AllocationProperties = {};
		AllocationProperties.type = CU_MEM_ALLOCATION_TYPE_PINNED;
		AllocationProperties.allocFlags.gpuDirectRDMACapable = 1;
		AllocationProperties.allocFlags.usage = 0;
		AllocationProperties.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
		AllocationProperties.location.id = CudaDevice;

		// Get memory granularity required for cuda device. We need to align allocation with this.
		size_t Granularity;
		Status = CudaModule.DriverAPI()->cuMemGetAllocationGranularity(&Granularity, &AllocationProperties, CU_MEM_ALLOC_GRANULARITY_RECOMMENDED);
		if(Status != CUDA_SUCCESS)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Can't initialize output to use GPUDirect. Failed to get allocation granularity. Status: %d"), Status);
			return false;
		}

		// Cuda requires allocated memory to be aligned with a certain granularity
		// We align each frame size to the desired granularity and multiply that by number of buffer
		// This causes more memory to be allocated but doing a single allocation fails rmax stream creation
		const size_t CudaAlignedFrameSize = (FrameAllocSize % Granularity) ? FrameAllocSize + (Granularity - (FrameAllocSize % Granularity)) : FrameAllocSize;
		const size_t TotalCudaAllocSize = CudaAlignedFrameSize * Options.NumberOfBuffers;

		// Reserve contiguous memory to contain required number of buffers. 
		CUdeviceptr CudaBaseAddress;
		constexpr CUdeviceptr InitialAddress = 0;
		constexpr int32 Flags = 0;
		Status = CudaModule.DriverAPI()->cuMemAddressReserve(&CudaBaseAddress, TotalCudaAllocSize, Granularity, InitialAddress, Flags);
		if (Status != CUDA_SUCCESS)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Can't initialize output to use GPUDirect. Failed to reserve memory for %d bytes. Status: %d"), TotalCudaAllocSize, Status);
			return false;
		}

		// Make the allocation on device memory
		CUmemGenericAllocationHandle Handle;
		Status = CudaModule.DriverAPI()->cuMemCreate(&Handle, TotalCudaAllocSize, &AllocationProperties, Flags);
		if (Status != CUDA_SUCCESS)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Can't initialize output to use GPUDirect. Failed to create memory on device. Status: %d"), Status);
			return false;
		}
		
		bool bExit = false;
		constexpr int32 Offset = 0;
		Status = CudaModule.DriverAPI()->cuMemMap(CudaBaseAddress, TotalCudaAllocSize, Offset, Handle, Flags);
		if (Status != CUDA_SUCCESS)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Can't initialize output to use GPUDirect. Failed to map memory. Status: %d"), Status);
			// Need to release handle no matter what
			bExit = true;
		}
		
		// Cache to know we need to unmap/deallocate even if it fails down the road
		CudaAllocatedMemory = TotalCudaAllocSize;
		CudaAllocatedMemoryBaseAddress = reinterpret_cast<void*>(CudaBaseAddress);

		Status = CudaModule.DriverAPI()->cuMemRelease(Handle);
		if (Status != CUDA_SUCCESS)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Can't initialize output to use GPUDirect. Failed to release handle. Status: %d"), Status);
			return false;
		}

		if(bExit)
		{
			return false;
		}

		// Setup access description.
		CUmemAccessDesc MemoryAccessDescription = {};
		MemoryAccessDescription.flags = CU_MEM_ACCESS_FLAGS_PROT_READWRITE;
		MemoryAccessDescription.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
		MemoryAccessDescription.location.id = CudaDevice;
		constexpr int32 Count = 1;
		Status = CudaModule.DriverAPI()->cuMemSetAccess(CudaBaseAddress, TotalCudaAllocSize, &MemoryAccessDescription, Count);
		if (Status != CUDA_SUCCESS)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Can't initialize output to use GPUDirect. Failed to configure memory access. Status: %d"), Status);
			return false;
		}

		CUstream CudaStream;
		Status = CudaModule.DriverAPI()->cuStreamCreate(&CudaStream, CU_STREAM_NON_BLOCKING);
		if (Status != CUDA_SUCCESS)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Can't initialize output to use GPUDirect. Failed to create its stream. Status: %d"), Status);
			return false;
		}

		GPUStream = CudaStream;
		
		Status = CudaModule.DriverAPI()->cuCtxSynchronize();
		if (Status != CUDA_SUCCESS)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Can't initialize output to use GPUDirect. Failed to synchronize context. Status: %d"), Status);
			return false;
		}

		// Now that Cuda memory was allocated, we can allocate our buffers with an address in cuda space
		AvailableFrames.Reserve(Options.NumberOfBuffers);
		for (int32 Index = 0; Index < Options.NumberOfBuffers; ++Index)
		{
			TFunction<void(void*)> DeallocatorFunc = nullptr; // We handle a single deallocation for gpu memory
			TSharedPtr<FRivermaxOutputFrame> Frame = MakeShared<FRivermaxOutputFrame>(Index, DeallocatorFunc);
			Frame->VideoBuffer = reinterpret_cast<void*>(CudaBaseAddress + (Index * CudaAlignedFrameSize));
			AvailableFrames.Add(MoveTemp(Frame));
		}		

		CudaModule.DriverAPI()->cuCtxPopCurrent(nullptr);

		return true;
	}

	void FRivermaxOutputStream::DeallocateBuffers()
	{
		if (CudaAllocatedMemory > 0)
		{
			FCUDAModule& CudaModule = FModuleManager::GetModuleChecked<FCUDAModule>("CUDA");
			CudaModule.DriverAPI()->cuCtxPushCurrent(CudaModule.GetCudaContext());

			const CUdeviceptr BaseAddress = reinterpret_cast<CUdeviceptr>(CudaAllocatedMemoryBaseAddress);
			CUresult Status = CudaModule.DriverAPI()->cuMemUnmap(BaseAddress, CudaAllocatedMemory);
			if (Status != CUDA_SUCCESS)
			{
				UE_LOG(LogRivermax, Warning, TEXT("Failed to unmap cuda memory. Status: %d"), Status);
			}

			Status = CudaModule.DriverAPI()->cuMemAddressFree(BaseAddress, CudaAllocatedMemory);
			if (Status != CUDA_SUCCESS)
			{
				UE_LOG(LogRivermax, Warning, TEXT("Failed to free cuda memory. Status: %d"), Status);
			}

			for (const TPair<FBufferRHIRef, void*>& Entry : BufferCudaMemoryMap)
			{
				if(Entry.Value)
				{
					CudaModule.DriverAPI()->cuMemFree(reinterpret_cast<CUdeviceptr>(Entry.Value));
				}
			}
			BufferCudaMemoryMap.Empty();

			Status = CudaModule.DriverAPI()->cuStreamDestroy(reinterpret_cast<CUstream>(GPUStream));
			if (Status != CUDA_SUCCESS)
			{
				UE_LOG(LogRivermax, Warning, TEXT("Failed to destroy cuda stream. Status: %d"), Status);
			}
			GPUStream = nullptr;

			CudaModule.DriverAPI()->cuCtxPopCurrent(nullptr);
		}
	}

	int32 FRivermaxOutputStream::GetStride() const
	{
		check(FormatInfo.PixelGroupCoverage != 0);
		return (Options.AlignedResolution.X / FormatInfo.PixelGroupCoverage) * FormatInfo.PixelGroupSize;
	}

	void* FRivermaxOutputStream::GetMappedAddress(const FBufferRHIRef& InBuffer)
	{
		// If we are here, d3d12 had to have been validated
		const ERHIInterfaceType RHIType = RHIGetInterfaceType();
		check(RHIType == ERHIInterfaceType::D3D12);

		//Do we already have a mapped address for this buffer
		if (BufferCudaMemoryMap.Find((InBuffer)) == nullptr)
		{
			int64 BufferMemorySize = 0;
			CUexternalMemory MappedExternalMemory = nullptr;
			HANDLE D3D12BufferHandle = 0;
			CUDA_EXTERNAL_MEMORY_HANDLE_DESC CudaExtMemHandleDesc = {};

			// Create shared handle for our buffer
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(Rmax_D3D12CreateSharedHandle);

				ID3D12Resource* NativeD3D12Resource = GetID3D12DynamicRHI()->RHIGetResource(InBuffer);
				BufferMemorySize = GetID3D12DynamicRHI()->RHIGetResourceMemorySize(InBuffer);

				TRefCountPtr<ID3D12Device> OwnerDevice;
				HRESULT QueryResult;
				if ((QueryResult = NativeD3D12Resource->GetDevice(IID_PPV_ARGS(OwnerDevice.GetInitReference()))) != S_OK)
				{
					UE_LOG(LogRivermax, Error, TEXT("Failed to get D3D12 device for captured buffer ressource: %d)"), QueryResult);
					return nullptr;
				}

				if ((QueryResult = OwnerDevice->CreateSharedHandle(NativeD3D12Resource, NULL, GENERIC_ALL, NULL, &D3D12BufferHandle)) != S_OK)
				{
					UE_LOG(LogRivermax, Error, TEXT("Failed to create shared handle for captured buffer ressource: %d"), QueryResult);
					return nullptr;
				}

				CudaExtMemHandleDesc.type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE;
				CudaExtMemHandleDesc.handle.win32.name = nullptr;
				CudaExtMemHandleDesc.handle.win32.handle = D3D12BufferHandle;
				CudaExtMemHandleDesc.size = BufferMemorySize;
				CudaExtMemHandleDesc.flags |= CUDA_EXTERNAL_MEMORY_DEDICATED;
			}

			{
				TRACE_CPUPROFILER_EVENT_SCOPE(Rmax_CudaImportMemory);

				const CUresult Result = FCUDAModule::CUDA().cuImportExternalMemory(&MappedExternalMemory, &CudaExtMemHandleDesc);
				
				if (D3D12BufferHandle)
				{
					CloseHandle(D3D12BufferHandle);
				}

				if (Result != CUDA_SUCCESS)
				{
					UE_LOG(LogRivermax, Error, TEXT("Failed to import shared buffer. Error: %d"), Result);
					return nullptr;
				}
			}

			{
				TRACE_CPUPROFILER_EVENT_SCOPE(Rmax_MapCudaMemory);

				CUDA_EXTERNAL_MEMORY_BUFFER_DESC BufferDescription = {};
				BufferDescription.offset = 0;
				BufferDescription.size = BufferMemorySize;
				CUdeviceptr NewMemory;
				const CUresult Result = FCUDAModule::CUDA().cuExternalMemoryGetMappedBuffer(&NewMemory, MappedExternalMemory, &BufferDescription);
				if(Result != CUDA_SUCCESS || NewMemory == 0)
				{
					UE_LOG(LogRivermax, Error, TEXT("Failed to get shared buffer mapped memory. Error: %d"), Result);
					return nullptr;
				}
				
				BufferCudaMemoryMap.Add(InBuffer, reinterpret_cast<void*>(NewMemory));
			}
		}

		// At this point, we have the mapped buffer in cuda space and we can use it to schedule a memcpy on cuda engine.
		return BufferCudaMemoryMap[InBuffer];
	}

	void FRivermaxOutputStream::CalculateNextScheduleTime_AlignementPoints(uint64 CurrentClockTimeNanosec, uint64 CurrentFrameNumber)
	{
		// Frame number we will want to align with
		uint64 NextFrameNumber = CurrentFrameNumber;

		bool bFoundValidTimings = true;
		
		if (StreamData.bHasValidNextFrameNumber == false)
		{
			// Now that the stream starts when a frame was produced, we can reduce our wait
			// We wait one frame here to start sending at the next frame boundary.
			// Since it takes a frame to send it, we could detect if we are in the first 10% (arbitrary)
			// of the interval and start sending right away but we might be overlapping with the next one
			NextFrameNumber = CurrentFrameNumber + 1;
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
					ensureMsgf(false, TEXT("Unexpected behaviour during output stream's alignment point calculation. Current time has gone back in time compared to last scheduling."));
					bFoundValidTimings = false;
				}
			}
		}

		// Get next alignment point based on the frame number we are aligning with
		const uint64 NextAlignmentNano = UE::RivermaxCore::GetAlignmentPointFromFrameNumber(NextFrameNumber, Options.FrameRate);

		// Add Tro offset to next alignment point and configurable offset
		StreamData.NextAlignmentPointNanosec = NextAlignmentNano;
		StreamData.NextScheduleTimeNanosec = NextAlignmentNano + TransmitOffsetNanosec + CVarRivermaxScheduleOffset.GetValueOnAnyThread();
		StreamData.NextAlignmentPointFrameNumber = NextFrameNumber;

		StreamData.bHasValidNextFrameNumber = bFoundValidTimings;
	}

	void FRivermaxOutputStream::CalculateNextScheduleTime_FrameCreation(uint64 CurrentClockTimeNanosec, uint64 CurrentFrameNumber)
	{
		double NextWaitTime = 0.0;
		if (StreamData.bHasValidNextFrameNumber == false)
		{
			StreamData.NextAlignmentPointNanosec = CurrentClockTimeNanosec;
			StreamData.NextScheduleTimeNanosec = StreamData.NextAlignmentPointNanosec + CVarRivermaxScheduleOffset.GetValueOnAnyThread();
			StreamData.NextAlignmentPointFrameNumber = CurrentFrameNumber;
			StreamData.bHasValidNextFrameNumber = true;
		}
		else
		{
			// In this mode, we just take last time we started to send and add a frame interval
			StreamData.NextAlignmentPointNanosec = StreamData.LastSendStartTimeNanoSec + StreamData.FrameFieldTimeIntervalNs;
			StreamData.NextScheduleTimeNanosec = StreamData.NextAlignmentPointNanosec + CVarRivermaxScheduleOffset.GetValueOnAnyThread();
			StreamData.NextAlignmentPointFrameNumber = UE::RivermaxCore::GetFrameNumber(StreamData.NextAlignmentPointNanosec, Options.FrameRate);
		}
	}

	void FRivermaxOutputStream::MarkFrameToBeSent(TSharedPtr<FRivermaxOutputFrame> ReadyFrame)
	{
		// Make frame available to be sent
		FScopeLock Lock(&FrameCriticalSection);
		ReadyFrame->ReadyTimestamp = RivermaxModule->GetRivermaxManager()->GetTime();
		AvailableFrames.Remove(ReadyFrame);
		FramesToSend.Add(MoveTemp(ReadyFrame));
		ReadyToSendEvent->Trigger();
	}

}


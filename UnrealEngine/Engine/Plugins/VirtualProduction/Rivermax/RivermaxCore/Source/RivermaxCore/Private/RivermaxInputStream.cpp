// Copyright Epic Games, Inc. All Rights Reserved.

#include "RivermaxInputStream.h"

#include "Async/Async.h"
#include "IRivermaxCoreModule.h"
#include "IRivermaxManager.h"
#include "Misc/ByteSwap.h"
#include "RivermaxLog.h"
#include "RivermaxUtils.h"

#if PLATFORM_WINDOWS
#include <WS2tcpip.h>
#endif

namespace UE::RivermaxCore::Private
{
	static TAutoConsoleVariable<int32> CVarUseIncompleteFrames(
		TEXT("Rivermax.Input.UseIncompleteFrames"),
		1,
		TEXT("Whether to keep incomplete frames in the case of missing packets or drop them."),
		ECVF_Default);


#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack(push, 1)
#endif
	struct FRivermaxRTPSampleRowData
	{
		uint32 ContributingSourceCount : 4;
		uint32 ExtensionBit : 1;
		uint32 PaddingBit : 1;
		uint32 Version : 2;
		uint32 PayloadType : 7;
		uint32 MarkerBit : 1;
		uint32 SequenceNumber : 16;
		uint32 Timestamp : 32;
		uint32 SynchronizationSource : 32;
		uint32 ExtendedSequenceNumber : 16;
		//SRD 1
		uint32 SRDLength1 : 16;
		uint32 SRDRowNumberHigh1 : 7;
		uint32 FieldIdentification1 : 1;
		uint32 SRDRowNumberLow1 : 8;
		uint32 SRDOffsetHigh1 : 7;
		uint32 ContinuationBit1 : 1;
		uint32 SRDOffsetLow1 : 8;
		//SRD 2
		uint32 SRDLength2 : 16;
		uint32 SRDRowNumberHigh2 : 7;
		uint32 FieldIdentification2 : 1;
		uint32 SRDRowNumberLow2 : 8;
		uint32 SRDOffsetHigh2 : 7;
		uint32 ContinuationBit2 : 1;
		uint32 SRDOffsetLow2 : 8;

		uint16 GetSrd1RowNumber() { return ((SRDRowNumberHigh1 << 8) | SRDRowNumberLow1); }
		uint16 GetSrd1Offset() { return ((SRDOffsetHigh1 << 8) | SRDOffsetLow1); }

		uint16 GetSrd2RowNumber() { return ((SRDRowNumberHigh2 << 8) | SRDRowNumberLow2); }
		uint16 GetSrd2Offset() { return ((SRDOffsetHigh2 << 8) | SRDOffsetLow2); }
	};
#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif


	uint8* GetRTPHeaderPointer(uint8* InHeader)
	{
		if (!InHeader /*|| RMAX_APP_PROTOCOL_PACKET == m_type*/)
		{
			return nullptr;
		}

		static constexpr uint32 ETH_TYPE_802_1Q = 0x8100;          /* 802.1Q VLAN Extended Header  */
		static constexpr uint32 RTP_HEADER_SIZE = 12;
		uint16* ETHProto = (uint16_t*)(InHeader + RTP_HEADER_SIZE);
		if (ETH_TYPE_802_1Q == ByteSwap(*ETHProto))
		{
			InHeader += 46; // 802 + 802.1Q + IP + UDP
		}
		else
		{
			InHeader += 42; // 802 + IP + UDP
		}
		return InHeader;
	}

	FRivermaxInputStream::FRivermaxInputStream()
	{

	}

	FRivermaxInputStream::~FRivermaxInputStream()
	{
		Uninitialize();
	}

	bool FRivermaxInputStream::Initialize(const FRivermaxStreamOptions& InOptions, IRivermaxInputStreamListener& InListener)
	{
		IRivermaxCoreModule& RivermaxModule = FModuleManager::LoadModuleChecked<IRivermaxCoreModule>(TEXT("RivermaxCore"));
		if (RivermaxModule.GetRivermaxManager()->IsInitialized() == false)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Can't create Rivermax Input Stream. Library isn't initialized."));
			return false;
		}

		Options = InOptions;
		Listener = &InListener;
		FormatInfo = FStandardVideoFormat::GetVideoFormatInfo(Options.PixelFormat);

		InitTaskFuture = Async(EAsyncExecution::TaskGraph, [this]()
		{
			// If the stream is trying to shutdown before the init task has even started, don't bother
			if (bIsShuttingDown)
			{
					return;
			}

			bool bWasSuccessful = false;
			int32 FlowId = 0; //todo configure

			//Configure local IP interface
			const rmax_in_stream_type StreamType = RMAX_RAW_PACKET;
			sockaddr_in RivermaxInterface;
			memset(&RivermaxInterface, 0, sizeof(RivermaxInterface));
			if (inet_pton(AF_INET, StringCast<ANSICHAR>(*Options.InterfaceAddress).Get(), &RivermaxInterface.sin_addr) != 1)
			{
				UE_LOG(LogRivermax, Warning, TEXT("inet_pton failed to %s"), *Options.InterfaceAddress);
			}
			else
			{
				RivermaxInterface.sin_family = AF_INET;

				//Configure Flow and destination IP (multicast)
				memset(&FlowAttribute, 0, sizeof(FlowAttribute));
				FlowAttribute.local_addr.sin_family = AF_INET;
				FlowAttribute.flow_id = FlowId;
				if (inet_pton(AF_INET, StringCast<ANSICHAR>(*Options.StreamAddress).Get(), &FlowAttribute.local_addr.sin_addr) != 1)
				{
					UE_LOG(LogRivermax, Warning, TEXT("inet_pton failed to %s"), *Options.StreamAddress);
				}
				else
				{
					FlowAttribute.local_addr.sin_port = ByteSwap((uint16)Options.Port);

					const rmax_in_buffer_attr_flags_t BufferAttributeFlags = RMAX_IN_BUFFER_ATTER_FLAG_NONE; //todo whether ordering is based on sequence or extended sequence
					uint32 BufferElement = 1 << 18;//todo number of packets to allocate memory for
					rmax_in_buffer_attr BufferAttributes;
					FMemory::Memset(&BufferAttributes, 0, sizeof(BufferAttributes));
					BufferAttributes.num_of_elements = BufferElement;
					BufferAttributes.attr_flags = BufferAttributeFlags;

					FMemory::Memset(&BufferConfiguration.DataMemory, 0, sizeof(BufferConfiguration.DataMemory));

					// Todo : Unreal doesn't send Payloads greater than 1280 but other devices could.
					// While consuming input chunks, we memcpy based on SRD data but SRD data could be greater than 
					// max_size so our loop should be updated to support that case.
					BufferConfiguration.DataMemory.min_size = 1500; 
					BufferConfiguration.DataMemory.max_size = 1500;
					BufferAttributes.data = &BufferConfiguration.DataMemory;

					FMemory::Memset(&BufferConfiguration.HeaderMemory, 0, sizeof(BufferConfiguration.HeaderMemory));
					BufferConfiguration.HeaderMemory.max_size = BufferConfiguration.HeaderMemory.min_size = BufferConfiguration.HeaderExpectedSize;
					BufferAttributes.hdr = &BufferConfiguration.HeaderMemory;

					rmax_status_t Status = rmax_in_query_buffer_size(StreamType, &RivermaxInterface, &BufferAttributes, &BufferConfiguration.PayloadSize, &BufferConfiguration.HeaderSize);
					if (Status == RMAX_OK)
					{
						const uint32 CacheLineSize = PLATFORM_CACHE_LINE_SIZE;

						BufferConfiguration.DataMemory.ptr = FMemory::Malloc(BufferConfiguration.PayloadSize, CacheLineSize);
						BufferConfiguration.HeaderMemory.ptr = FMemory::Malloc(BufferConfiguration.HeaderSize, CacheLineSize);

						//Buffers configured, now configure stream and attach flow
						const rmax_in_timestamp_format TimestampFormat = rmax_in_timestamp_format::RMAX_PACKET_TIMESTAMP_RAW_NANO; //how packets are stamped. counter or nanoseconds
						const rmax_in_flags InputFlags = rmax_in_flags::RMAX_IN_CREATE_STREAM_INFO_PER_PACKET; //default value for 2110 in example
						Status = rmax_in_create_stream(StreamType, &RivermaxInterface, &BufferAttributes, TimestampFormat, InputFlags, &StreamId);
						if (Status == RMAX_OK)
						{
							Status = rmax_in_attach_flow(StreamId, &FlowAttribute);
							if (Status == RMAX_OK)
							{
								bIsActive = true;
								RivermaxThread = FRunnableThread::Create(this, TEXT("Rivermax InputStream Thread"), 128 * 1024, TPri_AboveNormal, FPlatformAffinity::GetPoolThreadMask());
								bWasSuccessful = true;
							}
							else
							{
								UE_LOG(LogRivermax, Warning, TEXT("Could not attach flow to stream. Status: %d."), Status);
							}
						}
						else
						{
							UE_LOG(LogRivermax, Warning, TEXT("Could not create stream. Status: %d."), Status);
						}
					}
					else
					{
						UE_LOG(LogRivermax, Warning, TEXT("Could not query buffer size. Status: %d"), Status);
					}
				}
			}

			Listener->OnInitializationCompleted(bWasSuccessful);
		});
		
		return true;
	}

	void FRivermaxInputStream::Uninitialize()
	{
		bIsShuttingDown = true;

		//If init task is ongoing, wait till it's done
		if (InitTaskFuture.IsReady() == false)
		{
			InitTaskFuture.Wait();
		}

		if (RivermaxThread != nullptr)
		{
			Stop();
			RivermaxThread->Kill(true);
			delete RivermaxThread;
			RivermaxThread = nullptr;
			UE_LOG(LogRivermax, Log, TEXT("Rivermax Input stream has shutdown"));
		}
	}

	void FRivermaxInputStream::Process_AnyThread()
	{
		const size_t MinChunkSize = 0;
		const size_t MaxChunkSize = 5000;
		const int Timeout = 0;
		const int Flags = 0;
		rmax_in_completion Completion;
		FMemory::Memset(&Completion, 0, sizeof(Completion));
		rmax_status_t Status = rmax_in_get_next_chunk(StreamId, MinChunkSize, MaxChunkSize, Timeout, Flags, &Completion);
		if (Status == RMAX_OK)
		{
			ParseChunk(Completion);
		}
		else
		{
			UE_LOG(LogRivermax, Warning, TEXT("Rivermax Input stream failed to get next chunk. Status: %d"), Status);
		}
	}

	bool FRivermaxInputStream::Init()
	{
		return true;
	}

	uint32 FRivermaxInputStream::Run()
	{
		while (bIsActive)
		{
			Process_AnyThread();
			LogStats();
		}

		if (StreamId)
		{
			rmax_status_t Status = rmax_in_detach_flow(StreamId, &FlowAttribute);
			if (Status != RMAX_OK)
			{
				UE_LOG(LogRivermax, Warning, TEXT("Failed to detach rivermax flow %d from input stream %d. Status: %d"), FlowAttribute.flow_id, StreamId, Status);
			}

			Status = rmax_in_destroy_stream(StreamId);

			if (Status != RMAX_OK)
			{
				UE_LOG(LogRivermax, Warning, TEXT("Failed to destroy input stream %d correctly. Status: %d"), StreamId, Status);
			}
		}

		return 0;
	}

	void FRivermaxInputStream::Stop()
	{
		bIsActive = false;
	}

	void FRivermaxInputStream::Exit()
	{

	}

	bool FRivermaxInputStream::GetRTPParameter(uint8* InputRTP, FRTPParameter& OutParameter)
	{
		OutParameter.Timestamp = 0;
		
		const uint8* RTPHeaderPtr = GetRTPHeaderPointer(InputRTP);
		if (((RTPHeaderPtr[0] & 0xC0) != 0x80))
		{
			return false;
		}

		OutParameter.SequencerNumber = RTPHeaderPtr[3] | RTPHeaderPtr[2] << 8;
		if (IsExtendedSequenceNumber()) //todo if 2022 is supported extended sequence number is supported in other ways
		{
			OutParameter.SequencerNumber |= RTPHeaderPtr[12] << 24 | RTPHeaderPtr[13] << 16;
			OutParameter.bIsFBit = !!(RTPHeaderPtr[16] & 0x80);
		}
		OutParameter.Timestamp = ByteSwap(*(uint32*)((RTPHeaderPtr)+4));
		OutParameter.bIsMBit = !!(RTPHeaderPtr[1] & 0x80);
		return true;
	}

	void FRivermaxInputStream::ParseChunk(const rmax_in_completion& Completion)
	{
		
		for (uint64 StrideIndex = 0; StrideIndex < Completion.chunk_size; ++StrideIndex)
		{
			++StreamStats.ChunksReceived;
		
			ensure(Completion.hdr_ptr);
			if (Completion.hdr_ptr == nullptr)
			{
				break;
			}

			uint8* HeaderPtr = (uint8*)Completion.hdr_ptr + StrideIndex * (size_t)BufferConfiguration.HeaderMemory.stride_size; //when using RMAX_RAW_PACKET then header is preceded by net header
			uint8* DataPtr = (uint8*)Completion.data_ptr + StrideIndex * (size_t)BufferConfiguration.DataMemory.stride_size; // The payload is our data
			uint32 DataOffset = 0;

			if (Completion.packet_info_arr[StrideIndex].data_size)
			{
				FRTPParameter Parameter;
				const bool bIsValid = GetRTPParameter(HeaderPtr, Parameter);
				if (bIsValid)
				{
					if (bIsFirstFrameReceived)
					{
						StreamStats.BytesReceived += Completion.packet_info_arr[StrideIndex].data_size + Completion.packet_info_arr[StrideIndex].hdr_size;

						uint64 LastSequenceNumberIncremented = StreamData.LastSequenceNumber + 1;
						if (IsExtendedSequenceNumber() == false)
						{
							LastSequenceNumberIncremented = LastSequenceNumberIncremented & 0xFFFF;
						}

						bool bCanProcessChunk = true;
						const uint64 LostPackets = ((uint64)Parameter.SequencerNumber + 0x100000000 - LastSequenceNumberIncremented) & 0xFFFFFFFF;
						if (LostPackets > 0)
						{
							bCanProcessChunk = false;
							StreamData.WritingOffset = 0;
							StreamData.ReceivedSize = 0;
							++StreamStats.TotalPacketLossCount;
							++StreamStats.FramePacketLossCount;
							
							UE_LOG(LogRivermax, Warning, TEXT("Lost %llu packets"), LostPackets);
						}

						StreamData.LastSequenceNumber = Parameter.SequencerNumber;

						//if flags are RMAX_IN_CREATE_STREAM_INFO_PER_PACKET todo 
						{
							if (FlowAttribute.flow_id && Completion.packet_info_arr[StrideIndex].flow_id != FlowAttribute.flow_id)
							{
								UE_LOG(LogRivermax, Error, TEXT("Received data from unexpected FlowId '%d'. Expected '%d'."), Completion.packet_info_arr[StrideIndex].flow_id, FlowAttribute.flow_id);
							}
						}

						if (bCanProcessChunk)
						{
							const FRivermaxRTPSampleRowData* HeaderStart = reinterpret_cast<FRivermaxRTPSampleRowData*>(GetRTPHeaderPointer(HeaderPtr));

							uint16 SizeToCopy = ByteSwap((uint16)HeaderStart->SRDLength1);
							FMemory::Memcpy(&StreamData.CurrentFrame[StreamData.WritingOffset], &DataPtr[DataOffset], SizeToCopy);
							StreamData.WritingOffset += SizeToCopy;
							StreamData.ReceivedSize += SizeToCopy;

							// todo Warning !!! GPUDirect doesn't support more than 1 SRD
							if (HeaderStart->ContinuationBit1)
							{
								DataOffset += SizeToCopy;
								SizeToCopy = ByteSwap((uint16)HeaderStart->SRDLength2);
								FMemory::Memcpy(&StreamData.CurrentFrame[StreamData.WritingOffset], &DataPtr[DataOffset], SizeToCopy);
								StreamData.WritingOffset += SizeToCopy;
								StreamData.ReceivedSize += SizeToCopy;
							}

							if (StreamData.ReceivedSize > StreamData.ExpectedSize)
							{
								UE_LOG(LogRivermax, Warning, TEXT("Received too much data (%d). Expected %d but received (%d)"), StreamData.ReceivedSize - StreamData.ExpectedSize, StreamData.ExpectedSize, StreamData.ReceivedSize);
								StreamData.WritingOffset = 0;
								StreamData.ReceivedSize = 0;
								++StreamStats.BiggerFramesCount;
							}
							else if (HeaderStart->MarkerBit)
							{
								if (StreamData.ReceivedSize == StreamData.ExpectedSize)
								{
									TRACE_BOOKMARK(TEXT("RMAX RX %llu"), Parameter.Timestamp);
									TRACE_CPUPROFILER_EVENT_SCOPE(FRivermaxInputStream::ProcessingReceivedFrame)

									++StreamStats.FramesReceived;

									FRivermaxInputVideoFrameDescriptor Descriptor;
									Descriptor.Width = Options.Resolution.X;
									Descriptor.Height = Options.Resolution.Y;
									Descriptor.Stride = Options.AlignedResolution.X / FormatInfo.PixelGroupCoverage * FormatInfo.PixelGroupSize;
									FRivermaxInputVideoFrameReception NewFrame;
									NewFrame.VideoBuffer = StreamData.CurrentFrame;
									Listener->OnVideoFrameReceived(Descriptor, NewFrame);
									PrepareNextFrame();
								}
								else
								{
									UE_LOG(LogRivermax, Warning, TEXT("End of frame received (Marker bit) but not enough data was received (missing %d). Expected %d but received (%d)"), StreamData.ExpectedSize - StreamData.ReceivedSize, StreamData.ExpectedSize, StreamData.ReceivedSize);
									StreamData.WritingOffset = 0;
									StreamData.ReceivedSize = 0;
									++StreamStats.InvalidFramesCount;
								}
								
								StreamStats.FramePacketLossCount = 0;
								++StreamStats.EndOfFrameReceived;
							}
						}

					}
					else
					{
						if (Parameter.bIsMBit)
						{
							StreamData.LastSequenceNumber = Parameter.SequencerNumber;
							bIsFirstFrameReceived = true;
							PrepareNextFrame();
						}
					}

					//todo why  Reset RTP header
					*(uint32_t*)GetRTPHeaderPointer(HeaderPtr) = 0;
				}
				else
				{
					++StreamStats.InvalidHeadercount;
				}
			}
			else
			{
				++StreamStats.EmptyCompletionCount;
			}

			//todo why?
			Completion.packet_info_arr[StrideIndex].data_size = 0;
		}
	}

	bool FRivermaxInputStream::IsExtendedSequenceNumber() const
	{
		return RivermaxStreamType == ERivermaxStreamType::VIDEO_2110_20_STREAM;
	}

	void FRivermaxInputStream::PrepareNextFrame()
	{
		using namespace UE::RivermaxCore::Private::Utils;

		FRivermaxInputVideoFrameDescriptor Descriptor;
		FRivermaxInputVideoFrameRequest Request;
		const int32 Stride = Options.AlignedResolution.X / FormatInfo.PixelGroupCoverage * FormatInfo.PixelGroupSize;
		Descriptor.VideoBufferSize = Options.Resolution.Y * Stride;
		Listener->OnVideoFrameRequested(Descriptor, Request);
		StreamData.CurrentFrame = Request.VideoBuffer;
		StreamData.WritingOffset = 0;
		StreamData.ReceivedSize = 0;
		StreamData.ExpectedSize = Descriptor.VideoBufferSize;
	}

	void FRivermaxInputStream::LogStats()
	{
		static constexpr double LoggingInterval = 1.0;

		const double CurrentTime = FPlatformTime::Seconds();
		if (CurrentTime - LastLoggingTimestamp >= LoggingInterval)
		{
			LastLoggingTimestamp = CurrentTime;
			UE_LOG(LogRivermax, Verbose, TEXT("Stream %d stats: FrameCount: %llu, EndOfFrame: %llu, Chunks: %llu, Bytes: %llu, PacketLossInFrame: %llu, TotalPacketLoss: %llu, BiggerFrames: %llu, InvalidFrames: %llu, InvalidHeader: %llu, EmptyCompletion: %llu")
			, StreamId
			, StreamStats.FramesReceived
			, StreamStats.EndOfFrameReceived
			, StreamStats.ChunksReceived
			, StreamStats.BytesReceived
			, StreamStats.FramePacketLossCount
			, StreamStats.TotalPacketLossCount
			, StreamStats.BiggerFramesCount
			, StreamStats.InvalidFramesCount
			, StreamStats.InvalidHeadercount
			, StreamStats.EmptyCompletionCount
			);
		}
	}

}


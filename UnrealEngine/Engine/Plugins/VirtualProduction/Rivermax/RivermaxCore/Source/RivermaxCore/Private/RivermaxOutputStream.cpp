// Copyright Epic Games, Inc. All Rights Reserved.

#include "RivermaxOutputStream.h"

#include "Async/Async.h"
#include "CudaModule.h"
#include "IRivermaxCoreModule.h"
#include "IRivermaxBoundaryMonitor.h"
#include "IRivermaxManager.h"
#include "Misc/ByteSwap.h"
#include "RivermaxFrameAllocator.h"
#include "RivermaxFrameManager.h"
#include "RivermaxLog.h"
#include "RivermaxPTPUtils.h"
#include "RivermaxTracingUtils.h"
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

	static TAutoConsoleVariable<float> CVarRivermaxOutputTROOverride(
		TEXT("Rivermax.Output.TRO"), 0,
		TEXT("If not 0, overrides transmit offset calculation (TRO) based on  frame rate and resolution with a fixed value. Value in seconds."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRivermaxOutputSkipSchedulingCutOffTime(
		TEXT("Rivermax.Output.Scheduling.SkipCutoff"), 50,
		TEXT("Required time in microseconds from scheduling time to avoid skipping an interval."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRivermaxOutputForceImmediateSchedulingThreshold(
		TEXT("Rivermax.Output.Scheduling.ForceImmediateCutoff"), 600,
		TEXT("Required time in nanoseconds from scheduling time before we clamp to do it immediately."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRivermaxOutputMaxFrameMemorySliceCount(
		TEXT("Rivermax.Output.FrameSliceCount"), 30,
		TEXT("Max number of memcopies done per frame when using intermediate buffer. As frame gets bigger, we can't do a single memcopy or timings will be broken. Can be smaller in order to fit inside chunk count."),
		ECVF_Default);

	static TAutoConsoleVariable<bool> CVarRivermaxOutputEnableIntermediateBuffer(
		TEXT("Rivermax.Output.Alignment.EnableIntermediateBuffer"), true,
		TEXT("Uses an intermediate buffer used by Rivermax when sending data out.\n")
		TEXT("During scheduling, captured frame data will be copied over intermediate buffer.\n")
		TEXT("Only applies to alignment points scheduling mode."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRivermaxOutputUseSingleMemblock(
		TEXT("Rivermax.Output.UseSingleMemblock"), 1,
		TEXT("Configures Rivermax stream to use a single memblock potentially improving SDK performance."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRivermaxOutputRandomDelay(
		TEXT("Rivermax.Output.TriggerRandomDelay"), 0,
		TEXT("Will cause a delay of variable amount of time when next frame is sent."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRivermaxOutputEnableTimingProtection(
		TEXT("Rivermax.Output.Scheduling.EnableTimingProtection"), 1,
		TEXT("Whether timing verification is done on commit to avoid misalignment. Next frame interval is skipped if it happens."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRivermaxOutputMemcopyChunkSpacing(
		TEXT("Rivermax.Output.Scheduling.MemcopyChunkSpacing"), 10,
		TEXT("Number of chunks between each memcopy to help with timing for different frame format."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRivermaxOutputEnableTimeCriticalThread(
		TEXT("Rivermax.Output.EnableTimeCriticalThread"), 0,
		TEXT("Whether to set output thread as time critical."),
		ECVF_Default);

	static TAutoConsoleVariable<bool> CVarRivermaxOutputPrefillRTPHeaders(
		TEXT("Rivermax.Output.PrefillRTPHeaders"), true,
		TEXT("Optimization used to prefill every RTP headers with known data."),
		ECVF_Default);

	static TAutoConsoleVariable<bool> CVarRivermaxOutputForceSkip(
		TEXT("Rivermax.Output.ForceSkip"), false,
		TEXT("Used to prevent enforced blank window when multiplier is used to prevent timing issues.")
		TEXT("Only affects alignment point method."),
		ECVF_Default);

	static TAutoConsoleVariable<float> CVarRivermaxOutputFrameRateMultiplier(
		TEXT("Rivermax.Output.FrameRateMultiplier"), 1.0,
		TEXT("Multiplier applied to desired output frame rate in order to reduce time it takes to send out a frame and slowly correct misalignment that could happen."),
		ECVF_Default);

	static bool GbTriggerRandomTimingIssue = false;
	FAutoConsoleVariableRef CVarTriggerRandomTimingIssue(
		TEXT("Rivermax.Sync.TriggerRandomTimingIssue")
		, UE::RivermaxCore::Private::GbTriggerRandomTimingIssue
		, TEXT("Randomly triggers a timing issue to test self repair."), ECVF_Cheat);


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

	/** Sidecar used when copying  parts of frame data while scheduling */
	struct FMemChunkCopiedInfo : public FBaseDataCopySideCar
	{
		/** Time covered inside frame interval including last chunk copied. */
		uint64 TimeCovered = 0;
	};

	struct FRTPHeaderPrefiller
	{
		FRTPHeaderPrefiller(FRivermaxOutputStream& InRmaxOutputStream)
			: Stream(InRmaxOutputStream)
		{
			RunningSRDOffsetPerFrame.SetNumZeroed(Stream.Options.NumberOfBuffers);
			RunningLineNumberPerFrame.SetNumZeroed(Stream.Options.NumberOfBuffers);
		}

		void Update(uint32 PacketIndex)
		{
			using namespace UE::RivermaxCore::Private::Utils;

			const uint32 Width = Stream.Options.AlignedResolution.X;
			const uint32 Height = Stream.Options.AlignedResolution.Y;
			const int32 PacketCount = Stream.StreamMemory.ChunksPerFrameField * Stream.StreamMemory.PacketsPerChunk;
			uint32 BufferIndex = 0;
			for (int32 MemblockIndex = 0; MemblockIndex < Stream.StreamMemory.RTPHeaders.Num(); ++MemblockIndex)
			{
				for (uint32 FrameInBlockIndex = 0; FrameInBlockIndex < Stream.StreamMemory.FramesFieldPerMemoryBlock; ++FrameInBlockIndex)
				{
					uint16& SRDOffset = RunningSRDOffsetPerFrame[BufferIndex];
					uint32& LineNumber = RunningLineNumberPerFrame[BufferIndex];
					const uint32 HeaderIndex = PacketIndex + (FrameInBlockIndex * PacketCount);
					if (ensure(Stream.StreamMemory.RTPHeaders[MemblockIndex].IsValidIndex(HeaderIndex)))
					{
						FRawRTPHeader& CurrentHeader = Stream.StreamMemory.RTPHeaders[MemblockIndex][HeaderIndex];
						CurrentHeader = {};
						CurrentHeader.Version = 2;
						CurrentHeader.PaddingBit = 0;
						CurrentHeader.ExtensionBit = 0;
						CurrentHeader.PayloadType = 96; //Payload type should probably be inferred from SDP

						//2110 specific header
						CurrentHeader.SynchronizationSource = ByteSwap((uint32)0x0eb51dbd);  // Should Unreal has its own synch source ID

						{
							if (PacketIndex + 1 == Stream.StreamMemory.PacketsPerFrame)
							{
								CurrentHeader.MarkerBit = 1; // last packet in frame (Marker)
							}

							// Verify if payload size exceeds line 
							const uint32 CurrentPayloadSize = Stream.StreamMemory.PayloadSizes[PacketIndex];

							const uint32 LineSizeOffset = ((SRDOffset / Stream.FormatInfo.PixelGroupCoverage) * Stream.FormatInfo.PixelGroupSize);
							const uint32 LineSize = ((Width / Stream.FormatInfo.PixelGroupCoverage) * Stream.FormatInfo.PixelGroupSize);

							const uint16 SRD1Length = FMath::Min(LineSize - LineSizeOffset, CurrentPayloadSize);
							const uint16 SRD1PixelCount = SRD1Length / Stream.FormatInfo.PixelGroupSize * Stream.FormatInfo.PixelGroupCoverage;
							uint16 SRD2Length = SRD1Length < CurrentPayloadSize ? CurrentPayloadSize - SRD1Length : 0;
							if (SRD2Length && LineNumber == (Height - 1))
							{
								SRD2Length = 0;
							}

							CurrentHeader.SRD1Length = ByteSwap(SRD1Length);
							CurrentHeader.SetSrd1RowNumber(LineNumber); //todo divide by 2 if interlaced
							CurrentHeader.FieldIdentification1 = 0; //todo when fields are sent for interlace
							CurrentHeader.SetSrd1Offset(SRDOffset);

							SRDOffset += SRD1PixelCount;
							if (SRDOffset >= Width)
							{
								SRDOffset = 0;
								++LineNumber;
							}

							if (SRD2Length > 0)
							{
								CurrentHeader.SRD2Length = ByteSwap(SRD2Length);

								CurrentHeader.ContinuationBit1 = 1;
								CurrentHeader.FieldIdentification2 = 0;
								CurrentHeader.SetSrd2RowNumber(LineNumber);
								CurrentHeader.SetSrd2Offset(SRDOffset);

								const uint16 SRD2PixelCount = SRD2Length / Stream.FormatInfo.PixelGroupSize * Stream.FormatInfo.PixelGroupCoverage;
								SRDOffset += SRD2PixelCount;
								if (SRDOffset >= Width)
								{
									SRDOffset = 0;
									++LineNumber;
								}
							}
						}
					}

					++BufferIndex;
				}
			}
		}

	private:
		TArray<uint16> RunningSRDOffsetPerFrame;
		TArray<uint32> RunningLineNumberPerFrame;
		FRivermaxOutputStream& Stream;
	};

	FRivermaxOutputStream::~FRivermaxOutputStream()
	{
		Uninitialize();
	}

	bool FRivermaxOutputStream::Initialize(const FRivermaxOutputStreamOptions& InOptions, IRivermaxOutputStreamListener& InListener)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FRivermaxOutputStream::Initialize);

		RivermaxModule = FModuleManager::GetModulePtr<IRivermaxCoreModule>(TEXT("RivermaxCore"));
		if (RivermaxModule->GetRivermaxManager()->ValidateLibraryIsLoaded() == false)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Can't create Rivermax Output Stream. Library isn't initialized."));
			return false;
		}

		Options = InOptions;
		Listener = &InListener;
		FormatInfo = FStandardVideoFormat::GetVideoFormatInfo(Options.PixelFormat);

		if (IConsoleVariable* CvarDelay = CVarRivermaxOutputRandomDelay.AsVariable())
		{
			CvarDelay->OnChangedDelegate().AddRaw(this, &FRivermaxOutputStream::OnCVarRandomDelayChanged);
		}

		const uint32 FrameSize = GetStride() * Options.AlignedResolution.Y;
		if (FrameSize == 0)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Can't create Rivermax Output Stream. Frame size of 0 is invalid. Verify resolution."));
			return false;
		}
		
		CacheCVarValues();

		// Verify resolution for sampling type
		if (Options.AlignedResolution.X % FormatInfo.PixelGroupCoverage != 0)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Can't create Rivermax Output Stream. Aligned horizontal resolution of %d doesn't align with pixel group coverage of %d."), Options.AlignedResolution.X, FormatInfo.PixelGroupCoverage);
			return false;
		}
		
		// Cache API entry point
		CachedAPI = RivermaxModule->GetRivermaxManager()->GetApi();
		checkSlow(CachedAPI);

		if (!InitializeStreamMemoryConfig())
		{
			return false;
		}

		InitializeTimingProtections();

		// Enable frame boundary monitoring
		MonitoringGuid = RivermaxModule->GetRivermaxBoundaryMonitor().StartMonitoring(Options.FrameRate);

		InitializationFuture = Async(EAsyncExecution::TaskGraph, [this]()
		{
			TAnsiStringBuilder<2048> SDPDescription;
			UE::RivermaxCore::Private::Utils::StreamOptionsToSDPDescription(Options, StreamMemory.FrameRateMultiplier, SDPDescription);

			// Create Rivermax stream using memory configuration
			{
				// Initialize rivermax output stream with desired config
				const uint32 NumberPacketsPerFrame = StreamMemory.PacketsPerFrame;


				rmx_output_media_stream_params OutputStreamParameters;
				CachedAPI->rmx_output_media_init(&OutputStreamParameters);
				CachedAPI->rmx_output_media_set_sdp(&OutputStreamParameters, SDPDescription.GetData());
				CachedAPI->rmx_output_media_assign_mem_blocks(&OutputStreamParameters, StreamMemory.MemoryBlocks.GetData(), StreamMemory.MemoryBlocks.Num());
				
				constexpr uint8 PCPAttribute = 0; //PCP attribute for QOS layer 2
				CachedAPI->rmx_output_media_set_pcp(&OutputStreamParameters, PCPAttribute);
				constexpr uint8 DSCP = 34; //For AES67, RTP Media streams' DSCP value is 34
				CachedAPI->rmx_output_media_set_dscp(&OutputStreamParameters, DSCP);
				constexpr uint8 ECN = 0; //Explicit congestion notification
				CachedAPI->rmx_output_media_set_ecn(&OutputStreamParameters, ECN);

				// Sometimes, chunk count will have more packets than needed so last ones might be 0 sized. 
				// Verify if new API work with the actual amount of packet with data or it needs the padded version 
				CachedAPI->rmx_output_media_set_packets_per_frame(&OutputStreamParameters, StreamMemory.ChunksPerFrameField * StreamMemory.PacketsPerChunk);

				CachedAPI->rmx_output_media_set_packets_per_chunk(&OutputStreamParameters, StreamMemory.PacketsPerChunk);
				CachedAPI->rmx_output_media_set_stride_size(&OutputStreamParameters, StreamMemory.DataBlockID, StreamMemory.PayloadSize);
				CachedAPI->rmx_output_media_set_stride_size(&OutputStreamParameters, StreamMemory.HeaderBlockID, StreamMemory.HeaderStrideSize);

				rmx_stream_id NewId;
				rmx_status Status = CachedAPI->rmx_output_media_create_stream(&OutputStreamParameters, &NewId);
				if (Status == RMX_OK)
				{
					struct sockaddr_in SourceAddress;
					FMemory::Memset(&SourceAddress, 0, sizeof(SourceAddress));
					
					rmx_output_media_context MediaContext;
					CachedAPI->rmx_output_media_init_context(&MediaContext, NewId);

					constexpr size_t SDPMediaIndex = 0;
					CachedAPI->rmx_output_media_set_context_block(&MediaContext, SDPMediaIndex);
					Status = CachedAPI->rmx_output_media_get_local_address(&MediaContext,	reinterpret_cast<sockaddr*>(&SourceAddress));
					if (Status == RMX_OK) 
					{
						struct sockaddr_in DestinationAddress;
						FMemory::Memset(&DestinationAddress, 0, sizeof(DestinationAddress));

						Status = CachedAPI->rmx_output_media_get_remote_address(&MediaContext, reinterpret_cast<sockaddr*>(&DestinationAddress));
						if (Status == RMX_OK)
						{
							StreamId = NewId;

							CachedAPI->rmx_output_media_init_chunk_handle(&StreamData.ChunkHandle, StreamId);

							StreamData.FrameFieldTimeIntervalNs = 1E9 / Options.FrameRate.AsDecimal();
							InitializeStreamTimingSettings();

							TStringBuilder<512> StreamDescription;
							StreamDescription.Appendf(TEXT("Output stream started sending on stream %s:%d using interface %s%s. ")
								, *Options.StreamAddress
								, Options.Port
								, *Options.InterfaceAddress
								, bUseGPUDirect ? TEXT(" using GPUDirect") : TEXT(""));

							StreamDescription.Appendf(TEXT("Settings: Resolution = %dx%d, "), Options.AlignedResolution.X, Options.AlignedResolution.Y);
							StreamDescription.Appendf(TEXT("FrameRate = %s, "), *Options.FrameRate.ToPrettyText().ToString());
							StreamDescription.Appendf(TEXT("Pixel format = %s, "), LexToString(Options.PixelFormat));
							StreamDescription.Appendf(TEXT("Alignment = %s, "), LexToString(Options.AlignmentMode));
							StreamDescription.Appendf(TEXT("Framelocking = %s."), LexToString(Options.FrameLockingMode));

							UE_LOG(LogRivermax, Display, TEXT("%s"), *FString(StreamDescription));

							UE_LOG(LogRivermax, Verbose, TEXT("Created stream using SDP:\n%S"), SDPDescription.GetData());

							bIsActive = true;
							RivermaxThread.Reset(FRunnableThread::Create(this, TEXT("Rmax OutputStream Thread"), 128 * 1024, TPri_TimeCritical, FPlatformAffinity::GetPoolThreadMask()));
						}
						else
						{
							UE_LOG(LogRivermax, Warning, TEXT("Failed querying destination address. Output Stream won't be created. Status: %d"), Status);
						}
					}
					else
					{
						UE_LOG(LogRivermax, Warning, TEXT("Failed querying local address. Output Stream won't be created. Status: %d"), Status);
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
		if (!InitializationFuture.IsReady())
		{
			InitializationFuture.Wait();
		}

		if (RivermaxThread != nullptr)
		{
			Stop();
			
			FrameAvailableSignal->Trigger();
			FrameReadyToSendSignal->Trigger();
			RivermaxThread->Kill(true);
			RivermaxThread.Reset();

			CleanupFrameManagement();

			RivermaxModule->GetRivermaxBoundaryMonitor().StopMonitoring(MonitoringGuid, Options.FrameRate);
			
			UE_LOG(LogRivermax, Log, TEXT("Rivermax Output stream has shutdown"));
		}

		if (IConsoleVariable* CvarDelay = CVarRivermaxOutputRandomDelay.AsVariable())
		{
			CvarDelay->OnChangedDelegate().RemoveAll(this);
		}
	}

	bool FRivermaxOutputStream::PushVideoFrame(const FRivermaxOutputVideoFrameInfo& NewFrame)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FRivermaxOutputStream::PushVideoFrame);

		FRivermaxOutputVideoFrameInfo CopyInfo;
		CopyInfo.Stride = GetStride();
		CopyInfo.Height = Options.AlignedResolution.Y;
		CopyInfo.FrameIdentifier = NewFrame.FrameIdentifier;
		CopyInfo.GPUBuffer = NewFrame.GPUBuffer;
		CopyInfo.VideoBuffer = NewFrame.VideoBuffer;
		CopyInfo.Width = NewFrame.Width;
		return FrameManager->SetFrameData(CopyInfo);
	}

	void FRivermaxOutputStream::Process_AnyThread()
	{
		using namespace UE::RivermaxCore::Private::Utils;

		// Wait for the next time a frame should be sent (based on frame interval)
		// if interm buffer is used (alignment points) and a frame is ready before frame interval
		//		Start copying data into next memory block from the intermediate buffer
		//		At frame interval:
		//			Release last sent frame if any
		//			Make next frame the one being sent
		// Otherwise
		//		FrameCreation:
		//			Release last sent frame if any
		//			Wait for a new frame to be available
		//		Alignment points:
		//			Release last sent frame if any
		// 
		// Send frame
		//		Get next chunk
		//		Continue copy to intermediate buffer if required
		//		Fill dynamic data for RTP headers of next chunk
		//		Commit next chunk
		// 
		// Restart
		{
			bool bCanEarlyCopy = false;
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(RmaxOut::Wait);
				bCanEarlyCopy = WaitForNextRound();
			}

			if(bIsActive && bCanEarlyCopy)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(RmaxOut::PreprocessNextFrame);
				PreprocessNextFrame();
			}
			else
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(RmaxOut::PrepareNextFrame);
				PrepareNextFrame();
			}

			// At this point, if there is no frame to send, move on to wait for next round
			if (CurrentFrame.IsValid() && bIsActive)
			{
				SendFrame();
				
				// If frame that was just sent failed timing requirements, we have to tell rivermax to skip 0 chunks 
				// in order to reset internal states. Otherwise, scheduling time / Tro isn't respected next time we schedule.
				if (CurrentFrame->bCaughtTimingIssue)
				{
					++Stats.TimingIssueCount;
				}

				if (CurrentFrame->bCaughtTimingIssue || StreamMemory.bAlwaysSkipChunk)
				{
					constexpr uint64 ChunksToSkip = 0;
					SkipChunks(ChunksToSkip);
				}
			}

			Stats.TotalChunkRetries += Stats.LastFrameChunkRetries;
			StreamData.bHasFrameFirstChunkBeenFetched = false;
			Stats.LastFrameChunkRetries = 0;
		}
	}

	void FRivermaxOutputStream::PreprocessNextFrame()
	{
		checkSlow(Options.AlignmentMode == ERivermaxAlignmentMode::AlignmentPoint);

		TSharedPtr<FRivermaxOutputFrame> NextFrameToSend = FrameManager->GetReadyFrame();
		if (ensure(NextFrameToSend))
		{
			InitializeNextFrame(NextFrameToSend);

			// Now that we have the next frame, we can start copying data into it.
			// We can't get chunks since commit will only commit the chunks returned by last call to get next chunk. 
			// So, we calculate the next data and header pointer based on the current frame. 
			const uint64 CurrentRmaxTimeNanosec = RivermaxModule->GetRivermaxManager()->GetTime();
			const double CurrentPlatformTime = FPlatformTime::Seconds();
			const double TimeLeftSec = double(StreamData.NextAlignmentPointNanosec - CurrentRmaxTimeNanosec) / 1E9;
			const double TargetPlatformTimeSec = CurrentPlatformTime + TimeLeftSec;
			if (CurrentRmaxTimeNanosec < StreamData.NextAlignmentPointNanosec)
			{
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(RmaxOut::CopyFrame);

					bool bHasDataToCopy = true;
					while (FPlatformTime::Seconds() < TargetPlatformTimeSec && bHasDataToCopy && bIsActive)
					{
						bHasDataToCopy = CopyFrameData(NextFrameToSend, reinterpret_cast<uint8*>(StreamMemory.BufferAddresses[StreamData.ExpectedFrameIndex]));
						
					}
				}

				const double PostCopyTimeLeftSec = TargetPlatformTimeSec - FPlatformTime::Seconds();
				if (PostCopyTimeLeftSec > 0)
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(RmaxOut::Waiting);
					static constexpr float YieldTimeSec = 2.0f / 1000;

					while (FPlatformTime::Seconds() < TargetPlatformTimeSec && bIsActive)
					{
						const double TimeLeft = TargetPlatformTimeSec - FPlatformTime::Seconds();
						const double SleepTime = TimeLeft > YieldTimeSec ? TimeLeft - YieldTimeSec : 0.0;
						FPlatformProcess::SleepNoStats(SleepTime);
					}
				}
			}

			{
				TRACE_CPUPROFILER_EVENT_SCOPE(RmaxOut::WrappingUp);
				if (CurrentFrame)
				{	
					constexpr bool bReleaseFrame = true;
					CompleteCurrentFrame(bReleaseFrame);
				}

				// Make the next frame to send the current one and update its state
				CurrentFrame = MoveTemp(NextFrameToSend);
				FrameManager->MarkAsSending(CurrentFrame);
			}
		}
		else
		{
			UE_LOG(LogRivermax, Error, TEXT("Unexpected error, no frame was available."));
			Listener->OnStreamError();
			Stop();
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
		StreamMemory.FramesFieldPerMemoryBlock = CachedCVars.bUseSingleMemblock ? Options.NumberOfBuffers : 1;

		// Chunk count won't necessarily align with the number of packets required. We need an integer amount of chunks to initialize our stream
		// and calculate how many packets that represents. Rivermax will expect the payload/header array to be that size. It just means that
		// we will mark the extra packets as 0 size.
		StreamMemory.ChunksPerFrameField = FMath::CeilToInt32((float)StreamMemory.PacketsPerFrame / StreamMemory.PacketsPerChunk);
		const uint64 RealPacketsPerFrame = StreamMemory.ChunksPerFrameField * StreamMemory.PacketsPerChunk;
		StreamMemory.PacketsPerMemoryBlock = RealPacketsPerFrame * StreamMemory.FramesFieldPerMemoryBlock;
		StreamMemory.ChunksPerMemoryBlock = StreamMemory.FramesFieldPerMemoryBlock * StreamMemory.ChunksPerFrameField;
		StreamMemory.MemoryBlockCount = Options.NumberOfBuffers / StreamMemory.FramesFieldPerMemoryBlock;

		// Setup arrays with the right sizes so we can give pointers to rivermax
		StreamMemory.RTPHeaders.SetNumZeroed(StreamMemory.MemoryBlockCount);
		StreamMemory.PayloadSizes.SetNumZeroed(StreamMemory.PacketsPerMemoryBlock);
		StreamMemory.HeaderSizes.SetNumZeroed(StreamMemory.PacketsPerMemoryBlock);
		StreamMemory.HeaderStrideSize = sizeof(FRawRTPHeader);

		if (!SetupFrameManagement())
		{
			return false;
		}

		FRTPHeaderPrefiller RTPFiller(*this);

		StreamMemory.MemoryBlocks.SetNumZeroed(StreamMemory.MemoryBlockCount);
		CachedAPI->rmx_output_media_init_mem_blocks(StreamMemory.MemoryBlocks.GetData(), StreamMemory.MemoryBlockCount);
		for (uint32 BlockIndex = 0; BlockIndex < StreamMemory.MemoryBlockCount; ++BlockIndex)
		{
			rmx_output_media_mem_block& Block = StreamMemory.MemoryBlocks[BlockIndex];
			CachedAPI->rmx_output_media_set_chunk_count(&Block, StreamMemory.ChunksPerMemoryBlock);

			// We have two sub block, header and data
			constexpr uint8 SubBlockCount = 2;
			CachedAPI->rmx_output_media_set_sub_block_count(&Block, SubBlockCount);

			// Describe Header block
			CachedAPI->rmx_output_media_set_packet_layout(&Block, StreamMemory.HeaderBlockID, StreamMemory.HeaderSizes.GetData());

			// Describe Data block
			CachedAPI->rmx_output_media_set_packet_layout(&Block, StreamMemory.DataBlockID, StreamMemory.PayloadSizes.GetData());

			rmx_mem_multi_key_region* DataMemory;
			DataMemory = CachedAPI->rmx_output_media_get_dup_sub_block(&Block, StreamMemory.DataBlockID);
			if (DataMemory == nullptr)
			{
				UE_LOG(LogRivermax, Warning, TEXT("Faild to get payload memory block. Output stream won't be created."));
				return false;
			}

			// If intermediate buffer is used, we setup rmax memblock to use that address. Otherwise, we map it to our actual frame's address
			if (StreamMemory.bUseIntermediateBuffer)
			{
				DataMemory->addr = Allocator->GetFrameAddress(BlockIndex);
			}
			else
			{
				DataMemory->addr = FrameManager->GetFrame(BlockIndex)->VideoBuffer;
			}

			DataMemory->length = StreamMemory.ChunksPerFrameField * StreamMemory.PacketsPerChunk * StreamMemory.PayloadSize;

			constexpr rmx_mkey_id InvalidKey = ((rmx_mkey_id)(-1L));
			DataMemory->mkey[0] = InvalidKey;
			DataMemory->mkey[1] = InvalidKey;
			
			
			rmx_mem_multi_key_region* HeaderMemory;
			HeaderMemory = CachedAPI->rmx_output_media_get_dup_sub_block(&Block, StreamMemory.HeaderBlockID);
			if (HeaderMemory == nullptr)
			{
				UE_LOG(LogRivermax, Warning, TEXT("Faild to get header memory block. Output stream won't be created."));
				return false;
			}

			StreamMemory.RTPHeaders[BlockIndex].SetNumZeroed(RealPacketsPerFrame * StreamMemory.FramesFieldPerMemoryBlock);
			HeaderMemory->addr = &StreamMemory.RTPHeaders[BlockIndex][0];
			HeaderMemory->length = StreamMemory.HeaderStrideSize;
			HeaderMemory->mkey[0] = InvalidKey;
			HeaderMemory->mkey[1] = InvalidKey;
		}
	
		uint64 TotalSize = 0;
		uint64 LineSize = 0;
		for (int32 PayloadSizeIndex = 0; PayloadSizeIndex < RealPacketsPerFrame; ++PayloadSizeIndex)
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

			// All buffers are configured the same so compute header and payload sizes once and assigned to all impacted locations
			for (uint32 BufferIndex = 0; BufferIndex < StreamMemory.FramesFieldPerMemoryBlock; ++BufferIndex)
			{
				StreamMemory.HeaderSizes[PayloadSizeIndex + (BufferIndex * RealPacketsPerFrame)] = HeaderSize;
				StreamMemory.PayloadSizes[PayloadSizeIndex + (BufferIndex * RealPacketsPerFrame)] = ThisPayloadSize;
			}

			if(CachedCVars.bPrefillRTPHeaders && HeaderSize > 0)
			{
				RTPFiller.Update(PayloadSizeIndex);
			}
			
			TotalSize += ThisPayloadSize;
		}

		// Verify memcopy config to make sure it works for current frame size / chunking
		if (StreamMemory.bUseIntermediateBuffer)
		{
			StreamMemory.FrameMemorySliceCount = FMath::Clamp(CVarRivermaxOutputMaxFrameMemorySliceCount.GetValueOnAnyThread(), 1, 100);
			StreamMemory.ChunkSpacingBetweenMemcopies = FMath::Clamp(CVarRivermaxOutputMemcopyChunkSpacing.GetValueOnAnyThread(), 1, 20);

			const uint32 ChunkRequired = StreamMemory.ChunkSpacingBetweenMemcopies * StreamMemory.FrameMemorySliceCount;
			if (ChunkRequired > 0 && ChunkRequired > StreamMemory.ChunksPerFrameField)
			{
				// Favor reducing number of memcopies. If required packet is smaller, chances are it's a small frame size
				// so memcopies will be smaller.
				const double Ratio = StreamMemory.ChunksPerFrameField / (double)ChunkRequired;
				StreamMemory.FrameMemorySliceCount *= Ratio;
			}
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
		NextFrame->FrameStartPtr = nullptr;
		NextFrame->ScheduleTimeCopied = 0;
		NextFrame->bCaughtTimingIssue = false;
		NextFrame->Offset = 0;
	}

	void FRivermaxOutputStream::BuildRTPHeader(FRawRTPHeader& OutHeader) const
	{
		using namespace UE::RivermaxCore::Private::Utils;

		if (CachedCVars.bPrefillRTPHeaders)
		{
			OutHeader.SequenceNumber = ByteSwap((uint16)(StreamData.SequenceNumber & 0xFFFF));
			OutHeader.Timestamp = ByteSwap(CurrentFrame->MediaTimestamp);
			OutHeader.ExtendedSequenceNumber = ByteSwap((uint16)((StreamData.SequenceNumber >> 16) & 0xFFFF));
		}
		else
		{
			OutHeader = {};
			OutHeader.Version = 2;
			OutHeader.PaddingBit = 0;
			OutHeader.ExtensionBit = 0;
			OutHeader.PayloadType = 96; //Payload type should probably be infered from SDP

			OutHeader.SequenceNumber = ByteSwap((uint16)(StreamData.SequenceNumber & 0xFFFF));
			OutHeader.Timestamp = ByteSwap(CurrentFrame->MediaTimestamp);

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
				if (SRD2Length && CurrentFrame->LineNumber == ((uint32)Options.AlignedResolution.Y - 1))
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
	}

	void FRivermaxOutputStream::DestroyStream()
	{
		rmx_status Status = CachedAPI->rmx_output_media_cancel_unsent_chunks(&StreamData.ChunkHandle);
		if (Status != RMX_OK)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Could not cancel unsent chunks when destroying output stream. Status: %d"), Status);
		}

		do 
		{
			Status = CachedAPI->rmx_output_media_destroy_stream(StreamId);
			if (RMX_BUSY == Status) 
			{
				FPlatformProcess::SleepNoStats(Utils::SleepTimeSeconds);
			}
		} while (Status == RMX_BUSY);
	}

	bool FRivermaxOutputStream::WaitForNextRound()
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

		static constexpr float SleepThresholdSec = 5.0f / 1000;
		static constexpr float YieldTimeSec = 2.0f / 1000;
		const double WaitTimeSec = FMath::Min(WaitTimeNanosec / 1E9, 1.0);
		StreamData.LastSleepTimeNanoSec = WaitTimeNanosec;

		bool bIsFrameReady = false;
		if (StreamMemory.bUseIntermediateBuffer)
		{	
			// When using intermediate buffer, we verify if next buffer is ready sooner than wake up time
			// If a frame is ready already, we can move on. Otherwise, we wait for FrameReady signal with a
			// wait timeout. In the case of a repeated frame, we will always timeout and we won't be able to 
			// do an early copy.
			TSharedPtr<FRivermaxOutputFrame> ReadyFrame = FrameManager->GetReadyFrame();
			if (ReadyFrame)
			{
				bIsFrameReady = true;
			}
			else
			{
				const uint32 WaitMs = FMath::Floor((WaitTimeSec - YieldTimeSec) * 1000.);
				do 
				{
					bIsFrameReady = FrameReadyToSendSignal->Wait(WaitMs);
					ReadyFrame = FrameManager->GetReadyFrame();
				} while (!ReadyFrame && bIsFrameReady && bIsActive);
			}
		}
		else
		{
			// Sleep for the largest chunk of time
			if (WaitTimeSec > SleepThresholdSec)
			{
				FPlatformProcess::SleepNoStats(WaitTimeSec - YieldTimeSec);
			}
		}

		if (!bIsFrameReady)
		{
			// We are past the long sleep so no more early data access possible. Just yield until the wake up time.
			{
				// Use platform time instead of rivermax get PTP to avoid making calls to it. Haven't been profiled if it impacts
				while (FPlatformTime::Seconds() < (CurrentPlatformTime + WaitTimeSec))
				{
					FPlatformProcess::SleepNoStats(0.f);
				}
			}

			if (StreamData.bHasValidNextFrameNumber && CachedCVars.bShowOutputStats)
			{
				const uint64 AfterSleepTimeNanosec = RivermaxModule->GetRivermaxManager()->GetTime();
				const uint64 RealWaitNs = AfterSleepTimeNanosec - CurrentTimeNanosec;
				const uint64 OvershootSleep = AfterSleepTimeNanosec > StreamData.NextAlignmentPointNanosec ? AfterSleepTimeNanosec - StreamData.NextAlignmentPointNanosec : 0;
				const double OvershootSleepSec = OvershootSleep / 1e9;

				UE_LOG(LogRivermax, Verbose, TEXT("CurrentTime %llu. OvershootSleep: %0.9f. ExpectedWait: %0.9f. RealWait: %0.9f, Scheduling at %llu. NextAlign %llu. ")
					, CurrentTimeNanosec
					, OvershootSleepSec
					, (double)WaitTimeNanosec / 1E9
					, (double)RealWaitNs / 1E9
					, StreamData.NextScheduleTimeNanosec
					, StreamData.NextAlignmentPointNanosec);
			}
		}
		else
		{
			if (StreamData.bHasValidNextFrameNumber && CachedCVars.bShowOutputStats)
			{
				const uint64 AfterSleepTimeNanosec = RivermaxModule->GetRivermaxManager()->GetTime();
				UE_LOG(LogRivermax, Verbose, TEXT("Early data available. CurrentTime %llu. Scheduling at %llu. NextAlign %llu. ")
					, AfterSleepTimeNanosec
					, StreamData.NextScheduleTimeNanosec
					, StreamData.NextAlignmentPointNanosec
					, (StreamData.NextScheduleTimeNanosec - AfterSleepTimeNanosec));
			}
		}

		return bIsFrameReady;
	}

	void FRivermaxOutputStream::GetNextChunk()
	{
		bool bHasAddedTrace = false;
		rmx_status Status;
		do
		{
			Status = CachedAPI->rmx_output_media_get_next_chunk(&StreamData.ChunkHandle);
			CurrentFrame->PayloadPtr = rmx_output_media_get_chunk_strides(&StreamData.ChunkHandle, StreamMemory.DataBlockID);
			CurrentFrame->HeaderPtr = rmx_output_media_get_chunk_strides(&StreamData.ChunkHandle, StreamMemory.HeaderBlockID);
			if (Status == RMX_OK)
			{
				if (StreamData.bHasFrameFirstChunkBeenFetched == false)
				{
					StreamData.bHasFrameFirstChunkBeenFetched = true;

					// Stamp frame start in order to copy frame data sequentially as we query chunks
					CurrentFrame->FrameStartPtr = CurrentFrame->PayloadPtr;

					if (CurrentFrame->PayloadPtr != StreamMemory.BufferAddresses[StreamData.ExpectedFrameIndex])
					{
						UE_LOG(LogRivermax, Warning, TEXT("Frame being sent (%d) doesn't match chunks being processed."), CurrentFrame->FrameIndex);
					}
				}

				break;
			}
			else if (Status == RMX_NO_FREE_CHUNK)
			{
				//We should not be here
				if (!bHasAddedTrace)
				{
					Stats.LastFrameChunkRetries++;
					UE_LOG(LogRivermax, Verbose, TEXT("No free chunks to get for chunk '%u'. Waiting"), CurrentFrame->ChunkNumber);
					TRACE_CPUPROFILER_EVENT_SCOPE(GetNextChunk::NoFreeChunk);
					bHasAddedTrace = true;
				}
			}
			else
			{
				UE_LOG(LogRivermax, Error, TEXT("Invalid error happened while trying to get next chunks. Status: %d"), Status);
				Listener->OnStreamError();
				Stop();
			}
		} while (Status != RMX_OK && bIsActive);
	}

	bool FRivermaxOutputStream::CopyFrameData(const TSharedPtr<FRivermaxOutputFrame>& SourceFrame, uint8* DestinationBase)
	{
		// Make sure copy size doesn't go over frame size
		const uint32 FrameSize = GetStride() * Options.AlignedResolution.Y;
		if (ensure(FrameSize > 0))
		{
			const uint32 BlockSize = 1 + ((FrameSize - 1) / StreamMemory.FrameMemorySliceCount);
			const uint32 MaxSize = FrameSize - SourceFrame->Offset;
			const uint32 CopySize = FMath::Min(BlockSize, MaxSize);

			// Copy data until we have covered the whole frame. Last block might be smaller.
			if (CopySize > 0)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(CopyFrameData);
				const uint32 PacketsToCopy = CopySize / StreamMemory.PayloadSize;
				const uint32 PacketOffset = SourceFrame->ChunkNumber * StreamMemory.PacketsPerChunk;
				const uint64 PacketDeltaTime = (StreamData.FrameFieldTimeIntervalNs - TransmitOffsetNanosec) / StreamMemory.PacketsPerFrame;

				// Time covered in frame interval including this copy
				const uint64 TimeCopied = StreamData.NextScheduleTimeNanosec + ((PacketOffset + PacketsToCopy) * PacketDeltaTime);

				// Sidecar used to track copy progress
				TSharedPtr<FMemChunkCopiedInfo> CopyInfo = MakeShared<FMemChunkCopiedInfo>();
				CopyInfo->TimeCovered = TimeCopied;

				FCopyArgs Args;
				uint8* SourceStart = reinterpret_cast<uint8*>(SourceFrame->VideoBuffer);
				uint8* DestinationStart = DestinationBase;
				Args.SourceMemory = SourceStart + SourceFrame->Offset;
				Args.DestinationMemory = DestinationStart + SourceFrame->Offset;
				Args.SizeToCopy = CopySize;
				Args.SideCar = MoveTemp(CopyInfo);

				// Update memory offset for next copy
				SourceFrame->Offset += CopySize;

				Allocator->CopyData(Args);

				return true;
			}
		}
		else
		{
			UE_LOG(LogRivermax, Error, TEXT("Invalid frame size detected while stream was active. Shutting down."));
			Listener->OnStreamError();
			Stop();
		}

		return false;
	}

	void FRivermaxOutputStream::SetupRTPHeaders()
	{
		FRawRTPHeader* HeaderRawPtr = reinterpret_cast<FRawRTPHeader*>(CurrentFrame->HeaderPtr);
		check(HeaderRawPtr);

		const uint32 MemblockIndex = StreamData.ExpectedFrameIndex / StreamMemory.FramesFieldPerMemoryBlock;
		const uint32 MemblockFrameIndex = StreamData.ExpectedFrameIndex % StreamMemory.FramesFieldPerMemoryBlock;
		const uint32 PacketsPerFrame = StreamMemory.ChunksPerFrameField * StreamMemory.PacketsPerChunk;
		const uint32 BaseHeaderIndex = CurrentFrame->PacketCounter + (MemblockFrameIndex * PacketsPerFrame);

		if (ensure(HeaderRawPtr == &StreamMemory.RTPHeaders[MemblockIndex][BaseHeaderIndex]))
		{
			for (uint32 PacketIndex = 0; PacketIndex < StreamMemory.PacketsPerChunk && CurrentFrame->PacketCounter < StreamMemory.PacketsPerFrame; ++PacketIndex)
			{
				BuildRTPHeader(StreamMemory.RTPHeaders[MemblockIndex][BaseHeaderIndex + PacketIndex]);
				
				CurrentFrame->BytesSent += StreamMemory.PayloadSizes[CurrentFrame->PacketCounter];
				++StreamData.SequenceNumber;
				++CurrentFrame->PacketCounter;
			}
		}
	}

	void FRivermaxOutputStream::CommitNextChunks()
	{
		rmx_status Status;
		int32 ErrorCount = 0;
		const uint64 CurrentTimeNanosec = RivermaxModule->GetRivermaxManager()->GetTime();
		uint64 ScheduleTime = CurrentFrame->ChunkNumber == 0 ? StreamData.NextScheduleTimeNanosec : 0;

		do
		{
			//Only first chunk gets scheduled with a timestamp. Following chunks are queued after it using 0
			if (ScheduleTime != 0)
			{
				// If scheduling time is not far away enough, force it immediately otherwise rmax_commit will throw an error 
				if (ScheduleTime <= (CurrentTimeNanosec + CachedCVars.ForceCommitImmediateTimeNanosec))
				{
					ScheduleTime = 0;
					++Stats.CommitImmediate;
				}
			}

			checkSlow(CachedAPI);
			Status = CachedAPI->rmx_output_media_commit_chunk(&StreamData.ChunkHandle, ScheduleTime);

			if (Status == RMX_OK)
			{
				break;
			}
			else if (Status == RMX_HW_SEND_QUEUE_IS_FULL)
			{
				Stats.CommitRetries++;
				TRACE_CPUPROFILER_EVENT_SCOPE(CommitNextChunks::QUEUEFULL);
				++ErrorCount;
			}
			else if (Status == RMX_HW_COMPLETION_ISSUE)
			{
				UE_LOG(LogRivermax, Error, TEXT("Completion issue while trying to commit next round of chunks."));
				Listener->OnStreamError();
				Stop();
			}
			else
			{
				UE_LOG(LogRivermax, Error, TEXT("Unhandled error (%d) while trying to commit next round of chunks."), Status);
				Listener->OnStreamError();
				Stop();
			}

		} while (Status != RMX_OK && bIsActive);

		if (bIsActive && CurrentFrame->ChunkNumber == 0 && CachedCVars.bShowOutputStats)
		{
			UE_LOG(LogRivermax, Verbose, TEXT("Committed frame [%u]. Scheduled for '%llu'. Aligned with '%llu'. Current time '%llu'. Was late: %d. Slack: %llu. Errorcount: %d")
				, CurrentFrame->FrameIdentifier
				, ScheduleTime
				, StreamData.NextAlignmentPointNanosec
				, CurrentTimeNanosec
				, CurrentTimeNanosec >= StreamData.NextScheduleTimeNanosec ? 1 : 0
				, StreamData.NextScheduleTimeNanosec >= CurrentTimeNanosec ? StreamData.NextScheduleTimeNanosec - CurrentTimeNanosec : 0
				, ErrorCount);
		}
	}

	bool FRivermaxOutputStream::Init()
	{
		return true;
	}

	uint32 FRivermaxOutputStream::Run()
	{
		if (CachedCVars.bEnableTimeCriticalThread)
		{
			::SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
		}

		// Initial wait for a frame to be produced
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Rmax::InitialWait);
			FrameReadyToSendSignal->Wait();
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
		switch (Options.AlignmentMode)
		{
			case ERivermaxAlignmentMode::FrameCreation:
			{
				PrepareNextFrame_FrameCreation();
				break;
			}
			case ERivermaxAlignmentMode::AlignmentPoint:
			{
				PrepareNextFrame_AlignmentPoint();
				break;
			}
			default:
			{
				checkNoEntry();
			}
		}
	}

	void FRivermaxOutputStream::PrepareNextFrame_FrameCreation()
	{
		// When aligning on frame creation, we will always wait for a frame to be available.
		TRACE_CPUPROFILER_EVENT_SCOPE(Rmax::WaitForReadyFrame);
		TSharedPtr<FRivermaxOutputFrame> NextFrameToSend = FrameManager->GetReadyFrame();
		while (!NextFrameToSend && bIsActive)
		{
			FrameReadyToSendSignal->Wait();
			NextFrameToSend = FrameManager->GetReadyFrame();
		}

		// In frame creation alignment, we always release the last frame sent
		if (CurrentFrame.IsValid())
		{
			constexpr bool bReleaseFrame = true;
			CompleteCurrentFrame(bReleaseFrame);
		}

		// Make the next frame to send the current one and update its state
		if (NextFrameToSend)
		{
			CurrentFrame = MoveTemp(NextFrameToSend);
			FrameManager->MarkAsSending(CurrentFrame);

			InitializeNextFrame(CurrentFrame);
		}
	}

	void FRivermaxOutputStream::PrepareNextFrame_AlignmentPoint()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Rmax::GetNextFrame_AlignmentPoint);

		// When aligning on alignment points:
		// We prepare to send the next frame that is ready if there is one available
		// if none are available and bDoContinuousOutput == true
		//		Repeat the last frame
		// if none are available and bDoContinuousOutput == false
		//		Don't send a frame and go back waiting for the next alignment point
		
		TSharedPtr<FRivermaxOutputFrame> NextFrameToSend = FrameManager->GetReadyFrame();

		// If we have a new frame, release the previous one
		// If we don't have a frame and we're not doing continuous output, we release it and we won't send a new one
		// If we don't have a frame but we are doing continuous output, we will reschedule the current one, so no release.
		if (!Options.bDoContinuousOutput || NextFrameToSend)
		{
			if (CurrentFrame)
			{
				constexpr bool bReleaseFrame = true;
				CompleteCurrentFrame(bReleaseFrame);
			}

			// Make the next frame to send the current one and update its state
			if (NextFrameToSend)
			{
				CurrentFrame = MoveTemp(NextFrameToSend);
				FrameManager->MarkAsSending(CurrentFrame);
				InitializeNextFrame(CurrentFrame);
			}
		}
		else
		{
			// We finished sending a frame so complete it but don't release it as we will repeat it
			constexpr bool bReleaseFrame = false;
			CompleteCurrentFrame(bReleaseFrame);

			// We will resend the last one so just reinitialize it to resend
			InitializeNextFrame(CurrentFrame);
			
			// If intermediate buffer isn't used and frame has to be repeated, we use skip chunk method 
			// which might cause timing errors caused by chunk managements issue 
			if (!StreamMemory.bUseIntermediateBuffer)
			{
				// No frame to send, keep last one and restart its internal counters
				UE_LOG(LogRivermax, Verbose, TEXT("No frame to send. Reusing last frame '%d' with Id %u"), CurrentFrame->FrameIndex, CurrentFrame->FrameIdentifier);

				// Since we want to resend last frame, we need to fast forward chunk pointer to re-point to the one we just sent
				SkipChunks(StreamMemory.ChunksPerFrameField * (Options.NumberOfBuffers - 1));
				
				// Keep RTP header in sync when skipping chunks
				StreamData.ExpectedFrameIndex = CurrentFrame->FrameIndex;
			}
		}
	}

	void FRivermaxOutputStream::InitializeStreamTimingSettings()
	{
		using namespace UE::RivermaxCore::Private::Utils;

		const double TROOverride = CVarRivermaxOutputTROOverride.GetValueOnAnyThread();
		if (TROOverride != 0)
		{
			TransmitOffsetNanosec = TROOverride * 1E9;
			return;
		}

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
		if (CachedCVars.bShowOutputStats)
		{
			const double CurrentTime = FPlatformTime::Seconds();
			if (CurrentTime - LastStatsShownTimestamp > CachedCVars.ShowOutputStatsIntervalSeconds)
			{
				LastStatsShownTimestamp = CurrentTime;
				UE_LOG(LogRivermax, Log, TEXT("Stats: FrameSent: %llu. CommitImmediate: %u. CommitRetries: %u. ChunkRetries: %u. ChunkSkippingRetries: %u. Timing issues: %llu"), Stats.FramesSentCounter, Stats.CommitImmediate, Stats.CommitRetries, Stats.TotalChunkRetries, Stats.ChunkSkippingRetries, Stats.TimingIssueCount);
			}
		}
	}

	bool FRivermaxOutputStream::IsGPUDirectSupported() const
	{
		return bUseGPUDirect;
	}

	bool FRivermaxOutputStream::SetupFrameManagement()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FRivermaxOutputStream::SetupFrameManagement);

		FrameManager = MakeUnique<FFrameManager>();

		// We do (try) to make gpu allocations here to let the capturer know if we require it or not.
		bool bTryGPUDirect = RivermaxModule->GetRivermaxManager()->IsGPUDirectOutputSupported() && Options.bUseGPUDirect;
		if (bTryGPUDirect)
		{
			const ERHIInterfaceType RHIType = RHIGetInterfaceType();
			if (RHIType != ERHIInterfaceType::D3D12)
			{
				UE_LOG(LogRivermax, Warning, TEXT("Can't initialize output to use GPUDirect. RHI is %d but only Dx12 is supported at the moment."), RHIType);
				bTryGPUDirect = false;
			}
		}

		// Work around when dealing with multi memblocks. Rivermax fails to create stream 
		// With memblock not starting on the right cuda alignment. 
		const bool bAlignEachFrameMemory = !CachedCVars.bUseSingleMemblock;
		FFrameManagerSetupArgs FrameManagerArgs;
		FrameManagerArgs.Resolution = Options.AlignedResolution;
		FrameManagerArgs.bTryGPUAllocation = bTryGPUDirect;
		FrameManagerArgs.NumberOfFrames = Options.NumberOfBuffers;
		FrameManagerArgs.Stride = GetStride();
		FrameManagerArgs.FrameDesiredSize = StreamMemory.ChunksPerFrameField * StreamMemory.PacketsPerChunk * StreamMemory.PayloadSize;
		FrameManagerArgs.bAlignEachFrameAlloc = bAlignEachFrameMemory;
		FrameManagerArgs.OnFreeFrameDelegate = FOnFrameReadyDelegate::CreateRaw(this, &FRivermaxOutputStream::OnFrameReadyToBeUsed);
		FrameManagerArgs.OnPreFrameReadyDelegate = FOnFrameReadyDelegate::CreateRaw(this, &FRivermaxOutputStream::OnPreFrameReadyToBeSent);
		FrameManagerArgs.OnFrameReadyDelegate = FOnFrameReadyDelegate::CreateRaw(this, &FRivermaxOutputStream::OnFrameReadyToBeSent);
		FrameManagerArgs.OnCriticalErrorDelegate = FOnCriticalErrorDelegate::CreateRaw(this, &FRivermaxOutputStream::OnFrameManagerCriticalError);
		const EFrameMemoryLocation FrameLocation = FrameManager->Initialize(FrameManagerArgs);
		bUseGPUDirect = FrameLocation == EFrameMemoryLocation::GPU;


		// Only support intermediate buffer for alignment point method to avoid running into chunk issue when repeating a frame
		const bool bHasAllocatedFrames = (FrameLocation != EFrameMemoryLocation::None);
		if (bHasAllocatedFrames && Options.AlignmentMode == ERivermaxAlignmentMode::AlignmentPoint && CVarRivermaxOutputEnableIntermediateBuffer.GetValueOnAnyThread())
		{
			// Allocate intermediate buffer in same memory space as frame memory.
			FOnFrameDataCopiedDelegate OnDataCopiedDelegate = FOnFrameDataCopiedDelegate::CreateRaw(this, &FRivermaxOutputStream::OnMemoryChunksCopied);

			const int32 DesiredSize = FrameManagerArgs.FrameDesiredSize * StreamMemory.FramesFieldPerMemoryBlock;
			if (bUseGPUDirect)
			{
				Allocator = MakeUnique<FGPUAllocator>(DesiredSize, OnDataCopiedDelegate);
			}
			else
			{
				Allocator = MakeUnique<FSystemAllocator>(DesiredSize, OnDataCopiedDelegate);
			}
			
			if (!Allocator->Allocate(StreamMemory.MemoryBlockCount, bAlignEachFrameMemory))
			{
				return false;
			}

			StreamMemory.bUseIntermediateBuffer = true;
		}

		// Cache buffer addresses used by rivermax in order to start copying into it early on
		StreamMemory.BufferAddresses.Reserve(Options.NumberOfBuffers);
		if (StreamMemory.bUseIntermediateBuffer)
		{
			check(Allocator);

			for (uint32 MemblockIndex = 0; MemblockIndex < StreamMemory.MemoryBlockCount; ++MemblockIndex)
			{
				uint8* BaseAddress = static_cast<uint8*>(Allocator->GetFrameAddress(MemblockIndex));
				for (uint32 FrameIndex = 0; FrameIndex < StreamMemory.FramesFieldPerMemoryBlock; ++FrameIndex)
				{
					uint8* FrameAddress = BaseAddress + (FrameIndex * FrameManagerArgs.FrameDesiredSize);
					StreamMemory.BufferAddresses.Add(FrameAddress);
				}
			}
		}
		else
		{
			// When we don't use intermediate buffer, each frame has its own buffer addresses and we don't need to look at memblocks
			for (int32 BufferIndex = 0; BufferIndex < Options.NumberOfBuffers; ++BufferIndex)
			{
				StreamMemory.BufferAddresses.Add(FrameManager->GetFrame(BufferIndex)->VideoBuffer);
			}
		}
		check(StreamMemory.BufferAddresses.Num() == Options.NumberOfBuffers);

		return FrameLocation != EFrameMemoryLocation::None;
	}

	void FRivermaxOutputStream::CleanupFrameManagement()
	{
		if (FrameManager)
		{
			FrameManager->Cleanup();
			FrameManager.Reset();
		}

		if (Allocator)
		{
			Allocator->Deallocate();
			Allocator.Reset();
		}
	}

	int32 FRivermaxOutputStream::GetStride() const
	{
		check(FormatInfo.PixelGroupCoverage != 0);
		return (Options.AlignedResolution.X / FormatInfo.PixelGroupCoverage) * FormatInfo.PixelGroupSize;
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
					// Verify if last frame had timing issues. If yes, we skip next interval
					if (CurrentFrame && CurrentFrame->bCaughtTimingIssue)
					{
						NextFrameNumber = CurrentFrameNumber + 2;
						UE_LOG(LogRivermax, Warning, TEXT("Timing issue detected during frame %llu. Skipping frame %llu to keep sync."), CurrentFrameNumber, CurrentFrameNumber + 1);
					}
					else
					{
						// If current frame is greater than last scheduled, we missed an alignment point.
						const uint64 DeltaFrames = CurrentFrameNumber - StreamData.NextAlignmentPointFrameNumber;
						if (DeltaFrames >= 1)
						{
							UE_LOG(LogRivermax, Warning, TEXT("Output missed %llu frames."), DeltaFrames);

							// If we missed a sync point, this means that last scheduled frame might still be ongoing and 
							// sending it might be crossing the frame boundary so we skip one entire frame to empty the queue.
							NextFrameNumber = CurrentFrameNumber + 2;
						}
						else
						{
							NextFrameNumber = CurrentFrameNumber + 1;
						}
					}
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
		StreamData.LastAlignmentPointFrameNumber = StreamData.NextAlignmentPointFrameNumber;
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

	bool FRivermaxOutputStream::ReserveFrame(uint32 FrameIdentifier) const
	{
		TSharedPtr<FRivermaxOutputFrame> ReservedFrame = FrameManager->GetNextFrame(FrameIdentifier);
		if (!ReservedFrame && Options.FrameLockingMode == EFrameLockingMode::BlockOnReservation)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Rmax::WaitForAvailableFrame);
			while(!ReservedFrame && bIsActive)
			{
				FrameAvailableSignal->Wait();
				ReservedFrame = FrameManager->GetNextFrame(FrameIdentifier);
			}
		}

		return ReservedFrame != nullptr;
	}

	void FRivermaxOutputStream::OnFrameReadyToBeSent()
	{	
		FrameReadyToSendSignal->Trigger();
	}

	void FRivermaxOutputStream::OnFrameReadyToBeUsed()
	{
		FrameAvailableSignal->Trigger();
	}

	void FRivermaxOutputStream::OnPreFrameReadyToBeSent()
	{
		Listener->OnPreFrameEnqueue();
	}

	void FRivermaxOutputStream::OnFrameManagerCriticalError()
	{
		Listener->OnStreamError();
		Stop();
	}

	void FRivermaxOutputStream::CacheCVarValues()
	{
		CachedCVars.bEnableCommitTimeProtection = CVarRivermaxOutputEnableTimingProtection.GetValueOnAnyThread() != 0;
		CachedCVars.ForceCommitImmediateTimeNanosec = CVarRivermaxOutputForceImmediateSchedulingThreshold.GetValueOnAnyThread();
		CachedCVars.SkipSchedulingTimeNanosec = CVarRivermaxOutputSkipSchedulingCutOffTime.GetValueOnAnyThread() * 1E3;
		CachedCVars.bUseSingleMemblock = CVarRivermaxOutputUseSingleMemblock.GetValueOnAnyThread() == 1;
		CachedCVars.bEnableTimeCriticalThread = CVarRivermaxOutputEnableTimeCriticalThread.GetValueOnAnyThread() != 0;
		CachedCVars.bShowOutputStats = CVarRivermaxOutputShowStats.GetValueOnAnyThread() != 0;
		CachedCVars.ShowOutputStatsIntervalSeconds = CVarRivermaxOutputShowStatsInterval.GetValueOnAnyThread();
		CachedCVars.bPrefillRTPHeaders = CVarRivermaxOutputPrefillRTPHeaders.GetValueOnAnyThread();
	}

	bool FRivermaxOutputStream::IsChunkOnTime() const
	{
		switch (Options.AlignmentMode)
		{
		case ERivermaxAlignmentMode::AlignmentPoint:
		{
			return IsChunkOnTime_AlignmentPoints();
		}
		case ERivermaxAlignmentMode::FrameCreation:
		{
			return IsChunkOnTime_FrameCreation();
		}
		default:
		{
			checkNoEntry();
			return false;
		}
		}
	}

	bool FRivermaxOutputStream::IsChunkOnTime_FrameCreation() const
	{
		return true;
	}

	bool FRivermaxOutputStream::IsChunkOnTime_AlignmentPoints() const
	{
		if (CachedCVars.bEnableCommitTimeProtection)
		{
			// Calculate at what time this chunk is supposed to be sent
			const uint64 DeltaTimePerChunk = (StreamData.FrameFieldTimeIntervalNs - TransmitOffsetNanosec) / StreamMemory.ChunksPerFrameField;
			const uint64 NextChunkCommitTime = StreamData.NextScheduleTimeNanosec + (CurrentFrame->ChunkNumber * DeltaTimePerChunk);

			// Verify if we are on time to send it. Use CVar to tighten / extend needed window. 
			// This is to avoid messing up timing
			const uint64 CurrentTime = RivermaxModule->GetRivermaxManager()->GetTime();
			if (NextChunkCommitTime <= (CurrentTime + CachedCVars.SkipSchedulingTimeNanosec))
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(RmaxOut::ChunkTooLate);
				return false;
			}

			// Add other causes of timing issues. 
			// Possible options : Chunk warnings, Last commit time too close to frame boundary, etc...
		}
		
		return true;
	}

	void FRivermaxOutputStream::OnMemoryChunksCopied(const TSharedPtr<FBaseDataCopySideCar>& Sidecar)
	{
		if (Sidecar)
		{
			TSharedPtr<FMemChunkCopiedInfo> CopyInfo = StaticCastSharedPtr<FMemChunkCopiedInfo>(Sidecar);
			if (CurrentFrame)
			{
				CurrentFrame->ScheduleTimeCopied = CopyInfo->TimeCovered;
			}
		}
	}

	void FRivermaxOutputStream::OnCVarRandomDelayChanged(IConsoleVariable* Var)
	{
		bTriggerRandomDelay = true;
	}

	void FRivermaxOutputStream::CalculateFrameTimestamp()
	{
		using namespace UE::RivermaxCore::Private::Utils;

		// For now, in order to be able to use a framelocked input, we pipe frame number in the timestamp for a UE-UE interaction
		// Follow up work to investigate adding this in RTP header
		uint64 InputTime = StreamData.NextAlignmentPointNanosec;
		if (Options.bDoFrameCounterTimestamping)
		{
			InputTime = UE::RivermaxCore::GetAlignmentPointFromFrameNumber(CurrentFrame->FrameIdentifier, Options.FrameRate);
		}

		CurrentFrame->MediaTimestamp = GetTimestampFromTime(InputTime, MediaClockSampleRate);
	}

	void FRivermaxOutputStream::SkipChunks(uint64 ChunkCount)
	{
		bool bHasAddedTrace = false;
		rmx_status Status;
		do
		{
			checkSlow(CachedAPI);
			Status = CachedAPI->rmx_output_media_skip_chunks(&StreamData.ChunkHandle, ChunkCount);
			if (Status != RMX_OK)
			{
				if (Status == RMX_NO_FREE_CHUNK)
				{
					// Wait until there are enough free chunk to be skipped
					if (!bHasAddedTrace)
					{
						UE_LOG(LogRivermax, Warning, TEXT("No chunks ready to skip. Waiting"));
						TRACE_CPUPROFILER_EVENT_SCOPE(NoFreeChunk);
						bHasAddedTrace = true;
					}
				}
				else
				{
					ensure(false);
					UE_LOG(LogRivermax, Error, TEXT("Invalid error happened while trying to skip chunks. Status: %d."), Status);
					Listener->OnStreamError();
					Stop();
				}
			}
		} while (Status != RMX_OK && bIsActive);
	}

	void FRivermaxOutputStream::SendFrame()
	{
		StreamData.LastSendStartTimeNanoSec = RivermaxModule->GetRivermaxManager()->GetTime();

		if (bTriggerRandomDelay)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FRivermaxOutputStream::RandomDelay);
			bTriggerRandomDelay = false;
			FPlatformProcess::SleepNoStats(FMath::RandRange(2e-3, 4e-3));
		}

		// Calculate frame's timestamp only once and reuse in RTP build
		CalculateFrameTimestamp();

		const uint32 MediaFrameNumber = Utils::TimestampToFrameNumber(CurrentFrame->MediaTimestamp, Options.FrameRate);
		TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FRivermaxTracingUtils::RmaxOutSendingFrameTraceEvents[MediaFrameNumber % 10]);
		TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FRivermaxTracingUtils::RmaxOutMediaCapturePipeTraceEvents[CurrentFrame->FrameIdentifier % 10]);
		UE_LOG(LogRivermax, VeryVerbose, TEXT("RmaxRX Sending frame number %u with timestamp %u."), MediaFrameNumber, CurrentFrame->MediaTimestamp);

		do
		{
			if (bIsActive)
			{
				GetNextChunk();
			}

			if (bIsActive && StreamMemory.bUseIntermediateBuffer && ((CurrentFrame->ChunkNumber % StreamMemory.ChunkSpacingBetweenMemcopies) == 0))
			{
				CopyFrameData(CurrentFrame, reinterpret_cast<uint8*>(CurrentFrame->FrameStartPtr));
			}

			if (bIsActive)
			{
				SetupRTPHeaders();
			}

			if (bIsActive)
			{
				if (!CurrentFrame->bCaughtTimingIssue)
				{
					// As long as our frame is good, verify if we commit chunks before it is expected to be sent
					// We keep committing the frame even if we detect timing issue to avoid having to skip
					// chunks in the internals of Rivermax and keep it going for entirety of frames. 
					// We skip an interval instead but it is quite drastic.
					const bool bIsChunkOnTime = IsChunkOnTime();
					CurrentFrame->bCaughtTimingIssue = !bIsChunkOnTime;

					if (UE::RivermaxCore::Private::GbTriggerRandomTimingIssue)
					{
						FRandomStream RandomStream(FPlatformTime::Cycles64());
						const bool bTriggerDesync = (RandomStream.FRandRange(0.0, 1.0) > 0.7) ? true : false;
						if (bTriggerDesync)
						{
							TRACE_CPUPROFILER_EVENT_SCOPE(RmaxOut::ForceTimingIssue);
							CurrentFrame->bCaughtTimingIssue = true;
						}

						UE::RivermaxCore::Private::GbTriggerRandomTimingIssue = false;
					}
				}

				CommitNextChunks();
			}

			//Update frame progress
			if (bIsActive)
			{
				Stats.TotalPacketSent += StreamMemory.PacketsPerChunk;
				++CurrentFrame->ChunkNumber;
			}
		} while (CurrentFrame->ChunkNumber < StreamMemory.ChunksPerFrameField && bIsActive);

		Stats.FramesSentCounter++;
		StreamData.ExpectedFrameIndex = (StreamData.ExpectedFrameIndex + 1) % Options.NumberOfBuffers;
	}

	void FRivermaxOutputStream::InitializeTimingProtections()
	{
		switch (Options.AlignmentMode)
		{
			case ERivermaxAlignmentMode::FrameCreation:
			{
				StreamMemory.bAlwaysSkipChunk = false;
				StreamMemory.FrameRateMultiplier = 1.0f;
				break;
			}
			case ERivermaxAlignmentMode::AlignmentPoint:
			{
				StreamMemory.bAlwaysSkipChunk = CVarRivermaxOutputForceSkip.GetValueOnAnyThread();
				StreamMemory.FrameRateMultiplier = FMath::Clamp(CVarRivermaxOutputFrameRateMultiplier.GetValueOnAnyThread(), 1.0, 2.0);
				break;
			}
			default:
			{
				checkNoEntry();
			}
		}
	}

	void FRivermaxOutputStream::GetLastPresentedFrame(FPresentedFrameInfo& OutFrameInfo) const
	{
		FScopeLock Lock(&PresentedFrameCS);
		OutFrameInfo = LastPresentedFrame;
	}

	void FRivermaxOutputStream::CompleteCurrentFrame(bool bReleaseFrame)
	{
		if (ensure(CurrentFrame))
		{
			{
				FScopeLock Lock(&PresentedFrameCS);
				LastPresentedFrame.RenderedFrameNumber = CurrentFrame->FrameIdentifier;
				LastPresentedFrame.PresentedFrameBoundaryNumber = StreamData.LastAlignmentPointFrameNumber;
			}
			
			// We don't release when there is no new frame, so we keep a hold on it to repeat it.
			if (bReleaseFrame)
			{
				FrameManager->MarkAsSent(CurrentFrame);
				CurrentFrame.Reset();
			}
		}
	}
}


// Copyright Epic Games, Inc. All Rights Reserved.

#include "RivermaxInputStream.h"

#include "Async/Async.h"
#include "CudaModule.h"
#include "HAL/CriticalSection.h"
#include "ID3D12DynamicRHI.h"
#include "IRivermaxCoreModule.h"
#include "IRivermaxManager.h"
#include "Misc/ByteSwap.h"
#include "RivermaxLog.h"
#include "RivermaxPTPUtils.h"
#include "RivermaxTracingUtils.h"
#include "RivermaxUtils.h"
#include "RTPHeader.h"

#if PLATFORM_WINDOWS
#include <WS2tcpip.h>
#endif

namespace UE::RivermaxCore::Private
{
	/** Initialization calls to Rivermax SDK isn't threadsafe and to prevent stalling game thread, a critical section is shared across streams */
	static FCriticalSection InitCriticalSection;

	static TAutoConsoleVariable<float> CVarWaitForCompletionTimeout(
		TEXT("Rivermax.Input.WaitCompletionTimeout"),
		0.25,
		TEXT("Maximum time to wait, in seconds, when waiting for a memory copy operation to complete on the gpu."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarExpectedPayloadSize(
		TEXT("Rivermax.Input.ExpectedPayloadSize"),
		1500,
		TEXT("Expected payload size used to initialize rivermax stream."),
		ECVF_Default);

	static bool GEnableLargeFirstPacketIntervalLogging = false;
	FAutoConsoleVariableRef CVarRivermaxEnableLargeFirstPacketIntervalLogging(
		TEXT("Rivermax.Input.EnableLargeFistPacketIntervalLogging")
		, UE::RivermaxCore::Private::GEnableLargeFirstPacketIntervalLogging
		, TEXT("Enables detection and logging of large delta times between frame boundary and first packet reception.")
		, ECVF_Default);

	static bool GClearFirstPacketIntervalStats = false;
	FAutoConsoleVariableRef CVarRivermaxClearFirstPacketIntervalStats(
		TEXT("Rivermax.Input.ClearFistPacketIntervalStats")
		, UE::RivermaxCore::Private::GClearFirstPacketIntervalStats
		, TEXT("Clears stats related to first packet interval detection.")
		, ECVF_Default);

	static int32 GLargeFirstPacketIntervalThresholdMicroSec = 2000;
	FAutoConsoleVariableRef CVarRivermaxLargeFirstPacketIntervalThreshold(
		TEXT("Rivermax.Input.LargeFistPacketIntervalThreshold")
		, UE::RivermaxCore::Private::GLargeFirstPacketIntervalThresholdMicroSec
		, TEXT("Microseconds required to consider a first packet interval to be large and be logged.")
		, ECVF_Default);


	void FFrameDescriptionTrackingData::ResetSingleFrameTracking()
	{
		PayloadSizeReceived.Empty();
	}

	void FFrameDescriptionTrackingData::EvaluateNewRTP(const FRTPHeader& NewHeader)
	{
		UpdateResolutionDetection(NewHeader);
		UpdatePayloadSizeTracking(NewHeader);

		if (NewHeader.bIsMarkerBit)
		{
			ResetSingleFrameTracking();
		}
	}

	void FFrameDescriptionTrackingData::UpdateResolutionDetection(const FRTPHeader& NewHeader)
	{
		if (NewHeader.bIsMarkerBit)
		{
			const FVideoFormatInfo ExpectedFormatInfo = FStandardVideoFormat::GetVideoFormatInfo(ExpectedSamplingType);
			const uint16 LastPayloadSize = NewHeader.GetLastPayloadSize();
			if (LastPayloadSize % ExpectedFormatInfo.PixelGroupSize == 0)
			{
				DetectedResolution.X = NewHeader.GetLastRowOffset() + (LastPayloadSize / ExpectedFormatInfo.PixelGroupSize * ExpectedFormatInfo.PixelGroupCoverage);
				DetectedResolution.Y = NewHeader.GetLastRowNumber() + 1;
				
				// Renable logging when a valid incoming frame was detected
				bHasLoggedSamplingWarning = false;
			}
			else if(!bHasLoggedSamplingWarning)
			{
				bHasLoggedSamplingWarning = true;
				UE_LOG(LogRivermax, Warning, TEXT("Detected incoming signal with unexpected sampling type."));
			}
		}
	}

	void FFrameDescriptionTrackingData::UpdatePayloadSizeTracking(const FRTPHeader& NewHeader)
	{
		if (NewHeader.SRD1.Length > 0)
		{
			uint32 PayloadSize = NewHeader.SRD1.Length;
			if (NewHeader.SRD1.bHasContinuation)
			{
				PayloadSize += NewHeader.SRD2.Length;
			}

			if (PayloadSizeReceived.IsEmpty())
			{
				CommonPayloadSize = PayloadSize;
			}
			else if (!NewHeader.bIsMarkerBit && PayloadSize != CommonPayloadSize)
			{
				CommonPayloadSize = -1;
			}

			PayloadSizeReceived.FindOrAdd(PayloadSize) += 1;
		}
	}

	bool FFrameDescriptionTrackingData::HasDetectedValidResolution() const
	{
		return DetectedResolution.X > 0 && DetectedResolution.Y > 0;
	}

	FRivermaxInputStream::FRivermaxInputStream()
	{

	}

	FRivermaxInputStream::~FRivermaxInputStream()
	{
		Uninitialize();
	}

	bool FRivermaxInputStream::Initialize(const FRivermaxInputStreamOptions& InOptions, IRivermaxInputStreamListener& InListener)
	{
		IRivermaxCoreModule& RivermaxModule = FModuleManager::LoadModuleChecked<IRivermaxCoreModule>(TEXT("RivermaxCore"));
		if (RivermaxModule.GetRivermaxManager()->ValidateLibraryIsLoaded() == false)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Can't create Rivermax Input Stream. Library isn't initialized."));
			return false;
		}

		Options = InOptions;
		Listener = &InListener;
		FormatInfo = FStandardVideoFormat::GetVideoFormatInfo(Options.PixelFormat);
		ExpectedPayloadSize = CVarExpectedPayloadSize.GetValueOnGameThread();
		bIsDynamicHeaderEnabled = RivermaxModule.GetRivermaxManager()->EnableDynamicHeaderSupport(Options.InterfaceAddress);
		CachedAPI = RivermaxModule.GetRivermaxManager()->GetApi();
		checkSlow(CachedAPI);

		if (Options.bEnforceVideoFormat)
		{
			StreamResolution = Options.EnforcedResolution;
		}
		
		// For now, we don't try to detect pixel format so use what the user selected
		FrameDescriptionTracking.ExpectedSamplingType = Options.PixelFormat;

		InitTaskFuture = Async(EAsyncExecution::TaskGraph, [this]()
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FRivermaxInputStream::InitStream);
			FScopeLock Lock(&InitCriticalSection);

			// If the stream is trying to shutdown before the init task has even started, don't bother
			if (bIsShuttingDown)
			{
				return;
			}

			// Initialize stream builder
			CachedAPI->rmx_input_init_stream(&StreamParameters, RMX_INPUT_RAW_PACKET);

			FString ErrorMessage;
			FNetworkSettings NetworkSettings;
			bool bWasSuccessful = InitializeNetworkSettings(NetworkSettings, ErrorMessage);
			if(bWasSuccessful)
			{
				bWasSuccessful = InitializeStreamParameters(ErrorMessage);

				if (bWasSuccessful)
				{
					bWasSuccessful = FinalizeStreamCreation(NetworkSettings, ErrorMessage);
				}
			}

			if (bWasSuccessful)
			{
				// Stream creation succeeded, launch reception thread
				bIsActive = true;
				RivermaxThread = FRunnableThread::Create(this, TEXT("Rivermax InputStream Thread"), 128 * 1024, TPri_AboveNormal, FPlatformAffinity::GetPoolThreadMask());

				UE_LOG(LogRivermax, Display, TEXT("Input stream started listening to stream %s:%d on interface %s%s")
					, *Options.StreamAddress
					, Options.Port
					, *Options.InterfaceAddress
					, bIsUsingGPUDirect ? TEXT(" using GPUDirect") : TEXT(""));
			}
			else
			{
				UE_LOG(LogRivermax, Warning, TEXT("%s"), *ErrorMessage);
			}
			
			FRivermaxInputInitializationResult Result;
			Result.bHasSucceed = bWasSuccessful;
			Result.bIsGPUDirectSupported = bIsUsingGPUDirect;
			Listener->OnInitializationCompleted(Result);
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

		if (bIsDynamicHeaderEnabled)
		{
			if (IRivermaxCoreModule* RivermaxModule = FModuleManager::GetModulePtr<IRivermaxCoreModule>(TEXT("RivermaxCore")))
			{
				RivermaxModule->GetRivermaxManager()->DisableDynamicHeaderSupport(Options.InterfaceAddress);
			}
			bIsDynamicHeaderEnabled = false;
		}

		DeallocateBuffers();
	}

	void FRivermaxInputStream::Process_AnyThread()
	{
		const size_t MinChunkSize = 0;
		const size_t MaxChunkSize = 5000;
		const int Timeout = 0;
		const int Flags = 0;
		const rmx_input_completion* Completion = nullptr;
		
		checkSlow(CachedAPI);
		rmx_status Status = CachedAPI->rmx_input_get_next_chunk(&BufferConfiguration.ChunkHandle);
		if (Status == RMX_OK)
		{
			Completion = CachedAPI->rmx_input_get_chunk_completion(&BufferConfiguration.ChunkHandle);
			if (Completion)
			{
				ParseChunks(Completion);
			}
		}
		else
		{
			UE_LOG(LogRivermax, Warning, TEXT("Rivermax Input stream failed to get next chunk. Status: %d"), Status);
		}

		FPlatformProcess::SleepNoStats(UE::RivermaxCore::Private::Utils::SleepTimeSeconds);
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
			checkSlow(CachedAPI);
			rmx_status Status = CachedAPI->rmx_input_detach_flow(StreamId, &FlowAttribute);
			if (Status != RMX_OK)
			{
				UE_LOG(LogRivermax, Warning, TEXT("Failed to detach rivermax flow %d from input stream %d. Status: %d"), FlowTag, StreamId, Status);
			}

			Status = CachedAPI->rmx_input_destroy_stream(StreamId);

			if (Status != RMX_OK)
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

	void FRivermaxInputStream::ParseChunks(const rmx_input_completion* Completion)
	{
		const size_t ChunkCount = rmx_input_get_completion_chunk_size(Completion);

		for (uint64 StrideIndex = 0; StrideIndex < ChunkCount; ++StrideIndex)
		{
			++StreamStats.ChunksReceived;

			const uint8* RawHeaderPtr = reinterpret_cast<const uint8_t*>(rmx_input_get_completion_ptr(Completion, BufferConfiguration.HeaderBlockID));
			const uint8* DataPtr = reinterpret_cast<const uint8_t*>(rmx_input_get_completion_ptr(Completion, BufferConfiguration.DataBlockID));

			RawHeaderPtr += StrideIndex * BufferConfiguration.HeaderStride;
			DataPtr += StrideIndex * BufferConfiguration.DataStride;

			// When using RMX_INPUT_RAW_PACKET, app head is preceded by net header
			const uint8_t* AppHeader = RawHeaderPtr; // App header. If the stream type is RMX_INPUT_RAW_PACKET then app header is preceded by net header

			const rmx_input_packet_info* PacketInfo = CachedAPI->rmx_input_get_packet_info(&BufferConfiguration.ChunkHandle, StrideIndex);
			const size_t PacketSize = rmx_input_get_packet_size(PacketInfo, BufferConfiguration.DataBlockID);
			const size_t HeaderSize = rmx_input_get_packet_size(PacketInfo, BufferConfiguration.HeaderBlockID);
			if (PacketSize > 0 && HeaderSize > 0)
			{
				if (FlowTag)
				{
					const uint32 PacketTag = rmx_input_get_packet_flow_tag(PacketInfo);
					if (PacketTag != FlowTag)
					{
						UE_LOG(LogRivermax, Error, TEXT("Received data from unexpected FlowTag '%d'. Expected '%d'."), PacketTag, FlowTag);
						Listener->OnStreamError();
						bIsShuttingDown = true;
						return;
					}
				}

				const uint64 PacketTimstamp = rmx_input_get_packet_timestamp(PacketInfo);
				

				// Get RTPHeader address from the raw net header
				const FRawRTPHeader& RawRTPHeaderPtr = reinterpret_cast<const FRawRTPHeader&>(*GetRTPHeaderPointer(RawHeaderPtr));
				if (RawRTPHeaderPtr.Version == 2)
				{
					FRTPHeader RTPHeader(RawRTPHeaderPtr);
					
					// Add trace for the first packet of a frame to help visualize reception of a full frame in time
					if (bIsFirstPacketReceived == false)
					{
						const uint32 FrameNumber = Utils::TimestampToFrameNumber(RTPHeader.Timestamp, Options.FrameRate);
						TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FRivermaxTracingUtils::RmaxInStartingFrameTraceEvents[FrameNumber % 10]);

						IRivermaxCoreModule& RivermaxModule = FModuleManager::LoadModuleChecked<IRivermaxCoreModule>(TEXT("RivermaxCore"));
						const uint64 CurrentTime = RivermaxModule.GetRivermaxManager()->GetTime();

						bIsFirstPacketReceived = true;

						if (StreamStats.EndOfFrameReceived > 0)
						{
							const uint64 FrameBoundary = GetAlignmentPointFromFrameNumber(GetFrameNumber(CurrentTime, Options.FrameRate), Options.FrameRate);
							const uint64 FirstPacketInterval = CurrentTime - FrameBoundary;
							StreamStats.MinFirstPacketIntervalNS = FMath::Min(StreamStats.MinFirstPacketIntervalNS, FirstPacketInterval);
							StreamStats.MaxFirstPacketIntervalNS = FMath::Max(StreamStats.MaxFirstPacketIntervalNS, FirstPacketInterval);
							
							constexpr double Alpha = 0.8;
							StreamStats.FirstPacketIntervalAccumulatorNS = Alpha * FirstPacketInterval + ((1.0 - Alpha) * StreamStats.FirstPacketIntervalAccumulatorNS);

							if (GClearFirstPacketIntervalStats)
							{
								GClearFirstPacketIntervalStats = false;
								StreamStats.MinFirstPacketIntervalNS = TNumericLimits<uint64>::Max();
								StreamStats.MaxFirstPacketIntervalNS = TNumericLimits<uint64>::Min();
								StreamStats.FirstPacketIntervalAccumulatorNS = 0;
							}

							if (GEnableLargeFirstPacketIntervalLogging)
							{
								const uint32 IntervalMicroSec = FirstPacketInterval / 1000;
								if (IntervalMicroSec > (uint32)GLargeFirstPacketIntervalThresholdMicroSec)
								{

									UE_LOG(LogRivermax, Warning, TEXT("Large First packet interval: %llu. Min: %llu. Max: %llu. Avg: %llu.")
									, FirstPacketInterval
									, StreamStats.MinFirstPacketIntervalNS
									, StreamStats.MaxFirstPacketIntervalNS
									, StreamStats.FirstPacketIntervalAccumulatorNS);
								}
							}
						}
					}

					StreamStats.BytesReceived += PacketSize + HeaderSize;
					
					UpdateFrameTracking(RTPHeader);

					switch (State)
					{
					case EReceptionState::Receiving:
					{
						FrameReceptionState(RTPHeader, DataPtr);
						break;
					}
					case EReceptionState::WaitingForMarker:
					{
						WaitForMarkerState(RTPHeader);
						break;
					}
					case EReceptionState::FrameError:
					{
						FrameErrorState(RTPHeader);
						break;
					}
					default:
					{
						checkNoEntry();
					}
					}
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
		}
	}

	bool FRivermaxInputStream::PrepareNextFrame(const FRTPHeader& RTPHeader)
	{
		using namespace UE::RivermaxCore::Private::Utils;

		FRivermaxInputVideoFrameDescriptor Descriptor;
		Descriptor.Timestamp = RTPHeader.Timestamp;
		Descriptor.FrameNumber = TimestampToFrameNumber(RTPHeader.Timestamp, Options.FrameRate);
		Descriptor.Width = StreamResolution.X;
		Descriptor.Height = StreamResolution.Y;
		Descriptor.PixelFormat = Options.PixelFormat;
		const uint32 PixelCount = StreamResolution.X * StreamResolution.Y;
		const uint32 FrameSize = PixelCount / FormatInfo.PixelGroupCoverage * FormatInfo.PixelGroupSize;
		Descriptor.VideoBufferSize = FrameSize;
		FRivermaxInputVideoFrameRequest Request;
		Listener->OnVideoFrameRequested(Descriptor, Request);

		// Reset current frame to know when we have a valid one
		StreamData.CurrentFrame = nullptr;
		if (bIsUsingGPUDirect)
		{
			if(Request.GPUBuffer)
			{
				StreamData.CurrentFrame = GetMappedBuffer(Request.GPUBuffer);
			}
		}
		else
		{	
			if (Request.VideoBuffer)
			{
				StreamData.CurrentFrame = Request.VideoBuffer;
			}
		}

		// Verify if we were able to request a valid frame. If engine is blocked, it could happen that there is none available 
		if (StreamData.CurrentFrame == nullptr)
		{
			// If we failed getting one, reset the valid first frame received and wait for the next one
			UE_LOG(LogRivermax, Verbose, TEXT("Could not get a new frame for incoming frame with timestamp %u and frame number %u"), RTPHeader.Timestamp, Descriptor.FrameNumber);
			Listener->OnVideoFrameReceptionError(Descriptor);
			State = EReceptionState::FrameError;
			FrameErrorState(RTPHeader);
			return false;
		}

		StreamData.WritingOffset = 0;
		StreamData.ReceivedSize = 0;
		StreamData.ExpectedSize = Descriptor.VideoBufferSize;
		StreamData.DeviceWritePointerOne = nullptr;
		StreamData.SizeToWriteOne = 0;
		StreamData.DeviceWritePointerTwo = nullptr;
		StreamData.SizeToWriteTwo = 0;
		bIsFirstPacketReceived = false;

		return true;
	}

	void FRivermaxInputStream::LogStats()
	{
		static constexpr double LoggingInterval = 1.0;

		const double CurrentTime = FPlatformTime::Seconds();
		if (CurrentTime - LastLoggingTimestamp >= LoggingInterval)
		{
			LastLoggingTimestamp = CurrentTime;
			UE_LOG(LogRivermax, Verbose, TEXT("Stream %d (%s:%u) stats: FrameCount: %llu, EndOfFrame: %llu, Chunks: %llu, Bytes: %llu, PacketLossInFrame: %llu, TotalPacketLoss: %llu, BiggerFrames: %llu, InvalidFrames: %llu, InvalidHeader: %llu, EmptyCompletion: %llu")
			, StreamId
			, *Options.StreamAddress
			, Options.Port
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

			UE_LOG(LogRivermax, Verbose, TEXT("Stream %d (%s:%u) first packet interval: - Min: %llu. Max: %llu. Avg: %llu.")
				, StreamId
				, *Options.StreamAddress
				, Options.Port
				, StreamStats.MinFirstPacketIntervalNS
				, StreamStats.MaxFirstPacketIntervalNS
				, StreamStats.FirstPacketIntervalAccumulatorNS);

		}
	}

	void FRivermaxInputStream::AllocateBuffers()
	{
		IRivermaxCoreModule& RivermaxModule = FModuleManager::LoadModuleChecked<IRivermaxCoreModule>(TEXT("RivermaxCore"));
		if (RivermaxModule.GetRivermaxManager()->IsGPUDirectInputSupported() && Options.bUseGPUDirect)
		{
			bIsUsingGPUDirect = AllocateGPUBuffers();
		}

		constexpr uint32 CacheLineSize = PLATFORM_CACHE_LINE_SIZE;
		if (bIsUsingGPUDirect == false)
		{
			BufferConfiguration.DataMemory->addr = FMemory::Malloc(BufferConfiguration.PayloadSize, CacheLineSize);
		}
		
		BufferConfiguration.HeaderMemory->addr = FMemory::Malloc(BufferConfiguration.HeaderSize, CacheLineSize);

		constexpr rmx_mkey_id InvalidKey = ((rmx_mkey_id)(-1L));
		BufferConfiguration.DataMemory->mkey = InvalidKey;
		BufferConfiguration.HeaderMemory->mkey = InvalidKey;
	}

	bool FRivermaxInputStream::AllocateGPUBuffers()
	{
		// Allocate memory space where rivermax input will write received buffer to

		TRACE_CPUPROFILER_EVENT_SCOPE(FRivermaxInputStream::AllocateGPUBuffers);

		const ERHIInterfaceType RHIType = RHIGetInterfaceType();
		if (RHIType != ERHIInterfaceType::D3D12)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Can't initialize input to use GPUDirect. RHI is %d but only Dx12 is supported at the moment."), int(RHIType));
			return false;
		}

		FCUDAModule& CudaModule = FModuleManager::GetModuleChecked<FCUDAModule>("CUDA");
		GPUDeviceIndex = CudaModule.GetCudaDeviceIndex();
		CudaModule.DriverAPI()->cuCtxPushCurrent(CudaModule.GetCudaContextForDevice(GPUDeviceIndex));

		// Todo: Add support for mgpu. 
		CUdevice CudaDevice;
		CUresult Status = CudaModule.DriverAPI()->cuDeviceGet(&CudaDevice, GPUDeviceIndex);
		if (Status != CUDA_SUCCESS)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Can't initialize input to use GPUDirect. Failed to get a Cuda device for GPU %d. Status: %d"), GPUDeviceIndex, Status);
			return false;
		}

		CUmemAllocationProp AllocationProperties = {};
		AllocationProperties.type = CU_MEM_ALLOCATION_TYPE_PINNED;
		AllocationProperties.allocFlags.gpuDirectRDMACapable = 1; //is that required?
		AllocationProperties.allocFlags.usage = 0;
		AllocationProperties.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
		AllocationProperties.location.id = CudaDevice;

		// Get memory granularity required for cuda device. We need to align allocation with this.
		size_t Granularity;
		Status = CudaModule.DriverAPI()->cuMemGetAllocationGranularity(&Granularity, &AllocationProperties, CU_MEM_ALLOC_GRANULARITY_RECOMMENDED);
		if (Status != CUDA_SUCCESS)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Can't initialize input to use GPUDirect. Failed to get allocation granularity. Status: %d"), Status);
			return false;
		}

		// Cuda requires allocated memory to be aligned with a certain granularity
		const size_t CudaAlignedAllocation = (BufferConfiguration.PayloadSize % Granularity) ? BufferConfiguration.PayloadSize + (Granularity - (BufferConfiguration.PayloadSize % Granularity)) : BufferConfiguration.PayloadSize;
		
		CUdeviceptr CudaBaseAddress;
		constexpr CUdeviceptr InitialAddress = 0;
		constexpr int32 Flags = 0;
		Status = CudaModule.DriverAPI()->cuMemAddressReserve(&CudaBaseAddress, CudaAlignedAllocation, Granularity, InitialAddress, Flags);
		if (Status != CUDA_SUCCESS)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Can't initialize input to use GPUDirect. Failed to reserve memory for %d bytes. Status: %d"), CudaAlignedAllocation, Status);
			return false;
		}

		// Make the allocation on device memory
		CUmemGenericAllocationHandle Handle;
		Status = CudaModule.DriverAPI()->cuMemCreate(&Handle, CudaAlignedAllocation, &AllocationProperties, Flags);
		if (Status != CUDA_SUCCESS)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Can't initialize input to use GPUDirect. Failed to create memory on device. Status: %d"), Status);
			return false;
		}
		UE_LOG(LogRivermax, Verbose, TEXT("Allocated %d cuda memory"), CudaAlignedAllocation);

		bool bExit = false;
		constexpr int32 Offset = 0;
		Status = CudaModule.DriverAPI()->cuMemMap(CudaBaseAddress, CudaAlignedAllocation, Offset, Handle, Flags);
		if (Status != CUDA_SUCCESS)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Can't initialize input to use GPUDirect. Failed to map memory. Status: %d"), Status);
			// Need to release handle no matter what
			bExit = true;
		}

		GPUAllocatedMemorySize = CudaAlignedAllocation;
		GPUAllocatedMemoryBaseAddress = reinterpret_cast<void*>(CudaBaseAddress);

		Status = CudaModule.DriverAPI()->cuMemRelease(Handle);
		if (Status != CUDA_SUCCESS)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Can't initialize input to use GPUDirect. Failed to release handle. Status: %d"), Status);
			return false;
		}

		if (bExit)
		{
			return false;
		}

		// Setup access description.
		CUmemAccessDesc MemoryAccessDescription = {};
		MemoryAccessDescription.flags = CU_MEM_ACCESS_FLAGS_PROT_READWRITE;
		MemoryAccessDescription.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
		MemoryAccessDescription.location.id = CudaDevice;
		constexpr int32 Count = 1;
		Status = CudaModule.DriverAPI()->cuMemSetAccess(CudaBaseAddress, CudaAlignedAllocation, &MemoryAccessDescription, Count);
		if (Status != CUDA_SUCCESS)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Can't initialize input to use GPUDirect. Failed to configure memory access. Status: %d"), Status);
			return false;
		}

		CUstream CudaStream;
		Status = CudaModule.DriverAPI()->cuStreamCreate(&CudaStream, CU_STREAM_NON_BLOCKING);
		if (Status != CUDA_SUCCESS)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Can't initialize input to use GPUDirect. Failed to create its stream. Status: %d"), Status);
			return false;
		}

		GPUStream = CudaStream;

		Status = CudaModule.DriverAPI()->cuCtxSynchronize();
		if (Status != CUDA_SUCCESS)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Can't initialize input to use GPUDirect. Failed to synchronize context. Status: %d"), Status);
			return false;
		}

		// Give rivermax input buffer config the pointer to gpu allocated memory
		BufferConfiguration.DataMemory->addr = GPUAllocatedMemoryBaseAddress;

		CallbackPayload = MakeShared<FCallbackPayload>();

		return true;
	}

	void FRivermaxInputStream::DeallocateBuffers()
	{
		if (GPUAllocatedMemorySize > 0)
		{
			FCUDAModule& CudaModule = FModuleManager::GetModuleChecked<FCUDAModule>("CUDA");
			CudaModule.DriverAPI()->cuCtxPushCurrent(CudaModule.GetCudaContextForDevice(GPUDeviceIndex));

			const CUdeviceptr BaseAddress = reinterpret_cast<CUdeviceptr>(GPUAllocatedMemoryBaseAddress);
			CUresult Status = CudaModule.DriverAPI()->cuMemUnmap(BaseAddress, GPUAllocatedMemorySize);
			if (Status != CUDA_SUCCESS)
			{
				UE_LOG(LogRivermax, Warning, TEXT("Failed to unmap cuda memory used for input stream. Status: %d"), Status);
			}

			Status = CudaModule.DriverAPI()->cuMemAddressFree(BaseAddress, GPUAllocatedMemorySize);
			if (Status != CUDA_SUCCESS)
			{
				UE_LOG(LogRivermax, Warning, TEXT("Failed to free cuda memory used for input stream. Status: %d"), Status);
			}
			UE_LOG(LogRivermax, Verbose, TEXT("Deallocated %d cuda memory at address %d"), GPUAllocatedMemorySize, GPUAllocatedMemoryBaseAddress);

			GPUAllocatedMemorySize = 0;
			GPUAllocatedMemoryBaseAddress = 0;


			for (const TPair<FRHIBuffer*, void*>& Entry : BufferGPUMemoryMap)
			{
				if (Entry.Value)
				{
					CudaModule.DriverAPI()->cuMemFree(reinterpret_cast<CUdeviceptr>(Entry.Value));
				}
			}
			BufferGPUMemoryMap.Empty();

			Status = CudaModule.DriverAPI()->cuStreamDestroy(reinterpret_cast<CUstream>(GPUStream));
			if (Status != CUDA_SUCCESS)
			{
				UE_LOG(LogRivermax, Warning, TEXT("Failed to destroy cuda stream. Status: %d"), Status);
			}
			GPUStream = nullptr;


			CudaModule.DriverAPI()->cuCtxPopCurrent(nullptr);
		}
	}

	void* FRivermaxInputStream::GetMappedBuffer(const FBufferRHIRef& InBuffer)
	{
		// If we are here, d3d12 had to have been validated
		const ERHIInterfaceType RHIType = RHIGetInterfaceType();
		check(RHIType == ERHIInterfaceType::D3D12);

		//Do we already have a mapped address for this buffer
		if (BufferGPUMemoryMap.Find((InBuffer)) == nullptr)
		{
			int64 BufferMemorySize = 0;
			CUexternalMemory MappedExternalMemory = nullptr;
			HANDLE D3D12BufferHandle = 0;
			CUDA_EXTERNAL_MEMORY_HANDLE_DESC CudaExtMemHandleDesc = {};

			// Create shared handle for our buffer
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(RmaxInput_D3D12CreateSharedHandle);

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

			FCUDAModule& CudaModule = FModuleManager::GetModuleChecked<FCUDAModule>("CUDA");

			CudaModule.DriverAPI()->cuCtxPushCurrent(CudaModule.GetCudaContext());

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
				if (Result != CUDA_SUCCESS || NewMemory == 0)
				{
					UE_LOG(LogRivermax, Error, TEXT("Failed to get shared buffer mapped memory. Error: %d"), Result);
					CudaModule.DriverAPI()->cuCtxPushCurrent(nullptr);
					return nullptr;
				}

				BufferGPUMemoryMap.Add(InBuffer, reinterpret_cast<void*>(NewMemory));
			}

			CudaModule.DriverAPI()->cuCtxPushCurrent(nullptr);
		}

		// At this point, we have the mapped buffer in cuda space and we can use it to schedule a memcpy on cuda engine.
		return BufferGPUMemoryMap[InBuffer];
	}

	void FRivermaxInputStream::ProcessSRD(const FRTPHeader& RTPHeader, const uint8* DataPtr)
	{
		if (StreamData.CurrentFrame == nullptr)
		{
			if (PrepareNextFrame(RTPHeader) == false)
			{
				return;
			}
		}

		if (RTPHeader.SRD1.Length <= 0)
		{
			return;
		}

		uint32 DataOffset = 0;
		uint32 PayloadSize = RTPHeader.SRD1.Length;
		if (RTPHeader.SRD1.bHasContinuation)
		{
			PayloadSize += RTPHeader.SRD2.Length;
		}

		if (bIsUsingGPUDirect)
		{
			// Initial case (start address)
			if (StreamData.DeviceWritePointerOne == nullptr)
			{
				StreamData.DeviceWritePointerOne = DataPtr;
				StreamData.SizeToWriteOne = PayloadSize;
			}
			else
			{
				// Detection of wrap around -> Move tracking to second buffer
				if (StreamData.DeviceWritePointerTwo == nullptr && DataPtr < StreamData.DeviceWritePointerOne)
				{
					StreamData.DeviceWritePointerTwo = DataPtr;
					StreamData.SizeToWriteTwo = 0;
				}

				// Case where we track memory in first buffer
				if (StreamData.DeviceWritePointerTwo == nullptr)
				{
					StreamData.SizeToWriteOne += PayloadSize;
				}
				else // Tracking memory in second buffer
				{
					StreamData.SizeToWriteTwo += PayloadSize;
				}
			}
		}
		else
		{
			uint8* WriteBuffer = reinterpret_cast<uint8*>(StreamData.CurrentFrame);
			FMemory::Memcpy(&WriteBuffer[StreamData.WritingOffset], &DataPtr[DataOffset], RTPHeader.SRD1.Length);
			StreamData.WritingOffset += RTPHeader.SRD1.Length;

			if (RTPHeader.SRD1.bHasContinuation)
			{
				DataOffset += RTPHeader.SRD1.Length;
				FMemory::Memcpy(&WriteBuffer[StreamData.WritingOffset], &DataPtr[DataOffset], RTPHeader.SRD2.Length);
				StreamData.WritingOffset += RTPHeader.SRD2.Length;
			}
		}

		StreamData.ReceivedSize += PayloadSize;
	}

	void FRivermaxInputStream::ProcessLastSRD(const FRTPHeader& RTPHeader, const uint8* DataPtr)
	{
		FRivermaxInputVideoFrameDescriptor Descriptor;
		Descriptor.Width = StreamResolution.X;
		Descriptor.Height = StreamResolution.Y;
		Descriptor.PixelFormat = Options.PixelFormat;
		Descriptor.Timestamp = RTPHeader.Timestamp;
		const uint32 PixelCount = StreamResolution.X * StreamResolution.Y;
		Descriptor.VideoBufferSize = PixelCount / FormatInfo.PixelGroupCoverage * FormatInfo.PixelGroupSize;
		Descriptor.FrameNumber = Utils::TimestampToFrameNumber(RTPHeader.Timestamp, Options.FrameRate);

		if (StreamData.ReceivedSize == StreamData.ExpectedSize)
		{
			++StreamStats.FramesReceived;
			
			TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FRivermaxTracingUtils::RmaxInReceivedFrameTraceEvents[Descriptor.FrameNumber % 10]);

			if (bIsUsingGPUDirect)
			{
				// For gpudirect, we need all payload sizes to be equal with the exception of the last one.
				if (FrameDescriptionTracking.CommonPayloadSize <= 0)
				{
					UE_LOG(LogRivermax, Warning, TEXT("Unsupported variable SRD length detected while GPUDirect for input stream is used. Disable and reopen the stream."));
					Listener->OnStreamError();
					bIsShuttingDown = true;
				}

				//Frame received entirely, time to copy it from rivermax gpu scratchpad to our own gpu memory
				FCUDAModule& CudaModule = FModuleManager::GetModuleChecked<FCUDAModule>("CUDA");
				CUresult Result = CudaModule.DriverAPI()->cuCtxPushCurrent(CudaModule.GetCudaContext());

				const CUdeviceptr DestinationGPUMemory = reinterpret_cast<CUdeviceptr>(StreamData.CurrentFrame);
				const CUdeviceptr SourceGPUMemoryOne = reinterpret_cast<CUdeviceptr>(StreamData.DeviceWritePointerOne);

				const uint32 NumSRDPartOne = StreamData.SizeToWriteOne / FrameDescriptionTracking.CommonPayloadSize;
				const uint32 NumSRDPartTwo = StreamData.SizeToWriteTwo / FrameDescriptionTracking.CommonPayloadSize;

				// Use cuda's 2d memcopy to do a source and destination stride difference memcopy
				// We initialize rivermax stream with a payload size blindly since we don't know what the sender will use
				// So, we use a big value by default and expect SRD to be smaller
				// This memcopy will consume the SRD size value but jump the init payload size value on the source address
				// Limitation is that this will only work for fixed SRD across a frame
				CUDA_MEMCPY2D StrideDescription;
				FMemory::Memset(StrideDescription, 0);
				StrideDescription.srcDevice = SourceGPUMemoryOne;
				StrideDescription.dstDevice = DestinationGPUMemory;
				StrideDescription.dstMemoryType = CUmemorytype::CU_MEMORYTYPE_DEVICE;
				StrideDescription.srcMemoryType = CUmemorytype::CU_MEMORYTYPE_DEVICE;
				StrideDescription.srcPitch = ExpectedPayloadSize; //Source pitch is the expected payload used at init
				StrideDescription.dstPitch = FrameDescriptionTracking.CommonPayloadSize; //Destination pitch is the fixed SRD size we received
				StrideDescription.WidthInBytes = FrameDescriptionTracking.CommonPayloadSize; //Width in bytes is the amount to copy, the SRD size
				StrideDescription.Height = NumSRDPartOne;
				Result = CudaModule.DriverAPI()->cuMemcpy2DAsync(&StrideDescription, reinterpret_cast<CUstream>(GPUStream));

				// Keep track where we are at in the source buffer
				CUdeviceptr SourcePtr = SourceGPUMemoryOne + (ExpectedPayloadSize * NumSRDPartOne);

				if (StreamData.DeviceWritePointerTwo != nullptr && StreamData.SizeToWriteTwo > 0)
				{
					StrideDescription.srcDevice = reinterpret_cast<CUdeviceptr>(StreamData.DeviceWritePointerTwo);
					StrideDescription.dstDevice = reinterpret_cast<CUdeviceptr>(StreamData.CurrentFrame) + StreamData.SizeToWriteOne;
					StrideDescription.Height = NumSRDPartTwo;
					Result = CudaModule.DriverAPI()->cuMemcpy2DAsync(&StrideDescription, reinterpret_cast<CUstream>(GPUStream));

					// Update source pointer since we used the second pointer tracker
					SourcePtr = StrideDescription.srcDevice + (ExpectedPayloadSize * NumSRDPartTwo);
				}

				// Verify if we copied whole frame using the stride 2d copy or we're missing the last SRD of fractional size
				const uint32 TotalMemoryCopied = (FrameDescriptionTracking.CommonPayloadSize * (NumSRDPartOne + NumSRDPartTwo));
				if (TotalMemoryCopied < StreamData.ExpectedSize)
				{
					const uint32 SizeLeftToCopy = StreamData.ExpectedSize - TotalMemoryCopied;
					if (ensure(SizeLeftToCopy == RTPHeader.SRD1.Length))
					{
						CUdeviceptr DestinationPtr = DestinationGPUMemory + TotalMemoryCopied;
						Result = CudaModule.DriverAPI()->cuMemcpyDtoDAsync(DestinationPtr, SourcePtr, SizeLeftToCopy, reinterpret_cast<CUstream>(GPUStream));
					}
				}

				if (Result != CUDA_SUCCESS)
				{
					UE_LOG(LogRivermax, Warning, TEXT("Failed to copy received buffer to shared memory. Error: %d"), Result);
					Listener->OnVideoFrameReceptionError(Descriptor);
					State = EReceptionState::FrameError;
					FrameErrorState(RTPHeader);
					return;
				}

				auto CudaCallback = [](void* userData)
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(FRivermaxInputStream::MemcopyCallback);
					if (userData)
					{
						// It might happen that our stream has been closed once the callback is triggered
						TWeakPtr<FCallbackPayload>* WeakPayloadPtr = reinterpret_cast<TWeakPtr<FCallbackPayload>*>(userData);
						TWeakPtr<FCallbackPayload> WeakPayload = *WeakPayloadPtr;
						if (TSharedPtr<FCallbackPayload> Payload = WeakPayload.Pin())
						{
							Payload->bIsWaitingForPendingCopy = false;
						}
					}
				};

				// Schedule a callback to know when to make the frame available
				CallbackPayload->bIsWaitingForPendingCopy = true;
				TWeakPtr<FCallbackPayload> WeakPayload = CallbackPayload;
				CudaModule.DriverAPI()->cuLaunchHostFunc(reinterpret_cast<CUstream>(GPUStream), CudaCallback, &WeakPayload);

				FCUDAModule::CUDA().cuCtxPopCurrent(nullptr);

				// For now, we wait for the cuda callback before we move on receiving next frame. 
				// Will need to update this and make the frame available from the cuda callback to avoid losing packets 
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(FRivermaxInputStream:WaitingPendingOperation)
					const double CallbackTimestamp = FPlatformTime::Seconds();
					while (CallbackPayload->bIsWaitingForPendingCopy == true && bIsShuttingDown == false)
					{
						FPlatformProcess::SleepNoStats(UE::RivermaxCore::Private::Utils::SleepTimeSeconds);
						if (FPlatformTime::Seconds() - CallbackTimestamp > CVarWaitForCompletionTimeout.GetValueOnAnyThread())
						{
							UE_LOG(LogRivermax, Error, TEXT("Waiting for gpu copy of sample timed out."));
							Listener->OnStreamError();
							CallbackPayload->bIsWaitingForPendingCopy = false;
							break;
						}
					}
				}
			}

			// Finished processing incoming frame, reset tracking data associated with it
			FrameDescriptionTracking.ResetSingleFrameTracking();

			// No need to provide the new frame and prepare the next one if we are shutting down
			if (bIsShuttingDown == false)
			{
				UE_LOG(LogRivermax, VeryVerbose, TEXT("RmaxRX frame number %u with timestamp %u."), Descriptor.FrameNumber, Descriptor.Timestamp);
				
				FRivermaxInputVideoFrameReception NewFrame;
				NewFrame.VideoBuffer = reinterpret_cast<uint8*>(StreamData.CurrentFrame);
				Listener->OnVideoFrameReceived(Descriptor, NewFrame);
				StreamData.CurrentFrame = nullptr;

				// Finished receiving a frame, move back to reception
				State = EReceptionState::Receiving;
			}
		}
		else
		{
			UE_LOG(LogRivermax, Warning, TEXT("End of frame received but not enough data was received (missing %d). Expected %d but received (%d)"), StreamData.ExpectedSize - StreamData.ReceivedSize, StreamData.ExpectedSize, StreamData.ReceivedSize);
			
			Listener->OnVideoFrameReceptionError(Descriptor);
			State = EReceptionState::FrameError;
			FrameErrorState(RTPHeader);
			
			++StreamStats.InvalidFramesCount;
		}
	}

	void FRivermaxInputStream::FrameReceptionState(const FRTPHeader& RTPHeader, const uint8* DataPtr)
	{
		FRivermaxInputVideoFrameDescriptor Descriptor;
		Descriptor.Width = StreamResolution.X;
		Descriptor.Height = StreamResolution.Y;
		Descriptor.PixelFormat = Options.PixelFormat;
		Descriptor.Timestamp = RTPHeader.Timestamp;
		const uint32 PixelCount = StreamResolution.X * StreamResolution.Y;
		const uint32 FrameSize = PixelCount / FormatInfo.PixelGroupCoverage * FormatInfo.PixelGroupSize;
		Descriptor.VideoBufferSize = FrameSize;
		Descriptor.FrameNumber = Utils::TimestampToFrameNumber(RTPHeader.Timestamp, Options.FrameRate);

		const uint64 LastSequenceNumberIncremented = StreamData.LastSequenceNumber + 1;

		const uint64 LostPackets = ((uint64)RTPHeader.SequencerNumber + 0x100000000 - LastSequenceNumberIncremented) & 0xFFFFFFFF;
		if (LostPackets > 0)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Lost %llu packets during reception of packet %u"), LostPackets, Descriptor.FrameNumber);
			
			StreamData.WritingOffset = 0;
			StreamData.ReceivedSize = 0;
			++StreamStats.TotalPacketLossCount;
			++StreamStats.FramePacketLossCount;

			// For now, if packets were lost, skip the incoming frame. We could improve that and have corrupted frames instead of skipping them but can be added later
			Listener->OnVideoFrameReceptionError(Descriptor);
			State = EReceptionState::FrameError;
			FrameErrorState(RTPHeader);
			return;
		}

		StreamData.LastSequenceNumber = RTPHeader.SequencerNumber;

		ProcessSRD(RTPHeader, DataPtr);

		if (StreamData.ReceivedSize > StreamData.ExpectedSize)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Received too much data (%d). Expected %d but received (%d)"), StreamData.ReceivedSize - StreamData.ExpectedSize, StreamData.ExpectedSize, StreamData.ReceivedSize);
			StreamData.WritingOffset = 0;
			StreamData.ReceivedSize = 0;
			++StreamStats.BiggerFramesCount;

			Listener->OnVideoFrameReceptionError(Descriptor);
			State = EReceptionState::FrameError;
			FrameErrorState(RTPHeader);
			return;
		}

		if (RTPHeader.bIsMarkerBit)
		{
			ProcessLastSRD(RTPHeader, DataPtr);

			StreamStats.FramePacketLossCount = 0;
			++StreamStats.EndOfFrameReceived;
		}
	}

	void FRivermaxInputStream::WaitForMarkerState(const FRTPHeader& RTPHeader)
	{
		if (RTPHeader.bIsMarkerBit)
		{
			StreamData.LastSequenceNumber = RTPHeader.SequencerNumber;
			StreamData.WritingOffset = 0;
			StreamData.ReceivedSize = 0;
			StreamData.DeviceWritePointerOne = nullptr;
			StreamData.SizeToWriteOne = 0;
			StreamData.DeviceWritePointerTwo = nullptr;
			StreamData.SizeToWriteTwo = 0;
			bIsFirstPacketReceived = false;

			StreamData.CurrentFrame = nullptr;
			State = EReceptionState::Receiving;
		}
	}

	void FRivermaxInputStream::FrameErrorState(const FRTPHeader& RTPHeader)
	{
		// In error, we just wait for the end of frame (marker bit) to restart reception
		WaitForMarkerState(RTPHeader);
	}

	void FRivermaxInputStream::UpdateFrameTracking(const FRTPHeader& NewRTPHeader)
	{
		FrameDescriptionTracking.EvaluateNewRTP(NewRTPHeader);
		
		if (!Options.bEnforceVideoFormat)
		{
			if (FrameDescriptionTracking.DetectedResolution.X > 0 && FrameDescriptionTracking.DetectedResolution.Y > 0)
			{
				if (FrameDescriptionTracking.DetectedResolution != StreamResolution)
				{
					StreamResolution = FrameDescriptionTracking.DetectedResolution;

					FRivermaxInputVideoFormatChangedInfo FormatChangeInfo;
					FormatChangeInfo.Width = StreamResolution.X;
					FormatChangeInfo.Height = StreamResolution.Y;
					FormatChangeInfo.PixelFormat = FrameDescriptionTracking.ExpectedSamplingType;
					Listener->OnVideoFormatChanged(FormatChangeInfo);
				}
			}
		}
	}

	bool FRivermaxInputStream::InitializeNetworkSettings(FNetworkSettings& OutSettings, FString& OutErrorMessage)
	{
		//Retrieves all network data from string version of device interface and stream address

		memset(&OutSettings.DeviceInterface, 0, sizeof(OutSettings.DeviceInterface));
		if (inet_pton(AF_INET, StringCast<ANSICHAR>(*Options.InterfaceAddress).Get(), &OutSettings.DeviceInterface.sin_addr) != 1)
		{
			OutErrorMessage = FString::Printf(TEXT("Failed to convert Device interface '%s' to network address"), *Options.InterfaceAddress);
			return false;
		}

		memset(&OutSettings.DestinationAddress, 0, sizeof(OutSettings.DestinationAddress));
		FMemory::Memset(&OutSettings.DestinationAddress, 0, sizeof(OutSettings.DestinationAddress));
		if (inet_pton(AF_INET, StringCast<ANSICHAR>(*Options.StreamAddress).Get(), &OutSettings.DestinationAddress.sin_addr) != 1)
		{
			OutErrorMessage = FString::Printf(TEXT("Failed to convert Stream address '%s' to network address"), *Options.StreamAddress);
			return false;
		}

		OutSettings.DeviceInterface.sin_family = AF_INET;
		OutSettings.DestinationAddress.sin_family = AF_INET;
		OutSettings.DestinationAddress.sin_port = ByteSwap((uint16)Options.Port);

		OutSettings.DeviceAddress.family = AF_INET;
		OutSettings.DeviceAddress.addr.ipv4 = OutSettings.DeviceInterface.sin_addr;
		rmx_status Status = CachedAPI->rmx_retrieve_device_iface(&OutSettings.RMXDeviceInterface, &OutSettings.DeviceAddress);
		if(Status != RMX_OK)
		{
			OutErrorMessage = FString::Printf(TEXT("Could not retrieve Rivermax interface for IP '%s'"), *Options.InterfaceAddress);
			return false;
		}
		
		CachedAPI->rmx_input_set_stream_nic_address(&StreamParameters, reinterpret_cast<sockaddr*>(&OutSettings.DeviceInterface));

		return true;
	}

	bool FRivermaxInputStream::InitializeStreamParameters(FString& OutErrorMessage)
	{
		size_t BufferElement = 1 << 18;
		CachedAPI->rmx_input_set_mem_capacity_in_packets(&StreamParameters, BufferElement);

		// We always split header and data so we always have two sub blocks (Header + Payload)
		constexpr size_t SubBlockCount = 2;
		CachedAPI->rmx_input_set_mem_sub_block_count(&StreamParameters, SubBlockCount);
		CachedAPI->rmx_input_set_entry_size_range(&StreamParameters, BufferConfiguration.HeaderBlockID, BufferConfiguration.HeaderExpectedSize, BufferConfiguration.HeaderExpectedSize);
		CachedAPI->rmx_input_set_entry_size_range(&StreamParameters, BufferConfiguration.DataBlockID, ExpectedPayloadSize, ExpectedPayloadSize);

		constexpr rmx_input_option InputOptions = RMX_INPUT_STREAM_CREATE_INFO_PER_PACKET;
		constexpr rmx_input_timestamp_format TimestampFormat = RMX_INPUT_TIMESTAMP_RAW_NANO;
		CachedAPI->rmx_input_enable_stream_option(&StreamParameters, InputOptions);
		CachedAPI->rmx_input_set_timestamp_format(&StreamParameters, TimestampFormat);

		if (bIsDynamicHeaderEnabled)
		{
			CachedAPI->rmx_input_enable_stream_option(&StreamParameters, RMX_INPUT_STREAM_RTP_SMPTE_2110_20_DYNAMIC_HDS);
		}

		const rmx_status Status = CachedAPI->rmx_input_determine_mem_layout(&StreamParameters);
		if (Status != RMX_OK)
		{
			OutErrorMessage = FString::Printf(TEXT("Could not determine memory layout for input stream using IP %d. Status: %d."), *Options.InterfaceAddress, Status);
			return false;
		}

		BufferElement = CachedAPI->rmx_input_get_mem_capacity_in_packets(&StreamParameters);
		BufferConfiguration.DataMemory = CachedAPI->rmx_input_get_mem_block_buffer(&StreamParameters, BufferConfiguration.DataBlockID);
		BufferConfiguration.HeaderMemory = CachedAPI->rmx_input_get_mem_block_buffer(&StreamParameters, BufferConfiguration.HeaderBlockID);

		if (BufferConfiguration.HeaderMemory->length <= 0)
		{
			OutErrorMessage = FString::Printf(TEXT("Header data split not supported for device with IP %d. Can't initialize stream."), *Options.InterfaceAddress);
			return false;
		}
		
		BufferConfiguration.PayloadSize = BufferConfiguration.DataMemory->length;
		BufferConfiguration.HeaderSize = BufferConfiguration.HeaderMemory->length;

		AllocateBuffers();

		return true;
	}

	bool FRivermaxInputStream::FinalizeStreamCreation(const FNetworkSettings& NetworkSettings, FString& OutErrorMessage)
	{
		rmx_status Status = CachedAPI->rmx_input_create_stream(&StreamParameters, &StreamId);
		if (Status != RMX_OK)
		{
			OutErrorMessage = FString::Printf(TEXT("Could not create stream. Status: %d."), Status);
			return false;
		}

		// Once stream creation is done, retrieve strides it is using
		BufferConfiguration.DataStride = CachedAPI->rmx_input_get_stride_size(&StreamParameters, BufferConfiguration.DataBlockID);
		BufferConfiguration.HeaderStride = CachedAPI->rmx_input_get_stride_size(&StreamParameters, BufferConfiguration.HeaderBlockID);

		// Configure how we wnat to consume chunks
		constexpr size_t MinChunkSize = 0;
		constexpr size_t MaxChunkSize = 5000;
		constexpr int Timeout = 0;
		Status = CachedAPI->rmx_input_set_completion_moderation(StreamId, MinChunkSize, MaxChunkSize, Timeout);
		if (Status != RMX_OK)
		{
			OutErrorMessage = FString::Printf(TEXT("Could not setup expected packet count for stream. Status: %d."), Status);
			return false;
		}

		// Finalize stream creation
		CachedAPI->rmx_input_init_chunk_handle(&BufferConfiguration.ChunkHandle, StreamId);
		CachedAPI->rmx_input_init_flow(&FlowAttribute);
		CachedAPI->rmx_input_set_flow_local_addr(&FlowAttribute, reinterpret_cast<const sockaddr*>(&NetworkSettings.DestinationAddress));

		sockaddr_in SourceAddr;
		memset(&SourceAddr, 0, sizeof(SourceAddr));
		SourceAddr.sin_family = AF_INET;
		CachedAPI->rmx_input_set_flow_remote_addr(&FlowAttribute, reinterpret_cast<const sockaddr*>(&SourceAddr));
		CachedAPI->rmx_input_set_flow_tag(&FlowAttribute, FlowTag);

		Status = CachedAPI->rmx_input_attach_flow(StreamId, &FlowAttribute);
		if(Status != RMX_OK)
		{
			OutErrorMessage = FString::Printf(TEXT("Could not attach flow to stream. Status: %d."), Status);

			// Cleanup stream.
			Status = CachedAPI->rmx_input_destroy_stream(StreamId);

			if (Status != RMX_OK)
			{
				UE_LOG(LogRivermax, Warning, TEXT("Failed to destroy input stream %d correctly. Status: %d"), StreamId, Status);
			}

			return false;
		}
		
		return true;
	}
}

	
	


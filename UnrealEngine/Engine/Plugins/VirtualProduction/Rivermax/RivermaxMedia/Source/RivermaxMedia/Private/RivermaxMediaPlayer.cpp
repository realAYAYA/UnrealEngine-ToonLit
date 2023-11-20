// Copyright Epic Games, Inc. All Rights Reserved.

#include "RivermaxMediaPlayer.h"

#include "IMediaEventSink.h"
#include "IRivermaxCoreModule.h"
#include "IRivermaxManager.h"
#include "MediaIOCoreEncodeTime.h"
#include "MediaIOCoreFileWriter.h"
#include "MediaIOCoreSamples.h"
#include "Misc/ScopeLock.h"
#include "RenderCommandFence.h"
#include "RenderGraphUtils.h"
#include "RivermaxMediaLog.h"
#include "RivermaxMediaOutput.h"
#include "RivermaxMediaSourceOptions.h"
#include "RivermaxMediaTextureSample.h"
#include "RivermaxMediaUtils.h"
#include "RivermaxPTPUtils.h"
#include "RivermaxTracingUtils.h"
#include "RivermaxTypes.h"
#include "Stats/Stats2.h"
#include "Tasks/Task.h"

#if WITH_EDITOR
#include "EngineAnalytics.h"
#endif


#define LOCTEXT_NAMESPACE "FRivermaxMediaPlayer"

DECLARE_CYCLE_STAT(TEXT("Rivermax MediaPlayer Request frame"), STAT_Rivermax_MediaPlayer_RequestFrame, STATGROUP_Media);
DECLARE_CYCLE_STAT(TEXT("Rivermax MediaPlayer Process frame"), STAT_Rivermax_MediaPlayer_ProcessFrame, STATGROUP_Media);

DECLARE_GPU_STAT_NAMED(RivermaxMedia_SampleUsageFence, TEXT("RivermaxMedia_SampleUsageFence"));
DECLARE_GPU_STAT_NAMED(Rmax_WaitForPixels, TEXT("Rmax_WaitForPixels"));


namespace UE::RivermaxMedia
{
	static TAutoConsoleVariable<int32> CVarRivermaxForcedFramelockLatency(
		TEXT("Rivermax.Player.Latency"),
		-1,
		TEXT("Override latency in framelock mode. 0 for 0 frame of latency and 1 for 1 frame of latency."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRivermaxSampleUploadMode(
		TEXT("Rivermax.Player.UploadMode"),
		1,
		TEXT("Mode 0: Upload is done on the render thread.\n"
			"Mode 1: Upload is done in its own thread before being rendered."),
		ECVF_Default);

	/* FRivermaxVideoPlayer structors
	 *****************************************************************************/

	FRivermaxMediaPlayer::FRivermaxMediaPlayer(IMediaEventSink& InEventSink)
		: Super(InEventSink)
		, MaxNumVideoFrameBuffer(5)
		, RivermaxThreadNewState(EMediaState::Closed)
		, bIsSRGBInput(false)
		, bUseVideo(false)
		, SupportedSampleTypes(EMediaIOSampleType::None)
		, bPauseRequested(false)
		, MediaSamples(MakeUnique<FRivermaxMediaTextureSamples>())
	{
	}

	FRivermaxMediaPlayer::~FRivermaxMediaPlayer()
	{
		Close();
	}

	/* IMediaPlayer interface
	 *****************************************************************************/

	 /**
	  * @EventName MediaFramework.RivermaxSourceOpened
	  * @Trigger Triggered when an Rivermax media source is opened through a media player.
	  * @Type Client
	  * @Owner MediaIO Team
	  */
	bool FRivermaxMediaPlayer::Open(const FString& Url, const IMediaOptions* Options)
	{
		if (!Super::Open(Url, Options))
		{
			RivermaxThreadNewState = EMediaState::Error;
			return false;
		}

		PlayerMode = (ERivermaxPlayerMode)Options->GetMediaOption(RivermaxMediaOption::PlayerMode, (int64)ERivermaxPlayerMode::Latest);
		FrameTracking = {};

		//Video related options
		{
			bIsSRGBInput = Options->GetMediaOption(RivermaxMediaOption::SRGBInput, bIsSRGBInput);
			DesiredPixelFormat = (ERivermaxMediaSourcePixelFormat)Options->GetMediaOption(RivermaxMediaOption::PixelFormat, (int64)ERivermaxMediaSourcePixelFormat::RGB_8bit);
			const bool bUseZeroLatency = Options->GetMediaOption(RivermaxMediaOption::ZeroLatency, true);
			FrameLatency = bUseZeroLatency ? 0 : 1;
			const bool bOverrideResolution = Options->GetMediaOption(RivermaxMediaOption::OverrideResolution, false);
			bFollowsStreamResolution = !bOverrideResolution;
		}

		bUseVideo = true;

		IRivermaxCoreModule* Module = FModuleManager::GetModulePtr<IRivermaxCoreModule>("RivermaxCore");
		if (Module && ConfigureStream(Options))
		{
			InputStream = Module->CreateInputStream();
		}

		// If we are not following the stream resolution, make it the video track format and then reset to go through a format change once
		if (!bFollowsStreamResolution)
		{
			StreamResolution = StreamOptions.EnforcedResolution;
		}
		VideoTrackFormat.Dim = FIntPoint::ZeroValue;

		FrameTracking.bWasFrameRequested = false;
		FrameTracking.LastFrameRendered.Reset();
		CurrentState = EMediaState::Preparing;
		RivermaxThreadNewState = EMediaState::Preparing;
		
		if (InputStream == nullptr || !InputStream->Initialize(StreamOptions, *this))
		{
			UE_LOG(LogRivermaxMedia, Warning, TEXT("Failed to initialize Rivermax input stream."));
			RivermaxThreadNewState = EMediaState::Error;
			InputStream.Reset();
			return false;
		}

		// Setup our different supported channels based on source settings
		SetupSampleChannels();

		EventSink.ReceiveMediaEvent(EMediaEvent::MediaConnecting);

#if WITH_EDITOR
		if (FEngineAnalytics::IsAvailable())
		{
			TArray<FAnalyticsEventAttribute> EventAttributes;

			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("ResolutionWidth"), FString::Printf(TEXT("%d"), VideoTrackFormat.Dim.X)));
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("ResolutionHeight"), FString::Printf(TEXT("%d"), VideoTrackFormat.Dim.Y)));
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("FrameRate"), *VideoFrameRate.ToPrettyText().ToString()));

			FEngineAnalytics::GetProvider().RecordEvent(TEXT("MediaFramework.RivermaxSourceOpened"), EventAttributes);
		}
#endif

		return true;
	}

	void FRivermaxMediaPlayer::Close()
	{
		RivermaxThreadNewState = EMediaState::Closed;

		WaitForPendingTasks();

		if (InputStream)
		{
			InputStream->Uninitialize(); // this may block, until the completion of a callback from IRivermaxChannelCallbackInterface
			InputStream.Reset();
		}

		for(const TSharedPtr<FRivermaxSampleWrapper>& Sample : SamplePool)
		{
			Sample->Sample.Reset();
			Sample->SampleConversionFence.SafeRelease();
		}
		SamplePool.Empty();

		RivermaxThreadCurrentTextureSample.Reset();
		SkippedFrames.Empty();

		Super::Close();
	}

	FGuid FRivermaxMediaPlayer::GetPlayerPluginGUID() const
	{
		static FGuid PlayerPluginGUID(0xF537595A, 0x8E8D452B, 0xB8C05707, 0x6B334234);
		return PlayerPluginGUID;
	}

	FString FRivermaxMediaPlayer::GetStats() const
	{
		FString Stats;

		//todo

		return Stats;
	}

#if WITH_EDITOR
	const FSlateBrush* FRivermaxMediaPlayer::GetDisplayIcon() const
	{
		//todo for tdm
		return nullptr;
		//return IRivermaxMediaModule::Get().GetStyle()->GetBrush("RivermaxMediaIcon");
	}
#endif //WITH_EDITOR

	bool FRivermaxMediaPlayer::OnVideoFrameRequested(const FRivermaxInputVideoFrameDescriptor& FrameInfo, FRivermaxInputVideoFrameRequest& OutVideoFrameRequest)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FRivermaxMediaPlayer::OnVideoFrameRequested)

		// If video is not playing, no need to provide samples when requested
		if (!IsReadyToPlay())
		{
			return false;
		}

		// Track first frame number that was requested in order to avoid waiting for a frame that will never come
		if (FrameTracking.bWasFrameRequested == false)
		{
			FrameTracking.bWasFrameRequested = true;
			FrameTracking.FirstFrameRequested = FrameInfo.FrameNumber;
		}
		else
		{
			if(FrameInfo.FrameNumber > (FrameTracking.LastFrameNumberRequested + 1))
			{
				UE_LOG(LogRivermaxMedia, Warning, TEXT("Frames were skipped. Last frame requested was '%d'. Current frame being requested is '%d'."), FrameTracking.LastFrameNumberRequested, FrameInfo.FrameNumber);
				
				// Skipped frames are only used in framelock mode to avoid stalling
				if (PlayerMode == ERivermaxPlayerMode::Framelock)
				{
					FScopeLock Lock(&SkippedFrameCriticalSection);
					
					const uint32 MinInterval = FrameTracking.LastFrameNumberRequested + 1;
					const uint32 MaxInterval = FrameInfo.FrameNumber - 1;
					if (MinInterval <= MaxInterval)
					{
						const TInterval<uint32> Interval(MinInterval, MaxInterval);
						SkippedFrames.Add(Interval);
					}
					else
					{
						//Wrap around case
						const TInterval<uint32> FirstInterval(MinInterval, ~0);
						const TInterval<uint32> SecondInterval(0, MaxInterval);
						SkippedFrames.Add(FirstInterval);
						SkippedFrames.Add(SecondInterval);
					}
				}
			}
		}

		// Keep track of frames that are requested to detect gaps
		FrameTracking.LastFrameNumberRequested = FrameInfo.FrameNumber;

		if (FrameInfo.VideoBufferSize > 0)
		{
			uint32 NextRequestIndex = 0;
			if(GetFrameRequestedIndex(FrameInfo, NextRequestIndex))
			{
				UE_LOG(LogRivermaxMedia, Verbose, TEXT("Starting to receive frame '%u' with timestamp %u at location %d"), FrameInfo.FrameNumber, FrameInfo.Timestamp, NextRequestIndex);
				SamplePool[NextRequestIndex]->ReceptionState = ESampleReceptionState::Receiving;
				SamplePool[NextRequestIndex]->FrameNumber = FrameInfo.FrameNumber;
				SamplePool[NextRequestIndex]->Timestamp = FrameInfo.Timestamp;
				RivermaxThreadCurrentTextureSample = SamplePool[NextRequestIndex];
			
				if (bDoesStreamSupportsGPUDirect)
				{
					// Hand out location where to copy received data on gpu
					OutVideoFrameRequest.GPUBuffer = RivermaxThreadCurrentTextureSample->Sample->GetGPUBuffer()->GetRHI();
				}
				else
				{
					OutVideoFrameRequest.VideoBuffer = reinterpret_cast<uint8*>(RivermaxThreadCurrentTextureSample->Sample->RequestBuffer(FrameInfo.VideoBufferSize));
				}
				return true;
			}

			UE_LOG(LogRivermaxMedia, Verbose, TEXT("Failed to provide a frame for incoming frame %u with timestamp %u"), FrameInfo.FrameNumber, FrameInfo.Timestamp);
		}

		return false;
	}


	void FRivermaxMediaPlayer::OnVideoFrameReceived(const FRivermaxInputVideoFrameDescriptor& FrameInfo, const FRivermaxInputVideoFrameReception& ReceivedVideoFrame)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FRivermaxMediaPlayer::OnVideoFrameReceived)


		if (!IsReadyToPlay())
		{
			return;
		}

		if (bUseVideo && ReceivedVideoFrame.VideoBuffer)
		{
			if (RivermaxThreadCurrentTextureSample.IsValid())
			{
				if(RivermaxThreadCurrentTextureSample->ReceptionState == ESampleReceptionState::Receiving)
				{
					RivermaxThreadCurrentTextureSample->ReceptionState = ESampleReceptionState::Received;
					RivermaxThreadCurrentTextureSample->bIsReadyToRender = bDoesStreamSupportsGPUDirect;
				}
				else
				{
					UE_LOG(LogRivermaxMedia, Verbose, TEXT("Discarding received frame %u since it was deemed unusable."), FrameInfo.FrameNumber);
				}
			}
		}
		
		RivermaxThreadCurrentTextureSample.Reset();
	}

	void FRivermaxMediaPlayer::TickFetch(FTimespan DeltaTime, FTimespan Timecode)
	{
		Super::TickFetch(DeltaTime, Timecode);
		if (InputStream && CurrentState == EMediaState::Playing)
		{
			ProcessFrame();
		}
	}

	void FRivermaxMediaPlayer::TickInput(FTimespan DeltaTime, FTimespan Timecode)
	{
		// update player state
		EMediaState NewState = RivermaxThreadNewState;

		if (NewState != CurrentState)
		{
			CurrentState = NewState;
			if (CurrentState == EMediaState::Playing)
			{
				EventSink.ReceiveMediaEvent(EMediaEvent::TracksChanged);
				EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpened);
				EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackResumed);
			}
			else if (NewState == EMediaState::Error)
			{
				EventSink.ReceiveMediaEvent(EMediaEvent::MediaOpenFailed);
				Close();
			}
		}

		if (CurrentState != EMediaState::Playing)
		{
			return;
		}
		
		// Cache current stream detection, it could change while we are applying it
		FIntPoint CachedStreamResolution;
		{
			FScopeLock Lock(&StreamResolutionCriticalSection);
			CachedStreamResolution = StreamResolution;
		}

		if (VideoTrackFormat.Dim != CachedStreamResolution)
		{
			UE_LOG(LogRivermaxMedia, Log, TEXT("Player needs to apply newly detected stream resolution : %dx%d"), CachedStreamResolution.X, CachedStreamResolution.Y);
			
			// Reset some frame tracking info while changing resolution
			FrameTracking.bWasFrameRequested = false;
			FrameTracking.LastFrameRendered.Reset();

			{
				WaitForPendingTasks();

				// Cleanup allocated ressources for the current resolution
				for (const TSharedPtr<FRivermaxSampleWrapper>& Sample : SamplePool)
				{
					Sample->Sample.Reset();
					Sample->SampleConversionFence.SafeRelease();
				}
				SamplePool.Empty();

				RivermaxThreadCurrentTextureSample.Reset();
				SkippedFrames.Empty();

				AllocateBuffers(CachedStreamResolution);
				
				VideoTrackFormat.Dim = CachedStreamResolution;
			}

		}

		TickTimeManagement();
	}


	/* FRivermaxMediaPlayer implementation
	 *****************************************************************************/
	void FRivermaxMediaPlayer::ProcessFrame()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(RivermaxPlayerProcessFrame);

		// Don't start making frame available until one is being received
		if (FrameTracking.bWasFrameRequested == false)
		{
			return;
		}

		// Create a new media sample acting as a dummy container to be picked by MFW which we will fill during the late update
		FSampleConfigurationArgs Args;
		Args.bInIsSRGBInput = bIsSRGBInput;
		Args.FrameRate = VideoFrameRate;
		Args.Width = VideoTrackFormat.Dim.X;
		Args.Height = VideoTrackFormat.Dim.Y;
		Args.Player = StaticCastSharedPtr<FRivermaxMediaPlayer>(AsShared().ToSharedPtr());
		Args.SampleFormat = DesiredPixelFormat;
		Args.Time = FTimespan(GFrameCounter);

		TSharedPtr<FRivermaxMediaTextureSample> EmptySample = MakeShared<FRivermaxMediaTextureSample>();
		EmptySample->ConfigureSample(Args);
		EmptySample->SetBuffer(CommonGPUBuffer);

		// Update the current sample to be picked by MFW
		MediaSamples->CurrentSample = EmptySample;
	}

	bool FRivermaxMediaPlayer::IsHardwareReady() const
	{
		return (RivermaxThreadNewState == EMediaState::Playing) || (RivermaxThreadNewState == EMediaState::Paused);
	}

	void FRivermaxMediaPlayer::SetupSampleChannels()
	{
		FMediaIOSamplingSettings VideoSettings = BaseSettings;
		VideoSettings.BufferSize = MaxNumVideoFrameBuffer;
		Samples->InitializeVideoBuffer(VideoSettings);
	}

	bool FRivermaxMediaPlayer::SetRate(float Rate)
	{
		if (FMath::IsNearlyEqual(Rate, 1.0f))
		{
			bPauseRequested = false;
			return true;
		}

		if (FMath::IsNearlyEqual(Rate, 0.0f))
		{
			bPauseRequested = true;
			return true;
		}

		return false;
	}

	void FRivermaxMediaPlayer::OnInitializationCompleted(const FRivermaxInputInitializationResult& Result)
	{
		RivermaxThreadNewState = Result.bHasSucceed ? EMediaState::Playing : EMediaState::Error;
		bDoesStreamSupportsGPUDirect = Result.bIsGPUDirectSupported;
	}

	bool FRivermaxMediaPlayer::ConfigureStream(const IMediaOptions* Options)
	{
		using namespace UE::RivermaxCore;

		// Resolve interface address
		IRivermaxCoreModule* Module = FModuleManager::GetModulePtr<IRivermaxCoreModule>("RivermaxCore");
		if (Module == nullptr)
		{
			return false;
		}

		const FString DesiredInterface = Options->GetMediaOption(RivermaxMediaOption::InterfaceAddress, FString());
		const bool bFoundDevice = Module->GetRivermaxManager()->GetMatchingDevice(DesiredInterface, StreamOptions.InterfaceAddress);
		if (bFoundDevice == false)
		{
			UE_LOG(LogRivermaxMedia, Error, TEXT("Could not find a matching interface for IP '%s'"), *DesiredInterface);
			return false;
		}

		StreamOptions.StreamAddress = Options->GetMediaOption(RivermaxMediaOption::StreamAddress, FString());
		StreamOptions.Port = Options->GetMediaOption(RivermaxMediaOption::Port, (int64)0);
		StreamOptions.bUseGPUDirect = Options->GetMediaOption(RivermaxMediaOption::UseGPUDirect, false);
		StreamOptions.FrameRate = VideoFrameRate;
		StreamOptions.PixelFormat = UE::RivermaxMediaUtils::Private::MediaSourcePixelFormatToRivermaxSamplingType(DesiredPixelFormat);
		const FVideoFormatInfo FormatInfo = FStandardVideoFormat::GetVideoFormatInfo(StreamOptions.PixelFormat);
		const uint32 PixelAlignment = FormatInfo.PixelGroupCoverage;
		const uint32 AlignedHorizontalResolution = (VideoTrackFormat.Dim.X % PixelAlignment) ? VideoTrackFormat.Dim.X + (PixelAlignment - (VideoTrackFormat.Dim.X % PixelAlignment)) : VideoTrackFormat.Dim.X;
		StreamOptions.EnforcedResolution = FIntPoint(AlignedHorizontalResolution, VideoTrackFormat.Dim.Y);
		StreamOptions.bEnforceVideoFormat = !bFollowsStreamResolution;

		return true;
	}

	void FRivermaxMediaPlayer::AllocateBuffers(const FIntPoint& InResolution)
	{
		using namespace UE::RivermaxCore;
		using namespace UE::RivermaxMediaUtils::Private;

		// Take care of the common buffers first
		{
			// Create the common texture we are going to use
			const FSourceBufferDesc BufferDescription = GetBufferDescription(InResolution, DesiredPixelFormat);
			FRDGBufferDesc RDGBufferDesc = FRDGBufferDesc::CreateStructuredDesc(BufferDescription.BytesPerElement, BufferDescription.NumberOfElements);

			// Required to share resource across different graphics API (DX, Cuda)
			RDGBufferDesc.Usage |= EBufferUsageFlags::Shared;

			TWeakPtr<FRivermaxMediaPlayer> WeakPlayer = StaticCastSharedRef<FRivermaxMediaPlayer>(AsShared());
			ENQUEUE_RENDER_COMMAND(RivermaxPlayerBufferCreation)(
				[WeakPlayer, RDGBufferDesc](FRHICommandListImmediate& CommandList)
				{
					if (TSharedPtr<FRivermaxMediaPlayer> Player = WeakPlayer.Pin())
					{
						Player->CommonGPUBuffer = AllocatePooledBuffer(RDGBufferDesc, TEXT("RmaxInput Buffer"));
					}
				});
		}

		SamplePool.Empty();

		// Allocate our pool of samples where incoming ones will be written and chosen from
		for (int32 Index = 0; Index < MaxNumVideoFrameBuffer; Index++)
		{
			TSharedPtr<FRivermaxSampleWrapper> NewWrapper = MakeShared<FRivermaxSampleWrapper>();
			NewWrapper->Sample = MakeShared<FRivermaxMediaTextureSample>();
			NewWrapper->Sample->InitializeGPUBuffer(InResolution, DesiredPixelFormat);
			NewWrapper->SampleConversionFence = RHICreateGPUFence(*FString::Printf(TEXT("RmaxConversionDoneFence_%02d"), Index));
			NewWrapper->ReceptionState = ESampleReceptionState::Available;
			SamplePool.Add(MoveTemp(NewWrapper));
		}

		// Allocation is done on render thread so let's make sure it's completed before pursuing
		FRenderCommandFence RenderFence;
		RenderFence.BeginFence();
		RenderFence.Wait();		
	}

	void FRivermaxMediaPlayer::OnStreamError()
	{
		// If the stream ends up in error, stop the player
		UE_LOG(LogRivermaxMedia, Error, TEXT("Stream caught an error. Player will stop."));
		RivermaxThreadNewState = EMediaState::Error;
	}

	bool FRivermaxMediaPlayer::LateUpdateSetupSample(FSampleConverterOperationSetup& OutConverterSetup)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(RivermaxPlayerLateUpdate);

		// We only allow this function to run once per frame.
		if (LastFrameNumberThatUpdatedJustInTime == GFrameCounterRenderThread)
		{
			UE_LOG(LogRivermaxMedia, Verbose, TEXT("LateUpdate called more than once in GFrameCounterRenderThread %llu"), GFrameCounterRenderThread);
			return false;
		}
		LastFrameNumberThatUpdatedJustInTime = GFrameCounterRenderThread;

		FFrameExpectation NextFrameExpectations;
		const bool bShouldRender = GetNextExpectedFrameInfo(NextFrameExpectations);

		if (!bShouldRender)
		{
			UE_LOG(LogRivermaxMedia, VeryVerbose, TEXT("Skipping render for frame %llu."), GFrameCounterRenderThread);
			return false;
		}

		if (NextFrameExpectations.FrameNumber < FrameTracking.FirstFrameRequested)
		{
			UE_LOG(LogRivermaxMedia, VeryVerbose, TEXT("Skipping render for frame number %llu. Expecting frame %u but first frame received is greater, %u."), GFrameCounterRenderThread, NextFrameExpectations.FrameNumber, FrameTracking.FirstFrameRequested);
			return false;
		}

		// Always clean up frames from the past. They will never be used
		for (const TSharedPtr<FRivermaxSampleWrapper>& Frame : SamplePool)
		{
			if (Frame->ReceptionState != ESampleReceptionState::Available)
			{
				if (Frame->FrameNumber < NextFrameExpectations.FrameNumber)
				{
					Frame->ReceptionState = ESampleReceptionState::Available;

					const FString LastRender = FrameTracking.LastFrameRendered.IsSet() ? FString::Printf(TEXT("%u"), FrameTracking.LastFrameRendered.GetValue()) : FString(TEXT("None"));
					UE_LOG(LogRivermaxMedia, Verbose, TEXT("Making frame %u as available since it will never be processed. Last render = %s, next render = %u"), Frame->FrameNumber, *LastRender, NextFrameExpectations.FrameNumber);
				}
			}
		}
		
		FrameTracking.LastFrameRendered = NextFrameExpectations.FrameNumber;

		// Verify if the frame we will use for rendering is still being rendered for the previous one.
		if (SamplePool[NextFrameExpectations.FrameIndex]->bIsPendingRendering)
		{
			UE_LOG(LogRivermaxMedia, Verbose, TEXT("Frame %u was still rendering when we expected to reuse its location. Waiting for it to complete."), NextFrameExpectations.FrameIndex);

			TRACE_CPUPROFILER_EVENT_SCOPE(RmaxMediaWaitForFrameToBeAvailable);

			// Frame should be rendered at some point so a timeout isn't required but we add one as a last resort to avoid lockups.
			constexpr double TimeoutSeconds = 2.0;
			const double StartTimeSeconds = FPlatformTime::Seconds();
			while (SamplePool[NextFrameExpectations.FrameIndex]->bIsPendingRendering)
			{
				FPlatformProcess::SleepNoStats(SleepTimeSeconds);

				if ((FPlatformTime::Seconds() - StartTimeSeconds) > TimeoutSeconds)
				{
					break;
				}
			}
		}

		// Mark this sample as pending since it's now going on the rendering path
		SamplePool[NextFrameExpectations.FrameIndex]->bIsPendingRendering = true;

		{
			const int32 SampleUploadMode = CVarRivermaxSampleUploadMode.GetValueOnRenderThread();
			if (SampleUploadMode == 0)
			{
				SampleUploadSetupRenderThreadMode(NextFrameExpectations, OutConverterSetup);
				
			}
			else 
			{
				SampleUploadSetupTaskThreadMode(NextFrameExpectations, OutConverterSetup);
			}

			// Setup post sample usage pass 
			OutConverterSetup.PostConvertFunc = [NextFrameExpectations, this] (FRDGBuilder& GraphBuilder)
			{
				PostSampleUsage(GraphBuilder, NextFrameExpectations);
			};
		}

		return true;
	}

	bool FRivermaxMediaPlayer::GetNextExpectedFrameInfo(FFrameExpectation& OutExpectation)
	{
		switch (PlayerMode)
		{
			case ERivermaxPlayerMode::Framelock:
			{
				return GetNextExpectedFrameInfoForFramelock(OutExpectation);
			}
			case ERivermaxPlayerMode::Latest:
			{
				return GetNextExpectedFrameInfoForLatest(OutExpectation);
			}
			default:
			{
				checkNoEntry()
			}
		}

		return false;
	}

	bool FRivermaxMediaPlayer::GetNextExpectedFrameInfoForFramelock(FFrameExpectation& OutExpectation)
	{
		uint32 CurrentLatency = FrameLatency;
		const uint32 ForcedLatency = CVarRivermaxForcedFramelockLatency.GetValueOnRenderThread();
		if (ForcedLatency == 0 || ForcedLatency == 1)
		{
			CurrentLatency = ForcedLatency;
		}

		OutExpectation.FrameNumber = GFrameCounterRenderThread - CurrentLatency;
		OutExpectation.FrameIndex = OutExpectation.FrameNumber % MaxNumVideoFrameBuffer;
		return true;
	}

	bool FRivermaxMediaPlayer::GetNextExpectedFrameInfoForLatest(FFrameExpectation& OutExpectation)
	{
		{
			// Look for the latest frame already arrived
			int32 HighestTimestampIndex = INDEX_NONE;
			uint32 HighestTimestamp = 0;

			int32 LowestTimestampIndex = INDEX_NONE;
			uint32 LowestTimestamp = ~0;

			// Use timestamps to pick the latest one. 
			// If we have our frame rate mismatching 
			for (int32 Index = 0; Index < SamplePool.Num(); ++Index)
			{
				const TSharedPtr<FRivermaxSampleWrapper>& Frame = SamplePool[Index];
				if (Frame->ReceptionState == ESampleReceptionState::Received)
				{
					if (HighestTimestamp < Frame->Timestamp)
					{
						HighestTimestamp = Frame->Timestamp;
						HighestTimestampIndex = Index;
					}

					if (LowestTimestamp > Frame->Timestamp)
					{
						LowestTimestamp = Frame->Timestamp;
						LowestTimestampIndex = Index;
					}
				}
			}

			// Early exit if no frame was found
			if (HighestTimestampIndex < 0)
			{
				return false;
			}

			check(LowestTimestampIndex >= 0);

			// No point in rendering the same frame as last time
			if (FrameTracking.LastFrameExpectation.FrameNumber == SamplePool[HighestTimestampIndex]->FrameNumber)
			{
				return false;
			}
			else if (FrameTracking.LastFrameExpectation.FrameNumber > SamplePool[HighestTimestampIndex]->FrameNumber)
			{
				// We have picked in the past a frame number higher than the highest in the buffers.
				// This is unexpected, unless frame number has wrapped around.
				UE_LOG(LogRivermaxMedia, Warning, TEXT("In latest mode, the sender's frame count seems to have reset from %u to %u."), FrameTracking.LastFrameExpectation.FrameNumber, SamplePool[HighestTimestampIndex]->FrameNumber);
			}

			// Fill what we are expecting to use to render
			OutExpectation.FrameNumber = SamplePool[HighestTimestampIndex]->FrameNumber;
			OutExpectation.FrameIndex = HighestTimestampIndex;

			// Make the ones skipped available for reception again
			TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FRivermaxTracingUtils::RmaxInSelectedFrameTraceEvents[OutExpectation.FrameNumber % 10]);

			for (const TSharedPtr<FRivermaxSampleWrapper>& Frame : SamplePool)
			{
				if (Frame == SamplePool[OutExpectation.FrameIndex])
				{
					continue;
				}

				if (Frame->ReceptionState == ESampleReceptionState::Received)
				{
					UE_LOG(LogRivermaxMedia, Verbose, TEXT("Skipping frame %u since a better one was chosen, %u"), Frame->FrameNumber, OutExpectation.FrameNumber);
					Frame->ReceptionState = ESampleReceptionState::Available;
				}
			}

			FrameTracking.LastFrameExpectation = OutExpectation;
		}

		return true;
	}

	bool FRivermaxMediaPlayer::GetFrameRequestedIndex(const FRivermaxInputVideoFrameDescriptor& FrameInfo, uint32& OutExpectedIndex)
	{
		switch (PlayerMode)
		{
		case ERivermaxPlayerMode::Framelock:
		{
			return GetFrameRequestedIndexForFramelock(FrameInfo, OutExpectedIndex);
		}
		case ERivermaxPlayerMode::Latest:
		{
			return GetFrameRequestedIndexForLatest(FrameInfo, OutExpectedIndex);
		}
		default:
		{
			checkNoEntry()
		}
		}

		return false;
	}

	bool FRivermaxMediaPlayer::GetFrameRequestedIndexForFramelock(const FRivermaxInputVideoFrameDescriptor& FrameInfo, uint32& OutExpectedIndex)
	{
		// We always store incoming frame number in its respective bucket
		OutExpectedIndex = FrameInfo.FrameNumber % MaxNumVideoFrameBuffer;

		TSharedPtr<FRivermaxSampleWrapper> ExpectedFrame = SamplePool[OutExpectedIndex];

		// If the frame is available, it hasn't been picked up for render yet or it's done rendering
		if (ExpectedFrame->ReceptionState == ESampleReceptionState::Available)
		{
			return true;
		}

		// If the frame is marked as received, it might not have been rendered yet 
		else if (ExpectedFrame->ReceptionState == ESampleReceptionState::Received)
		{
			// Odd case where an incoming frame has the same frame number as before. In this case, we start over reception
			if (ExpectedFrame->FrameNumber == FrameInfo.FrameNumber)
			{
				UE_LOG(LogRivermaxMedia, Warning, TEXT("Expected to receive frame while a matching frame number %u had not yet been rendered."), ExpectedFrame->FrameNumber);
			}
			else
			{
				// Otherwise, it means we have overran the render so we need to wait for it to become available
				UE_LOG(LogRivermaxMedia, Warning, TEXT("Expected to receive frame %u but location is busy with frame not rendered yet. Waiting"), ExpectedFrame->FrameNumber);
				
				const double StartTimeSeconds = FPlatformTime::Seconds();
				constexpr double TimeoutSeconds = 0.5;
				while (ExpectedFrame->ReceptionState != ESampleReceptionState::Available)
				{
					FPlatformProcess::SleepNoStats(SleepTimeSeconds);
					if ((FPlatformTime::Seconds() - StartTimeSeconds) > TimeoutSeconds)
					{
						UE_LOG(LogRivermaxMedia, Error, TEXT("Timed out waiting for frame %u to be rendered to receive frame %u into."), ExpectedFrame->FrameNumber, FrameInfo.FrameNumber);
					
						// Mark this frame as skipped since we're stealing it
						FScopeLock Lock(&SkippedFrameCriticalSection);
						const TInterval<uint32> Interval(ExpectedFrame->FrameNumber, ExpectedFrame->FrameNumber);
						SkippedFrames.Add(Interval);

						break;
					}
				}
			}
			
			return true;
		}

		return false;
	}

	bool FRivermaxMediaPlayer::GetFrameRequestedIndexForLatest(const FRivermaxInputVideoFrameDescriptor& FrameInfo, uint32& OutExpectedIndex)
	{
		//If we are free running, go forward with the write index. If we receive too fast, we'll skip some.
		const uint32 NextIndex = ((FrameTracking.LastFrameRequestedIndex + 1) % MaxNumVideoFrameBuffer);
		if (SamplePool[NextIndex]->ReceptionState == ESampleReceptionState::Available)
		{
			OutExpectedIndex = NextIndex;
			FrameTracking.LastFrameRequestedIndex = NextIndex;
			return true;
		}
		else
		{
			return false;
		}
	}

	IMediaSamples& FRivermaxMediaPlayer::GetSamples()
	{
		return *MediaSamples.Get();
	}

	void FRivermaxMediaPlayer::TickTimeManagement()
	{
		// When other means of alignment will be required, we will need to update this
		// For example, aligning streams using timecode won't work with this.
		CurrentTime = FTimespan(GFrameCounter);
	}

	bool FRivermaxMediaPlayer::GetPlayerFeatureFlag(EFeatureFlag flag) const
	{
		//switch (flag)
		//{
		//case EFeatureFlag::UsePlaybackTimingV2:
		//	return true;
		//default:
		//	break;
		//}

		return IMediaPlayer::GetPlayerFeatureFlag(flag);
	}

	void FRivermaxMediaPlayer::PostSampleUsage(FRDGBuilder& GraphBuilder, const FFrameExpectation& FrameExpectation)
	{
		GraphBuilder.AddPass(RDG_EVENT_NAME("RivermaxPostSampleUsage"),
			ERDGPassFlags::NeverCull, 
			[FrameExpectation, this](FRHICommandList& RHICmdList)
			{
				SCOPED_GPU_STAT(RHICmdList, RivermaxMedia_SampleUsageFence);
				SCOPED_DRAW_EVENT(RHICmdList, RivermaxMedia_SampleUsageFence);

				// Write a fence in the post sample usage pass to be able to know when we can reuse it
				RHICmdList.WriteGPUFence(SamplePool[FrameExpectation.FrameIndex]->SampleConversionFence);

				++TasksInFlight;

				UE::Tasks::Launch(UE_SOURCE_LOCATION,
					[FrameExpectation, this]()
					{
						ON_SCOPE_EXIT
						{
							--TasksInFlight;
						};

						TRACE_CPUPROFILER_EVENT_SCOPE(RmaxWaitForShader);
						do
						{
							const bool bHasValidFence = SamplePool[FrameExpectation.FrameIndex]->SampleConversionFence.IsValid();
							const bool bHasFenceCompleted = bHasValidFence ? SamplePool[FrameExpectation.FrameIndex]->SampleConversionFence->Poll() : false;
							if (bHasValidFence == false || bHasFenceCompleted)
							{
								break;
							}

							FPlatformProcess::SleepNoStats(SleepTimeSeconds);

						} while (true);


						UE_LOG(LogRivermaxMedia, Verbose, TEXT("Finished rendering frame %u with expectations being frame %u."), SamplePool[FrameExpectation.FrameIndex]->FrameNumber, FrameExpectation.FrameNumber);

						// We clear the fence and signal that it can be re-used.
						SamplePool[FrameExpectation.FrameIndex]->SampleConversionFence->Clear();
						SamplePool[FrameExpectation.FrameIndex]->ReceptionState = ESampleReceptionState::Available;
						SamplePool[FrameExpectation.FrameIndex]->bIsReadyToRender = false;
						SamplePool[FrameExpectation.FrameIndex]->bIsPendingRendering = false;
						SamplePool[FrameExpectation.FrameIndex]->LockedMemory = nullptr;
						TryClearSkippedInterval(FrameExpectation.FrameNumber);
					});
			});
	}

	void FRivermaxMediaPlayer::TryClearSkippedInterval(uint32 LastFrameRendered)
	{
		FScopeLock Lock(&SkippedFrameCriticalSection);

		for (auto Iter = SkippedFrames.CreateIterator(); Iter; ++Iter)
		{
			// If we just rendered a frame matching the max boundary of an interval, we are past it from now on
			const TInterval<uint32>& Interval = *Iter;
			if (LastFrameRendered == Interval.Max)
			{
				Iter.RemoveCurrent();
				break;
			}
		}
	}

	bool FRivermaxMediaPlayer::IsFrameSkipped(uint32 FrameNumber) const
	{
		FScopeLock Lock(&SkippedFrameCriticalSection);

		for (auto Iter = SkippedFrames.CreateConstIterator(); Iter; ++Iter)
		{
			// If we just rendered a frame matching the max boundary of an interval, we are passed it from now on
			const TInterval<uint32>& Interval = *Iter;
			if (Interval.Contains(FrameNumber))
			{
				return true;
			}
		}

		return false;
	}

	void FRivermaxMediaPlayer::OnVideoFrameReceptionError(const FRivermaxInputVideoFrameDescriptor& FrameInfo)
	{
		// In the case of an error, stamp back the frame in receiving state to available and mark it as skipped in case we are waiting on it
		for (const TSharedPtr<FRivermaxSampleWrapper>& Sample : SamplePool)
		{
			// There can only be one frame receiving at the time
			if (Sample->ReceptionState == ESampleReceptionState::Receiving)
			{
				UE_LOG(LogRivermaxMedia, Warning, TEXT("Error occured while receiving frame %u with timestamp %u."), Sample->FrameNumber, Sample->Timestamp);
				Sample->ReceptionState = ESampleReceptionState::Available;

				FScopeLock Lock(&SkippedFrameCriticalSection);
				const TInterval<uint32> Interval(Sample->FrameNumber, Sample->FrameNumber);
				SkippedFrames.Add(Interval);

				return;
			}
		}
	}

	void FRivermaxMediaPlayer::OnVideoFormatChanged(const FRivermaxInputVideoFormatChangedInfo& NewFormatInfo)
	{
		const ERivermaxMediaSourcePixelFormat NewFormat = UE::RivermaxMediaUtils::Private::RivermaxPixelFormatToMediaSourcePixelFormat(NewFormatInfo.PixelFormat);
		const FIntPoint NewResolution = { (int32)NewFormatInfo.Width, (int32)NewFormatInfo.Height };
		bool bNeedReinitializing = (NewFormatInfo.PixelFormat != StreamOptions.PixelFormat);
		bNeedReinitializing |= (NewFormatInfo.Width != VideoTrackFormat.Dim.X || NewFormatInfo.Height != VideoTrackFormat.Dim.Y);
		
		UE_LOG(LogRivermaxMedia, Log, TEXT("New video format detected: %dx%d with pixel format '%s'"), NewResolution.X, NewResolution.Y, *UEnum::GetValueAsString(NewFormat));
		
		if (bNeedReinitializing && bFollowsStreamResolution)
		{
			FScopeLock Lock(&StreamResolutionCriticalSection);
			StreamResolution = NewResolution;
		}
	}

	bool FRivermaxMediaPlayer::IsReadyToPlay() const
	{
		if (RivermaxThreadNewState == EMediaState::Playing)
		{
			FScopeLock Lock(&StreamResolutionCriticalSection);
			return StreamResolution == VideoTrackFormat.Dim;
		}

		return false;
	}

	void FRivermaxMediaPlayer::WaitForPendingTasks()
	{
		// Flush any rendering activity to be sure we can move on with clearing resources. 
		FlushRenderingCommands();

		// Wait for all pending tasks to complete. They should all complete at some point but add a timeout as a last resort. 
		constexpr double TimeoutSeconds = 2.0;
		const double StartTimeSeconds = FPlatformTime::Seconds();
		while (TasksInFlight > 0)
		{
			FPlatformProcess::SleepNoStats(SleepTimeSeconds);
			if ((FPlatformTime::Seconds() - StartTimeSeconds) > TimeoutSeconds)
			{
				UE_LOG(LogRivermaxMedia, Warning, TEXT("Timed out waiting for pendings tasks to finish."));
				break;
			}
		}
	}

	void FRivermaxMediaPlayer::WaitForSample(const FFrameExpectation& FrameExpectation, FWaitConditionFunc WaitConditionFunction, bool bCanTimeout)
	{
		const double StartTimeSeconds = FPlatformTime::Seconds();
		constexpr double TimeoutSeconds = 0.5;

		while (true)
		{
			{
				if (!IsReadyToPlay())
				{
					break;
				}

				if (FrameExpectation.FrameIndex == INDEX_NONE)
				{
					break;
				}

				if (IsFrameSkipped(FrameExpectation.FrameNumber))
				{
					UE_LOG(LogRivermaxMedia, Verbose, TEXT("Stopped waiting for frame %u as it was marked as skipped."), FrameExpectation.FrameNumber);
					break;
				}

				// Our goal here is to wait until the expected frame is available to be used (received) unless there is a timeout
				const TSharedPtr<FRivermaxSampleWrapper>& Frame = SamplePool[FrameExpectation.FrameIndex];
				if (WaitConditionFunction(Frame))
				{
					if (Frame->FrameNumber != FrameExpectation.FrameNumber)
					{
						UE_LOG(LogRivermaxMedia, Warning, TEXT("Rendering unexpected frame %u, when frame %u was expected."), Frame->FrameNumber, FrameExpectation.FrameNumber);
					}
					break;
				}

				{
					FPlatformProcess::SleepNoStats(SleepTimeSeconds);
					if (bCanTimeout && ((FPlatformTime::Seconds() - StartTimeSeconds) > TimeoutSeconds))
					{
						UE_LOG(LogRivermaxMedia, Error, TEXT("Timed out waiting for frame %u."), FrameExpectation.FrameNumber);
						break;
					}
				}
			}
		}
	}

	void FRivermaxMediaPlayer::SampleUploadSetupRenderThreadMode(const FFrameExpectation& NextFrameExpectations, FSampleConverterOperationSetup& OutConverterSetup)
	{
		TSharedPtr<FRivermaxSampleWrapper> SampleWrapper = SamplePool[NextFrameExpectations.FrameIndex];
		if (bDoesStreamSupportsGPUDirect)
		{
			OutConverterSetup.GetGPUBufferFunc = [SampleWrapper]() { return SampleWrapper->Sample->GetGPUBuffer(); };

			// Setup requirements for sample to be ready to be rendered
			FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
			RHICmdList.EnqueueLambda(
				[NextFrameExpectations, this](FRHICommandList& RHICmdList)
				{
					FWaitConditionFunc ReadyToRenderConditionFunc = [](const TSharedPtr<FRivermaxSampleWrapper>& Sample)
					{
						return Sample->bIsReadyToRender.load();
					};

					TRACE_CPUPROFILER_EVENT_SCOPE(RmaxWaitSampleReadyness);
					constexpr bool bCanTimeout = true;
					WaitForSample(NextFrameExpectations, MoveTemp(ReadyToRenderConditionFunc), bCanTimeout);
				}
			);
		}
		else
		{
			// For system memory path, we wait for the sample to be received before returning it.
			OutConverterSetup.GetSystemBufferFunc = [NextFrameExpectations, SampleWrapper, this]()
			{
				FWaitConditionFunc ReceivedConditionFunc = [](const TSharedPtr<FRivermaxSampleWrapper>& Sample)
				{
					return Sample->ReceptionState == ESampleReceptionState::Received;
				};

				TRACE_CPUPROFILER_EVENT_SCOPE(RmaxWaitSampleReception);
				constexpr bool bCanTimeout = true;
				WaitForSample(NextFrameExpectations, MoveTemp(ReceivedConditionFunc), bCanTimeout);

				// We can return the system buffer once we know it has arrived
				return SampleWrapper->Sample->GetBuffer();
			};
		}
	}

	void FRivermaxMediaPlayer::SampleUploadSetupTaskThreadMode(const FFrameExpectation& NextFrameExpectations, FSampleConverterOperationSetup& OutConverterSetup)
	{
		TSharedPtr<FRivermaxSampleWrapper> SampleWrapper = SamplePool[NextFrameExpectations.FrameIndex];

		// We will always be providing a buffer already located on the GPU even when not using gpudirect
		// Once a frame has arrived on system, we will upload it to the allocated gpu buffer.
		OutConverterSetup.GetGPUBufferFunc = [SampleWrapper]() { return SampleWrapper->Sample->GetGPUBuffer(); };

		OutConverterSetup.PreConvertFunc = [NextFrameExpectations, SampleWrapper, this](const FRDGBuilder& GraphBuilder)
		{
			// When GPUDirect is not involved, we have an extra step to do. We need to wait for the sample to be received
			// but also initiate the memcopy to gpu memory for it to be rendered
			if (bDoesStreamSupportsGPUDirect == false)
			{
				constexpr uint32 Offset = 0;
				const uint32 Size = SampleWrapper->Sample->GetGPUBuffer()->GetSize();
				SampleWrapper->LockedMemory = GraphBuilder.RHICmdList.LockBuffer(SampleWrapper->Sample->GetGPUBuffer()->GetRHI(), Offset, Size, EResourceLockMode::RLM_WriteOnly);

				++TasksInFlight;
				UE::Tasks::Launch(UE_SOURCE_LOCATION,
					[NextFrameExpectations, Size, SampleWrapper, this]()
					{
						ON_SCOPE_EXIT
						{
							--TasksInFlight;
						};

						FWaitConditionFunc ReceivedConditionFunc = [](const TSharedPtr<FRivermaxSampleWrapper>& Sample)
						{
							return Sample->ReceptionState == ESampleReceptionState::Received;
						};

						{
							TRACE_CPUPROFILER_EVENT_SCOPE(RmaxWaitSampleReception);
							constexpr bool bCanTimeout = true;
							WaitForSample(NextFrameExpectations, MoveTemp(ReceivedConditionFunc), bCanTimeout);
						}

						// In case the wait failed, make sure it's received before copying
						if (SamplePool[NextFrameExpectations.FrameIndex]->ReceptionState == ESampleReceptionState::Received)
						{
							TRACE_CPUPROFILER_EVENT_SCOPE(RmaxSampleUpload);
							FMemory::Memcpy(SampleWrapper->LockedMemory, SampleWrapper->Sample->GetBuffer(), Size);
						}
						
						// We always consider the sample ready to be rendered even if we haven't copied something over (timeout)
						// RHI commands have already been submitted to look for that frame so we can't back out at this point
						SampleWrapper->bIsReadyToRender = true;
					}
				);
			}
			
			// Setup requirements for sample to be ready to be rendered
			FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

			SCOPED_GPU_STAT(RHICmdList, Rmax_WaitForPixels);
			SCOPED_DRAW_EVENT(RHICmdList, Rmax_WaitForPixels);

			// Since we are going to enqueue a lambda that can potentially sleep in the RHI thread if the pixels haven't arrived,
			// we dispatch the existing commands (including the draw event start timing in the SCOPED_DRAW_EVENT above) before any potential sleep.
			RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);

			RHICmdList.EnqueueLambda(
				[NextFrameExpectations, this](FRHICommandList& RHICmdList)
				{
					FWaitConditionFunc ReadyToRenderConditionFunc = [](const TSharedPtr<FRivermaxSampleWrapper>& Sample)
					{
						return Sample->bIsReadyToRender.load();
					};

					TRACE_CPUPROFILER_EVENT_SCOPE(RmaxWaitSampleReadyness);

					// Only accept timeouts here if we're going through the system memory in order to avoid timing out while doing
					// the memcopy (buffer upload)
					bool bCanTimeout = bDoesStreamSupportsGPUDirect;
					WaitForSample(NextFrameExpectations, MoveTemp(ReadyToRenderConditionFunc), bCanTimeout);
				}
			);

			// Final step, if the memory was locked (non gpu direct), enqueue unlock after the wait for sample in order to render it
			if (SampleWrapper->LockedMemory && ensure(!bDoesStreamSupportsGPUDirect))
			{
				RHICmdList.UnlockBuffer(SampleWrapper->Sample->GetGPUBuffer()->GetRHI());
			}
		};
	}

}

#undef LOCTEXT_NAMESPACE

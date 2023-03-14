// Copyright Epic Games, Inc. All Rights Reserved.

#include "RivermaxMediaPlayer.h"

#include "IMediaEventSink.h"
#include "IRivermaxCoreModule.h"
#include "IRivermaxManager.h"
#include "MediaIOCoreEncodeTime.h"
#include "MediaIOCoreFileWriter.h"
#include "MediaIOCoreSamples.h"
#include "Misc/ScopeLock.h"
#include "RivermaxMediaLog.h"
#include "RivermaxMediaOutput.h"
#include "RivermaxMediaSourceOptions.h"
#include "RivermaxMediaTextureSample.h"
#include "RivermaxMediaUtils.h"
#include "RivermaxTypes.h"
#include "Stats/Stats2.h"

#if WITH_EDITOR
#include "EngineAnalytics.h"
#endif


#define LOCTEXT_NAMESPACE "FRivermaxMediaPlayer"

DECLARE_CYCLE_STAT(TEXT("Rivermax MediaPlayer Request frame"), STAT_Rivermax_MediaPlayer_RequestFrame, STATGROUP_Media);
DECLARE_CYCLE_STAT(TEXT("Rivermax MediaPlayer Process frame"), STAT_Rivermax_MediaPlayer_ProcessFrame, STATGROUP_Media);

namespace UE::RivermaxMedia
{
	/* FRivermaxVideoPlayer structors
	 *****************************************************************************/

	FRivermaxMediaPlayer::FRivermaxMediaPlayer(IMediaEventSink& InEventSink)
		: Super(InEventSink)
		, TextureSamplePool(MakeUnique<FRivermaxMediaTextureSamplePool>())
		, MaxNumVideoFrameBuffer(8)
		, RivermaxThreadNewState(EMediaState::Closed)
		, EventSink(InEventSink)
		, bIsSRGBInput(false)
		, bUseVideo(false)
		, SupportedSampleTypes(EMediaIOSampleType::None)
		, bPauseRequested(false)
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
			return false;
		}

		//Video related options
		{
			bIsSRGBInput = Options->GetMediaOption(RivermaxMediaOption::SRGBInput, bIsSRGBInput);
			DesiredPixelFormat = (ERivermaxMediaSourcePixelFormat)Options->GetMediaOption(RivermaxMediaOption::PixelFormat, (int64)ERivermaxMediaSourcePixelFormat::RGB_8bit);
		}

		{
			//Adjust supported sample types based on what's being captured
			SupportedSampleTypes = EMediaIOSampleType::Video;
			Samples->EnableTimedDataChannels(this, SupportedSampleTypes);
		}

		MaxNumVideoFrameBuffer = 8;
		bUseVideo = true;

		IRivermaxCoreModule* Module = FModuleManager::GetModulePtr<IRivermaxCoreModule>("RivermaxCore");
		if (Module && ConfigureStream(Options))
		{
			InputStream = Module->CreateInputStream();
		}
		if (InputStream == nullptr || !InputStream->Initialize(StreamOptions, *this))
		{
			UE_LOG(LogRivermaxMedia, Warning, TEXT("The Rivermax port couldn't be opened."));
			CurrentState = EMediaState::Error;
			RivermaxThreadNewState = EMediaState::Error;
			InputStream.Reset();
			return false;
		}

		// Setup our different supported channels based on source settings
		SetupSampleChannels();

		// finalize
		CurrentState = EMediaState::Preparing;
		RivermaxThreadNewState = EMediaState::Preparing;
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

		if (InputStream)
		{
			InputStream->Uninitialize(); // this may block, until the completion of a callback from IRivermaxChannelCallbackInterface
			InputStream.Reset();
		}

		TextureSamplePool->Reset();

		//Disable all our channels from the monitor
		Samples->EnableTimedDataChannels(this, EMediaIOSampleType::None);

		RivermaxThreadCurrentTextureSample.Reset();

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
		if (FrameInfo.VideoBufferSize > 0)
		{
			RivermaxThreadCurrentTextureSample = TextureSamplePool->AcquireShared();
			OutVideoFrameRequest.VideoBuffer = reinterpret_cast<uint8*>(RivermaxThreadCurrentTextureSample->RequestBuffer(FrameInfo.VideoBufferSize));
			return OutVideoFrameRequest.VideoBuffer != nullptr;
		}

		return false;
	}


	void FRivermaxMediaPlayer::OnVideoFrameReceived(const FRivermaxInputVideoFrameDescriptor& FrameInfo, const FRivermaxInputVideoFrameReception& ReceivedVideoFrame)
	{
		FTimespan DecodedTime = FTimespan::FromSeconds(GetPlatformSeconds());
		TOptional<FTimecode> DecodedTimecode;


		if (bUseVideo && ReceivedVideoFrame.VideoBuffer)
		{
			if (RivermaxThreadCurrentTextureSample.IsValid())
			{
				if (RivermaxThreadCurrentTextureSample->ConfigureSample(FrameInfo.Width, FrameInfo.Height, FrameInfo.Stride, DesiredPixelFormat, DecodedTime, VideoFrameRate, DecodedTimecode, bIsSRGBInput))
				{
					Samples->AddVideo(RivermaxThreadCurrentTextureSample.ToSharedRef());
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

		TickTimeManagement();
	}


	/* FRivermaxMediaPlayer implementation
	 *****************************************************************************/
	void FRivermaxMediaPlayer::ProcessFrame()
	{
		if (CurrentState == EMediaState::Playing)
		{
		}
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

	void FRivermaxMediaPlayer::OnInitializationCompleted(bool bHasSucceed)
	{
		RivermaxThreadNewState = bHasSucceed ? EMediaState::Playing : EMediaState::Error;
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
		StreamOptions.Resolution = VideoTrackFormat.Dim;
		StreamOptions.FrameRate = VideoFrameRate;

		// We need to adjust horizontal resolution if it's not aligned to pgroup pixel coverage
		// RTP header has to describe a full pgroup even if it's outside of effective resolution.
		StreamOptions.PixelFormat = UE::RivermaxMediaUtils::Private::MediaSourcePixelFormatToRivermaxSamplingType(DesiredPixelFormat);
		const FVideoFormatInfo FormatInfo = FStandardVideoFormat::GetVideoFormatInfo(StreamOptions.PixelFormat);
		const uint32 PixelAlignment = FormatInfo.PixelGroupCoverage;
		const uint32 AlignedHorizontalResolution = (StreamOptions.Resolution.X % PixelAlignment) ? StreamOptions.Resolution.X + (PixelAlignment - (StreamOptions.Resolution.X % PixelAlignment)) : StreamOptions.Resolution.X;
		StreamOptions.AlignedResolution = FIntPoint(AlignedHorizontalResolution, StreamOptions.Resolution.Y);

		return true;
	}

}

#undef LOCTEXT_NAMESPACE

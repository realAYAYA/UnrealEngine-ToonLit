// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlackmagicMediaPlayer.h"

#include "Blackmagic.h"
#include "BlackmagicMediaPrivate.h"
#include "BlackmagicMediaSource.h"

#include "Engine/GameEngine.h"
#include "HAL/CriticalSection.h"
#include "HAL/PlatformProcess.h"
#include "IBlackmagicMediaModule.h"
#include "IMediaEventSink.h"
#include "IMediaOptions.h"
#include "MediaIOCoreEncodeTime.h"
#include "MediaIOCoreFileWriter.h"
#include "MediaIOCoreSamples.h"
#include "Misc/App.h"
#include "Misc/ScopeLock.h"
#include "RenderCommandFence.h"
#include "Slate/SceneViewport.h"
#include "Stats/Stats2.h"
#include "Styling/SlateStyle.h"
#include "Templates/Atomic.h"

#include "BlackmagicMediaSource.h"

#if WITH_EDITOR
#include "EngineAnalytics.h"
#endif


#define LOCTEXT_NAMESPACE "BlackmagicMediaPlayer"

DECLARE_CYCLE_STAT(TEXT("Blackmagic MediaPlayer Process received frame"), STAT_Blackmagic_MediaPlayer_ProcessReceivedFrame, STATGROUP_Media);

bool bBlackmagicWriteOutputRawDataCmdEnable = false;
static FAutoConsoleCommand BlackmagicWriteOutputRawDataCmd(
	TEXT("Blackmagic.WriteOutputRawData"),
	TEXT("Write Blackmagic raw output buffer to file."),
	FConsoleCommandDelegate::CreateLambda([]() { bBlackmagicWriteOutputRawDataCmdEnable = true;	})
	);

namespace BlackmagicMediaPlayerHelpers
{
	static const int32 ToleratedExtraMaxBufferCount = 2;

	class FBlackmagicMediaPlayerEventCallback : public BlackmagicDesign::IInputEventCallback
	{
	public:
		FBlackmagicMediaPlayerEventCallback(FBlackmagicMediaPlayer* InMediaPlayer, const BlackmagicDesign::FChannelInfo& InChannelInfo)
			: RefCounter(0)
			, ChannelInfo(InChannelInfo)
			, MediaPlayer(InMediaPlayer)
			, MediaState(EMediaState::Closed)
			, PrevousTimespan(FTimespan::Zero())
			, bEncodeTimecodeInTexel(false)
			, LastBitsPerSample(0)
			, LastNumChannels(0)
			, LastSampleRate(0)
			, PreviousAudioFrameDropCount(0)
			, PreviousVideoFrameDropCount(0)
			, LastHasFrameTime(0.0)
			, bReceivedValidFrame(false)
			, bIsTimecodeExpected(false)
			, bHasWarnedMissingTimecode(false)
			, bIsSRGBInput(false)
		{
		}

		bool Initialize(const BlackmagicDesign::FInputChannelOptions& InChannelInfo, bool bInEncodeTimecodeInTexel, int32 InMaxNumAudioFrameBuffer, int32 InMaxNumVideoFrameBuffer, bool bInIsSRGBInput)
		{
			AddRef();

			bEncodeTimecodeInTexel = bInEncodeTimecodeInTexel;
			MaxNumAudioFrameBuffer = InMaxNumAudioFrameBuffer;
			MaxNumVideoFrameBuffer = InMaxNumVideoFrameBuffer;
			bIsTimecodeExpected = InChannelInfo.TimecodeFormat != BlackmagicDesign::ETimecodeFormat::TCF_None;
			bIsSRGBInput = bInIsSRGBInput;

			BlackmagicDesign::ReferencePtr<BlackmagicDesign::IInputEventCallback> SelfRef(this);
			BlackmagicIdendifier = BlackmagicDesign::RegisterCallbackForChannel(ChannelInfo, InChannelInfo, SelfRef);
			MediaState = BlackmagicIdendifier.IsValid() ? EMediaState::Preparing : EMediaState::Error;
			return BlackmagicIdendifier.IsValid();
		}

		void Uninitialize()
		{
			FScopeLock Lock(&CallbackLock);
			MediaPlayer = nullptr;

			if (BlackmagicIdendifier.IsValid())
			{
				MediaState = EMediaState::Stopped;
				BlackmagicDesign::UnregisterCallbackForChannel(ChannelInfo, BlackmagicIdendifier);
				BlackmagicIdendifier = BlackmagicDesign::FUniqueIdentifier();
			}

			Release();
		}

		EMediaState GetMediaState() const { return MediaState; }

		void UpdateAudioTrackFormat(FMediaAudioTrackFormat& OutAudioTrackFormat)
		{
			OutAudioTrackFormat.BitsPerSample = LastBitsPerSample;
			OutAudioTrackFormat.NumChannels = LastNumChannels;
			OutAudioTrackFormat.SampleRate = LastSampleRate;
		}

		void VerifyFrameDropCount_GameThread(const FString& InUrl)
		{
			if (MediaPlayer->bVerifyFrameDropCount)
			{
				const int32 CurrentAudioDropCount = MediaPlayer->Samples->GetAudioFrameDropCount();
				int32 DeltaAudioDropCount = CurrentAudioDropCount;
				if (CurrentAudioDropCount >= PreviousAudioFrameDropCount)
				{
					DeltaAudioDropCount = CurrentAudioDropCount - PreviousAudioFrameDropCount;
				}
				PreviousAudioFrameDropCount = CurrentAudioDropCount;
				if (DeltaAudioDropCount > 0)
				{
					UE_LOG(LogBlackmagicMedia, Warning, TEXT("Lost %d audio frames on input %s. Frame rate is either too slow or buffering capacity is too small."), DeltaAudioDropCount, *InUrl);
				}

				const int32 CurrentVideoDropCount = MediaPlayer->Samples->GetVideoFrameDropCount();
				int32 DeltaVideoDropCount = CurrentVideoDropCount;
				if (CurrentVideoDropCount >= PreviousVideoFrameDropCount)
				{
					DeltaVideoDropCount = CurrentVideoDropCount - PreviousVideoFrameDropCount;
				}
				PreviousVideoFrameDropCount = CurrentVideoDropCount;
				if (DeltaVideoDropCount > 0)
				{
					UE_LOG(LogBlackmagicMedia, Warning, TEXT("Lost %d video frames on input %s. Frame rate is either too slow or buffering capacity is too small."), DeltaVideoDropCount, *InUrl);
				}
			}
		}

	private:
		virtual void AddRef() override
		{
			++RefCounter;
		}

		virtual void Release() override
		{
			--RefCounter;
			if (RefCounter == 0)
			{
				delete this;
			}
		}

		virtual void OnInitializationCompleted(bool bSuccess) override
		{
			MediaState = bSuccess ? EMediaState::Playing : EMediaState::Error;
		}

		virtual void OnShutdownCompleted() override
		{
			MediaState = EMediaState::Closed;
		}

		virtual void OnFrameReceived(const BlackmagicDesign::IInputEventCallback::FFrameReceivedInfo& InFrameInfo) override
		{
			SCOPE_CYCLE_COUNTER(STAT_Blackmagic_MediaPlayer_ProcessReceivedFrame);

			FScopeLock Lock(&CallbackLock);

			if (MediaPlayer == nullptr)
			{
				return;
			}

			if (!InFrameInfo.bHasInputSource && InFrameInfo.AudioBuffer == nullptr)
			{
				const double CurrentTime = FApp::GetCurrentTime();
				const double TimeAllowedToConnect = 2.0;
				if (LastHasFrameTime < 0.1)
				{
					LastHasFrameTime = CurrentTime;
				}
				if (bReceivedValidFrame || CurrentTime - LastHasFrameTime > TimeAllowedToConnect)
				{
					if (!MediaPlayer->bAutoDetect)
					{
						UE_LOG(LogBlackmagicMedia, Error, TEXT("There is no video input for '%s'."), *MediaPlayer->GetUrl());
						MediaState = EMediaState::Error;
					}
					else
					{
						MediaState = EMediaState::Paused;
					}
				}
				return;
			}
			else if (MediaState == EMediaState::Paused)
			{
				MediaState = EMediaState::Playing;
			}

			bReceivedValidFrame = bReceivedValidFrame || InFrameInfo.bHasInputSource;

			FTimespan DecodedTime = FTimespan::FromSeconds(MediaPlayer->GetPlatformSeconds());
			FTimespan DecodedTimeF2 = DecodedTime + FTimespan::FromSeconds(MediaPlayer->VideoFrameRate.AsInterval());

			if (MediaState == EMediaState::Playing)
			{
				TOptional<FTimecode> DecodedTimecode;
				TOptional<FTimecode> DecodedTimecodeF2;

				if (InFrameInfo.bHaveTimecode)
				{
					//We expect the timecode to be processed in the library. What we receive will be a "linear" timecode even for frame rates greater than 30.
					const int32 FrameLimit = InFrameInfo.FieldDominance != BlackmagicDesign::EFieldDominance::Interlaced ? FMath::RoundToInt(MediaPlayer->VideoFrameRate.AsDecimal()) : FMath::RoundToInt(MediaPlayer->VideoFrameRate.AsDecimal()) - 1;
					if ((int32)InFrameInfo.Timecode.Frames >= FrameLimit)
					{
						UE_LOG(LogBlackmagicMedia, Warning, TEXT("Input '%s' received an invalid Timecode frame number (%d) for the current frame rate (%s)."), *MediaPlayer->GetUrl(), InFrameInfo.Timecode.Frames, *MediaPlayer->VideoFrameRate.ToPrettyText().ToString());
					}

					DecodedTimecode = FTimecode(InFrameInfo.Timecode.Hours, InFrameInfo.Timecode.Minutes, InFrameInfo.Timecode.Seconds, InFrameInfo.Timecode.Frames, InFrameInfo.Timecode.bIsDropFrame);
					DecodedTimecodeF2 = DecodedTimecode;
					++DecodedTimecodeF2->Frames;

					const FFrameNumber ConvertedFrameNumber = DecodedTimecode.GetValue().ToFrameNumber(MediaPlayer->VideoFrameRate);
					const double NumberOfSeconds = ConvertedFrameNumber.Value * MediaPlayer->VideoFrameRate.AsInterval();
					const FTimespan TimecodeDecodedTime = FTimespan::FromSeconds(NumberOfSeconds);
					if (MediaPlayer->bUseTimeSynchronization)
					{
						DecodedTime = TimecodeDecodedTime;
						DecodedTimeF2 = TimecodeDecodedTime + FTimespan::FromSeconds(MediaPlayer->VideoFrameRate.AsInterval());
					}

					PreviousTimecode = InFrameInfo.Timecode;
					PrevousTimespan = TimecodeDecodedTime;

					if (MediaPlayer->IsTimecodeLogEnabled())
					{
						UE_LOG(LogBlackmagicMedia, Log, TEXT("Input '%s' has timecode : %02d:%02d:%02d:%02d"), *MediaPlayer->GetUrl()
							, InFrameInfo.Timecode.Hours, InFrameInfo.Timecode.Minutes, InFrameInfo.Timecode.Seconds, InFrameInfo.Timecode.Frames);
					}
				}
				else if (!bHasWarnedMissingTimecode && bIsTimecodeExpected)
				{
					bHasWarnedMissingTimecode = true;
					UE_LOG(LogBlackmagicMedia, Warning, TEXT("Input '%s' is expecting timecode but didn't receive any in the last frame. Is your source configured correctly?"), *MediaPlayer->GetUrl());
				}

				if (InFrameInfo.AudioBuffer)
				{
					auto AudioSamle = MediaPlayer->AudioSamplePool->AcquireShared();
					if (AudioSamle->Initialize(reinterpret_cast<int32*>(InFrameInfo.AudioBuffer)
						, InFrameInfo.AudioBufferSize / sizeof(int32)
						, InFrameInfo.NumberOfAudioChannel
						, InFrameInfo.AudioRate
						, DecodedTime
						, DecodedTimecode))
					{
						MediaPlayer->Samples->AddAudio(AudioSamle);

						LastBitsPerSample = sizeof(int32);
						LastSampleRate = InFrameInfo.AudioRate;
						LastNumChannels = InFrameInfo.NumberOfAudioChannel;
					}
				}

				if (InFrameInfo.VideoBuffer)
				{
					const bool bIsProgressivePicture = InFrameInfo.FieldDominance == BlackmagicDesign::EFieldDominance::Progressive;
					EMediaTextureSampleFormat SampleFormat = EMediaTextureSampleFormat::CharBGRA;
					EMediaIOCoreEncodePixelFormat EncodePixelFormat = EMediaIOCoreEncodePixelFormat::CharUYVY;
					FString OutputFilename = "";

					switch (InFrameInfo.PixelFormat)
					{
					case BlackmagicDesign::EPixelFormat::pf_8Bits:
						SampleFormat = EMediaTextureSampleFormat::CharUYVY;
						EncodePixelFormat = EMediaIOCoreEncodePixelFormat::CharUYVY;
						OutputFilename = FString::Printf(TEXT("Blackmagic_Output_8_YUV_ch%d"), ChannelInfo.DeviceIndex);
						break;
					case BlackmagicDesign::EPixelFormat::pf_10Bits:
						SampleFormat = EMediaTextureSampleFormat::YUVv210;
						EncodePixelFormat = EMediaIOCoreEncodePixelFormat::YUVv210;
						OutputFilename = FString::Printf(TEXT("Blackmagic_Output_10_YUV_ch%d"), ChannelInfo.DeviceIndex);
						break;
					}

					if (bBlackmagicWriteOutputRawDataCmdEnable)
					{
						MediaIOCoreFileWriter::WriteRawFile(OutputFilename, reinterpret_cast<uint8*>(InFrameInfo.VideoBuffer), InFrameInfo.VideoPitch * InFrameInfo.VideoHeight);
						bBlackmagicWriteOutputRawDataCmdEnable = false;
					}

					if (bIsProgressivePicture)
					{
						if (bEncodeTimecodeInTexel && DecodedTimecode.IsSet())
						{
							FTimecode SetTimecode = DecodedTimecode.GetValue();
							FMediaIOCoreEncodeTime EncodeTime(EncodePixelFormat, InFrameInfo.VideoBuffer, InFrameInfo.VideoPitch, InFrameInfo.VideoWidth, InFrameInfo.VideoHeight);
							EncodeTime.Render(SetTimecode.Hours, SetTimecode.Minutes, SetTimecode.Seconds, SetTimecode.Frames);
						}

						bool bGPUDirectTexturesAvailable = false;
						if (MediaPlayer->CanUseGPUTextureTransfer())
						{
							if (!MediaPlayer->Textures.Num())
							{
								bGPUDirectTexturesAvailable = false;
								UE_LOG(LogBlackmagicMedia, Error, TEXT("No texture available while doing a gpu texture transfer."));
							}
							else
							{
								bGPUDirectTexturesAvailable = true;
							}
						}

						auto TextureSample = MediaPlayer->TextureSamplePool->AcquireShared();
						bool bInitializeResult = false;
						if (bGPUDirectTexturesAvailable)
						{
							bInitializeResult = TextureSample->SetProperties(InFrameInfo.VideoPitch, InFrameInfo.VideoWidth, InFrameInfo.VideoHeight, SampleFormat, DecodedTime, MediaPlayer->VideoFrameRate, DecodedTimecode, bIsSRGBInput);
							if (bInitializeResult)
							{
								TextureSample->SetBuffer(InFrameInfo.VideoBuffer);
							}
						}
						else
						{
							bInitializeResult = TextureSample->Initialize(InFrameInfo.VideoBuffer
								, InFrameInfo.VideoPitch * InFrameInfo.VideoHeight
								, InFrameInfo.VideoPitch
								, InFrameInfo.VideoWidth
								, InFrameInfo.VideoHeight
								, SampleFormat
								, DecodedTime
								, MediaPlayer->VideoFrameRate
								, DecodedTimecode
								, bIsSRGBInput);
						}

						if (bInitializeResult)
						{
							if (bGPUDirectTexturesAvailable && MediaPlayer->CanUseGPUTextureTransfer())
							{
								MediaPlayer->PreGPUTransfer(TextureSample);
								MediaPlayer->ExecuteGPUTransfer(TextureSample);
							}
							else
							{
								MediaPlayer->Samples->AddVideo(TextureSample);
							}
						}
					}
					else
					{
						bool bEven = true;

						if (FMediaIOCorePlayerBase::CVarExperimentalFieldFlipFix.GetValueOnAnyThread())
						{
							bEven = GFrameCounterRenderThread % 2 != FMediaIOCorePlayerBase::CVarFlipInterlaceFields.GetValueOnAnyThread();
						}

						auto TextureSampleEven = MediaPlayer->TextureSamplePool->AcquireShared();

						if (TextureSampleEven->InitializeWithEvenOddLine(bEven
							, InFrameInfo.VideoBuffer
							, InFrameInfo.VideoPitch * InFrameInfo.VideoHeight
							, InFrameInfo.VideoPitch
							, InFrameInfo.VideoWidth
							, InFrameInfo.VideoHeight
							, SampleFormat
							, DecodedTime
							, MediaPlayer->VideoFrameRate
							, DecodedTimecode
							, bIsSRGBInput))
						{
							MediaPlayer->Samples->AddVideo(TextureSampleEven);
						}

						auto TextureSampleOdd = MediaPlayer->TextureSamplePool->AcquireShared();
						if (TextureSampleOdd->InitializeWithEvenOddLine(!bEven
							, InFrameInfo.VideoBuffer
							, InFrameInfo.VideoPitch * InFrameInfo.VideoHeight
							, InFrameInfo.VideoPitch
							, InFrameInfo.VideoWidth
							, InFrameInfo.VideoHeight
							, SampleFormat
							, DecodedTimeF2
							, MediaPlayer->VideoFrameRate
							, DecodedTimecodeF2
							, bIsSRGBInput))
						{
							MediaPlayer->Samples->AddVideo(TextureSampleOdd);
						}
					}
				}
			}
		}

		virtual void OnFrameFormatChanged(const BlackmagicDesign::FFormatInfo& NewFormat) override
		{
			if (MediaPlayer->bAutoDetect)
			{
				MediaPlayer->VideoFrameRate = FFrameRate(NewFormat.FrameRateNumerator, NewFormat.FrameRateDenominator);
			}
			else
			{
				UE_LOG(LogBlackmagicMedia, Error, TEXT("The video format changed for '%s'."), MediaPlayer ? *MediaPlayer->GetUrl() : TEXT("<Invalid>"));
				MediaState = EMediaState::Error;
			}
		}

		virtual void OnInterlacedOddFieldEvent(int64 FrameNumber) override
		{
			
		}

	private:
		TAtomic<int32> RefCounter;

		BlackmagicDesign::FUniqueIdentifier BlackmagicIdendifier;
		BlackmagicDesign::FChannelInfo ChannelInfo;

		mutable FCriticalSection CallbackLock;
		FBlackmagicMediaPlayer* MediaPlayer;

		EMediaState MediaState;

		BlackmagicDesign::FTimecode PreviousTimecode;
		FTimespan PrevousTimespan;
		bool bEncodeTimecodeInTexel;

		/** Number of audio bits per sample, audio channels and sample rate. */
		uint32 LastBitsPerSample;
		uint32 LastNumChannels;
		uint32 LastSampleRate;

		/** Frame drop count from the previous tick to keep track of deltas */
		int32 PreviousAudioFrameDropCount;
		int32 PreviousVideoFrameDropCount;

		int32 MaxNumAudioFrameBuffer;
		int32 MaxNumVideoFrameBuffer;

		/** Has video frame detection */
		double LastHasFrameTime;
		bool bReceivedValidFrame;

		bool bIsTimecodeExpected;
		bool bHasWarnedMissingTimecode;

		/** Whether this input is in sRGB space and needs a to linear conversion */
		bool bIsSRGBInput;
	};
}

/* FBlackmagicVideoPlayer structors
*****************************************************************************/

FBlackmagicMediaPlayer::FBlackmagicMediaPlayer(IMediaEventSink& InEventSink)
	: Super(InEventSink)
	, EventCallback(nullptr)
	, AudioSamplePool(MakeUnique<FBlackmagicMediaAudioSamplePool>())
	, TextureSamplePool(MakeUnique<FBlackmagicMediaTextureSamplePool>())
	, bVerifyFrameDropCount(false)
	, SupportedSampleTypes(EMediaIOSampleType::None)
{
}

FBlackmagicMediaPlayer::~FBlackmagicMediaPlayer()
{
	Close();
}

/* IMediaPlayer interface
*****************************************************************************/

void FBlackmagicMediaPlayer::Close()
{
	if (EventCallback)
	{
		EventCallback->Uninitialize();
		EventCallback = nullptr;
	}

	AudioSamplePool->Reset();
	TextureSamplePool->Reset();

	//Disable all our channels from the monitor
	Samples->EnableTimedDataChannels(this, EMediaIOSampleType::None);

	UnregisterSampleBuffers();
	UnregisterTextures();

	Super::Close();
}

FGuid FBlackmagicMediaPlayer::GetPlayerPluginGUID() const
{
	static FGuid PlayerPluginGUID(0x62a47ff5, 0xf61243a1, 0x9b377536, 0xc906c883);
	return PlayerPluginGUID;
}

/**
 * @EventName MediaFramework.BlackmagicMediaSourceOpened
 * @Trigger Triggered when a Blackmagic media source is opened through a media player.
 * @Type Client
 * @Owner MediaIO Team
 */
bool FBlackmagicMediaPlayer::Open(const FString& Url, const IMediaOptions* Options)
{
	if (!IBlackmagicMediaModule::Get().IsInitialized())
	{
		UE_LOG(LogBlackmagicMedia, Error, TEXT("The BlackmagicMediaPlayer can't open URL '%s'. Blackmagic is not initialized on your machine."), *Url);
		return false;
	}

	if (!Super::Open(Url, Options))
	{
		return false;
	}
	
	const EMediaIOAutoDetectableTimecodeFormat TimecodeFormat = (EMediaIOAutoDetectableTimecodeFormat)(Options->GetMediaOption(BlackmagicMediaOption::TimecodeFormat, (int64)EMediaIOAutoDetectableTimecodeFormat::None));
	const bool bAutoDetectTimecode = TimecodeFormat == EMediaIOAutoDetectableTimecodeFormat::Auto;
	
	BlackmagicDesign::FChannelInfo ChannelInfo;
	ChannelInfo.DeviceIndex = Options->GetMediaOption(BlackmagicMediaOption::DeviceIndex, (int64)0);

	check(EventCallback == nullptr);
	EventCallback = new BlackmagicMediaPlayerHelpers::FBlackmagicMediaPlayerEventCallback(this, ChannelInfo);

	BlackmagicDesign::FInputChannelOptions ChannelOptions;
	ChannelOptions.bAutoDetect = bAutoDetect;
	ChannelOptions.CallbackPriority = 10;
	ChannelOptions.bReadVideo = Options->GetMediaOption(BlackmagicMediaOption::CaptureVideo, true);
	ChannelOptions.FormatInfo.DisplayMode = Options->GetMediaOption(BlackmagicMediaOption::BlackmagicVideoFormat, (int64)BlackmagicMediaOption::DefaultVideoFormat);
	BlackmagicColorFormat = (EBlackmagicMediaSourceColorFormat)(Options->GetMediaOption(BlackmagicMediaOption::ColorFormat, (int64)EBlackmagicMediaSourceColorFormat::YUV8));

	ChannelOptions.PixelFormat = BlackmagicColorFormat == EBlackmagicMediaSourceColorFormat::YUV8 ? BlackmagicDesign::EPixelFormat::pf_8Bits : BlackmagicDesign::EPixelFormat::pf_10Bits;
	const bool bIsSRGBInput = Options->GetMediaOption(BlackmagicMediaOption::SRGBInput, true);

	switch (TimecodeFormat)
	{
	case EMediaIOAutoDetectableTimecodeFormat::Auto:
		ChannelOptions.TimecodeFormat = BlackmagicDesign::ETimecodeFormat::TCF_Auto;
		break;
	case EMediaIOAutoDetectableTimecodeFormat::LTC:
		ChannelOptions.TimecodeFormat = BlackmagicDesign::ETimecodeFormat::TCF_LTC;
		break;
	case EMediaIOAutoDetectableTimecodeFormat::VITC:
		ChannelOptions.TimecodeFormat = BlackmagicDesign::ETimecodeFormat::TCF_VITC1;
		break;
	case EMediaIOAutoDetectableTimecodeFormat::None:
	default:
		ChannelOptions.TimecodeFormat = BlackmagicDesign::ETimecodeFormat::TCF_None;
		break;
	}

	//Audio options
	{
		ChannelOptions.bReadAudio = Options->GetMediaOption(BlackmagicMediaOption::CaptureAudio, false);
		const EBlackmagicMediaAudioChannel AudioChannelOption = (EBlackmagicMediaAudioChannel)(Options->GetMediaOption(BlackmagicMediaOption::AudioChannelOption, (int64)EBlackmagicMediaAudioChannel::Stereo2));
		ChannelOptions.NumberOfAudioChannel = (AudioChannelOption == EBlackmagicMediaAudioChannel::Surround8) ? 8 : 2;
	}

	//Adjust supported sample types based on what's being captured
	SupportedSampleTypes = ChannelOptions.bReadVideo ? EMediaIOSampleType::Video : EMediaIOSampleType::None;
	SupportedSampleTypes |= ChannelOptions.bReadAudio ? EMediaIOSampleType::Audio : EMediaIOSampleType::None;
	Samples->EnableTimedDataChannels(this, SupportedSampleTypes);

	bVerifyFrameDropCount = Options->GetMediaOption(BlackmagicMediaOption::LogDropFrame, false);
	const bool bEncodeTimecodeInTexel = TimecodeFormat != EMediaIOAutoDetectableTimecodeFormat::None && Options->GetMediaOption(BlackmagicMediaOption::EncodeTimecodeInTexel, false);
	MaxNumAudioFrameBuffer = Options->GetMediaOption(BlackmagicMediaOption::MaxAudioFrameBuffer, (int64)8);
	MaxNumVideoFrameBuffer = Options->GetMediaOption(BlackmagicMediaOption::MaxVideoFrameBuffer, (int64)8);

	// Setup our different supported channels based on source settings
	SetupSampleChannels();

	bool bSuccess = EventCallback->Initialize(ChannelOptions, bEncodeTimecodeInTexel, MaxNumAudioFrameBuffer, MaxNumVideoFrameBuffer, bIsSRGBInput);

	if (!bSuccess)
	{
		EventCallback->Uninitialize();
		EventCallback = nullptr;
	}
#if WITH_EDITOR
	else if (FEngineAnalytics::IsAvailable())
	{
		TArray<FAnalyticsEventAttribute> EventAttributes;
		
		const int64 ResolutionWidth = Options->GetMediaOption( FMediaIOCoreMediaOption::ResolutionWidth, (int64)1920);
		const int64 ResolutionHeight = Options->GetMediaOption( FMediaIOCoreMediaOption::ResolutionHeight, (int64)1080);
	
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("ResolutionWidth"), FString::Printf(TEXT("%d"), ResolutionWidth)));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("ResolutionHeight"), FString::Printf(TEXT("%d"), ResolutionHeight)));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("FrameRate"), *VideoFrameRate.ToPrettyText().ToString()));
		
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("MediaFramework.BlackmagicMediaSourceOpened"), EventAttributes);
	}
#endif

	return bSuccess;
}

void FBlackmagicMediaPlayer::TickInput(FTimespan DeltaTime, FTimespan Timecode)
{
	// update player state
	EMediaState NewState = EventCallback ? EventCallback->GetMediaState() : EMediaState::Closed;

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

void FBlackmagicMediaPlayer::TickFetch(FTimespan DeltaTime, FTimespan Timecode)
{
	Super::TickFetch(DeltaTime, Timecode);
	if (IsHardwareReady())
	{
		ProcessFrame();
		VerifyFrameDropCount();
	}
}

#if WITH_EDITOR
const FSlateBrush* FBlackmagicMediaPlayer::GetDisplayIcon() const
{
	return IBlackmagicMediaModule::Get().GetStyle()->GetBrush("BlackmagicMediaIcon");
}
#endif //WITH_EDITOR

void FBlackmagicMediaPlayer::ProcessFrame()
{
	EventCallback->UpdateAudioTrackFormat(AudioTrackFormat);
}

void FBlackmagicMediaPlayer::VerifyFrameDropCount()
{
	EventCallback->VerifyFrameDropCount_GameThread(OpenUrl);
}

bool FBlackmagicMediaPlayer::IsHardwareReady() const
{
	return EventCallback && EventCallback->GetMediaState() == EMediaState::Playing;
}

void FBlackmagicMediaPlayer::SetupSampleChannels()
{
	FMediaIOSamplingSettings VideoSettings = BaseSettings;
	VideoSettings.BufferSize = MaxNumVideoFrameBuffer;
	Samples->InitializeVideoBuffer(VideoSettings);

	FMediaIOSamplingSettings AudioSettings = BaseSettings;
	AudioSettings.BufferSize = MaxNumAudioFrameBuffer;
	Samples->InitializeAudioBuffer(AudioSettings);
}

void FBlackmagicMediaPlayer::AddVideoSample(const TSharedRef<FMediaIOCoreTextureSampleBase>& InSample)
{
	Samples->AddVideo(InSample);
}

#undef LOCTEXT_NAMESPACE


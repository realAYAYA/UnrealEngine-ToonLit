// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaIOCorePlayerBase.h"

#include "Engine/Engine.h"
#include "Engine/TimecodeProvider.h"
#include "GPUTextureTransferModule.h"
#include "HAL/IConsoleManager.h"
#include "IMediaEventSink.h"
#include "IMediaOptions.h"
#include "IMediaModule.h"
#include "IMediaPlayerFactory.h"
#include "ITimeManagementModule.h"
#include "MediaIOCoreModule.h"
#include "MediaIOCoreSamples.h"
#include "Misc/App.h"
#include "Misc/ScopeLock.h"
#include "RenderingThread.h"
#include "PixelFormat.h"
#include "TimedDataInputCollection.h"
#include "TimeSynchronizableMediaSource.h"

#define LOCTEXT_NAMESPACE "MediaIOCorePlayerBase"

/* MediaIOCorePlayerDetail
 *****************************************************************************/

namespace MediaIOCorePlayerDetail
{
	static double ComputeTimeCodeOffset()
	{
		const FDateTime DateTime = FDateTime::Now();
		double HighPerformanceClock = FPlatformTime::Seconds();
		const FTimespan Timespan = DateTime.GetTimeOfDay();
		double Delta = Timespan.GetTotalSeconds() - HighPerformanceClock;
		return Delta;
	}

	static const double HighPerformanceClockDelta = ComputeTimeCodeOffset();
	static bool bLogTimecode = false;

	TAutoConsoleVariable<int32> CVarMediaIOMaxBufferSize(
		TEXT("MediaIO.TimedDataChannel.MaxBufferSize"),
		32,
		TEXT("The max size the MediaIO channels is allowed to set the buffer size."),
		ECVF_Default
	);

	static FAutoConsoleCommand CCommandShowTimecode(
		TEXT("MediaIO.ShowInputTimecode"),
		TEXT("All media player will log the frame timecode when a new frame is captured."),
		FConsoleCommandDelegate::CreateLambda([](){ MediaIOCorePlayerDetail::bLogTimecode = true; }),
		ECVF_Cheat);

	static FAutoConsoleCommand CCommandHideTimecode(
		TEXT("MediaIO.HideInputTimecode"),
		TEXT("All media player will stop logging the frame timecode when a new frame is captured."),
		FConsoleCommandDelegate::CreateLambda([]() { MediaIOCorePlayerDetail::bLogTimecode = false; }),
		ECVF_Cheat);

	static TAutoConsoleVariable<int32> CVarEnableGPUDirectInput(
		TEXT("MediaIO.EnableGPUDirectInput"), 0,
		TEXT("Whether to enable GPU direct for faster video frame copies. (Experimental)"),
		ECVF_RenderThreadSafe);

}

/* FMediaIOCoreMediaOption structors
 *****************************************************************************/

const FName FMediaIOCoreMediaOption::FrameRateNumerator("FrameRateNumerator");
const FName FMediaIOCoreMediaOption::FrameRateDenominator("FrameRateDenominator");
const FName FMediaIOCoreMediaOption::ResolutionWidth("ResolutionWidth");
const FName FMediaIOCoreMediaOption::ResolutionHeight("ResolutionHeight");
const FName FMediaIOCoreMediaOption::VideoModeName("VideoModeName");

TAutoConsoleVariable<int32> FMediaIOCorePlayerBase::CVarFlipInterlaceFields(
	TEXT("MediaIO.FlipInputInterlaceFields"), 0,
	TEXT("Whether to flip input interlace fields. (Experimental)"),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> FMediaIOCorePlayerBase::CVarExperimentalFieldFlipFix(
	TEXT("MediaIO.PreventFieldFlipping"), 1,
	TEXT("Whether to attempt fixing field flipping on input. (Experimental)"),
	ECVF_RenderThreadSafe);

/* FMediaIOCorePlayerBase structors
 *****************************************************************************/

FMediaIOCorePlayerBase::FMediaIOCorePlayerBase(IMediaEventSink& InEventSink)
	: CurrentState(EMediaState::Closed)
	, CurrentTime(FTimespan::Zero())
	, EventSink(InEventSink)
	, VideoFrameRate(30, 1)
	, bUseTimeSynchronization(false)
	, bWarnedIncompatibleFrameRate(false)
	, bAutoDetect(false)
	, FrameDelay(0)
	, TimeDelay(0.0)
	, PreviousFrameTimespan(FTimespan::Zero())
{
	Samples = new FMediaIOCoreSamples();

	FGPUTextureTransferModule& Module = FGPUTextureTransferModule::Get();
	GPUTextureTransfer = Module.GetTextureTransfer();
}

FMediaIOCorePlayerBase::~FMediaIOCorePlayerBase()
{
	delete Samples;
	Samples = nullptr;
}

/* IMediaPlayer interface
*****************************************************************************/

void FMediaIOCorePlayerBase::Close()
{
	if (CurrentState != EMediaState::Closed)
	{
		CurrentState = EMediaState::Closed;
		CurrentTime = FTimespan::Zero();
		AudioTrackFormat.NumChannels = 0;
		AudioTrackFormat.SampleRate = 0;

		Samples->FlushSamples();
		EventSink.ReceiveMediaEvent(EMediaEvent::TracksChanged);
		EventSink.ReceiveMediaEvent(EMediaEvent::MediaClosed);
	}

	ITimeManagementModule::Get().GetTimedDataInputCollection().Remove(this);
}

FString FMediaIOCorePlayerBase::GetInfo() const
{
	FString Info;

	if (AudioTrackFormat.NumChannels > 0)
	{
		Info += FString::Printf(TEXT("Stream\n"));
		Info += FString::Printf(TEXT("    Type: Audio\n"));
		Info += FString::Printf(TEXT("    Channels: %i\n"), AudioTrackFormat.NumChannels);
		Info += FString::Printf(TEXT("    Sample Rate: %i Hz\n"), AudioTrackFormat.SampleRate);
		Info += FString::Printf(TEXT("    Bits Per Sample: 32\n"));
	}

	if (VideoTrackFormat.Dim != FIntPoint::ZeroValue)
	{
		if (!Info.IsEmpty())
		{
			Info += TEXT("\n");
		}
		Info += FString::Printf(TEXT("Stream\n"));
		Info += FString::Printf(TEXT("    Type: Video\n"));
		Info += FString::Printf(TEXT("    Dimensions: %i x %i\n"), VideoTrackFormat.Dim.X, VideoTrackFormat.Dim.Y);
		Info += FString::Printf(TEXT("    Frame Rate: %g fps\n"), VideoFrameRate.AsDecimal());
	}
	return Info;
}

IMediaCache& FMediaIOCorePlayerBase::GetCache()
{
	return *this;
}

IMediaControls& FMediaIOCorePlayerBase::GetControls()
{
	return *this;
}

IMediaSamples& FMediaIOCorePlayerBase::GetSamples()
{
	return *Samples;
}

const FMediaIOCoreSamples& FMediaIOCorePlayerBase::GetSamples() const
{
	return *Samples;
}

FString FMediaIOCorePlayerBase::GetStats() const
{
	return FString();
}

IMediaTracks& FMediaIOCorePlayerBase::GetTracks()
{
	return *this;
}

FString FMediaIOCorePlayerBase::GetUrl() const
{
	return OpenUrl;
}

IMediaView& FMediaIOCorePlayerBase::GetView()
{
	return *this;
}

bool FMediaIOCorePlayerBase::Open(const FString& Url, const IMediaOptions* Options)
{
	Close();

	ITimeManagementModule::Get().GetTimedDataInputCollection().Add(this);

	OpenUrl = Url;
	bool bReadMediaOptions = ReadMediaOptions(Options);

	if (MediaIOCorePlayerDetail::CVarEnableGPUDirectInput.GetValueOnAnyThread())
	{
		CreateAndRegisterTextures(Options);
	}

	return bReadMediaOptions;
}

bool FMediaIOCorePlayerBase::Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& /*Archive*/, const FString& /*OriginalUrl*/, const IMediaOptions* /*Options*/)
{
	return false;
}

void FMediaIOCorePlayerBase::TickTimeManagement()
{
	if (bUseTimeSynchronization)
	{
		TOptional<FQualifiedFrameTime> CurrentFrameTime = FApp::GetCurrentFrameTime();
		if (CurrentFrameTime.IsSet())
		{
			if (!bWarnedIncompatibleFrameRate)
			{
				if (!CurrentFrameTime->Rate.IsMultipleOf(VideoFrameRate) && !VideoFrameRate.IsMultipleOf(CurrentFrameTime->Rate))
				{
					bWarnedIncompatibleFrameRate = true;
					UE_LOG(LogMediaIOCore, Warning, TEXT("The video's frame rate is incompatible with engine's frame rate. %s"), *OpenUrl);
				}
			}

			if (FrameDelay != 0)
			{
				FFrameTime FrameNumberVideoRate = CurrentFrameTime->ConvertTo(VideoFrameRate);
				FrameNumberVideoRate.FrameNumber -= FrameDelay;
				CurrentFrameTime->Time = FFrameRate::TransformTime(FrameNumberVideoRate, VideoFrameRate, CurrentFrameTime->Rate);
			}

			CurrentTime = FTimespan::FromSeconds(CurrentFrameTime->AsSeconds());
		}
		else
		{
			UE_LOG(LogMediaIOCore, Verbose, TEXT("The video '%s' is configured to use timecode but none is available on the engine."), *OpenUrl);
		}
	}
	else
	{
		// As default, use the App time
		CurrentTime = FTimespan::FromSeconds(GetApplicationSeconds() - TimeDelay);
	}
}

void FMediaIOCorePlayerBase::TickFetch(FTimespan DeltaTime, FTimespan Timecode)
{
	Samples->CacheSamplesState(GetTime());
}

/* IMediaCache interface
 *****************************************************************************/
bool FMediaIOCorePlayerBase::QueryCacheState(EMediaCacheState State, TRangeSet<FTimespan>& OutTimeRanges) const
{
	if (!Samples || Samples->NumVideoSamples() <= 0)
	{
		return false;
	}
	
	bool bHasQueried = false;
	if (State == EMediaCacheState::Loaded)
	{
		const FTimespan FrameDuration = FTimespan::FromSeconds(VideoFrameRate.AsInterval());
		const FTimespan NextSampleTime = Samples->GetNextVideoSampleTime();
		OutTimeRanges.Add(TRange<FTimespan>(NextSampleTime, NextSampleTime + FrameDuration * Samples->NumVideoSamples()));
		bHasQueried = true;
	}

	return bHasQueried;
}

int32 FMediaIOCorePlayerBase::GetSampleCount(EMediaCacheState State) const
{
	int32 Count = 0;
	if (State == EMediaCacheState::Loaded)
	{
		if (Samples)
		{
			Count = Samples->NumVideoSamples();
		}
	}

	return Count;
}

/* IMediaControls interface
 *****************************************************************************/

bool FMediaIOCorePlayerBase::CanControl(EMediaControl Control) const
{
	return false;
}


FTimespan FMediaIOCorePlayerBase::GetDuration() const
{
	return (CurrentState == EMediaState::Playing) ? FTimespan::MaxValue() : FTimespan::Zero();
}


float FMediaIOCorePlayerBase::GetRate() const
{
	return (CurrentState == EMediaState::Playing) ? 1.0f : 0.0f;
}


EMediaState FMediaIOCorePlayerBase::GetState() const
{
	return CurrentState;
}


EMediaStatus FMediaIOCorePlayerBase::GetStatus() const
{
	return (CurrentState == EMediaState::Preparing) ? EMediaStatus::Connecting : EMediaStatus::None;
}

TRangeSet<float> FMediaIOCorePlayerBase::GetSupportedRates(EMediaRateThinning /*Thinning*/) const
{
	TRangeSet<float> Result;
	Result.Add(TRange<float>(1.0f));
	return Result;
}

FTimespan FMediaIOCorePlayerBase::GetTime() const
{
	return CurrentTime;
}


bool FMediaIOCorePlayerBase::IsLooping() const
{
	return false; // not supported
}


bool FMediaIOCorePlayerBase::Seek(const FTimespan& Time)
{
	return false; // not supported
}


bool FMediaIOCorePlayerBase::SetLooping(bool Looping)
{
	return false; // not supported
}


bool FMediaIOCorePlayerBase::SetRate(float Rate)
{
	//Return true if a proper rate is applied. 
	return FMath::IsNearlyEqual(Rate, 1.0f);
}


/* IMediaTracks interface
 *****************************************************************************/

bool FMediaIOCorePlayerBase::GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaAudioTrackFormat& OutFormat) const
{
	if (!IsHardwareReady() || TrackIndex != 0 || FormatIndex != 0)
	{
		return false;
	}

	OutFormat = AudioTrackFormat;
	return true;
}


int32 FMediaIOCorePlayerBase::GetNumTracks(EMediaTrackType TrackType) const
{
	return 1;
}

int32 FMediaIOCorePlayerBase::GetNumTrackFormats(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return 1;
}


int32 FMediaIOCorePlayerBase::GetSelectedTrack(EMediaTrackType TrackType) const
{
	switch (TrackType)
	{
	case EMediaTrackType::Audio:
	case EMediaTrackType::Video:
		return 0;

	default:
		return INDEX_NONE;
	}
}


FText FMediaIOCorePlayerBase::GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	if (!IsHardwareReady() || TrackIndex != 0)
	{
		return FText::GetEmpty();
	}

	switch (TrackType)
	{
	case EMediaTrackType::Audio:
		return LOCTEXT("DefaultAudioTrackName", "Audio Track");

	case EMediaTrackType::Video:
		return LOCTEXT("DefaultVideoTrackName", "Video Track");

	default:
		return FText::GetEmpty();
	}

	return FText::GetEmpty();
}


int32 FMediaIOCorePlayerBase::GetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex) const
{
	if (TrackType == EMediaTrackType::Video) {
		return 0;
	}
	return INDEX_NONE;
}


FString FMediaIOCorePlayerBase::GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return FString();
}


FString FMediaIOCorePlayerBase::GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return FString();
}


bool FMediaIOCorePlayerBase::GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaVideoTrackFormat& OutFormat) const
{
	if (!IsHardwareReady() || TrackIndex != 0 || FormatIndex != 0)
	{
		return false;
	}

	OutFormat = VideoTrackFormat;
	return true;
}


bool FMediaIOCorePlayerBase::SelectTrack(EMediaTrackType TrackType, int32 TrackIndex)
{
	if (!IsHardwareReady() || TrackIndex < INDEX_NONE || TrackIndex != 0)
	{
		return false;
	}

	// Only 1 track supported
	return (TrackType == EMediaTrackType::Audio || TrackType == EMediaTrackType::Video);
}

bool FMediaIOCorePlayerBase::SetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex, int32 FormatIndex)
{
	return false;
}

bool FMediaIOCorePlayerBase::ReadMediaOptions(const IMediaOptions* Options)
{
	bUseTimeSynchronization = Options->GetMediaOption(TimeSynchronizableMedia::UseTimeSynchronizatioOption, false);
	FrameDelay = Options->GetMediaOption(TimeSynchronizableMedia::FrameDelay, (int64)0);
	TimeDelay = Options->GetMediaOption(TimeSynchronizableMedia::TimeDelay, 0.0);
	bAutoDetect = Options->GetMediaOption(TimeSynchronizableMedia::AutoDetect, false);

	{
		int32 Numerator = Options->GetMediaOption(FMediaIOCoreMediaOption::FrameRateNumerator, (int64)30);
		int32 Denominator = Options->GetMediaOption(FMediaIOCoreMediaOption::FrameRateDenominator, (int64)1);
		VideoFrameRate = FFrameRate(Numerator, Denominator);
	}
	{
		int32 ResolutionX = Options->GetMediaOption(FMediaIOCoreMediaOption::ResolutionWidth, (int64)1920);
		int32 ResolutionY = Options->GetMediaOption(FMediaIOCoreMediaOption::ResolutionHeight, (int64)1080);
		VideoTrackFormat.Dim = FIntPoint(ResolutionX, ResolutionY);
		VideoTrackFormat.FrameRates = TRange<float>(VideoFrameRate.AsDecimal());
		VideoTrackFormat.FrameRate = VideoFrameRate.AsDecimal();
		VideoTrackFormat.TypeName = Options->GetMediaOption(FMediaIOCoreMediaOption::VideoModeName, FString(TEXT("1080p30")));
	}

	//Setup base sampling settings
	BaseSettings.EvaluationType = bUseTimeSynchronization ? ETimedDataInputEvaluationType::Timecode : ETimedDataInputEvaluationType::PlatformTime;
	BaseSettings.FrameRate = VideoFrameRate;
	BaseSettings.PlayerTimeOffset = MediaIOCorePlayerDetail::HighPerformanceClockDelta;
	BaseSettings.AbsoluteMaxBufferSize = MediaIOCorePlayerDetail::CVarMediaIOMaxBufferSize.GetValueOnGameThread();

	return true;
}

double FMediaIOCorePlayerBase::GetApplicationSeconds()
{
	return MediaIOCorePlayerDetail::HighPerformanceClockDelta + FApp::GetCurrentTime();
}

double FMediaIOCorePlayerBase::GetPlatformSeconds()
{
	return MediaIOCorePlayerDetail::HighPerformanceClockDelta + FPlatformTime::Seconds();
}

bool FMediaIOCorePlayerBase::IsTimecodeLogEnabled()
{
#if !(UE_BUILD_SHIPPING)
	return MediaIOCorePlayerDetail::bLogTimecode;
#else
	return false;
#endif
}

bool FMediaIOCorePlayerBase::CanUseGPUTextureTransfer()
{
	return GPUTextureTransfer && MediaIOCorePlayerDetail::CVarEnableGPUDirectInput.GetValueOnAnyThread();
}

void FMediaIOCorePlayerBase::OnSampleDestroyed(TRefCountPtr<FRHITexture> InTexture)
{
	FScopeLock ScopeLock(&TexturesCriticalSection);
	if (InTexture && RegisteredTextures.Contains(InTexture))
	{
		Textures.Add(InTexture);
	}
}

void FMediaIOCorePlayerBase::RegisterSampleBuffer(const TSharedPtr<FMediaIOCoreTextureSampleBase>& InSample)
{
	if (!RegisteredBuffers.Contains(InSample->GetMutableBuffer()))
	{
		RegisteredBuffers.Add(InSample->GetMutableBuffer());

		// Enqueue buffer registration on the render  thread to ensure it happens before the call to TransferTexture
		ENQUEUE_RENDER_COMMAND(RegisterSampleBuffers)([this, InSample](FRHICommandList& CommandList)
			{
				if (!InSample->GetMutableBuffer())
				{
					UE_LOG(LogMediaIOCore, Error, TEXT("A buffer was not available while performing a gpu texture transfer."));
					return;
				}

				if (!InSample->GetTexture())
				{
					UE_LOG(LogMediaIOCore, Error, TEXT("A texture was not available while performing a gpu texture transfer."));
					return;
				}

				const uint32 TextureWidth = InSample->GetTexture()->GetDesc().Extent.X;
				const uint32 TextureHeight = InSample->GetTexture()->GetDesc().Extent.Y;
				uint32 TextureStride = TextureWidth * 4;

				EPixelFormat Format = InSample->GetTexture()->GetFormat();
				if (Format == PF_R32G32B32A32_UINT)
				{
					TextureStride *= 4;
				}

				UE_LOG(LogMediaIOCore, Verbose, TEXT("Registering buffer %u"), reinterpret_cast<uintptr_t>(InSample->GetMutableBuffer()));
				UE::GPUTextureTransfer::FRegisterDMABufferArgs Args;
				Args.Buffer = InSample->GetMutableBuffer();
				Args.Width = TextureWidth;
				Args.Height = TextureHeight;
				Args.Stride = TextureStride;
				Args.PixelFormat = Format == PF_B8G8R8A8 ? UE::GPUTextureTransfer::EPixelFormat::PF_8Bit : UE::GPUTextureTransfer::EPixelFormat::PF_10Bit;
				GPUTextureTransfer->RegisterBuffer(Args);
			});
	}
	else
	{
		UE_LOG(LogMediaIOCore, VeryVerbose, TEXT("Buffer already registered: %u"), reinterpret_cast<uintptr_t>(InSample->GetMutableBuffer()));
	}
}

void FMediaIOCorePlayerBase::UnregisterSampleBuffers()
{
	ENQUEUE_RENDER_COMMAND(UnregisterSampleBuffers)([TextureTransfer = GPUTextureTransfer, BuffersToUnregister = RegisteredBuffers](FRHICommandList& CommandList)
		{
			if (TextureTransfer)
			{
				for (void* RegisteredBuffer : BuffersToUnregister)
				{
					TextureTransfer->UnregisterBuffer(RegisteredBuffer);
				}
			}
		});

	RegisteredBuffers.Reset();
}

void FMediaIOCorePlayerBase::CreateAndRegisterTextures(const IMediaOptions* Options)
{
	ENQUEUE_RENDER_COMMAND(MediaIOCorePlayerCreateTextures)(
		[this, Options](FRHICommandListImmediate& CommandList)
		{
			if (!GPUTextureTransfer)
			{
				UE_LOG(LogMediaIOCore, Error, TEXT("Could not register textures for gpu texture transfer, as the GPUTextureTransfer object was null."));
				return;
			}

			for (uint8 Index = 0; Index < GetNumVideoFrameBuffers(); Index++)
			{
				TRefCountPtr<FRHITexture> RHITexture;
				const FString TextureName = FString::Printf(TEXT("FMediaIOCorePlayerTexture %d"), Index);
				FRHIResourceCreateInfo CreateInfo(*TextureName);

				uint32 TextureWidth = VideoTrackFormat.Dim.X;
				const uint32 TextureHeight = VideoTrackFormat.Dim.Y;
				uint32 Stride = TextureWidth;
				EPixelFormat InputFormat = EPixelFormat::PF_Unknown;

				EMediaIOCoreColorFormat ColorFormat = GetColorFormat();

				if (ColorFormat == EMediaIOCoreColorFormat::YUV8)
				{
					InputFormat = EPixelFormat::PF_B8G8R8A8;
					TextureWidth /= 2;
					Stride = TextureWidth * 4;
				}
				else if (ColorFormat == EMediaIOCoreColorFormat::YUV10)
				{
					InputFormat = EPixelFormat::PF_R32G32B32A32_UINT;
					TextureWidth /= 6;
					Stride = TextureWidth * 16;
				}
				else
				{
					checkf(false, TEXT("Format not supported"));
				}

				ETextureCreateFlags CreateFlags = ETextureCreateFlags::Shared;
				if (RHIGetInterfaceType() == ERHIInterfaceType::Vulkan)
				{
					CreateFlags = ETextureCreateFlags::External;
				}

				FRHITextureCreateDesc CreateDesc = FRHITextureCreateDesc::Create2D(*TextureName)
					.SetExtent(TextureWidth, TextureHeight)
					.SetFormat(InputFormat)
					.SetClearValue(FClearValueBinding::White)
					.SetFlags(CreateFlags | ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource)
					.SetInitialState(ERHIAccess::SRVMask);

				RHITexture = RHICreateTexture(CreateDesc);

				UE_LOG(LogMediaIOCore, Verbose, TEXT("Registering texture %u"), reinterpret_cast<uintptr_t>(RHITexture->GetNativeResource()));

				UE::GPUTextureTransfer::FRegisterDMATextureArgs Args;
				Args.RHITexture = RHITexture.GetReference();
				Args.Stride = Stride;
				Args.Width = TextureWidth;
				Args.Height = TextureHeight;
				Args.RHIResourceMemory = nullptr;
				Args.PixelFormat = ColorFormat == EMediaIOCoreColorFormat::YUV8 ? UE::GPUTextureTransfer::EPixelFormat::PF_8Bit : UE::GPUTextureTransfer::EPixelFormat::PF_10Bit;

				GPUTextureTransfer->RegisterTexture(Args);
				RegisteredTextures.Add(RHITexture);

				{
					FScopeLock Lock(&TexturesCriticalSection);
					Textures.Add(RHITexture);
				}
			}
		});

	FRenderCommandFence RenderFence;
	RenderFence.BeginFence();
	RenderFence.Wait();
}

void FMediaIOCorePlayerBase::UnregisterTextures()
{
	// Keep a copy of the textures to make sure they were not destoryed before this gets executed.
	ENQUEUE_RENDER_COMMAND(UnregisterSampleBuffers)([TextureTransfer = GPUTextureTransfer, TexturesToUnregister = RegisteredTextures](FRHICommandList& CommandList)
		{
			if (TextureTransfer)
			{
				for (const TRefCountPtr<FRHITexture>& RHITexture : TexturesToUnregister)
				{
					UE_LOG(LogMediaIOCore, Verbose, TEXT("Unregistering texture %u"), reinterpret_cast<uintptr_t>(RHITexture->GetNativeResource()));
					TextureTransfer->UnregisterTexture(RHITexture.GetReference());
				}
			}
		});

	RegisteredTextures.Reset();

	{
		FScopeLock Lock(&TexturesCriticalSection);
		Textures.Reset();
	}
}

void FMediaIOCorePlayerBase::PreGPUTransfer(const TSharedPtr<FMediaIOCoreTextureSampleBase>& InSample)
{
	// Register the buffer and the texture
	TRefCountPtr<FRHITexture> Texture = Textures.Pop();
	if (Texture && RegisteredTextures.Contains(Texture))
	{
		InSample->SetTexture(Texture);
		InSample->SetDestructionCallback([MediaPlayerWeakPtr = TWeakPtr<FMediaIOCorePlayerBase>(AsShared())](TRefCountPtr<FRHITexture> InTexture)
		{
			if (TSharedPtr<FMediaIOCorePlayerBase> MediaPlayerPtr = MediaPlayerWeakPtr.Pin())
			{
				MediaPlayerPtr->OnSampleDestroyed(InTexture);
			}
		});

		RegisterSampleBuffer(InSample);
	}
	else
	{
		UE_LOG(LogMediaIOCore, Display, TEXT("Unregistered texture %u encountered while doing a gpu texture transfer."), Texture ? reinterpret_cast<uintptr_t>(Texture->GetNativeResource()) : 0);
	}
}

void FMediaIOCorePlayerBase::ExecuteGPUTransfer(const TSharedPtr<FMediaIOCoreTextureSampleBase>& InSample)
{
	ENQUEUE_RENDER_COMMAND(StartGPUTextureTransferCopy)(
		[MediaPlayerWeakPtr = TWeakPtr<FMediaIOCorePlayerBase>(AsShared()), InSample](FRHICommandListImmediate& RHICmdList)
	{
		if (InSample->GetTexture())
		{
			if (TSharedPtr<FMediaIOCorePlayerBase> MediaPlayerPtr = MediaPlayerWeakPtr.Pin();
				MediaPlayerPtr != nullptr && MediaPlayerPtr->GPUTextureTransfer != nullptr)
			{
				void* Buffer = InSample->GetMutableBuffer();
				UE_LOG(LogMediaIOCore, Verbose, TEXT("Starting Transfer with buffer %u"), reinterpret_cast<uintptr_t>(Buffer));
				MediaPlayerPtr->GPUTextureTransfer->TransferTexture(Buffer, InSample->GetTexture(), UE::GPUTextureTransfer::ETransferDirection::CPU_TO_GPU);
				MediaPlayerPtr->GPUTextureTransfer->BeginSync((InSample)->GetMutableBuffer(), UE::GPUTextureTransfer::ETransferDirection::CPU_TO_GPU);
				MediaPlayerPtr->AddVideoSample(InSample.ToSharedRef());
				MediaPlayerPtr->GPUTextureTransfer->EndSync(InSample->GetMutableBuffer());
			}
		}
	});
}

TArray<ITimedDataInputChannel*> FMediaIOCorePlayerBase::GetChannels() const
{
	return Channels;
}

FText FMediaIOCorePlayerBase::GetDisplayName() const
{
	IMediaModule* MediaModule = FModuleManager::Get().GetModulePtr<IMediaModule>("Media");
	if (!MediaModule)
	{
		return LOCTEXT("InvalidMediaModule", "NoMediaModule");
	}

	IMediaPlayerFactory* PlayerFactory = MediaModule->GetPlayerFactory(GetPlayerPluginGUID());
	if (PlayerFactory == nullptr)
	{
		return LOCTEXT("InvalidPlayerFactory", "NoPlayerFactory");
	}

	return FText::Format(LOCTEXT("PlayerDisplayName", "{0} - {1}"), FText::FromName(PlayerFactory->GetPlayerName()), FText::FromString(GetUrl()));
}
ETimedDataInputEvaluationType FMediaIOCorePlayerBase::GetEvaluationType() const
{
	if (bUseTimeSynchronization)
	{
		return ETimedDataInputEvaluationType::Timecode;
	}
	else
	{
		return ETimedDataInputEvaluationType::PlatformTime;
	}
}

void FMediaIOCorePlayerBase::SetEvaluationType(ETimedDataInputEvaluationType Evaluation)
{
	bool bNewEvaluation = false;
	switch (Evaluation)
	{
	case ETimedDataInputEvaluationType::Timecode:
		bNewEvaluation = true;
		break;
	case ETimedDataInputEvaluationType::PlatformTime:
	case ETimedDataInputEvaluationType::None:
	default:
		bNewEvaluation = false;
		break;
	}

	if (bNewEvaluation != bUseTimeSynchronization)
	{
		bUseTimeSynchronization = bNewEvaluation;
		BaseSettings.EvaluationType = bUseTimeSynchronization ? ETimedDataInputEvaluationType::Timecode : ETimedDataInputEvaluationType::PlatformTime;
		SetupSampleChannels();
	}
}

double FMediaIOCorePlayerBase::GetEvaluationOffsetInSeconds() const
{
	if (bUseTimeSynchronization)
	{
		return ITimedDataInput::ConvertFrameOffsetInSecondOffset(FrameDelay, VideoFrameRate);
	}
	else
	{
		return TimeDelay;
	}
}

void FMediaIOCorePlayerBase::SetEvaluationOffsetInSeconds(double Offset)
{
	if (bUseTimeSynchronization)
	{
		//Media doesn't support subframes playback (interpolation between frames) so offsets are always in full frame
		const double FrameOffset = ITimedDataInput::ConvertSecondOffsetInFrameOffset(Offset, VideoFrameRate);
		FrameDelay = FMath::CeilToInt(FrameOffset);
	}
	else
	{
		TimeDelay = Offset;
	}
}

FFrameRate FMediaIOCorePlayerBase::GetFrameRate() const
{
	return VideoFrameRate;
}

bool FMediaIOCorePlayerBase::IsDataBufferSizeControlledByInput() const
{
	//Each channel (audio, video, etc...) can have a different size
	return false;
}

void FMediaIOCorePlayerBase::AddChannel(ITimedDataInputChannel* Channel)
{
	Channels.AddUnique(Channel);
}

void FMediaIOCorePlayerBase::RemoveChannel(ITimedDataInputChannel* Channel)
{
	Channels.Remove(Channel);
}

bool FMediaIOCorePlayerBase::SupportsSubFrames() const
{
	return false;
}

#undef LOCTEXT_NAMESPACE

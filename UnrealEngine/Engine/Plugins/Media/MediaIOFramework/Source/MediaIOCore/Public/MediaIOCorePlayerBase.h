// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "IMediaCache.h"
#include "IMediaControls.h"
#include "IMediaPlayer.h"
#include "IMediaTracks.h"
#include "IMediaView.h"
#include "ITimedDataInput.h"

#include "MediaIOCoreDefinitions.h"
#include "MediaIOCoreSamples.h"
#include "MediaIOCoreSampleContainer.h"


#include "HAL/CriticalSection.h"
#include "Misc/CoreMisc.h"
#include "Misc/FrameRate.h"

class FMediaIOCoreAudioSampleBase;
class FMediaIOCoreBinarySampleBase;
class FMediaIOCoreCaptionSampleBase;
class FMediaIOCoreSamples;
class FMediaIOCoreSubtitleSampleBase;
class FMediaIOCoreTextureSampleBase;
class FMediaIOCoreTextureSampleConverter;
class IMediaEventSink;

enum class EMediaTextureSampleFormat;

enum class EMediaIOCoreColorFormat : uint8
{
	YUV8,
	YUV10
};

struct MEDIAIOCORE_API FMediaIOCoreMediaOption
{
	static const FName FrameRateNumerator;
	static const FName FrameRateDenominator;
	static const FName ResolutionWidth;
	static const FName ResolutionHeight;
	static const FName VideoModeName;
};

namespace UE::GPUTextureTransfer
{
	using TextureTransferPtr = TSharedPtr<class ITextureTransfer>;
}

namespace UE::MediaIOCore
{
	class FDeinterlacer;
	struct FVideoFrame;
}
 
/**
 * Implements a base player for hardware IO cards. 
 *
 * The processing of metadata and video frames is delayed until the fetch stage
 * (TickFetch) in order to increase the window of opportunity for receiving
 * frames for the current render frame time code.
 *
 * Depending on whether the media source enables time code synchronization,
 * the player's current play time (CurrentTime) is derived either from the
 * time codes embedded in frames or from the Engine's global time code.
 */
class MEDIAIOCORE_API FMediaIOCorePlayerBase
	: public IMediaPlayer
	, protected IMediaCache
	, protected IMediaControls
	, protected IMediaTracks
	, protected IMediaView
	, public ITimedDataInput
	, public TSharedFromThis<FMediaIOCorePlayerBase>
{
public:

	/**
	 * Create and initialize a new instance.
	 *
	 * @param InEventSink The object that receives media events from this player.
	 */
	FMediaIOCorePlayerBase(IMediaEventSink& InEventSink);

	/** Virtual destructor. */
	virtual ~FMediaIOCorePlayerBase();

public:

	//~ IMediaPlayer interface

	virtual void Close() override;
	virtual bool Open(const FString& Url, const IMediaOptions* Options) override;
	virtual bool Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl, const IMediaOptions* Options) override;

	FString GetInfo() const;
	virtual IMediaCache& GetCache() override;
	virtual IMediaControls& GetControls() override;
	virtual IMediaSamples& GetSamples() override;
	const FMediaIOCoreSamples& GetSamples() const;
	virtual FString GetStats() const override;
	virtual IMediaTracks& GetTracks() override;
	virtual FString GetUrl() const override;
	virtual IMediaView& GetView() override;
	virtual void TickTimeManagement();
	virtual void TickFetch(FTimespan DeltaTime, FTimespan Timecode) override;

public:
	//~ IMediaCache interface
	
	virtual bool QueryCacheState(EMediaCacheState State, TRangeSet<FTimespan>& OutTimeRanges) const override;
	virtual int32 GetSampleCount(EMediaCacheState State) const override;

protected:

	//~ IMediaControls interface

	virtual bool CanControl(EMediaControl Control) const override;
	virtual FTimespan GetDuration() const override;
	virtual float GetRate() const override;
	virtual EMediaState GetState() const override;
	virtual EMediaStatus GetStatus() const override;
	virtual TRangeSet<float> GetSupportedRates(EMediaRateThinning Thinning) const override;
	virtual FTimespan GetTime() const override;
	virtual bool IsLooping() const override;
	virtual bool Seek(const FTimespan& Time) override;
	virtual bool SetLooping(bool Looping) override;
	virtual bool SetRate(float Rate) override;

protected:

	//~ IMediaTracks interface

	virtual bool GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaAudioTrackFormat& OutFormat) const override;
	virtual int32 GetNumTracks(EMediaTrackType TrackType) const override;
	virtual int32 GetNumTrackFormats(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual int32 GetSelectedTrack(EMediaTrackType TrackType) const override;
	virtual FText GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual int32 GetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual FString GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual FString GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual bool GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaVideoTrackFormat& OutFormat) const override;
	virtual bool SelectTrack(EMediaTrackType TrackType, int32 TrackIndex) override;
	virtual bool SetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex, int32 FormatIndex) override;

public:
	//~ ITimedDataInput interface
	virtual FText GetDisplayName() const override;
	virtual TArray<ITimedDataInputChannel*> GetChannels() const override;
	virtual ETimedDataInputEvaluationType GetEvaluationType() const override;
	virtual void SetEvaluationType(ETimedDataInputEvaluationType Evaluation) override;
	virtual double GetEvaluationOffsetInSeconds() const override;
	virtual void SetEvaluationOffsetInSeconds(double Offset) override;
	virtual FFrameRate GetFrameRate() const override;
	virtual bool IsDataBufferSizeControlledByInput() const override;
	virtual void AddChannel(ITimedDataInputChannel* Channel) override;
	virtual void RemoveChannel(ITimedDataInputChannel * Channel) override;
	virtual bool SupportsSubFrames() const override;

public:

	/**
	 * Just in time sample rendering. This method is responsible for late sample picking,
	 * then rendering it into the proxy sample provided.
	 */
	bool JustInTimeSampleRender_RenderThread(TSharedPtr<FMediaIOCoreTextureSampleBase>& JITRProxySample);

protected:

	/** Internal API to push an audio sample to the internal samples pool */
	void AddAudioSample(const TSharedRef<FMediaIOCoreAudioSampleBase>& Sample);

	/** Internal API to push a caption sample to the internal samples pool */
	void AddCaptionSample(const TSharedRef<FMediaIOCoreCaptionSampleBase>& Sample);

	/** Internal API to push a metadata sample to the internal samples pool */
	void AddMetadataSample(const TSharedRef<FMediaIOCoreBinarySampleBase>& Sample);

	/** Internal API to push a subtitle sample to the internal samples pool */
	void AddSubtitleSample(const TSharedRef<FMediaIOCoreSubtitleSampleBase>& Sample);

	/** Internal API to push a video sample to the internal samples pool */
	void AddVideoSample(const TSharedRef<FMediaIOCoreTextureSampleBase>& Sample);

protected:
	/** Is the IO hardware/device ready to be used. */
	virtual bool IsHardwareReady() const = 0;

	/** Return true if the options combination are valid. */
	virtual bool ReadMediaOptions(const IMediaOptions* Options);

	/**
	 * Allows children to notify when video format is known after auto-detection. The main purpose
	 * is to start intialization of the DMA textures at the right moment.
	 */
	void NotifyVideoFormatDetected();

	/** Get the application time with a delta to represent the actual system time. Use instead of FApp::GetCurrentTime. */
	static double GetApplicationSeconds();

	/** Get the platform time with a delta to represent the actual system time. Use instead of FApp::Seconds. */
	static double GetPlatformSeconds();

	/** Log the timecode when a frame is received. */
	static bool IsTimecodeLogEnabled();
	
	/** 
	 * Setup settings for the different kind of supported data channels. 
	 * PlayerBase will setup the common settings
	 */
	virtual void SetupSampleChannels() = 0;

	/**
	 * Get the number of video frames to buffer.
	 */
	virtual uint32 GetNumVideoFrameBuffers() const
	{ 
		return 1;
	}

	virtual EMediaIOCoreColorFormat GetColorFormat() const 
	{ 
		return EMediaIOCoreColorFormat::YUV8;
	}

	/** Called after fast GPUDirect texture transferring is finished */
	virtual void AddVideoSampleAfterGPUTransfer_RenderThread(const TSharedRef<FMediaIOCoreTextureSampleBase>& Sample);

	/** Whether fast GPU transferring is available */
	virtual bool CanUseGPUTextureTransfer() const;

	/** Whether a texture availabled for GPU transfer */
	bool HasTextureAvailableForGPUTransfer() const;

	/** Whether JITR is available and active */
	virtual bool IsJustInTimeRenderingEnabled() const;

	/** Factory method to generate texture samples. */
	virtual TSharedPtr<FMediaIOCoreTextureSampleBase> AcquireTextureSample_AnyThread() const = 0;

	/** Deinterlace a video frame. Only valid to call when the video stream is open. */
	TArray<TSharedRef<FMediaIOCoreTextureSampleBase>> Deinterlace(const UE::MediaIOCore::FVideoFrame& InVideoFrame) const;

protected:

	/** Acquire a proxy sample for JITR. That sample must be initialized for JITR. */
	virtual TSharedPtr<FMediaIOCoreTextureSampleBase> AcquireJITRProxySampleInitialized();

	/** Factory method to create sample converters. */
	virtual TSharedPtr<FMediaIOCoreTextureSampleConverter> CreateTextureSampleConverter() const;

	/** Pick a sample to render */
	TSharedPtr<FMediaIOCoreTextureSampleBase> PickSampleToRender_RenderThread(const TSharedPtr<FMediaIOCoreTextureSampleBase>& JITRProxySample);
	TSharedPtr<FMediaIOCoreTextureSampleBase> PickSampleToRenderForLatest_RenderThread(const TSharedPtr<FMediaIOCoreTextureSampleBase>& JITRProxySample);
	TSharedPtr<FMediaIOCoreTextureSampleBase> PickSampleToRenderForTimeSynchronized_RenderThread(const TSharedPtr<FMediaIOCoreTextureSampleBase>& JITRProxySample);
	TSharedPtr<FMediaIOCoreTextureSampleBase> PickSampleToRenderFramelocked_RenderThread(const TSharedPtr<FMediaIOCoreTextureSampleBase>& JITRProxySample);

	/**
	 * A wrapper method responsible for transferring of the sample textures into GPU memory based
	 * on the current settings and hardware capabilities.
	 */
	void TransferTexture_RenderThread(const TSharedPtr<FMediaIOCoreTextureSampleBase>& Sample, const TSharedPtr<FMediaIOCoreTextureSampleBase>& JITRProxySample);

protected:
	/** Url used to open the media player. */
	FString OpenUrl;

	/** format of the video. */
	FMediaVideoTrackFormat VideoTrackFormat;

	/** format of the audio. */
	FMediaAudioTrackFormat AudioTrackFormat;

	/** Current state of the media player. */
	EMediaState CurrentState = EMediaState::Closed;

	/** Current playback time. */
	FTimespan CurrentTime = FTimespan::Zero();

	/** The media event handler. */
	IMediaEventSink& EventSink;

	/** Video frame rate in the last received sample. */
	FFrameRate VideoFrameRate = { 30, 1 };

	/** The media sample cache. */
	const TUniquePtr<FMediaIOCoreSamples> Samples;

	/** Sample evaluation type. */
	EMediaIOSampleEvaluationType EvaluationType = EMediaIOSampleEvaluationType::PlatformTime;

	/** Warn when the video frame rate is not the same as the engine's frame rate. */
	bool bWarnedIncompatibleFrameRate = false;

	/** Whether we are using autodetection. */
	bool bAutoDetect = false;

	/** When using Time Synchronization (TC synchronization), how many frame back of a delay would you like. */
	int32 FrameDelay = 0;

	/** When not using Time Synchronization (use computer time), how many sec back of a delay would you like. */
	double TimeDelay = 0.0;

	/** Previous frame Timespan */
	FTimespan PreviousFrameTimespan;

	/** Timed Data Input handler */
	TArray<ITimedDataInputChannel*> Channels;

	/** Base set of settings to start from when setuping channels */
	FMediaIOSamplingSettings BaseSettings;

	/** Open color IO conversion data. */
	TSharedPtr<struct FOpenColorIOColorConversionSettings> OCIOSettings;

private:
	void OnSampleDestroyed(TRefCountPtr<FRHITexture> InTexture);
	void RegisterSampleBuffer(const TSharedPtr<FMediaIOCoreTextureSampleBase>& InSample);
	void UnregisterSampleBuffers();
	void CreateAndRegisterTextures();
	void UnregisterTextures();
	void PreGPUTransfer(const TSharedPtr<FMediaIOCoreTextureSampleBase>& InSample);
	void PreGPUTransferJITR(const TSharedPtr<FMediaIOCoreTextureSampleBase>& InSample, const TSharedPtr<FMediaIOCoreTextureSampleBase>& InJITRProxySample);
	void ExecuteGPUTransfer(const TSharedPtr<FMediaIOCoreTextureSampleBase>& InSample);

	/** Get the right samples buffer for pushing samples depending on if we're in JITR mode or not. */
	FMediaIOCoreSamples& GetSamples_Internal();

	/** GPU Texture transfer object */
	UE::GPUTextureTransfer::TextureTransferPtr GPUTextureTransfer;

	/** Buffers Registered with GPU Texture Transfer */
	TSet<void*> RegisteredBuffers;

	/** Textures registerd with GPU Texture transfer. */
	TSet<TRefCountPtr<FRHITexture>> RegisteredTextures;

	/** Pool of textures available for GPU Texture transfer. */
	TArray<TRefCountPtr<FRHITexture>> Textures;

	/** Critical section to control access to the pool. */
	mutable FCriticalSection TexturesCriticalSection;

	/** Utility object to handle deinterlacing. */
	TSharedPtr<UE::MediaIOCore::FDeinterlacer> Deinterlacer;

private:

	/** Utility function to log various sample events (i.e. received/transferred/picked) */
	void LogBookmark(const FString& Text, const TSharedRef<IMediaTextureSample>& Sample);

private:

	/** Special JITR sample container, overrides FetchVideo to return a ProxySample that will be populated by JustInTimeSampleRender_RenderThread. */
	class FJITRMediaTextureSamples
		: public FMediaIOCoreSamples
	{
	public:
		FJITRMediaTextureSamples() = default;
		FJITRMediaTextureSamples(const FJITRMediaTextureSamples&) = delete;
		FJITRMediaTextureSamples& operator=(const FJITRMediaTextureSamples&) = delete;
	public:
		//~ Begin IMediaSamples interface
		virtual bool FetchVideo(TRange<FTimespan> TimeRange, TSharedPtr<IMediaTextureSample>& OutSample) override;
		virtual bool PeekVideoSampleTime(FMediaTimeStamp& TimeStamp) override;
		virtual void FlushSamples() override;
		//~ End IMediaSamples interface

	public:
		// JITR sample
		TSharedPtr<FMediaIOCoreTextureSampleBase> ProxySample;
	};

	/** Is Just-In-Time Rendering enabled */
	bool bJustInTimeRender = false;

	/** Whether media playback should be framelocked to the engine's timecode */
	bool bFramelock = false;

	/** JITR samples proxy */
	const TUniquePtr<FJITRMediaTextureSamples> JITRSamples;

	/** Used to ensure that JIT rendering is executed only once per frame */
	uint64 LastEngineRTFrameThatUpdatedJustInTime = TNumericLimits<uint64>::Max();
};

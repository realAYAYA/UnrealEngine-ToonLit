// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "IMediaCache.h"
#include "IMediaControls.h"
#include "IMediaPlayer.h"
#include "IMediaTracks.h"
#include "IMediaView.h"
#include "ITimedDataInput.h"

#include "HAL/CriticalSection.h"
#include "MediaIOCoreSampleContainer.h"
#include "MediaIOCoreTextureSampleBase.h"
#include "Misc/CoreMisc.h"

#include "Misc/FrameRate.h"

class FMediaIOCoreSamples;
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
	static TAutoConsoleVariable<int32> CVarFlipInterlaceFields;
	static TAutoConsoleVariable<int32> CVarExperimentalFieldFlipFix;
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

protected:
	/** Is the IO hardware/device ready to be used. */
	virtual bool IsHardwareReady() const = 0;

	/** Return true if the options combination are valid. */
	virtual bool ReadMediaOptions(const IMediaOptions* Options);

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

	virtual void AddVideoSample(const TSharedRef<FMediaIOCoreTextureSampleBase>& InSample)
	{
	}

	virtual bool CanUseGPUTextureTransfer();

	void OnSampleDestroyed(TRefCountPtr<FRHITexture> InTexture);
	void RegisterSampleBuffer(const TSharedPtr<FMediaIOCoreTextureSampleBase>& InSample);
	void UnregisterSampleBuffers();
	void CreateAndRegisterTextures(const IMediaOptions* Options);
	void UnregisterTextures();
	void PreGPUTransfer(const TSharedPtr<FMediaIOCoreTextureSampleBase>& InSample);
	void ExecuteGPUTransfer(const TSharedPtr<FMediaIOCoreTextureSampleBase>& InSample);

protected:
	/** Critical section for synchronizing access to receiver and sinks. */
	FCriticalSection CriticalSection;

	/** Url used to open the media player. */
	FString OpenUrl;

	/** format of the video. */
	FMediaVideoTrackFormat VideoTrackFormat;
	
	/** format of the audio. */
	FMediaAudioTrackFormat AudioTrackFormat;

	/** Current state of the media player. */
	EMediaState CurrentState;

	/** Current playback time. */
	FTimespan CurrentTime;

	/** The media event handler. */
	IMediaEventSink& EventSink;

	/** Video frame rate in the last received sample. */
	FFrameRate VideoFrameRate;

	/** The media sample cache. */
	FMediaIOCoreSamples* Samples;

	/** Whether to use the Synchronization Time module as time source. */
	bool bUseTimeSynchronization;

	/** Warn when the video frame rate is not the same as the engine's frame rate. */
	bool bWarnedIncompatibleFrameRate;

	/** Whether we are using autodetection. */
	bool bAutoDetect;

	/** When using Time Synchronization (TC synchronization), how many frame back of a delay would you like. */
	int32 FrameDelay;

	/** When not using Time Synchronization (use computer time), how many sec back of a delay would you like. */
	double TimeDelay;

	/** Previous frame Timespan */
	FTimespan PreviousFrameTimespan;

	/** Timed Data Input handler */
	TArray<ITimedDataInputChannel*> Channels;
	
	/** Base set of settings to start from when setuping channels */
	FMediaIOSamplingSettings BaseSettings;

	FCriticalSection TexturesCriticalSection;

	/** Pool of textures registerd with GPU Texture transfer. */
	TArray<TRefCountPtr<FRHITexture>> Textures;

private:
	/** GPU Texture transfer object */
	UE::GPUTextureTransfer::TextureTransferPtr GPUTextureTransfer;
	/** Buffers Registered with GPU Texture Transfer */
	TSet<void*> RegisteredBuffers;
	/** Pool of textures registerd with GPU Texture transfer. */
	TSet<TRefCountPtr<FRHITexture>> RegisteredTextures;

};

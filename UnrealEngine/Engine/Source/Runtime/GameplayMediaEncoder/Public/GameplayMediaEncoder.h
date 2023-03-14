// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Logging/LogMacros.h"
#include "AudioMixerDevice.h"

#include "RHI.h"
#include "RHIResources.h"

#include "HAL/Thread.h"

THIRD_PARTY_INCLUDES_START
#pragma warning(push)
#pragma warning(disable: 4596 6319 6323)

#pragma warning(pop)
THIRD_PARTY_INCLUDES_END

#include "AudioEncoder.h"
#include "VideoEncoder.h"
#include "MediaPacket.h"
#include "VideoEncoderInput.h"

class SWindow;

class IGameplayMediaEncoderListener
{
public:
	virtual void OnMediaSample(const AVEncoder::FMediaPacket& Sample) = 0;
};

class GAMEPLAYMEDIAENCODER_API FGameplayMediaEncoder final : private ISubmixBufferListener, public AVEncoder::IAudioEncoderListener
{
public:

	/**
	 * Get the singleton
	 */
	static FGameplayMediaEncoder* Get();

	~FGameplayMediaEncoder();

	bool RegisterListener(IGameplayMediaEncoderListener* Listener);
	void UnregisterListener(IGameplayMediaEncoderListener* Listener);

	void SetVideoBitrate(uint32 Bitrate);
	void SetVideoFramerate(uint32 Framerate);

	///**
	// * Returns the audio codec name and configuration
	// */
	//TPair<FString, AVEncoder::FAudioConfig> GetAudioConfig() const;
	//TPair<FString, AVEncoder::FVideoConfig> GetVideoConfig() const;

	bool Initialize();
	void Shutdown();
	bool Start();
	void Stop();

	static void InitializeCmd()
	{
		// We call Get(), so it creates the singleton
		Get()->Initialize();
	}

	static void ShutdownCmd()
	{
		// We call Get(), so it creates the singleton
		Get()->Shutdown();
	}

	static void StartCmd()
	{
		// We call Get(), so it creates the singleton
		Get()->Start();
	}

	static void StopCmd()
	{
		// We call Get(), so it creates the singleton
		Get()->Stop();
	}

	AVEncoder::FAudioConfig GetAudioConfig() const;
	AVEncoder::FVideoConfig GetVideoConfig() const { return VideoConfig; }

private:

	// Private to control how our single instance is created
	FGameplayMediaEncoder();

	// Returns how long it has been recording for.
	FTimespan GetMediaTimestamp() const;

	// Back buffer capture
	void OnFrameBufferReady(SWindow& SlateWindow, const FTexture2DRHIRef& FrameBuffer);
	// ISubmixBufferListener interface
	void OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData, int32 NumSamples, int32 NumChannels, const int32 SampleRate, double AudioClock) override;

	void ProcessAudioFrame(const float* AudioData, int32 NumSamples, int32 NumChannels, int32 SampleRate);
	void ProcessVideoFrame(const FTexture2DRHIRef& FrameBuffer);

	void UpdateVideoConfig();

	void OnEncodedAudioFrame(const AVEncoder::FMediaPacket& Packet) override;
	void OnEncodedVideoFrame(uint32 LayerIndex, const TSharedPtr<AVEncoder::FVideoEncoderInputFrame> Frame, const AVEncoder::FCodecPacket& Packet);

	TSharedPtr<AVEncoder::FVideoEncoderInputFrame> ObtainInputFrame();
	void CopyTexture(const FTexture2DRHIRef& SourceTexture, FTexture2DRHIRef& DestinationTexture) const;

	void FloatToPCM16(float const* floatSamples, int32 numSamples, TArray<int16>& out) const;

	FCriticalSection ListenersCS;
	TArray<IGameplayMediaEncoderListener*> Listeners;

	FCriticalSection AudioProcessingCS;
	FCriticalSection VideoProcessingCS;

	TUniquePtr<AVEncoder::FAudioEncoder> AudioEncoder;

	AVEncoder::FVideoConfig VideoConfig;

	TUniquePtr<AVEncoder::FVideoEncoder> VideoEncoder;
	TSharedPtr<AVEncoder::FVideoEncoderInput> VideoEncoderInput;

	uint64 NumCapturedFrames = 0;
	FTimespan StartTime = 0;

	// Instead of using the AudioClock parameter ISubmixBufferListener::OnNewSubmixBuffer gives us, we calculate our own, by
	// advancing it as we receive more data.
	// This is so that we can adjust the clock if things get out of sync, such as if we break into the debugger.
	double AudioClock = 0;

	FTimespan LastVideoInputTimestamp = 0;

	bool bAudioFormatChecked = false;
	bool bDoFrameSkipping = false;

	friend class FGameplayMediaEncoderModule;
	static FGameplayMediaEncoder* Singleton;

	// live streaming: quality adaptation to available uplink b/w
	TAtomic<uint32> NewVideoBitrate{ 0 };
	FThreadSafeBool bChangeBitrate = false;
	TAtomic<uint32> NewVideoFramerate{ 0 };
	FThreadSafeBool bChangeFramerate = false;

	TArray<int16> PCM16;
	TMap<TSharedPtr<AVEncoder::FVideoEncoderInputFrame>, FTexture2DRHIRef> BackBuffers;
};


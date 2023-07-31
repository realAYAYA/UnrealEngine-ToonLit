// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaIOCorePlayerBase.h"
#include "IRivermaxInputStream.h"
#include "RivermaxMediaSource.h"
#include "RivermaxTypes.h"

#include <atomic>

class FRivermaxMediaTextureSample;
class FRivermaxMediaTextureSamplePool;
struct FSlateBrush;
class IMediaEventSink;

enum class EMediaTextureSampleFormat;
enum class EMediaIOSampleType;


namespace  UE::RivermaxMedia
{
	using namespace UE::RivermaxCore;

	/**
	 * Implements a media player using rivermax.
	 */
	class FRivermaxMediaPlayer : public FMediaIOCorePlayerBase, public IRivermaxInputStreamListener
	{
		using Super = FMediaIOCorePlayerBase;
	public:

		/**
		 * Create and initialize a new instance.
		 *
		 * @param InEventSink The object that receives media events from this player.
		 */
		FRivermaxMediaPlayer(IMediaEventSink& InEventSink);

		/** Virtual destructor. */
		virtual ~FRivermaxMediaPlayer();

	public:

		//~ IMediaPlayer interface

		virtual void Close() override;
		virtual FGuid GetPlayerPluginGUID() const override;

		virtual bool Open(const FString& Url, const IMediaOptions* Options) override;

		virtual void TickFetch(FTimespan DeltaTime, FTimespan Timecode) override;
		virtual void TickInput(FTimespan DeltaTime, FTimespan Timecode) override;

		virtual FString GetStats() const override;

		bool SetRate(float Rate) override;

		//~ ITimedDataInput interface
#if WITH_EDITOR
		virtual const FSlateBrush* GetDisplayIcon() const override;
#endif

		//~ Begin IRivermaxInputStreamListener interface
		virtual void OnInitializationCompleted(bool bHasSucceed) override;
		virtual bool OnVideoFrameRequested(const FRivermaxInputVideoFrameDescriptor& FrameInfo, FRivermaxInputVideoFrameRequest& OutVideoFrameRequest) override;
		virtual void OnVideoFrameReceived(const FRivermaxInputVideoFrameDescriptor& FrameInfo, const FRivermaxInputVideoFrameReception& ReceivedVideoFrame) override;
		//~ End IRivermaxInputStreamListener interface

	protected:

		/**
		 * Process pending audio and video frames, and forward them to the sinks.
		 */
		void ProcessFrame();

	protected:

		//~ Begin FMediaIOCorePlayerBase interface
		virtual bool IsHardwareReady() const override;
		virtual void SetupSampleChannels() override;
		//~ End FMediaIOCorePlayerBase interface

	private:
		bool ConfigureStream(const IMediaOptions* Options);

	private:

		/** Audio, MetaData, Texture  sample object pool. */
		TUniquePtr<FRivermaxMediaTextureSamplePool> TextureSamplePool;

		TSharedPtr<FRivermaxMediaTextureSample> RivermaxThreadCurrentTextureSample;

		/** The media sample cache. */
		int32 MaxNumVideoFrameBuffer;

		/** Current state of the media player. */
		EMediaState RivermaxThreadNewState;

		/** The media event handler. */
		IMediaEventSink& EventSink;

		FRivermaxStreamOptions StreamOptions;

		/** Whether the input is in sRGB and can have a ToLinear conversion. */
		bool bIsSRGBInput;

		/** Which field need to be capture. */
		bool bUseVideo;
		bool bVerifyFrameDropCount;

		/** Maps to the current input Device */
		TUniquePtr<IRivermaxInputStream> InputStream;

		/** Used to flag which sample types we advertise as supported for timed data monitoring */
		EMediaIOSampleType SupportedSampleTypes;

		/** Flag to indicate that pause is being requested */
		std::atomic<bool> bPauseRequested;

		/** Pixel format provided by media source */
		ERivermaxMediaSourcePixelFormat DesiredPixelFormat = ERivermaxMediaSourcePixelFormat::RGB_10bit;
	};
}



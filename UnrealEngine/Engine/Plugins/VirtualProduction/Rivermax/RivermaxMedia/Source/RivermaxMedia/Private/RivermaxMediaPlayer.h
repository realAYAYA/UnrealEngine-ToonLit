// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaIOCorePlayerBase.h"
#include "IRivermaxInputStream.h"
#include "RivermaxMediaSource.h"
#include "RivermaxMediaTextureSampleConverter.h"
#include "RivermaxTypes.h"
#include "Templates/Function.h"

#include <atomic>

struct FSlateBrush;
class IMediaEventSink;

enum class EMediaTextureSampleFormat;
enum class EMediaIOSampleType;


namespace  UE::RivermaxMedia
{
	using namespace UE::RivermaxCore;
	
	class FRivermaxMediaTextureSamples;
	class FRivermaxMediaTextureSample;
	class FRivermaxMediaTextureSamplePool;

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
		
		/** Sample update called by converter to setup sample to render */
		bool LateUpdateSetupSample(FSampleConverterOperationSetup& OutConverterSetup);

		//~ Begin IMediaPlayer interface
		virtual void Close() override;
		virtual FGuid GetPlayerPluginGUID() const override;
		virtual bool Open(const FString& Url, const IMediaOptions* Options) override;
		virtual void TickTimeManagement() override;
		virtual void TickFetch(FTimespan DeltaTime, FTimespan Timecode) override;
		virtual void TickInput(FTimespan DeltaTime, FTimespan Timecode) override;
		virtual IMediaSamples& GetSamples() override;
		virtual FString GetStats() const override;
		virtual bool GetPlayerFeatureFlag(EFeatureFlag flag) const override;
		virtual bool SetRate(float Rate) override;
		//~ End IMediaPlayer interface

		//~ Begin ITimedDataInput interface
#if WITH_EDITOR
		virtual const FSlateBrush* GetDisplayIcon() const override;
#endif

		//~ Begin IRivermaxInputStreamListener interface
		virtual void OnInitializationCompleted(const FRivermaxInputInitializationResult& Result) override;
		virtual bool OnVideoFrameRequested(const FRivermaxInputVideoFrameDescriptor& FrameInfo, FRivermaxInputVideoFrameRequest& OutVideoFrameRequest) override;
		virtual void OnVideoFrameReceived(const FRivermaxInputVideoFrameDescriptor& FrameInfo, const FRivermaxInputVideoFrameReception& ReceivedVideoFrame) override;
		virtual void OnVideoFrameReceptionError(const FRivermaxInputVideoFrameDescriptor& FrameInfo) override;
		virtual void OnStreamError() override;
		virtual void OnVideoFormatChanged(const FRivermaxInputVideoFormatChangedInfo& NewFormatInfo) override;
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
		virtual TSharedPtr<FMediaIOCoreTextureSampleBase> AcquireTextureSample_AnyThread() const override
		{
			// This needs to be fixed once FRivermaxMediaTextureSample is inherited from FMediaIOCoreTextureSampleBase
			// return MakeShared<FRivermaxMediaTextureSample>();
			return nullptr;
		}
		//~ End FMediaIOCorePlayerBase interface

	private:

		/** Sets up stream options based on the settings of the media source */
		bool ConfigureStream(const IMediaOptions* Options);

		/** Allocates the sample pool used to receive incoming data */
		void AllocateBuffers(const FIntPoint& InResolution);

		/** Wrapper struct holding information about frame expected to be rendered */
		struct FFrameExpectation
		{
			/** Frame number we are expecting to find at ExpectedIndex */
			uint32 FrameNumber = 0;

			/** Index where rendering should pickup its sample from */
			uint32 FrameIndex = 0;
		};

		enum class ESampleReceptionState : uint8
		{
			// Sample is ready to be requested by rivermax stream
			Available,

			// Sample has been requested and is being received
			Receiving,

			// Sample has been received and is ready to be rendered
			Received,
		};

		/** Wrapper around a sample holding more information about its state */
		struct FRivermaxSampleWrapper
		{
			/** State of this sample */
			std::atomic<ESampleReceptionState> ReceptionState = ESampleReceptionState::Available;

			/** True when queued for rendering. Will be false once fence has been written, after shader usage. */
			std::atomic<bool> bIsPendingRendering = false;

			/** True if sample can be rendered. For non-gpudirect, we need to copy sample from system memory to gpu to be ready */
			std::atomic<bool> bIsReadyToRender = false;

			/** Locked memory of gpu buffer when uploading */
			void* LockedMemory = nullptr;

			/** Actual sample buffer container used by media framework */
			TSharedPtr<FRivermaxMediaTextureSample> Sample;

			/** Write fence enqueued after sample conversion to know when it's ready to be reused */
			FGPUFenceRHIRef SampleConversionFence;

			/** Frame number of this sample */
			uint32 FrameNumber = 0;

			/** Timestamp of this sample */
			uint32 Timestamp = 0;
		};

		/** Buffer upload setup that will block render thread while waiting for sample and uploading it */
		void SampleUploadSetupRenderThreadMode(const FFrameExpectation& FrameExpectation, FSampleConverterOperationSetup& OutConverterSetup);
		
		/** Buffer upload setup that will wait on its own task to wait for sample and do the upload */
		void SampleUploadSetupTaskThreadMode(const FFrameExpectation& FrameExpectation, FSampleConverterOperationSetup& OutConverterSetup);

		/** Function waiting for the expected frame to be received. */
		using FWaitConditionFunc = TUniqueFunction<bool(const TSharedPtr<FRivermaxSampleWrapper>&)>;
		void WaitForSample(const FFrameExpectation& FrameExpectation, FWaitConditionFunc WaitConditionFunction, bool bCanTimeout);
		
		/** Called after sample was converted / rendered. Used write a fence to detect when sample is reusable */
		void PostSampleUsage(FRDGBuilder& GraphBuilder, const FFrameExpectation& FrameExpectation);

		/** Provides next frame to render expectations in terms of number and location in the pool.  */
		bool GetNextExpectedFrameInfo(FFrameExpectation& OutExpectation);
		bool GetNextExpectedFrameInfoForFramelock(FFrameExpectation& OutExpectation);
		bool GetNextExpectedFrameInfoForLatest(FFrameExpectation& OutExpectation);

		/** Provides next requested frame (by receiver stream) location in the pool */
		bool GetFrameRequestedIndex(const FRivermaxInputVideoFrameDescriptor& FrameInfo, uint32& OutExpectedIndex);
		bool GetFrameRequestedIndexForFramelock(const FRivermaxInputVideoFrameDescriptor& FrameInfo, uint32& OutExpectedIndex);
		bool GetFrameRequestedIndexForLatest(const FRivermaxInputVideoFrameDescriptor& FrameInfo, uint32& OutExpectedIndex);

		/** Verifies if a frame should be skipped, looking at gaps we might have had in the reception of frames */
		bool IsFrameSkipped(uint32 FrameNumber) const;

		/** Looks at last frame rendered and tries to clean skipped frames container */
		void TryClearSkippedInterval(uint32 LastFrameRendered);

		/** Whether player is ready to play */
		bool IsReadyToPlay() const;

		/** Waits for tasks in flight and flushes render commands before cleaning our ressources */
		void WaitForPendingTasks();


	private:

		TSharedPtr<FRivermaxSampleWrapper> RivermaxThreadCurrentTextureSample;

		/** Common gpu buffer to use when we render a sample */
		TRefCountPtr<FRDGPooledBuffer> CommonGPUBuffer;

		/** Size of the sample pool. */
		int32 MaxNumVideoFrameBuffer;

		/** Current state of the media player. */
		EMediaState RivermaxThreadNewState;

		/** Options used to configure the stream. i.e.  */
		FRivermaxInputStreamOptions StreamOptions;

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

		/** Pool of samples where incoming ones are written to and at render time, we select our candidate */
		TArray<TSharedPtr<FRivermaxSampleWrapper>> SamplePool;

		/** Used to ensure that JustInTimeSampleRender is only run once per frame */
		uint32 LastFrameNumberThatUpdatedJustInTime = 0;

		/** Critical section used when manipulating the skipped frame container */
		mutable FCriticalSection SkippedFrameCriticalSection;

		struct FFrameTracking
		{
			/** Whether a frame has been requested yet. Used to detect start of the stream and valid frame number expectations */
			bool bWasFrameRequested = false;
			
			/** Last frame number sent to render. Used to release received frames that will never be picked up early on */
			TOptional<uint32> LastFrameRendered;

			/** Last frame expectations used */
			FFrameExpectation LastFrameExpectation;
			
			/** Last index requested at the start of a frame reception. In latest mode, this just goes up incrementally */
			uint32 LastFrameRequestedIndex = 0;

			/** Used to track frame reception and detect gaps in received frames */
			uint32 LastFrameNumberRequested = 0;

			/** Used to detect frames to be rendered with invalid expectations. i.e Looking for a frame number that we will never get */
			uint32 FirstFrameRequested = 0;
		};
		FFrameTracking FrameTracking;

		/** Mode the player is in. Latest, framelocked, etc... */
		ERivermaxPlayerMode PlayerMode = ERivermaxPlayerMode::Latest;

		/** Number of tasks currently in progress. Used during shutdown to know when we are good to continue */
		std::atomic<uint32> TasksInFlight = 0;
	
		/** Used to detect skipped / missing frames in our reception and early exit the waiting in case we are waiting for a frame that will never come */
		TArray<TInterval<uint32>> SkippedFrames;

		/** Used in framelock mode. How far behind frame counter we will try to find a matching frame */
		uint32 FrameLatency = 0;

		/** Special sample container, holding a single one used to trigger media framework rendering */
		TUniquePtr<FRivermaxMediaTextureSamples> MediaSamples;

		/** Whether the created stream supports GPUDirect. Will be confirmed after initialization. */
		bool bDoesStreamSupportsGPUDirect = false;

		/** Time to sleep when waiting for an operation to complete */
		static constexpr double SleepTimeSeconds = 50.0 * 1E-6;

		/** Critical section used when accessing stream resolution and detect pending changes */
		mutable FCriticalSection StreamResolutionCriticalSection;

		/** Resolution detected by our stream. */
		FIntPoint StreamResolution = FIntPoint::ZeroValue;
		
		/** Whether the player follows resolution detected by our stream, adjusting texture size as required */
		bool bFollowsStreamResolution = true;
	};
}



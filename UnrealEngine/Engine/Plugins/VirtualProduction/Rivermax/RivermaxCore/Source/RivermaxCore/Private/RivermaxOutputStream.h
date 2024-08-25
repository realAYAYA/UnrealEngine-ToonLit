// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IRivermaxOutputStream.h"

#include "Async/Future.h"
#include "Containers/SpscQueue.h"
#include "HAL/Runnable.h"
#include "RivermaxWrapper.h"
#include "RivermaxOutputFrame.h"
#include "RivermaxTypes.h"
#include "RTPHeader.h"


class FEvent;
class IRivermaxCoreModule;

namespace UE::RivermaxCore::Private
{
	class FFrameManager;
	class FBaseFrameAllocator;
	struct FBaseDataCopySideCar;

	using UE::RivermaxCore::FRivermaxOutputStreamOptions;
	using UE::RivermaxCore::FRivermaxOutputVideoFrameInfo;


	/** Struct holding configuration information with regards to stream memory and packetization */
	struct FRivermaxOutputStreamMemory
	{
		/** Size of each data payload packet that will be used */
		uint16 PayloadSize = 0;

		/** Number of pixel group per packet */
		uint32 PixelGroupPerPacket = 0;

		/** Number of pixels per packet */
		uint32 PixelsPerPacket = 0;

		/** Number of pixels per frame */
		uint32 PixelsPerFrame = 0;

		/** Stride of RTP header data. */
		uint32 HeaderStrideSize = 20;

		/** Number of lines packed inside a chunk. Can be controlled with cvar */
		uint32 LinesInChunk = 4;

		/** Number of packets per line.  */
		uint32 PacketsInLine = 0;

		/** Number of packets per chunk. Depends on LinesInChunk */
		uint32 PacketsPerChunk = 0;

		/** Number of frames per memory block. */
		uint32 FramesFieldPerMemoryBlock = 0;

		/** Number of packets per frame */
		uint32 PacketsPerFrame = 0;

		/** Number of packets per memory block */
		uint32 PacketsPerMemoryBlock = 0;

		/**Number of chunks per frame  */
		uint32 ChunksPerFrameField = 0;

		/** Number of chunks per memory block */
		uint32 ChunksPerMemoryBlock = 0;

		/** Number of memory block */
		uint32 MemoryBlockCount = 0; 
		
		/** Whether intermediate buffer is used and captured frame has to be copied over again. */
		bool bUseIntermediateBuffer = false;

		/** Number of slices we split frame data into when copying it into intermediate buffer */
		uint32 FrameMemorySliceCount = 1;

		/** Chunk committed between each memcopy of frame data. Helps respect timing. */
		uint32 ChunkSpacingBetweenMemcopies = 1;

		/** 
		 * Whether we skip zero chunk, to reset rivermax's stream internal timings, vblank verification, etc...
		 * Alignment point method will default to true and be configurable through cvar and frame creation will never use it.
		 * Usage is to make sure Tro is respected when frame rate multiplier is involved.
		 */
		bool bAlwaysSkipChunk = true;

		/** 
		 * Multiplier applied to desired frame rate when creating Rivermax stream to add margin in case of packet timings not being respected
		 * This will break compliance with standard but if receiver can tolerate it, more stable stream timings are expected.
		 */
		float FrameRateMultiplier = 1.00;

		/** Memory blocks given to Rivermax which where data is located */
		TArray<rmx_output_media_mem_block> MemoryBlocks;

		/** Data Sub block ID */
		const uint8 HeaderBlockID = 0;

		/** Data Sub block ID */
		const uint8 DataBlockID = 1;

		/** Array with each packet size */
		TArray<uint16_t> PayloadSizes; 

		/** Array with each RTP header size */
		TArray<uint16_t> HeaderSizes;

		/** Contains RTP headers per memory block */
		TArray<TArray<FRawRTPHeader>> RTPHeaders;

		/** Start addresses of each buffer in memblock */
		TArray<void*> BufferAddresses;
	};

	struct FRivermaxOutputStreamStats
	{
		/** Chunk retries that were required since stream was started */
		uint32 TotalChunkRetries = 0;
		
		/** Chunk retries that happened during last frame */
		uint32 LastFrameChunkRetries = 0;
		
		/** Chunk skipping retries that happened since stream was started */
		uint32 ChunkSkippingRetries = 0;
		
		/** Total packets that have been sent since stream was started */
		uint32 TotalPacketSent = 0;
		
		/** Number of retries that were required when committing and queue was full since stream was started */
		uint32 CommitRetries = 0;

		/** Immediate commits that were done because we got there too close to scheduling time */
		uint32 CommitImmediate = 0;

		/** Number of frames that were sent since stream was started */
		uint64 FramesSentCounter = 0;

		/** Frames that had timing issues since stream was started */
		uint64 TimingIssueCount = 0;
	};

	struct FRivermaxOutputStreamData
	{
		// Handle used to retrieve chunks associated with output stream
		rmx_output_media_chunk_handle ChunkHandle;

		/** Current sequence number being done */
		uint32 SequenceNumber = 0;
		double FrameFieldTimeIntervalNs = 0.0;
		
		/** Data and RTP frame index expected to be used for next frame */
		uint8 ExpectedFrameIndex = 0;

		/** Used to detect misalignment between chunk being sent and frame memory we are writing in */
		bool bHasFrameFirstChunkBeenFetched = false;

		/** Next alignment point based on PTP standard */
		uint64 NextAlignmentPointNanosec = 0;

		/** Next schedule time using 2110 gapped model timing and controllable offset */
		uint64 NextScheduleTimeNanosec = 0;

		/** Whether next alignment frame number is deemed valid or not to detect missed frames. */
		bool bHasValidNextFrameNumber = false;
		
		/** Next alignment point frame number treated to detect missed frames */
		uint64 NextAlignmentPointFrameNumber = 0;

		/** Last alignment point frame number we have processed*/
		uint64 LastAlignmentPointFrameNumber = 0;

		/** Timestamp at which we started commiting a frame */
		uint64 LastSendStartTimeNanoSec = 0;
		
		/** Keeping track of how much time was slept last round. */
		uint64 LastSleepTimeNanoSec = 0;
	};

	/** Struct holding various cached cvar values that can't be changed once stream has been created and to avoid calling anythread getters continuously */
	struct FOutputStreamCachedCVars
	{
		/** Whether timing protection is active and next frame interval is skipped if it happens */
		bool bEnableCommitTimeProtection = true;

		/** Time padding from scheduling time required to avoid skipping it */
		uint64 SkipSchedulingTimeNanosec = 0;

		/** 
		 * Time from scheduling required to not commit it immediately 
		 * Rivermax sdk will throw an error if time is in the past when it
		 * gets to actually comitting it. 
		 */
		uint64 ForceCommitImmediateTimeNanosec = 0;
		
		/** Tentative optimization recommended for SDK where a single big memblock is allocated. When false, a memblock per frame is configured. */
		bool bUseSingleMemblock = true;

		/** Whether to bump output thread priority to time critical */
		bool bEnableTimeCriticalThread = true;

		/** Whether to show output stats at regular interval in logs */
		bool bShowOutputStats = false;

		/** Interval in seconds at which to display output stats */
		float ShowOutputStatsIntervalSeconds = 1.0f;

		/** Whether to prefill RTP header memory with known data at initialization time instead of during sending */
		bool bPrefillRTPHeaders = true;
	};

	class FRivermaxOutputStream : public UE::RivermaxCore::IRivermaxOutputStream, public FRunnable
	{
	public:
		FRivermaxOutputStream();
		virtual ~FRivermaxOutputStream();

	public:

		//~ Begin IRivermaxOutputStream interface
		virtual bool Initialize(const FRivermaxOutputStreamOptions& Options, IRivermaxOutputStreamListener& InListener) override;
		virtual void Uninitialize() override;
		virtual bool PushVideoFrame(const FRivermaxOutputVideoFrameInfo& NewFrame) override;
		virtual bool IsGPUDirectSupported() const override;
		virtual bool ReserveFrame(uint32 FrameIdentifier) const override;
		virtual void GetLastPresentedFrame(FPresentedFrameInfo& OutFrameInfo) const override;
		//~ End IRivermaxOutputStream interface

		void Process_AnyThread();

		//~ Begin FRunnable interface
		virtual bool Init() override;
		virtual uint32 Run() override;
		virtual void Stop() override;
		virtual void Exit() override;
		//~ End FRunnable interface

	private:

		/** Configures chunks, packetizing, memory blocks of the stream */
		bool InitializeStreamMemoryConfig();
		
		/** Initializes timing setup for this stream. TRO, frame interval etc... */
		void InitializeStreamTimingSettings();

		/** Configures settings related to timing protection dependant on cvars */
		void InitializeTimingProtections();

		/** Sets up frame management taking care of allocation, special cuda handling, etc... */
		bool SetupFrameManagement();

		/** Clean up frames */
		void CleanupFrameManagement();

		/** Resets NextFrame to be ready to send it out */
		void InitializeNextFrame(const TSharedPtr<FRivermaxOutputFrame>& NextFrame);

		/** Fills RTP and SRD header using current state */
		void BuildRTPHeader(FRawRTPHeader& OutHeader) const;

		/** Destroys rivermax stream. Will wait until it's ready to be destroyed */
		void DestroyStream();

		/** Waits for the next point in time to send out a new frame. Returns true if it exited earlier with the next frame ready to be processed */
		bool WaitForNextRound();

		/** Calculate next frame scheduling time for alignment points mode */
		void CalculateNextScheduleTime_AlignementPoints(uint64 CurrentClockTimeNanosec, uint64 CurrentFrameNumber);
		
		/** Calculate next frame scheduling time for frame creation mode */
		void CalculateNextScheduleTime_FrameCreation(uint64 CurrentClockTimeNanosec, uint64 CurrentFrameNumber);

		/** Validates timing on every commit to see if we are respecting alignment */
		bool IsChunkOnTime() const;
		
		/** Validates timing for frame creation alignment which always returns true. */
		bool IsChunkOnTime_FrameCreation() const;
		
		/** Validates timing to make sure chunk to be committed are on time. 
		 *  Once a chunk is late, timings are at risk and next frame will be skipped
		 */
		bool IsChunkOnTime_AlignmentPoints() const;

		/** Query rivermax library for the next chunk to work with */
		void GetNextChunk();

		/** Copies part of frame memory in next memblock's chunk to be sent out */
		bool CopyFrameData(const TSharedPtr<FRivermaxOutputFrame>& SourceFrame, uint8* DestinationBase);

		/** Fills RTP header for all packets to be sent for this chunk */
		void SetupRTPHeaders();

		/** Commits chunk to rivermax so they are scheduled to be sent */
		void CommitNextChunks();

		/** Fetches next frame to send and prepares it for sending */
		void PrepareNextFrame();

		/** Returns next frame to send for frame creation alignment */
		void PrepareNextFrame_FrameCreation();

		/** Returns next frame to send for alignement point method. Can return nullptr */
		void PrepareNextFrame_AlignmentPoint();

		/** Uses time before next frame interval to copy data from next ready frame to intermediate buffer */
		void PreprocessNextFrame();

		/** If enabled, print stats related to this stream */
		void ShowStats();

		/** Returns a mediaclock timestamp, for rtp, based on a clock time */
		uint32 GetTimestampFromTime(uint64 InTimeNanosec, double InMediaClockRate) const;
		
		/** Get row stride for the current stream configuration */
		int32 GetStride() const;

		/** Used to notify the listener that a frame is ready to be enqueued for transmission */
		void OnPreFrameReadyToBeSent();
		
		/** Used to detect when a frame is now ready to be sent */
		void OnFrameReadyToBeSent();

		/** Used to know when a frame is ready to be used and receive new data */
		void OnFrameReadyToBeUsed();

		/** Used to detect when the frame manager has caught a critical error */
		void OnFrameManagerCriticalError();

		/** Used to cache cvars at initialization */
		void CacheCVarValues();

		/** Called back when copy request was completed by allocator */
		void OnMemoryChunksCopied(const TSharedPtr<FBaseDataCopySideCar>& Sidecar);

		/** Called when delay request cvar has been changed */
		void OnCVarRandomDelayChanged(IConsoleVariable* Var);

		/** Update frame's timestamp to be used when setting every RTP headers */
		void CalculateFrameTimestamp();

		/** Tells Rivermax to skip a certain number of chunks in memory. Can be zero to just reset internals */
		void SkipChunks(uint64 ChunkCount);

		/** Go through all chunks of current frame and commit them to Rivermax to send them at the next desired time */
		void SendFrame();

		/** When a frame has been sent (after frame interval), we update last presented frame tracking and optionally release it in the presentation queue */
		void CompleteCurrentFrame(bool bReleaseFrame);

	private:

		/** Options related to this stream. i.e resolution, frame rate, etc... */
		FRivermaxOutputStreamOptions Options;

		/** Rivermax memory configuration. i.e. memblock, chunks */
		FRivermaxOutputStreamMemory StreamMemory;

		/** Various stats collected by this stream */
		FRivermaxOutputStreamStats Stats;

		/** State of various piece for this stream. Alignment points, schedule number, etc... */
		FRivermaxOutputStreamData StreamData;

		/** Stream id returned by rmax library */
		rmx_stream_id StreamId = 0;

		/** Current frame being sent */
		TSharedPtr<FRivermaxOutputFrame> CurrentFrame;

		/** Thread scheduling frame output */
		TUniquePtr<FRunnableThread> RivermaxThread;

		/** Manages allocation and memory manipulation of video frames */
		TUniquePtr<FFrameManager> FrameManager;

		/** Manages allocation of memory for rivermax memblocks */
		TUniquePtr<FBaseFrameAllocator> Allocator;

		/** Whether stream is active or not */
		std::atomic<bool> bIsActive;

		/** Event used to let scheduler that a frame is ready to be sent */
		FEventRef FrameReadyToSendSignal = FEventRef(EEventMode::AutoReset);

		/** Event used to unblock frame reservation as soon as one is free */
		FEventRef FrameAvailableSignal = FEventRef(EEventMode::AutoReset);

		/** Listener for this stream events */
		IRivermaxOutputStreamListener* Listener = nullptr;

		/** Type of stream created. Only 21110-20 (Video is supported now) */
		ERivermaxStreamType StreamType = ERivermaxStreamType::VIDEO_2110_20_STREAM;

		/** TRoffset time calculated based on ST2110 - 21 Gapped(for now) method. This is added to next alignment point */
		uint64 TransmitOffsetNanosec = 0;

		/** Format info for the active stream */
		FVideoFormatInfo FormatInfo;

		/** Timestamp at which we logged stats */
		double LastStatsShownTimestamp = 0.0;
		
		/** Whether stream is using gpudirect to host memory consumed by Rivermax */
		bool bUseGPUDirect = false;

		/** Our own module pointer kept for ease of use */
		IRivermaxCoreModule* RivermaxModule = nullptr;

		/** Guid given by boundary monitoring handler to unregister ourselves */
		FGuid MonitoringGuid;

		/** Future returned by the async initialization job we launch. Used to detect if it has completed during shutdown. */
		TFuture<void> InitializationFuture;

		/** Cached cvar values */
		FOutputStreamCachedCVars CachedCVars;

		/* Pointer to the rivermax API to avoid virtual calls in a hot loop. */ 
		const UE::RivermaxCore::Private::RIVERMAX_API_FUNCTION_LIST* CachedAPI = nullptr;
		/** Whether to trigger a delay in the output thread loop next time it ticks */
		bool bTriggerRandomDelay = false;

		/** Critical section to access data of last presented frame */
		mutable FCriticalSection PresentedFrameCS;

		/** Info of last presented frame */
		FPresentedFrameInfo LastPresentedFrame;

		friend struct FRTPHeaderPrefiller;
	};
}



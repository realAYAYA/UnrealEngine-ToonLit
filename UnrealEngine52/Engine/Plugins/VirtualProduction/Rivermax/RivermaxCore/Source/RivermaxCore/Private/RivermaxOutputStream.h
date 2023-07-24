// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IRivermaxOutputStream.h"

#include "Containers/SpscQueue.h"
#include "HAL/Runnable.h"
#include "RivermaxHeader.h"
#include "RivermaxOutputFrame.h"
#include "RivermaxTypes.h"
#include "RTPHeader.h"


class FEvent;
class IRivermaxCoreModule;

namespace UE::RivermaxCore::Private
{
	using UE::RivermaxCore::FRivermaxOutputStreamOptions;
	using UE::RivermaxCore::FRivermaxOutputVideoFrameInfo;

	struct FRivermaxOutputStreamMemory
	{
		uint16 PayloadSize = 0;
		uint32 PixelGroupPerPacket = 0;
		uint32 PixelsPerPacket = 0;
		uint32 PixelsPerFrame = 0;

		uint32 HeaderStrideSize = 20;
		uint32 LinesInChunk = 4;

		uint32 PacketsInLine = 0;
		uint32 PacketsPerChunk = 0;

		uint32 FramesFieldPerMemoryBlock = 0;
		uint32 PacketsPerFrame = 0;
		uint32 PacketsPerMemoryBlock = 0;
		uint32 ChunksPerFrameField = 0;
		uint32 ChunksPerMemoryBlock = 0;
		uint32 MemoryBlockCount = 0; 

		TArray<rmax_mem_block> MemoryBlocks;
		TArray<uint16_t> PayloadSizes; //Array describing stride payload size
		TArray<uint16_t> HeaderSizes; //Array describing header payload size
		TArray<TArray<FRawRTPHeader>> RTPHeaders;

		rmax_buffer_attr BufferAttributes;
	};

	struct FRivermaxOutputStreamStats
	{
		uint32 ChunkRetries = 0;
		uint32 TotalStrides = 0;
		uint32 ChunkWait = 0;
		uint32 CommitWaits = 0;
		uint32 CommitRetries = 0;
		uint32 CommitImmediate = 0;
		uint64 MemoryBlockSentCounter = 0;
	};

	struct FRivermaxOutputStreamData
	{
		/** Current sequence number being done */
		uint32 SequenceNumber = 0;
		double FrameFieldTimeIntervalNs = 0.0;

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

		/** Timestamp at which we started commiting a frame */
		uint64 LastSendStartTimeNanoSec = 0;
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
		virtual bool PushGPUVideoFrame(const FRivermaxOutputVideoFrameInfo& NewFrame, FBufferRHIRef CapturedBuffer) override;
		virtual bool IsGPUDirectSupported() const override;
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

		/** Allocates buffers on gpu for gpudirect usage */
		bool AllocateGPUBuffers();

		/** Allocates buffers on system memory */
		void AllocateSystemBuffers();

		/** Clean up allocated buffers */
		void DeallocateBuffers();

		/** Resets NextFrame to be ready to send it out */
		void InitializeNextFrame(const TSharedPtr<FRivermaxOutputFrame>& NextFrame);

		/** Returns next frame ready to be sent */
		TSharedPtr<FRivermaxOutputFrame> GetNextFrameToSend();

		/** Returns next frame ready to be filled / written by the capture */
		TSharedPtr<FRivermaxOutputFrame> GetNextAvailableFrame(uint32 InFrameIdentifier);

		/** Fills RTP and SRD header using current state */
		void BuildRTPHeader(FRawRTPHeader& OutHeader) const;

		/** Destroys rivermax stream. Will wait until it's ready to be destroyed */
		void DestroyStream();

		/** Waits for the next point in time to send out a new frame */
		void WaitForNextRound();

		/** Calculate next frame scheduling time for alignment points mode */
		void CalculateNextScheduleTime_AlignementPoints(uint64 CurrentClockTimeNanosec, uint64 CurrentFrameNumber);
		
		/** Calculate next frame scheduling time for frame creation mode */
		void CalculateNextScheduleTime_FrameCreation(uint64 CurrentClockTimeNanosec, uint64 CurrentFrameNumber);

		/** Query rivermax library for the next chunk to work with */
		void GetNextChunk();

		/** Fills RTP header for all packets to be sent for this chunk */
		void SetupRTPHeaders();

		/** Commits chunk to rivermax so they are scheduled to be sent */
		void CommitNextChunks();

		/** Fetches next frame to send and prepares it for sending */
		void PrepareNextFrame();

		/** If enabled, print stats related to this stream */
		void ShowStats();

		/** Returns a mediaclock timestamp, for rtp, based on a clock time */
		uint32 GetTimestampFromTime(uint64 InTimeNanosec, double InMediaClockRate) const;
		
		/** Get row stride for the current stream configuration */
		int32 GetStride() const;

		/** Get mapped address in cuda space for a given buffer. Cache will be updated if not found */
		void* GetMappedAddress(const FBufferRHIRef& InBuffer);

		/** Makes a frame available to be sent, i.e. moved to the right container and mark its arrival time */
		void MarkFrameToBeSent(TSharedPtr<FRivermaxOutputFrame> ReadyFrame);

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
		rmax_stream_id StreamId;

		/** Critical section to protect frames access */
		FCriticalSection FrameCriticalSection;

		/** Current frame being sent */
		TSharedPtr<FRivermaxOutputFrame> CurrentFrame;

		/** Available frames to write memory to (Ready to be written) */
		TArray<TSharedPtr<FRivermaxOutputFrame>> AvailableFrames;

		/** Frames ready to be sent to rivermax (Ready to be read) */
		TArray<TSharedPtr<FRivermaxOutputFrame>> FramesToSend;

		/** Thread scheduling frame output */
		TUniquePtr<FRunnableThread> RivermaxThread;

		/** Whether stream is active or not */
		std::atomic<bool> bIsActive;

		/** Event used to let scheduler that a frame is ready to be sent */
		FEvent* ReadyToSendEvent = nullptr;

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

		/** Allocated memory base address used when it's time to free */
		void* CudaAllocatedMemoryBaseAddress = nullptr;

		/** Total allocated gpu memory. */
		int32 CudaAllocatedMemory = 0;

		/** Map between buffer we are sending and their mapped address in gpu space */
		TMap<FBufferRHIRef, void*> BufferCudaMemoryMap;

		/** Queued identifiers to be consumed by cuda callback when work has been completed */
		TSpscQueue<uint32> PendingIdentifiers;

		/** Cuda stream used for our operations */
		void* GPUStream = nullptr;

		/** Our own module pointer kept for ease of use */
		IRivermaxCoreModule* RivermaxModule = nullptr;

		/** Time to sleep when waiting for an operation to complete */
		static constexpr double SleepTimeSeconds = 50.0 * 1E-6;
	};
}



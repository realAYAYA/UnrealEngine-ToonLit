// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IRivermaxOutputStream.h"

#include "HAL/Runnable.h"
#include "RivermaxHeader.h"
#include "RivermaxOutputFrame.h"
#include "RivermaxTypes.h"


class FEvent;

namespace UE::RivermaxCore::Private
{
	using UE::RivermaxCore::FRivermaxStreamOptions;
	using UE::RivermaxCore::FRivermaxOutputVideoFrameInfo;
	
	// Todo make a proper RTP header struct
	struct FRTPHeader
	{
		uint8 RawHeader[20];
	};

	struct FRivermaxOutputStreamMemory
	{
		uint16 PayloadSize = 0;
		uint32 PixelGroupPerPacket = 0;
		uint32 PixelsPerPacket = 0;
		uint32 PixelsPerFrame = 0;

		uint32 HeaderStrideSize = 20;
		uint32 LinesInChunk = 4;

		uint32 PacketsInLine = 0;
		uint32 ChunkSizeInStrides = 0;

		uint32 FramesFieldPerMemoryBlock = 0;
		uint32 PacketsPerFrame = 0;
		uint32 PacketsPerMemoryBlock = 0;
		uint32 ChunksPerFrameField = 0;
		uint32 ChunksPerMemoryBlock = 0;
		uint32 MemoryBlockCount = 0; 
		uint32 StridesPerMemoryBlock = 0;

		TArray<rmax_mem_block> MemoryBlocks;
		TArray<uint16_t> PayloadSizes; //Array describing stride payload size
		TArray<uint16_t> HeaderSizes; //Array describing header payload size
		TArray<FRTPHeader> RTPHeaders;

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
	};


	class FRivermaxOutputStream : public UE::RivermaxCore::IRivermaxOutputStream, public FRunnable
	{
	public:
		FRivermaxOutputStream();
		virtual ~FRivermaxOutputStream();

	public:

		//~ Begin IRivermaxOutputStream interface
		virtual bool Initialize(const FRivermaxStreamOptions& Options, IRivermaxOutputStreamListener& InListener) override;
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
		bool InitializeStreamMemoryConfig();
		void InitializeNextFrame(const TSharedPtr<FRivermaxOutputFrame>& NextFrame);
		TSharedPtr<FRivermaxOutputFrame> GetNextFrameToSend();
		TSharedPtr<FRivermaxOutputFrame> GetNextAvailableFrame(uint32 InFrameIdentifier);
		void BuildRTPHeader(FRTPHeader& OutHeader) const;
		void DestroyStream();
		void WaitForNextRound();
		void GetNextChunk();
		void SetupRTPHeaders();
		void CommitNextChunks();
		void PrepareNextFrame();
		void InitializeStreamTimingSettings();
		void ShowStats();
		uint32 GetTimestampFromTime(uint64 InTimeNanosec, double InMediaClockRate) const;
		void AllocateSystemBuffers();
		int32 GetStride() const;

	private:

		/** Options related to this stream. i.e resolution, frame rate, etc... */
		FRivermaxStreamOptions Options;

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

		/** Required to comply with SMTPE 2110 - 10.The Media Clock and RTP Clock rate for streams compliant to this standard shall be 90 kHz. */
		static constexpr double MediaClockSampleRate = 90000.0; 

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
	};
}



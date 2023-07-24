// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Queue.h"

#include "Renderer/RendererBase.h"

#include "MediaVideoDecoderOutput.h"

struct FTimespan;
class FElectraPlayer;

class FElectraPlayerPlatformVideoDecoderOutputFactory
{
public:
	static FVideoDecoderOutput* Create();
};


class FElectraRendererVideo : public Electra::IMediaRenderer, public TSharedFromThis<FElectraRendererVideo, ESPMode::ThreadSafe>
{
public:
	struct SystemConfiguration
	{
		SystemConfiguration();
	};

	static bool Startup(const SystemConfiguration& Configuration);
	static void Shutdown(void);

	FElectraRendererVideo(TSharedPtr<FElectraPlayer, ESPMode::ThreadSafe> InPlayer);
	virtual ~FElectraRendererVideo();

	struct InstanceConfiguration
	{
		InstanceConfiguration()
		{
			// todo
		}
	};

	enum OpenError
	{
		eOpen_Ok,
		eOpen_Error,
	};

	OpenError Open(const InstanceConfiguration& Config);
	void Close(void);

	void PrepareForDecoderShutdown();

	//-------------------------------------------------------------------------
	// Methods for UE interface
	//
	void TickInput(FTimespan DeltaTime, FTimespan Timecode);
	void DetachPlayer();


	//-------------------------------------------------------------------------
	// Methods for Electra::IMediaRenderer
	//

	// Returns the properties of the buffer pool. Those properties should not change
	const Electra::FParamDict& GetBufferPoolProperties() const override;

	// Create a buffer pool from where a decoder can get the block of memory to decode into.
	UEMediaError CreateBufferPool(const Electra::FParamDict& Parameters) override;

	// Asks for a sample buffer from the buffer pool created previously through CreateBufferPool()
	UEMediaError AcquireBuffer(IBuffer*& OutBuffer, int32 TimeoutInMicroseconds, const Electra::FParamDict& InParameters) override;

	// Releases the buffer for rendering and subsequent return to the buffer pool
	UEMediaError ReturnBuffer(IBuffer* Buffer, bool bRender, const Electra::FParamDict& InSampleProperties) override;

	// Informs that the decoder is done with this pool. NO FREE!!!
	UEMediaError ReleaseBufferPool() override;

	bool CanReceiveOutputFrames(uint64 NumFrames) const override;

	// Receives the render clock we need to update with the most recently rendered sample's timestamp.
	void SetRenderClock(TSharedPtr<Electra::IMediaRenderClock, ESPMode::ThreadSafe> RenderClock) override;

	// Called if this renderer is being wrapped by another renderer.
	void SetParentRenderer(TWeakPtr<IMediaRenderer, ESPMode::ThreadSafe> ParentRenderer) override;

	// Sets the next expected sample's approximate presentation time stamp
	void SetNextApproximatePresentationTime(const Electra::FTimeValue& NextApproxPTS) override;

	// Flushes all pending buffers not yet rendered
	UEMediaError Flush(const Electra::FParamDict& InOptions) override;

	// Begins rendering of the first sample buffer
	void StartRendering(const Electra::FParamDict& InOptions) override;

	// Stops rendering of sample buffers
	void StopRendering(const Electra::FParamDict& InOptions) override;

	//! Tick any output buffer logic that might need regular updates
	void TickOutputBufferPool() override;

	//! Called from FElectraPlayerVideoDecoderOutput[Platform] when texture is returned to pool
	void SampleReleasedToPool(IDecoderOutput* InDecoderOutput) override;

private:


	TWeakPtr<FElectraPlayer, ESPMode::ThreadSafe> Player;
	TWeakPtr<IMediaRenderer, ESPMode::ThreadSafe> ParentRenderer;

	TDecoderOutputObjectPool<FVideoDecoderOutput, FElectraPlayerPlatformVideoDecoderOutputFactory> DecoderOutputPool;
	int32 NumOutputTexturesInUse;


	class FMediaBufferSharedPtrWrapper : public Electra::IMediaRenderer::IBuffer
	{
	public:
		explicit FMediaBufferSharedPtrWrapper(const FVideoDecoderOutputPtr& InDecoderOutput)
			: DecoderOutput(InDecoderOutput)
		{ }

		~FMediaBufferSharedPtrWrapper() = default;

		const Electra::FParamDict& GetBufferProperties() const override
		{
			return BufferProperties;
		}
		Electra::FParamDict& GetMutableBufferProperties() override
		{
			return BufferProperties;
		}

		Electra::FParamDict		BufferProperties;
		FVideoDecoderOutputPtr	DecoderOutput;
	};




	void AcquireFromPool(FVideoDecoderOutputPtr& DelayedImage);


	// These queues are all single-producer, single-consumer which is the default mode...
	TQueue<FVideoDecoderOutputPtr> QueueTickedAndWaitingForDecoder;

	Electra::FParamDict BufferPoolProperties;
	int32 NumBuffers;
	int32 NumBuffersAcquiredForDecoder;
	TSharedPtr<Electra::IMediaRenderClock, ESPMode::ThreadSafe>	RenderClock;
};


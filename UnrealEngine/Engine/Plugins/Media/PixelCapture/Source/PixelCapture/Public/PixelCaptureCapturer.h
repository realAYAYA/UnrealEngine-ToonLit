// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPixelCaptureInputFrame.h"
#include "IPixelCaptureOutputFrame.h"

namespace UE::PixelCapture
{
	class FOutputFrameBuffer;
}

/**
 * The base class for all Capturers in the system.
 * Incoming frames will be user types implementing IPixelCaptureInputFrame.
 * Outgoing frames should be user types implementing IPixelCaptureOutputFrame.
 * Each capturer system should expect one known input user type.
 * Implement CreateOutputBuffer to create your custom IPixelCaptureOutputFrame
 * implementation to hold the result of the capture process.
 * Implement BeginProcess to start the capture work which ideally should be
 * an async task of some sort.
 * The capture work should fill the given IPixelCaptureOutputFrame and then
 * call EndProcess to indicate the work is done.
 * While the capture should be async it should only expect to work on one
 * frame at a time.
 */
class PIXELCAPTURE_API FPixelCaptureCapturer
{
public:
	FPixelCaptureCapturer();
	virtual ~FPixelCaptureCapturer();

	/**
	 * Called when an input frame needs capturing.
	 * @param InputFrame The input frame to be captured.
	 */
	void Capture(const IPixelCaptureInputFrame& InputFrame);

	/**
	 * Returns true if Initialize() has been called.
	 * Output data can depend on the incoming frames so we do lazy initialization when we first consume.
	 * @return True if this process has been initialized correctly.
	 */
	bool IsInitialized() const { return bInitialized; }

	/**
	 * Returns true when this process is actively working on capturing frame data.
	 * @return True when this process is busy.
	 */
	bool IsBusy() const { return bBusy; }

	/**
	 * Returns true if this process has a frame in the output buffer ready to be read.
	 * @return True when this process has output data.
	 */
	bool HasOutput() const { return bHasOutput; }

	/**
	 * Gets the output frame from the output buffer.
	 * @return The output data of this process.
	 */
	TSharedPtr<IPixelCaptureOutputFrame> ReadOutput();

	/**
	 * Listen on this to be notified when the capture process completes for each input.
	 */
	DECLARE_MULTICAST_DELEGATE(FOnComplete);
	FOnComplete OnComplete;

protected:
	/**
	 * Gets the human readable name for this capture process. This name will be used in stats
	 * readouts so the shorter the better.
	 * @return A human readable name for this capture process.
	 */
	virtual FString GetCapturerName() const = 0;

	/**
	 * Initializes the process to be ready for work. Called once at startup.
	 * @param InputWidth The pixel count of the input frame width
	 * @param InputHeight The pixel count of the input frame height
	 */
	virtual void Initialize(int32 InputWidth, int32 InputHeight);

	/**
	 * Implement this to create a buffer for the output.
	 * @param InputWidth The pixel width of the input frame.
	 * @param InputHeight The pixel height of the input frame.
	 * @return An empty output structure that the process can store the output of its process on.
	 */
	virtual IPixelCaptureOutputFrame* CreateOutputBuffer(int32 InputWidth, int32 InputHeight) = 0;

	/**
	 * Implement this with your specific process to capture the incoming frame.
	 * @param InputFrame The input frame data for the process to begin working on.
	 * @param OutputBuffer The destination buffer for the process. Is guaranteed to be of the type created in CreateOutputBuffer()
	 */
	virtual void BeginProcess(const IPixelCaptureInputFrame& InputFrame, IPixelCaptureOutputFrame* OutputBuffer) = 0;

	/**
	 * Metadata info (optional). Marks the start of some CPU work. Multiple work sections are valid.
	 */
	void MarkCPUWorkStart();

	/**
	 * Metadata info (optional). Marks the end of some CPU work.
	 */
	void MarkCPUWorkEnd();

	/**
	 * Metadata info (optional). Marks the start of some GPU work. Multiple work sections are valid.
	 */
	void MarkGPUWorkStart();

	/**
	 * Metadata info (optional). Marks the end of some GPU work.
	 */
	void MarkGPUWorkEnd();

	/**
	 * Call this to mark the end of processing. Will commit the current write buffer into the read buffer.
	 */
	void EndProcess();

private:
	bool bInitialized = false;
	bool bBusy = false;
	bool bHasOutput = false;

	int32 ExpectedInputWidth = 0;
	int32 ExpectedInputHeight = 0;

	uint64 StartTime;
	uint64 CPUStartTime;
	uint64 GPUStartTime;

	TUniquePtr<UE::PixelCapture::FOutputFrameBuffer> Buffer;
	TSharedPtr<IPixelCaptureOutputFrame> CurrentOutputBuffer;

	void InitMetadata(FPixelCaptureFrameMetadata InputFrame);
	void FinalizeMetadata();
};

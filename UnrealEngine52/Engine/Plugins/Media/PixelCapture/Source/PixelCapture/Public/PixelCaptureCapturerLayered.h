// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPixelCaptureCapturerSource.h"

/**
 * A capturer that contains multiple layers of differently scaled capture processes.
 * Feed it a IPixelCaptureCapturerSource that will create the appropriate base capturers.
 * Input: User defined
 * Output: Capturer defined
 */
class FPixelCaptureCapturerLayered : public TSharedFromThis<FPixelCaptureCapturerLayered>
{
public:
	/**
	 * Create a new Layered Capturer.
	 * @param InCapturerSource A source for capturers for each layer.
	 * @param InDestinationFormat The format to capture to.
	 * @param LayerScales A list of scales for each layer.
	 */
	static TSharedPtr<FPixelCaptureCapturerLayered> Create(IPixelCaptureCapturerSource* InCapturerSource, int32 InDestinationFormat, TArray<float> LayerScales);
	virtual ~FPixelCaptureCapturerLayered() = default;

	/**
	 * Begins the capture process of a given frame.
	 */
	void Capture(const IPixelCaptureInputFrame& SourceFrame);

	/**
	 * Try to read the result of the capture process. May return null if no output
	 * has been captured yet.
	 * @param LayerIndex The layer to try and read the output from.
	 * @return The captured frame layer if one exists. Null otherwise.
	 */
	TSharedPtr<IPixelCaptureOutputFrame> ReadOutput(int32 LayerIndex);

	/**
	 * A callback to broadcast on when the frame has completed the capture process.
	 * Called once when all layers of a given input frame have completed.
	 */
	DECLARE_MULTICAST_DELEGATE(FOnComplete);
	FOnComplete OnComplete;

protected:
	FPixelCaptureCapturerLayered(IPixelCaptureCapturerSource* InCapturerSource, int32 InDestinationFormat, TArray<float> LayerScales);

	void AddLayer(float Scale);
	void OnCaptureComplete();

	IPixelCaptureCapturerSource* CapturerSource;
	int32 DestinationFormat;
	TArray<float> LayerScales;
	TArray<TSharedPtr<FPixelCaptureCapturer>> LayerCapturers;
	TAtomic<int> PendingLayers = 0; // atomic because the complete events can come in on multiple threads
	mutable FCriticalSection LayersGuard;
};

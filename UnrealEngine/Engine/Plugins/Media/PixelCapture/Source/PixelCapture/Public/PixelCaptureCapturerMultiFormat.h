// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPixelCaptureCapturerSource.h"
#include "PixelCaptureCapturerLayered.h"

/**
 * A capturer that contains multiple formats of multi layer capture processes.
 * Feed it a IPixelCaptureCapturerSource that will create the appropriate base capturers.
 * Input: User defined
 * Output: Capturer defined
 */
class PIXELCAPTURE_API FPixelCaptureCapturerMultiFormat : public TSharedFromThis<FPixelCaptureCapturerMultiFormat>
{
public:
	/**
	 * Create a new multi-format multi-Layered Capturer.
	 * @param InCapturerSource A source for capturers for each layer.
	 * @param LayerScales A list of scales for each layer.
	 */
	static TSharedPtr<FPixelCaptureCapturerMultiFormat> Create(IPixelCaptureCapturerSource* InCapturerSource, TArray<float> LayerScales);
	virtual ~FPixelCaptureCapturerMultiFormat();

	/**
	 * Gets the number of layers in the multi-layered capturers.
	 * @return The number of layers in each multi-layered capturer.
	 */
	int32 GetNumLayers() const { return LayerScales.Num(); }

	/**
	 * Gets the frame width of a given output layer.
	 * @return The pixel count of the width of a output layer.
	 */
	int32 GetWidth(int LayerIndex) const;

	/**
	 * Gets the frame height of a given output layer.
	 * @return The pixel count of the height of a output layer.
	 */
	int32 GetHeight(int LayerIndex) const;

	/**
	 * Begins the capture process of a given frame.
	 */
	void Capture(const IPixelCaptureInputFrame& SourceFrame);

	/**
	 * Sets up a capture pipeline for the given destination format. No effect if the
	 * pipeline already exists.
	 * @param Format The destination format for the requested pipeline.
	 */
	void AddOutputFormat(int32 Format);

	/**
	 * Requests the output in a specific format. If this is the first request for
	 * the format and AddOutputFormat has not been called for the format then this
	 * call will return nullptr. Otherwise will return the buffer for the format
	 * provided the capture has completed.
	 * @param Format The format we want the output in.
	 * @param LayerIndex The layer we want to get the output of.
	 * @return The final buffer of a given format if it exists or null.
	 */
	TSharedPtr<IPixelCaptureOutputFrame> RequestFormat(int32 Format, int32 LayerIndex);

	/**
	 * Like RequestFormat except if the format does not exist it will add it and then
	 * wait for the format to have output.
	 * NOTE: This will block the calling thread so it is important that the capture
	 * process is not dependent on this calling thread or we will deadlock.
	 * @param Format The format we want the output in.
	 * @param LayerIndex The layer we want to get the output of.
	 * @param MaxWaitTime The max number of milliseconds to wait for a frame. Default is MAX_uint32 (forever).
	 * @return The final buffer of a given format or null in case of timeout or the capturer has been disconnected.
	 */
	TSharedPtr<IPixelCaptureOutputFrame> WaitForFormat(int32 Format, int32 LayerIndex, uint32 MaxWaitTime = MAX_uint32);

	/**
	 * Call to notify this capturer that it has been disconnected and no more frames
	 * will be captured and any waiting for format calls should stop waiting.
	 */
	void OnDisconnected();

	/**
	 * Listen on this to be notified when a frame completes all capture formats/layers.
	 */
	DECLARE_MULTICAST_DELEGATE(FOnComplete);
	FOnComplete OnComplete;

protected:
	FPixelCaptureCapturerMultiFormat(IPixelCaptureCapturerSource* InCapturerSource, TArray<float> InLayerScales);
	void OnCaptureFormatComplete(int32 Format);
	
	FEvent* GetEventForFormat(int32 Format);
	void CheckFormatEvent(int32 Format);
	void FreeEvent(int32 Format, FEvent* Event);
	void FlushWaitingEvents();

	IPixelCaptureCapturerSource* CapturerSource;
	TArray<float> LayerScales;
	TArray<FIntPoint> LayerSizes;
	TMap<int32, TSharedPtr<FPixelCaptureCapturerLayered>> FormatCapturers;
	TAtomic<int> PendingFormats = 0; // atomic because the complete events can come in on multiple threads
	mutable FCriticalSection FormatGuard;

	FCriticalSection EventMutex;
	TMap<int32, FEvent*> FormatEvents;

	bool bDisconnected = false;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPixelCaptureCapturerSource.h"
#include "PixelCaptureCapturerMultiFormat.h"
#include "Delegates/IDelegateInstance.h"
#include "PixelStreamingWebRTCIncludes.h"

/**
 * The input of the Pixel Streaming system. Frames enter the system when OnFrame is called.
 * The input buffers might need to be converted/adapted to suit the selected video encoder.
 * CreateAdaptProcess should create a PixelStreamingFrameAdapterProcess that does this work.
 */
class PIXELSTREAMING_API FPixelStreamingVideoInput : public IPixelCaptureCapturerSource
{
public:
	FPixelStreamingVideoInput();
	virtual ~FPixelStreamingVideoInput() = default;

	void AddOutputFormat(int32 Format);

	/**
	 * Feed the input with a new captured frame.
	 * @param InputFrame The raw input frame.
	 */
	virtual void OnFrame(const IPixelCaptureInputFrame& InputFrame);

	bool IsReady() const { return Ready; }

	/**
	 * Used internally by the encode process.
	 */
	rtc::scoped_refptr<webrtc::VideoFrameBuffer> GetFrameBuffer();

	/**
	 * 
	 */
	TSharedPtr<IPixelCaptureOutputFrame> RequestFormat(int32 Format, int32 LayerIndex = -1);

	/**
	 * This is broadcast each time a frame exits the adapt process. Used to synchronize framerates with input rates.
	 * This should be called once per frame taking into consideration all the target formats and layers within the frame.
	 */
	DECLARE_MULTICAST_DELEGATE(FOnFrameCaptured);
	FOnFrameCaptured OnFrameCaptured;
	
private:
	int32 LastFrameWidth = -1;
	int32 LastFrameHeight = -1;
	bool Ready = false;

	TSharedPtr<FPixelCaptureCapturerMultiFormat> FrameCapturer;
	FDelegateHandle CaptureCompleteHandle;
	TSet<int32> PreInitFormats;

	void CreateFrameCapturer();
	void OnCaptureComplete();
};

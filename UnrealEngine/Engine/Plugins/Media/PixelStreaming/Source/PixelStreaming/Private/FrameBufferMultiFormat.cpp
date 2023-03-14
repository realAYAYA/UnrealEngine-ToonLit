// Copyright Epic Games, Inc. All Rights Reserved.

#include "FrameBufferMultiFormat.h"

namespace UE::PixelStreaming
{
	FFrameBufferMultiFormatBase::FFrameBufferMultiFormatBase(TSharedPtr<FPixelCaptureCapturerMultiFormat> InFrameCapturer)
		: FrameCapturer(InFrameCapturer)
	{
	}

	FFrameBufferMultiFormatLayered::FFrameBufferMultiFormatLayered(TSharedPtr<FPixelCaptureCapturerMultiFormat> InFrameCapturer)
		: FFrameBufferMultiFormatBase(InFrameCapturer)
	{
	}

	int FFrameBufferMultiFormatLayered::width() const
	{
		return FrameCapturer->GetWidth(GetNumLayers() - 1);
	}

	int FFrameBufferMultiFormatLayered::height() const
	{
		return FrameCapturer->GetHeight(GetNumLayers() - 1);
	}

	int FFrameBufferMultiFormatLayered::GetNumLayers() const
	{
		return FrameCapturer->GetNumLayers();
	}

	rtc::scoped_refptr<FFrameBufferMultiFormat> FFrameBufferMultiFormatLayered::GetLayer(int LayerIndex) const
	{
		return new rtc::RefCountedObject<FFrameBufferMultiFormat>(FrameCapturer, LayerIndex);
	}

	FFrameBufferMultiFormat::FFrameBufferMultiFormat(TSharedPtr<FPixelCaptureCapturerMultiFormat> InFrameCapturer, int InLayerIndex)
		: FFrameBufferMultiFormatBase(InFrameCapturer)
		, LayerIndex(InLayerIndex)
	{
	}

	int FFrameBufferMultiFormat::width() const
	{
		return FrameCapturer->GetWidth(LayerIndex);
	}

	int FFrameBufferMultiFormat::height() const
	{
		return FrameCapturer->GetHeight(LayerIndex);
	}

	IPixelCaptureOutputFrame* FFrameBufferMultiFormat::RequestFormat(int32 Format) const
	{
		// ensure this frame buffer will always refer to the same frame
		if (TSharedPtr<IPixelCaptureOutputFrame>* CachedFrame = CachedFormat.Find(Format))
		{
			return CachedFrame->Get();
		}
		TSharedPtr<IPixelCaptureOutputFrame> Frame = FrameCapturer->WaitForFormat(Format, LayerIndex);
		CachedFormat.Add(Format, Frame);
		return Frame.Get();
	}
} // namespace UE::PixelStreaming

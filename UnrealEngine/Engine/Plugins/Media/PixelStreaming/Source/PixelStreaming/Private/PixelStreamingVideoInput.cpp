// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingVideoInput.h"
#include "Settings.h"
#include "FrameBufferMultiFormat.h"

FPixelStreamingVideoInput::FPixelStreamingVideoInput()
{
	CreateFrameCapturer();
}

void FPixelStreamingVideoInput::AddOutputFormat(int32 Format)
{
	PreInitFormats.Add(Format);
	FrameCapturer->AddOutputFormat(Format);
}

void FPixelStreamingVideoInput::OnFrame(const IPixelCaptureInputFrame& InputFrame)
{
	if (LastFrameWidth != -1 && LastFrameHeight != -1)
	{
		if (InputFrame.GetWidth() != LastFrameWidth || InputFrame.GetHeight() != LastFrameHeight)
		{
			CreateFrameCapturer();
		}
	}

	Ready = true;
	FrameCapturer->Capture(InputFrame);
	LastFrameWidth = InputFrame.GetWidth();
	LastFrameHeight = InputFrame.GetHeight();
}

rtc::scoped_refptr<webrtc::VideoFrameBuffer> FPixelStreamingVideoInput::GetFrameBuffer()
{
	return new rtc::RefCountedObject<UE::PixelStreaming::FFrameBufferMultiFormatLayered>(FrameCapturer);
}

TSharedPtr<IPixelCaptureOutputFrame> FPixelStreamingVideoInput::RequestFormat(int32 Format, int32 LayerIndex)
{
	if (FrameCapturer != nullptr)
	{
		return FrameCapturer->RequestFormat(Format, LayerIndex);
	}
	return nullptr;
}

void FPixelStreamingVideoInput::CreateFrameCapturer()
{
	if (FrameCapturer != nullptr)
	{
		FrameCapturer->OnDisconnected();
		FrameCapturer->OnComplete.Remove(CaptureCompleteHandle);
		FrameCapturer = nullptr;
	}

	TArray<float> LayerScaling;

	for (auto& Layer : UE::PixelStreaming::Settings::SimulcastParameters.Layers)
	{
		LayerScaling.Add(1.0f / Layer.Scaling);
	}
	LayerScaling.Sort([](float ScaleA, float ScaleB) { return ScaleA < ScaleB; });

	FrameCapturer = FPixelCaptureCapturerMultiFormat::Create(this, LayerScaling);
	CaptureCompleteHandle = FrameCapturer->OnComplete.AddRaw(this, &FPixelStreamingVideoInput::OnCaptureComplete);

	for (auto& Format : PreInitFormats)
	{
		FrameCapturer->AddOutputFormat(Format);
	}
}

void FPixelStreamingVideoInput::OnCaptureComplete()
{
	OnFrameCaptured.Broadcast();
}

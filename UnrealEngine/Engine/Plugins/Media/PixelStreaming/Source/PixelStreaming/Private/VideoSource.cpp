// Copyright Epic Games, Inc. All Rights Reserved.

#include "VideoSource.h"
#include "Settings.h"
#include "FrameBufferMultiFormat.h"

namespace UE::PixelStreaming
{
	FVideoSource::FVideoSource(TSharedPtr<FPixelStreamingVideoInput> InVideoInput, const TFunction<bool()>& InShouldGenerateFramesCheck)
		: CurrentState(webrtc::MediaSourceInterface::SourceState::kInitializing)
		, VideoInput(InVideoInput)
		, ShouldGenerateFramesCheck(InShouldGenerateFramesCheck)
	{
	}

	void FVideoSource::MaybePushFrame()
	{
		if (VideoInput->IsReady() && ShouldGenerateFramesCheck())
		{
			PushFrame();
		}
	}

	void FVideoSource::PushFrame()
	{
		static int32 FrameId = 1;

		CurrentState = webrtc::MediaSourceInterface::SourceState::kLive;

		rtc::scoped_refptr<webrtc::VideoFrameBuffer> FrameBuffer = VideoInput->GetFrameBuffer();
		check(FrameBuffer->width() != 0 && FrameBuffer->height() != 0);
		webrtc::VideoFrame Frame = webrtc::VideoFrame::Builder()
									   .set_video_frame_buffer(FrameBuffer)
									   .set_timestamp_us(webrtc::Clock::GetRealTimeClock()->TimeInMicroseconds())
									   .set_rotation(webrtc::VideoRotation::kVideoRotation_0)
									   .set_id(FrameId++)
									   .build();
		OnFrame(Frame);
	}
} // namespace UE::PixelStreaming

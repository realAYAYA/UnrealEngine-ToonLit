// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WebRTCIncludes.h"

namespace UE::PixelStreaming
{
	class VideoDecoderVPX : public webrtc::VideoDecoder
	{
	public:
		VideoDecoderVPX(int Version);
		virtual ~VideoDecoderVPX() = default;

		virtual bool Configure(const Settings& settings) override;
		virtual int32 Decode(const webrtc::EncodedImage& input_image, bool missing_frames, int64_t render_time_ms) override;
		virtual int32 RegisterDecodeCompleteCallback(webrtc::DecodedImageCallback* callback) override;
		virtual int32 Release() override;
		virtual const char* ImplementationName() const { return "VideoDecoderVPX"; }

	private:
		std::unique_ptr<webrtc::VideoDecoder> VideoDecoder;
	};
} // namespace UE::PixelStreaming

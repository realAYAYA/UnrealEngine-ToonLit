// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WebRTCIncludes.h"

#include "Video/Decoders/Configs/VideoDecoderConfigH265.h"
#include "Video/Resources/VideoResourceRHI.h"

namespace UE::PixelStreaming
{
	class VideoDecoderH265 : public webrtc::VideoDecoder
	{
	public:
		VideoDecoderH265();
		virtual ~VideoDecoderH265() = default;

		virtual bool Configure(const Settings& settings) override;
		virtual int32 Decode(const webrtc::EncodedImage& input_image, bool missing_frames, int64_t render_time_ms) override;
		virtual int32 RegisterDecodeCompleteCallback(webrtc::DecodedImageCallback* callback) override;
		virtual int32 Release() override;
		virtual const char* ImplementationName() const { return "VideoDecoderH265"; }

	private:
		TSharedPtr<TVideoDecoder<FVideoResourceRHI, FVideoDecoderConfigH265>> Decoder;

		webrtc::DecodedImageCallback* Output = nullptr;

		uint32 FrameCount = 0;
	};
} // namespace UE::PixelStreaming

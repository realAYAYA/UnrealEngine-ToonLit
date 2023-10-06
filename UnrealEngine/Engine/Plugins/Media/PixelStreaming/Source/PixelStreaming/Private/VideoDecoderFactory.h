// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "WebRTCIncludes.h"

namespace UE::PixelStreaming
{
	class FVideoDecoderFactory : public webrtc::VideoDecoderFactory
	{
	public:
		virtual std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override;
		virtual std::unique_ptr<webrtc::VideoDecoder> CreateVideoDecoder(const webrtc::SdpVideoFormat& format) override;
	};
} // namespace UE::PixelStreaming

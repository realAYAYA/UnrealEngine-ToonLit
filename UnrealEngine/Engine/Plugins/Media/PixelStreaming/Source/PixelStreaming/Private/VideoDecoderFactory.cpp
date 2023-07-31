// Copyright Epic Games, Inc. All Rights Reserved.
#include "VideoDecoderFactory.h"
#include "Utils.h"
#include "VideoDecoderStub.h"
#include "absl/strings/match.h"
#include "VideoDecoderVPX.h"

namespace UE::PixelStreaming
{
	std::vector<webrtc::SdpVideoFormat> FVideoDecoderFactory::GetSupportedFormats() const
	{
		std::vector<webrtc::SdpVideoFormat> video_formats;
		video_formats.push_back(webrtc::SdpVideoFormat(cricket::kVp8CodecName));
		video_formats.push_back(webrtc::SdpVideoFormat(cricket::kVp9CodecName));
		// uncomment these when we get a h264 decoder
// #if WEBRTC_VERSION == 84
// 		video_formats.push_back(CreateH264Format(webrtc::H264::kProfileConstrainedBaseline, webrtc::H264::kLevel3_1));
// #elif WEBRTC_VERSION == 96
// 		video_formats.push_back(CreateH264Format(webrtc::H264Profile::kProfileConstrainedBaseline, webrtc::H264Level::kLevel3_1));
// #endif
		return video_formats;
	}

	std::unique_ptr<webrtc::VideoDecoder> FVideoDecoderFactory::CreateVideoDecoder(const webrtc::SdpVideoFormat& format)
	{
		if (absl::EqualsIgnoreCase(format.name, cricket::kVp8CodecName))
		{
			return std::make_unique<VideoDecoderVPX>(8);
		}
		else if (absl::EqualsIgnoreCase(format.name, cricket::kVp9CodecName))
		{
			return std::make_unique<VideoDecoderVPX>(9);
		}
		return std::make_unique<FVideoDecoderStub>();
	}
} // namespace UE::PixelStreaming

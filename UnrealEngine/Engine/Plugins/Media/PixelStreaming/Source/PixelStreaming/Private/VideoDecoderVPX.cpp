// Copyright Epic Games, Inc. All Rights Reserved.

#include "VideoDecoderVPX.h"
#include "PixelStreamingPrivate.h"


namespace UE::PixelStreaming
{
	VideoDecoderVPX::VideoDecoderVPX(int Version)
	{
		if (Version == 8)
		{
			VideoDecoder = webrtc::VP8Decoder::Create();
		}
		else if (Version == 9)
		{
			VideoDecoder = webrtc::VP9Decoder::Create();
		}
		else
		{
			UE_LOG(LogPixelStreaming, Fatal, TEXT("Unknown VPX decoder version %d"), Version);
		}
	}

#if WEBRTC_VERSION == 84
	int32 VideoDecoderVPX::InitDecode(const webrtc::VideoCodec* codec_settings, int32 number_of_cores)
	{
		return VideoDecoder->InitDecode(codec_settings, number_of_cores);
	}
#elif WEBRTC_VERSION == 96
	bool VideoDecoderVPX::Configure(const Settings& settings)
	{
		return VideoDecoder->Configure(settings);
	}
#endif

	int32 VideoDecoderVPX::Decode(const webrtc::EncodedImage& input_image, bool missing_frames, int64_t render_time_ms)
	{
		return VideoDecoder->Decode(input_image, missing_frames, render_time_ms);
	}

	int32 VideoDecoderVPX::RegisterDecodeCompleteCallback(webrtc::DecodedImageCallback* callback)
	{
		return VideoDecoder->RegisterDecodeCompleteCallback(callback);
	}

	int32 VideoDecoderVPX::Release()
	{
		return VideoDecoder->Release();
	}
} // namespace UE::PixelStreaming

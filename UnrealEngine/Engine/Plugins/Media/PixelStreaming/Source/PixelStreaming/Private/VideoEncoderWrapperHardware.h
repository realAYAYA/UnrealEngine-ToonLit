// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "VideoEncoder.h"
#include "EncoderFrameFactory.h"

namespace webrtc
{
	class VideoFrame;
} // namespace webrtc

namespace UE::PixelStreaming 
{
	class FVideoEncoderFactorySingleLayer;
	
	/**
	 * A Pixel Streaming wrapper around an AVEncoder hardware encoder. Utilized by FVideoEncoderSingleLayerH264.
	 */
	class FVideoEncoderWrapperHardware
	{
	public:
		FVideoEncoderWrapperHardware(TUniquePtr<FEncoderFrameFactory> FrameFactory, TUniquePtr<AVEncoder::FVideoEncoder> Encoder);
		~FVideoEncoderWrapperHardware();

		void SetForceNextKeyframe() { bForceNextKeyframe = true; }

		void Encode(const webrtc::VideoFrame& WebRTCFrame, bool bKeyframe);

		AVEncoder::FVideoEncoder::FLayerConfig GetCurrentConfig();
		void SetConfig(const AVEncoder::FVideoEncoder::FLayerConfig& NewConfig);

		static void OnEncodedPacket(FVideoEncoderFactorySingleLayer* Factory, 
			uint32 InLayerIndex, const TSharedPtr<AVEncoder::FVideoEncoderInputFrame> InFrame, 
			const AVEncoder::FCodecPacket& InPacket);

	private:
		TUniquePtr<FEncoderFrameFactory> FrameFactory;
		TUniquePtr<AVEncoder::FVideoEncoder> Encoder;
		bool bForceNextKeyframe = false;
	};
} // namespace UE::PixelStreaming
// Copyright Epic Games, Inc. All Rights Reserved.

#include "VideoEncoderFactoryLayered.h"
#include "Settings.h"
#include "absl/strings/match.h"
#include "WebRTCIncludes.h"
#include "VideoEncoderLayered.h"
#include "VideoEncoderFactorySingleLayer.h"

namespace UE::PixelStreaming
{
	FVideoEncoderFactoryLayered::FVideoEncoderFactoryLayered()
		: PrimaryEncoderFactory(MakeUnique<FVideoEncoderFactorySingleLayer>())
	{
		FScopeLock Lock(&EncoderFactoriesGuard);
		EncoderFactories.SetNum(Settings::SimulcastParameters.Layers.Num());
		for (int i = 0; i < Settings::SimulcastParameters.Layers.Num(); i++)
		{
			EncoderFactories[i] = MakeUnique<FVideoEncoderFactorySingleLayer>();
		}
	}

	FVideoEncoderFactoryLayered::~FVideoEncoderFactoryLayered()
	{
	}

	std::unique_ptr<webrtc::VideoEncoder> FVideoEncoderFactoryLayered::CreateVideoEncoder(const webrtc::SdpVideoFormat& format)
	{
		return std::make_unique<FVideoEncoderLayered>(*this, format);
	}

	FVideoEncoderFactorySingleLayer* FVideoEncoderFactoryLayered::GetEncoderFactory(int StreamIndex)
	{
		FScopeLock Lock(&EncoderFactoriesGuard);
		return EncoderFactories[StreamIndex].Get();
	}

	void FVideoEncoderFactoryLayered::ForceKeyFrame()
	{
		FScopeLock Lock(&EncoderFactoriesGuard);
		for (auto&& Encoder : EncoderFactories)
		{
			Encoder->ForceKeyFrame();
		}
	}

	std::vector<webrtc::SdpVideoFormat> FVideoEncoderFactoryLayered::GetSupportedFormats() const
	{
		return PrimaryEncoderFactory->GetSupportedFormats();
	}

	FVideoEncoderFactoryLayered::CodecInfo FVideoEncoderFactoryLayered::QueryVideoEncoder(const webrtc::SdpVideoFormat& format) const
	{
		return PrimaryEncoderFactory->QueryVideoEncoder(format);
	}
} // namespace UE::PixelStreaming

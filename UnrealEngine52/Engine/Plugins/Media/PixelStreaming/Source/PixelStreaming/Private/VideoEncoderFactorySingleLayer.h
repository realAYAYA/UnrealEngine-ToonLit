// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "WebRTCIncludes.h"

#include "Video/VideoEncoder.h"
#include "Video/Encoders/Configs/VideoEncoderConfigH264.h"
#include "Video/Encoders/Configs/VideoEncoderConfigH265.h"
#include "Video/Resources/VideoResourceRHI.h"

#include "PixelStreamingPrivate.h"

namespace UE::PixelStreaming
{
	using FVideoEncoderHardware = TVideoEncoder<FVideoResourceRHI>;

	class FVideoEncoderSingleLayerHardware;

	/**
	 * An encoder factory for a single layer. Do not use this directly. Use FVideoEncoderFactoryLayered even
	 * when not using simulcast specifically.
	 */
	class PIXELSTREAMING_API FVideoEncoderFactorySingleLayer : public webrtc::VideoEncoderFactory
	{
	public:
		FVideoEncoderFactorySingleLayer() = default;
		virtual ~FVideoEncoderFactorySingleLayer() override = default;

		virtual std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override;

		// Always returns our H264 hardware encoders codec_info for now/
		virtual CodecInfo QueryVideoEncoder(const webrtc::SdpVideoFormat& Format) const override;

		// Always returns our H264 hardware encoder for now.
		virtual std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(const webrtc::SdpVideoFormat& Format) override;

		void ReleaseVideoEncoder(FVideoEncoderSingleLayerHardware* Encoder);

		void ForceKeyFrame();
		bool ShouldForceKeyframe() const;
		void UnforceKeyFrame();
		void OnEncodedImage(const webrtc::EncodedImage& Encoded_image, const webrtc::CodecSpecificInfo* CodecSpecificInfo, uint32 StreamId);

		template <typename ConfigType>
		TWeakPtr<TVideoEncoder<FVideoResourceRHI>> GetOrCreateHardwareEncoder(uint32 StreamId, const ConfigType& VideoConfig)
		{
			FScopeLock InitLock(&InitEncoderGuard);

			FreeUnusedEncoders();

			if (auto* ExistingEncoder = HardwareEncoders.Find(StreamId))
			{
				return *ExistingEncoder;
			}
			else
			{
				// Make the hardware encoder wrapper
				TSharedPtr<FVideoEncoderHardware> HardwareEncoder = FVideoEncoder::Create<FVideoResourceRHI>(FAVDevice::GetHardwareDevice(), VideoConfig);

				if (!HardwareEncoder.IsValid())
				{
					UE_LOG(LogPixelStreaming, Error, TEXT("Could not create encoder. Check encoder config or perhaps you used up all your HW encoders."));
					// We could not make the encoder, so indicate the id was not set successfully.
					return nullptr;
				}

				HardwareEncoders.Add(StreamId, HardwareEncoder);
				return HardwareEncoder;
			}
		}

	private:
		void FreeUnusedEncoders();
		bool CheckEncoderSessionAvailable() const;

		TMap<uint32, TSharedPtr<FVideoEncoderHardware>> HardwareEncoders;

		uint8 bForceNextKeyframe : 1;

		// Encoders assigned to each peer. Each one of these will be assigned to one of the hardware encoders
		// raw pointers here because the factory interface wants a unique_ptr here so we cant own the object.
		TArray<FVideoEncoderSingleLayerHardware*> ActiveEncoders;
		FCriticalSection ActiveEncodersGuard;

		// Init encoder guard
		FCriticalSection InitEncoderGuard;

		// A map of GPU device names and their respective Max # of Concurrent Sessions as defined here: https://developer.nvidia.com/video-encode-and-decode-gpu-support-matrix-new
		// If a device is not in this map, it has an unrestriced number of sessions
		static TMap<FString, uint8> MaxConcurrentNvEncSessionsMap;
	};
} // namespace UE::PixelStreaming

// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "WebRTCIncludes.h"
#include "VideoEncoder.h"
#include "VideoEncoderWrapperHardware.h"
#include "IPixelCaptureOutputFrame.h"

namespace UE::PixelStreaming
{
	class FVideoEncoderFactorySingleLayer;

	/**
	 * A H264 encoder for a single layer of a frame.
	 */
	class FVideoEncoderSingleLayerH264 : public webrtc::VideoEncoder
	{
	public:
		FVideoEncoderSingleLayerH264(FVideoEncoderFactorySingleLayer& InFactory);
		virtual ~FVideoEncoderSingleLayerH264() override;

		// WebRTC Interface
		virtual int InitEncode(webrtc::VideoCodec const* codec_settings, webrtc::VideoEncoder::Settings const& settings) override;
		virtual int32 RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* callback) override;
		virtual int32 Release() override;
		virtual int32 Encode(webrtc::VideoFrame const& frame, std::vector<webrtc::VideoFrameType> const* frame_types) override;
		virtual void SetRates(RateControlParameters const& parameters) override;
		virtual webrtc::VideoEncoder::EncoderInfo GetEncoderInfo() const override;

		// Note: These funcs can also be overriden but are not pure virtual
		// virtual void SetFecControllerOverride(FecControllerOverride* fec_controller_override) override;
		// virtual void OnPacketLossRateUpdate(float packet_loss_rate) override;
		// virtual void OnRttUpdate(int64_t rtt_ms) override;
		// virtual void OnLossNotification(const LossNotification& loss_notification) override;
		// End WebRTC Interface.

#if WEBRTC_VERSION == 84
		void SendEncodedImage(webrtc::EncodedImage const& encoded_image, webrtc::CodecSpecificInfo const* codec_specific_info, webrtc::RTPFragmentationHeader const* fragmentation);
#elif WEBRTC_VERSION == 96
		void SendEncodedImage(webrtc::EncodedImage const& encoded_image, webrtc::CodecSpecificInfo const* codec_specific_info);
#endif

	private:
		void UpdateConfig();
		void MaybeDumpFrame(webrtc::EncodedImage const& encoded_image);

		AVEncoder::FVideoEncoder::FLayerConfig CreateEncoderConfigFromCVars(AVEncoder::FVideoEncoder::FLayerConfig BaseEncoderConfig) const;

		void UpdateFrameMetadataPreEncode(IPixelCaptureOutputFrame& Frame);
		void UpdateFrameMetadataPostEncode(IPixelCaptureOutputFrame& Frame);
		webrtc::VideoFrame WrapAdaptedFrame(const webrtc::VideoFrame& ExistingFrame, const IPixelCaptureOutputFrame& AdaptedLayer);

		FVideoEncoderFactorySingleLayer& Factory;

		FVideoEncoderWrapperHardware* HardwareEncoder;

		// We store this so we can restore back to it if the user decides to use then stop using the PixelStreaming.Encoder.TargetBitrate CVar.
		int32 WebRtcProposedTargetBitrate = 5000000;

		webrtc::EncodedImageCallback* OnEncodedImageCallback = nullptr;

		// WebRTC may request a bitrate/framerate change using SetRates(), we only respect this if this encoder is actually encoding
		// so we use this optional object to store a rate change and act upon it when this encoder does its next call to Encode().
		TOptional<RateControlParameters> PendingRateChange;
	};
} // namespace UE::PixelStreaming

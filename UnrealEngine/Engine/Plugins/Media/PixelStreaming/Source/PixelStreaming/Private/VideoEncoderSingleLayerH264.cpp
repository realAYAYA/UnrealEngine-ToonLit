// Copyright Epic Games, Inc. All Rights Reserved.

#include "VideoEncoderSingleLayerH264.h"
#include "VideoEncoderFactorySingleLayer.h"
#include "VideoEncoderFactory.h"
#include "FrameBufferMultiFormat.h"
#include "Settings.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Stats.h"
#include "PixelStreamingPeerConnection.h"
#include "FrameBufferRHI.h"

#include "PixelCaptureOutputFrameRHI.h"
#include "PixelCaptureBufferFormat.h"

namespace UE::PixelStreaming
{
	FVideoEncoderSingleLayerH264::FVideoEncoderSingleLayerH264(FVideoEncoderFactorySingleLayer& InFactory)
		: Factory(InFactory)
	{
	}

	FVideoEncoderSingleLayerH264::~FVideoEncoderSingleLayerH264()
	{
		Factory.ReleaseVideoEncoder(this);
	}

	int FVideoEncoderSingleLayerH264::InitEncode(webrtc::VideoCodec const* InCodecSettings, VideoEncoder::Settings const& settings)
	{
		checkf(AVEncoder::FVideoEncoderFactory::Get().IsSetup(), TEXT("FVideoEncoderFactory not setup"));
		// Try and get the encoder. If it doesn't exist, create it.
		FVideoEncoderWrapperHardware* Encoder = Factory.GetOrCreateHardwareEncoder(InCodecSettings->width, InCodecSettings->height, InCodecSettings->maxBitrate, InCodecSettings->startBitrate, InCodecSettings->maxFramerate);

		if (Encoder != nullptr)
		{
			HardwareEncoder = Encoder;
			UpdateConfig();
			WebRtcProposedTargetBitrate = InCodecSettings->startBitrate;
			return WEBRTC_VIDEO_CODEC_OK;
		}
		else
		{
			return WEBRTC_VIDEO_CODEC_ERROR;
		}
	}

	int32 FVideoEncoderSingleLayerH264::RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* callback)
	{
		OnEncodedImageCallback = callback;
		return WEBRTC_VIDEO_CODEC_OK;
	}

	int32 FVideoEncoderSingleLayerH264::Release()
	{
		OnEncodedImageCallback = nullptr;
		return WEBRTC_VIDEO_CODEC_OK;
	}

	void FVideoEncoderSingleLayerH264::UpdateFrameMetadataPreEncode(IPixelCaptureOutputFrame& Frame)
	{
		FPixelCaptureFrameMetadata& FrameMetadata = Frame.Metadata;
		FrameMetadata.UseCount++;
		FrameMetadata.LastEncodeStartTime = FPlatformTime::Cycles64();
		if (FrameMetadata.UseCount == 1)
		{
			FrameMetadata.FirstEncodeStartTime = FrameMetadata.LastEncodeStartTime;
		}
	}

	void FVideoEncoderSingleLayerH264::UpdateFrameMetadataPostEncode(IPixelCaptureOutputFrame& Frame)
	{
		FPixelCaptureFrameMetadata& FrameMetadata = Frame.Metadata;
		FrameMetadata.LastEncodeEndTime = FPlatformTime::Cycles64();

		FStats::Get()->AddFrameTimingStats(FrameMetadata);
	}

	webrtc::VideoFrame FVideoEncoderSingleLayerH264::WrapAdaptedFrame(const webrtc::VideoFrame& ExistingFrame, const IPixelCaptureOutputFrame& AdaptedLayer)
	{
		webrtc::VideoFrame NewFrame(ExistingFrame);
		const FPixelCaptureOutputFrameRHI& RHILayer = StaticCast<const FPixelCaptureOutputFrameRHI&>(AdaptedLayer);
		rtc::scoped_refptr<FFrameBufferRHI> RHIBuffer = new rtc::RefCountedObject<FFrameBufferRHI>(RHILayer.GetFrameTexture());
		NewFrame.set_video_frame_buffer(RHIBuffer);
		return NewFrame;
	}

	int32 FVideoEncoderSingleLayerH264::Encode(webrtc::VideoFrame const& frame, std::vector<webrtc::VideoFrameType> const* frame_types)
	{
		if (HardwareEncoder == nullptr)
		{
			return WEBRTC_VIDEO_CODEC_ERROR;
		}

		const FFrameBufferMultiFormat* FrameBuffer = StaticCast<FFrameBufferMultiFormat*>(frame.video_frame_buffer().get());
		IPixelCaptureOutputFrame* AdaptedLayer = FrameBuffer->RequestFormat(PixelCaptureBufferFormat::FORMAT_RHI);

		if (AdaptedLayer == nullptr)
		{
			// probably the first request which starts the adapt pipeline for this format
			return WEBRTC_VIDEO_CODEC_OK;
		}

		UpdateConfig();

		UpdateFrameMetadataPreEncode(*AdaptedLayer);
		const bool bKeyframe = (frame_types && (*frame_types)[0] == webrtc::VideoFrameType::kVideoFrameKey);
		const webrtc::VideoFrame NewFrame = WrapAdaptedFrame(frame, *AdaptedLayer);
		HardwareEncoder->Encode(NewFrame, bKeyframe);
		UpdateFrameMetadataPostEncode(*AdaptedLayer);

		return WEBRTC_VIDEO_CODEC_OK;
	}

	// Pass rate control parameters from WebRTC to our encoder
	// This is how WebRTC can control the bitrate/framerate of the encoder.
	void FVideoEncoderSingleLayerH264::SetRates(RateControlParameters const& parameters)
	{
		PendingRateChange.Emplace(parameters);
	}

	webrtc::VideoEncoder::EncoderInfo FVideoEncoderSingleLayerH264::GetEncoderInfo() const
	{
		VideoEncoder::EncoderInfo info;
		info.supports_native_handle = true;
		info.is_hardware_accelerated = true;
		info.has_internal_source = false;
		info.supports_simulcast = false;
		info.implementation_name = TCHAR_TO_UTF8(*FString::Printf(TEXT("PIXEL_STREAMING_HW_ENCODER_%s"), GDynamicRHI->GetName()));

		const int LowQP = UE::PixelStreaming::Settings::CVarPixelStreamingWebRTCLowQpThreshold.GetValueOnAnyThread();
		const int HighQP = UE::PixelStreaming::Settings::CVarPixelStreamingWebRTCHighQpThreshold.GetValueOnAnyThread();
		info.scaling_settings = VideoEncoder::ScalingSettings(LowQP, HighQP);

		// if true means HW encoder must be perfect and drop frames itself etc
		info.has_trusted_rate_controller = false;

		return info;
	}

	void FVideoEncoderSingleLayerH264::UpdateConfig()
	{
		if (HardwareEncoder)
		{
			AVEncoder::FVideoEncoder::FLayerConfig NewConfig = HardwareEncoder->GetCurrentConfig();

			if (PendingRateChange.IsSet())
			{
				const RateControlParameters& RateChangeParams = PendingRateChange.GetValue();

				NewConfig.MaxFramerate = RateChangeParams.framerate_fps;

				// We store what WebRTC wants as the bitrate, even if we are overriding it, so we can restore back to it when user stops using CVar.
				WebRtcProposedTargetBitrate = RateChangeParams.bitrate.get_sum_kbps() * 1000;

				// Clear the rate change request
				PendingRateChange.Reset();
			}

			NewConfig = CreateEncoderConfigFromCVars(NewConfig);

			HardwareEncoder->SetConfig(NewConfig);
		}
	}

	AVEncoder::FVideoEncoder::FLayerConfig FVideoEncoderSingleLayerH264::CreateEncoderConfigFromCVars(AVEncoder::FVideoEncoder::FLayerConfig InEncoderConfig) const
	{
		// Change encoder settings through CVars
		const int32 MaxBitrateCVar = UE::PixelStreaming::Settings::CVarPixelStreamingEncoderMaxBitrate.GetValueOnAnyThread();
		const int32 TargetBitrateCVar = UE::PixelStreaming::Settings::CVarPixelStreamingEncoderTargetBitrate.GetValueOnAnyThread();
		const int32 MinQPCVar = UE::PixelStreaming::Settings::CVarPixelStreamingEncoderMinQP.GetValueOnAnyThread();
		const int32 MaxQPCVar = UE::PixelStreaming::Settings::CVarPixelStreamingEncoderMaxQP.GetValueOnAnyThread();
		const AVEncoder::FVideoEncoder::RateControlMode RateControlCVar = UE::PixelStreaming::Settings::GetRateControlCVar();
		const AVEncoder::FVideoEncoder::MultipassMode MultiPassCVar = UE::PixelStreaming::Settings::GetMultipassCVar();
		const bool bFillerDataCVar = UE::PixelStreaming::Settings::CVarPixelStreamingEnableFillerData.GetValueOnAnyThread();
		const AVEncoder::FVideoEncoder::H264Profile H264Profile = UE::PixelStreaming::Settings::GetH264Profile();

		InEncoderConfig.MaxBitrate = MaxBitrateCVar > -1 ? MaxBitrateCVar : InEncoderConfig.MaxBitrate;
		InEncoderConfig.TargetBitrate = TargetBitrateCVar > -1 ? TargetBitrateCVar : WebRtcProposedTargetBitrate;
		InEncoderConfig.QPMin = MinQPCVar;
		InEncoderConfig.QPMax = MaxQPCVar;
		InEncoderConfig.RateControlMode = RateControlCVar;
		InEncoderConfig.MultipassMode = MultiPassCVar;
		InEncoderConfig.FillData = bFillerDataCVar;
		InEncoderConfig.H264Profile = H264Profile;

		return InEncoderConfig;
	}

#if WEBRTC_VERSION == 84
	void FVideoEncoderSingleLayerH264::SendEncodedImage(webrtc::EncodedImage const& encoded_image, webrtc::CodecSpecificInfo const* codec_specific_info, webrtc::RTPFragmentationHeader const* fragmentation)
	{
		MaybeDumpFrame(encoded_image);
		if (OnEncodedImageCallback)
		{
			OnEncodedImageCallback->OnEncodedImage(encoded_image, codec_specific_info, fragmentation);
		}
	}
#elif WEBRTC_VERSION == 96
	void FVideoEncoderSingleLayerH264::SendEncodedImage(webrtc::EncodedImage const& encoded_image, webrtc::CodecSpecificInfo const* codec_specific_info)
	{
		MaybeDumpFrame(encoded_image);
		if (OnEncodedImageCallback)
		{
			OnEncodedImageCallback->OnEncodedImage(encoded_image, codec_specific_info);
		}
	}
#endif

	void FVideoEncoderSingleLayerH264::MaybeDumpFrame(webrtc::EncodedImage const& encoded_image)
	{
		// Dump H264 frames to file for debugging if CVar is turned on.
		if (UE::PixelStreaming::Settings::CVarPixelStreamingDebugDumpFrame.GetValueOnAnyThread())
		{
			static IFileHandle* FileHandle = nullptr;
			if (!FileHandle)
			{
				IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
				FString TempFilePath = FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("encoded_frame"), TEXT(".h264"));
				FileHandle = PlatformFile.OpenWrite(*TempFilePath);
				check(FileHandle);
			}

			FileHandle->Write(encoded_image.data(), encoded_image.size());
			FileHandle->Flush();
		}
	}
} // namespace UE::PixelStreaming
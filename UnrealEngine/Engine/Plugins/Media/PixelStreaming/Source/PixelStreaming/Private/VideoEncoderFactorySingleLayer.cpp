// Copyright Epic Games, Inc. All Rights Reserved.

#include "VideoEncoderFactorySingleLayer.h"
#include "absl/strings/match.h"
#include "VideoEncoderSingleLayerH264.h"
#include "VideoEncoderSingleLayerVPX.h"
#include "Settings.h"
#include "VideoEncoderFactory.h"
#include "PixelStreamingPrivate.h"
#include "Utils.h"

namespace
{
	// the list of each individual codec we have encoder support for
	const TArray<EPixelStreamingCodec> SupportedCodecList{ EPixelStreamingCodec::H264, EPixelStreamingCodec::VP8, EPixelStreamingCodec::VP9 };

	// mapping of codec to a list of video formats
	// done this way so we can order the list of formats based on selected codec in GetSupportedFormats
	TMap<EPixelStreamingCodec, std::vector<webrtc::SdpVideoFormat>> CreateSupportedFormatMap()
	{
		TMap<EPixelStreamingCodec, std::vector<webrtc::SdpVideoFormat>> Codecs;
		for (auto& Codec : SupportedCodecList)
		{
			Codecs.Add(Codec);
		}

		Codecs[EPixelStreamingCodec::VP8].push_back(webrtc::SdpVideoFormat(cricket::kVp8CodecName));
		Codecs[EPixelStreamingCodec::VP9].push_back(webrtc::SdpVideoFormat(cricket::kVp9CodecName));
#if WEBRTC_VERSION == 84
		Codecs[EPixelStreamingCodec::H264].push_back(UE::PixelStreaming::CreateH264Format(webrtc::H264::kProfileConstrainedBaseline, webrtc::H264::kLevel3_1));
		Codecs[EPixelStreamingCodec::H264].push_back(UE::PixelStreaming::CreateH264Format(webrtc::H264::kProfileBaseline, webrtc::H264::kLevel3_1));
#elif WEBRTC_VERSION == 96
		Codecs[EPixelStreamingCodec::H264].push_back(UE::PixelStreaming::CreateH264Format(webrtc::H264Profile::kProfileConstrainedBaseline, webrtc::H264Level::kLevel3_1));
		Codecs[EPixelStreamingCodec::H264].push_back(UE::PixelStreaming::CreateH264Format(webrtc::H264Profile::kProfileBaseline, webrtc::H264Level::kLevel3_1));
#endif
		return Codecs;
	}
} // namespace

namespace UE::PixelStreaming
{

	// we want this method to return all the formats we have encoders for but the selected codecs formats should be first in the list.
	// the reason for this is weird. when we receive video from another pixel streaming source, for some reason webrtc will query
	// the encoder factory on the receiving end and if it doesnt support the video we are receiving then transport_cc is not enabled
	// which leads to very low bitrate streams.
	std::vector<webrtc::SdpVideoFormat> FVideoEncoderFactorySingleLayer::GetSupportedFormats() const
	{

		// static so we dont create the list every time this is called since the list will not change during runtime.
		static TMap<EPixelStreamingCodec, std::vector<webrtc::SdpVideoFormat>> CodecMap = CreateSupportedFormatMap();

		// since this method is const we need to store this state statically. it means all instances will share this state
		// but that actually works in our favor since we're describing more about the plugin state than the actual
		// instance of this factory.
		static std::vector<webrtc::SdpVideoFormat> SupportedFormats;

		// If we are not negotiating codecs simply return just the one codec that is selected in UE
		if(!Settings::ShouldNegotiateCodecs())
		{
			const EPixelStreamingCodec SelectedCodec = UE::PixelStreaming::Settings::GetSelectedCodec();
			if (CodecMap.Contains(SelectedCodec))
			{
				for (auto& Format : CodecMap[SelectedCodec])
				{
					SupportedFormats.push_back(Format);
				}
			}
			else
			{
				UE_LOG(LogPixelStreaming, Error, TEXT("Selected codec was not a supported codec."));
			}
			return SupportedFormats;
		}

		static EPixelStreamingCodec LastSelectedCodec = EPixelStreamingCodec::Invalid;

		const EPixelStreamingCodec SelectedCodec = UE::PixelStreaming::Settings::GetSelectedCodec();
		if (LastSelectedCodec != SelectedCodec)
		{
			// build a new format list
			LastSelectedCodec = SelectedCodec;
			SupportedFormats.clear();

			// order the codecs so the selected is first
			TArray<EPixelStreamingCodec> OrderedCodecList;
			OrderedCodecList.Add(SelectedCodec);
			for (auto& SupportedCodec : SupportedCodecList)
			{
				if (SupportedCodec != SelectedCodec)
				{
					OrderedCodecList.Add(SupportedCodec);
				}
			}

			// now just add each of the formats in order
			for (auto& Codec : OrderedCodecList)
			{
				if (CodecMap.Contains(Codec))
				{
					for (auto& Format : CodecMap[Codec])
					{
						SupportedFormats.push_back(Format);
					}
				}
			}
		}

		return SupportedFormats;
	}

	FVideoEncoderFactorySingleLayer::CodecInfo FVideoEncoderFactorySingleLayer::QueryVideoEncoder(const webrtc::SdpVideoFormat& format) const
	{
		webrtc::VideoEncoderFactory::CodecInfo CodecInfo;
#if WEBRTC_VERSION == 84
		CodecInfo.is_hardware_accelerated = true;
#endif
		CodecInfo.has_internal_source = false;
		return CodecInfo;
	}

	std::unique_ptr<webrtc::VideoEncoder> FVideoEncoderFactorySingleLayer::CreateVideoEncoder(const webrtc::SdpVideoFormat& format)
	{
		if (absl::EqualsIgnoreCase(format.name, cricket::kVp8CodecName))
		{
			return std::make_unique<FVideoEncoderSingleLayerVPX>(8);
		}
		else if (absl::EqualsIgnoreCase(format.name, cricket::kVp9CodecName))
		{
			return std::make_unique<FVideoEncoderSingleLayerVPX>(9);
		}
		else
		{
			// Lock during encoder creation
			FScopeLock Lock(&ActiveEncodersGuard);
			auto VideoEncoder = std::make_unique<FVideoEncoderSingleLayerH264>(*this);
			ActiveEncoders.Add(VideoEncoder.get());
			return VideoEncoder;
		}
	}

#if WEBRTC_VERSION == 84
	void FVideoEncoderFactorySingleLayer::OnEncodedImage(const webrtc::EncodedImage& encoded_image, const webrtc::CodecSpecificInfo* codec_specific_info, const webrtc::RTPFragmentationHeader* fragmentation)
#elif WEBRTC_VERSION == 96
	void FVideoEncoderFactorySingleLayer::OnEncodedImage(const webrtc::EncodedImage& encoded_image, const webrtc::CodecSpecificInfo* codec_specific_info)
#endif
	{
		// Lock as we send encoded image to each encoder.
		FScopeLock Lock(&ActiveEncodersGuard);

		// Go through each encoder and send our encoded image to its callback
		for (FVideoEncoderSingleLayerH264* Encoder : ActiveEncoders)
		{
			Encoder->SendEncodedImage(encoded_image, codec_specific_info
#if WEBRTC_VERSION == 84
				,
				fragmentation
#endif
			);
		}
	}

	void FVideoEncoderFactorySingleLayer::ReleaseVideoEncoder(FVideoEncoderSingleLayerH264* Encoder)
	{
		// Lock during deleting an encoder
		FScopeLock Lock(&ActiveEncodersGuard);
		ActiveEncoders.Remove(Encoder);
	}

	FVideoEncoderWrapperHardware* FVideoEncoderFactorySingleLayer::GetOrCreateHardwareEncoder(int Width, int Height, int MaxBitrate, int TargetBitrate, int MaxFramerate)
	{
		FScopeLock InitLock(&InitEncoderGuard);

		if (HardwareEncoder != nullptr)
 		{
			return HardwareEncoder.Get();
		}
		else
		{
			// Make AVEncoder frame factory.
			TUniquePtr<FEncoderFrameFactory> FrameFactory = MakeUnique<FEncoderFrameFactory>();

			// Make the encoder config
			AVEncoder::FVideoEncoder::FLayerConfig EncoderConfig;
			EncoderConfig.Width = Width;
			EncoderConfig.Height = Height;
			EncoderConfig.MaxFramerate = MaxFramerate;
			EncoderConfig.TargetBitrate = TargetBitrate;
			EncoderConfig.MaxBitrate = MaxBitrate;

			// Make the actual AVEncoder encoder.
			const TArray<AVEncoder::FVideoEncoderInfo>& Available = AVEncoder::FVideoEncoderFactory::Get().GetAvailable();
			TUniquePtr<AVEncoder::FVideoEncoder> Encoder = AVEncoder::FVideoEncoderFactory::Get().Create(Available[0].ID, FrameFactory->GetOrCreateVideoEncoderInput(), EncoderConfig);
			if (Encoder.IsValid())
			{
				Encoder->SetOnEncodedPacket([this](uint32 InLayerIndex, const TSharedPtr<AVEncoder::FVideoEncoderInputFrame> InFrame, const AVEncoder::FCodecPacket& InPacket) {
					// Note: this is a static method call.
					FVideoEncoderWrapperHardware::OnEncodedPacket(this, InLayerIndex, InFrame, InPacket);
				});

				// Make the hardware encoder wrapper
				HardwareEncoder = MakeUnique<FVideoEncoderWrapperHardware>(MoveTemp(FrameFactory), MoveTemp(Encoder));
				return HardwareEncoder.Get();
			}
			else
			{
				UE_LOG(LogPixelStreaming, Error, TEXT("Could not create encoder. Check encoder config or perhaps you used up all your HW encoders."));
				// We could not make the encoder, so indicate the id was not set successfully.
				return nullptr;
			}
		}
	}

	FVideoEncoderWrapperHardware* FVideoEncoderFactorySingleLayer::GetHardwareEncoder()
	{
		return HardwareEncoder.Get();
	}

	void FVideoEncoderFactorySingleLayer::ForceKeyFrame()
	{
		if (HardwareEncoder)
		{
			HardwareEncoder->SetForceNextKeyframe();
		}
	}
} // namespace UE::PixelStreaming

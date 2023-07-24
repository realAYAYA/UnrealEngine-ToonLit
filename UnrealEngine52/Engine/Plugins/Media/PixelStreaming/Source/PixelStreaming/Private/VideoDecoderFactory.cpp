// Copyright Epic Games, Inc. All Rights Reserved.
#include "VideoDecoderFactory.h"
#include "Utils.h"
#include "VideoDecoderStub.h"
#include "absl/strings/match.h"
#include "VideoDecoderVPX.h"
#include "VideoDecoderH265.h"
#include "PixelStreamingPrivate.h"
#include "Settings.h"

namespace UE::PixelStreaming
{
	// the list of each individual codec we have decoder support for (the order of this array is preference order after the selected codec)
	const TArray<EPixelStreamingCodec> SupportedDecoderCodecList{ EPixelStreamingCodec::VP8, EPixelStreamingCodec::VP9, /* EPixelStreamingCodec::H264, */ EPixelStreamingCodec::H265 };

	// mapping of codec to a list of video formats
	// done this way so we can order the list of formats based on selected codec in GetSupportedFormats
	TMap<EPixelStreamingCodec, std::vector<webrtc::SdpVideoFormat>> CreateSupportedDecoderFormatMap()
	{
		TMap<EPixelStreamingCodec, std::vector<webrtc::SdpVideoFormat>> Codecs;
		for (auto& Codec : SupportedDecoderCodecList)
		{
			Codecs.Add(Codec);
		}

		Codecs[EPixelStreamingCodec::VP8].push_back(webrtc::SdpVideoFormat(cricket::kVp8CodecName));
		Codecs[EPixelStreamingCodec::VP9].push_back(webrtc::SdpVideoFormat(cricket::kVp9CodecName));
		// Codecs[EPixelStreamingCodec::H264].push_back(UE::PixelStreaming::CreateH264Format(webrtc::H264Profile::kProfileConstrainedBaseline, webrtc::H264Level::kLevel3_1));
		// Codecs[EPixelStreamingCodec::H264].push_back(UE::PixelStreaming::CreateH264Format(webrtc::H264Profile::kProfileBaseline, webrtc::H264Level::kLevel3_1));
		Codecs[EPixelStreamingCodec::H265].push_back(webrtc::SdpVideoFormat(cricket::kH265CodecName));

		return Codecs;
	}

	std::vector<webrtc::SdpVideoFormat> FVideoDecoderFactory::GetSupportedFormats() const
	{
		// static so we dont create the list every time this is called since the list will not change during runtime.
		static TMap<EPixelStreamingCodec, std::vector<webrtc::SdpVideoFormat>> CodecMap = CreateSupportedDecoderFormatMap();

		// since this method is const we need to store this state statically. it means all instances will share this state
		// but that actually works in our favor since we're describing more about the plugin state than the actual
		// instance of this factory.
		static std::vector<webrtc::SdpVideoFormat> SupportedFormats;

		// If we are not negotiating codecs simply return just the one codec that is selected in UE
		if (!Settings::CVarPixelStreamingWebRTCNegotiateCodecs.GetValueOnAnyThread())
		{
			const EPixelStreamingCodec SelectedCodec = UE::PixelStreaming::Settings::GetSelectedCodec();
			if (CodecMap.Contains(SelectedCodec))
			{
				for (auto& Format : CodecMap[SelectedCodec])
				{
					SupportedFormats.push_back(Format);
				}
				return SupportedFormats;
			}
			else
			{
				UE_LOG(LogPixelStreaming, Error, TEXT("Selected codec was not a supported codec, falling back to negotiating codecs..."));
			}
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
			for (auto& SupportedCodec : SupportedDecoderCodecList)
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
		else if (absl::EqualsIgnoreCase(format.name, cricket::kH265CodecName))
		{
			return std::make_unique<VideoDecoderH265>();
		}
		return std::make_unique<FVideoDecoderStub>();
	}
} // namespace UE::PixelStreaming

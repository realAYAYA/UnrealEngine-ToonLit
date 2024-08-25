// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Video/VideoDecoder.h"
#include "Video/CodecUtils/CodecUtilsH264.h"

/*
 * Configuration settings for H264 decoders.
 */
struct AVCODECSCORE_API FVideoDecoderConfigH264 : public FVideoDecoderConfig
{
	TMap<uint32, UE::AVCodecCore::H264::SPS_t> SPS;
	TMap<uint32, UE::AVCodecCore::H264::PPS_t> PPS;
	TArray<UE::AVCodecCore::H264::SEI_t> SEI;

	FVideoDecoderConfigH264(EAVPreset Preset = EAVPreset::Default)
		: FVideoDecoderConfig(Preset)
	{
	}

	FAVResult Parse(FVideoPacket const& Packet, TArray<UE::AVCodecCore::H264::Slice_t>& OutSlices);

    TOptional<int> GetLastSliceQP(TArray<UE::AVCodecCore::H264::Slice_t>& Slices);
};

DECLARE_TYPEID(FVideoDecoderConfigH264, AVCODECSCORE_API);

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Video/VideoEncoder.h"
#include "Video/CodecUtils/CodecUtilsH264.h"

/*
 * Configuration settings for H264 encoders.
 */
struct FVideoEncoderConfigH264 : public FVideoEncoderConfig
{
public:
	EH264Profile Profile = EH264Profile::Main;

	bool RepeatSPSPPS = false;

	uint32 IntraRefreshPeriodFrames = 0;
	uint32 IntraRefreshCountFrames = 0;

	FVideoEncoderConfigH264(EAVPreset Preset = EAVPreset::Default)
		: FVideoEncoderConfig(Preset)
	{
	}
};

DECLARE_TYPEID(FVideoEncoderConfigH264, AVCODECSCORE_API);

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Audio/Encoders/Configs/AudioEncoderConfigWMF.h"

THIRD_PARTY_INCLUDES_START
#include "Windows/AllowWindowsPlatformTypes.h"
#include <wmcodecdsp.h>
#include "Windows/HideWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_END

#include "Audio/Encoders/Configs/AudioEncoderConfigAAC.h"

REGISTER_TYPEID(FAudioEncoderConfigWMF);

template <>
DLLEXPORT FAVResult FAVExtension::TransformConfig(FAudioEncoderConfigWMF& OutConfig, FAudioEncoderConfigAAC const& InConfig)
{
	OutConfig.CodecType = __uuidof(AACMFTEncoder);

	return EAVResult::Success;
}

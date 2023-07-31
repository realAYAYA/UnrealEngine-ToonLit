// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAudioCodec.h"
#include "PcmCodec.generated.h"

UENUM()
enum class EPcmBitDepthConversion : uint8
{
	SameAsSource,
	Int16,
	Float32
};

UCLASS()
class AUDIOEXTENSIONS_API UAudioPcmEncoderSettings : public UAudioCodecEncoderSettings
{
public:
	GENERATED_BODY()	

	UPROPERTY()
	EPcmBitDepthConversion BitDepthConversion;

protected:
	virtual FString GetHashForDDC() const override;
};

namespace Audio
{
	TUniquePtr<ICodec> Create_PcmCodec();
}
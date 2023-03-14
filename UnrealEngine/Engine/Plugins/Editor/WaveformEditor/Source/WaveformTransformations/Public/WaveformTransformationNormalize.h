// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioExtensions/Public/IWaveformTransformation.h"
#include "UObject/Object.h"
#include "WaveformTransformationNormalize.generated.h"

UENUM()
enum class ENormalizationMode : uint8
{
	Peak,
	RMS,
	DWeightedLoudness,
	COUNT UMETA(Hidden)
};

class WAVEFORMTRANSFORMATIONS_API FWaveTransformationNormalize : public Audio::IWaveTransformation
{
public:
	FWaveTransformationNormalize(float InTarget, float InMaxGain, ENormalizationMode InMode);

	virtual void ProcessAudio(Audio::FWaveformTransformationWaveInfo& InOutWaveInfo) const override;

private:
	float Target = 0.f;
	float MaxGain = 0.f;
	ENormalizationMode Mode = ENormalizationMode::Peak;
};

UCLASS()
class WAVEFORMTRANSFORMATIONS_API UWaveformTransformationNormalize : public UWaveformTransformationBase
{
	GENERATED_BODY()

	// Target maximum volume for this soundwave, in decibels
	UPROPERTY(EditAnywhere, Category="Normalization", meta=(ClampMax = 0.0))
	float Target = 0.f;

	// Will not apply more gain than this, even if the sound is very quiet
	UPROPERTY(EditAnywhere, Category="Normalization", meta=(ClampMin = 0.0))
	float MaxGain = 0.f;

	// what type of analysis to run to find the peak value
	UPROPERTY(EditAnywhere, Category="Normalization")
	ENormalizationMode Mode;
	
public:
	virtual Audio::FTransformationPtr CreateTransformation() const override;
};
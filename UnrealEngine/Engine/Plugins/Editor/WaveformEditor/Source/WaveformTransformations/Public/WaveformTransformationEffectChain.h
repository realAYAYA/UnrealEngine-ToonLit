// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioExtensions/Public/IWaveformTransformation.h"
#include "Factories/SoundSourceEffectFactory.h"
#include "UObject/Object.h"
#include "UObject/StrongObjectPtr.h"
#include "WaveformTransformationEffectChain.generated.h"

class WAVEFORMTRANSFORMATIONS_API FWaveTransformationEffectChain : public Audio::IWaveTransformation
{
public:
	FWaveTransformationEffectChain(TArray<TObjectPtr<class USoundEffectSourcePreset>>& InEffectPresets);

	virtual void ProcessAudio(Audio::FWaveformTransformationWaveInfo& InOutWaveInfo) const override;

	virtual bool SupportsRealtimePreview() const override { return true; }
	virtual bool CanChangeFileLength() const override { return true; }

private:
	TArray<TStrongObjectPtr<class USoundEffectSourcePreset>> Presets;
};

UCLASS(hidden)
class WAVEFORMTRANSFORMATIONS_API UWaveformTransformationEffectChain : public UWaveformTransformationBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Trim")
	TObjectPtr<class USoundEffectSourcePresetChain> EffectChain;

	UPROPERTY(EditAnywhere, Instanced, Category = "Trim")
	TArray<TObjectPtr<class USoundEffectSourcePreset>> InlineEffects;
	
public:
	
	virtual Audio::FTransformationPtr CreateTransformation() const override;
};
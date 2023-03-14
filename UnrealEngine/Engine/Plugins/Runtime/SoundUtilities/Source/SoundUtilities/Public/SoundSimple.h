// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SoundUtilitiesModule.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundBase.h"
#include "SoundSimple.generated.h"

USTRUCT(BlueprintType)
struct SOUNDUTILITIES_API FSoundVariation
{
	GENERATED_USTRUCT_BODY()

	// The sound wave asset to use for this variation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SoundVariation")
	TObjectPtr<USoundWave> SoundWave;

	// The probability weight to use for this variation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth|Preset")
	float ProbabilityWeight;

	// The volume range to use for this variation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth|Preset")
	FVector2D VolumeRange;

	// The pitch range to use for this variation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth|Preset")
	FVector2D PitchRange;

	FSoundVariation()
		: SoundWave(nullptr)
		, ProbabilityWeight(1.0f)
		, VolumeRange(1.0f, 1.0f)
		, PitchRange(1.0f, 1.0f)
	{
	}
};

// Class which contains a simple list of sound wave variations
UCLASS(ClassGroup = Sound, meta = (BlueprintSpawnableComponent))
class SOUNDUTILITIES_API USoundSimple : public USoundBase
{
	GENERATED_BODY()

public:

	// List of variations for the simple sound
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variations")
	TArray<FSoundVariation> Variations;

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject INterface

	//~ Begin USoundBase Interface.
	virtual bool IsPlayable() const override;
	virtual void Parse(class FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances) override;
	virtual float GetMaxDistance() const override;
	virtual float GetDuration() const override;
	//~ End USoundBase Interface.

protected:
	void ChooseSoundWave();
	void CacheValues();

	// The current chosen sound wave
	UPROPERTY(transient)
	TObjectPtr<USoundWave> SoundWave;
};




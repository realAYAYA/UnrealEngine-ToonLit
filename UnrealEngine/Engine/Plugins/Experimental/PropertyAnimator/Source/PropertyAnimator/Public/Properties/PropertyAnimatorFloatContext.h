// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Properties/PropertyAnimatorCoreContext.h"
#include "PropertyAnimatorFloatContext.generated.h"

/** Property context used by float driving animator */
UCLASS(MinimalAPI, BlueprintType)
class UPropertyAnimatorFloatContext : public UPropertyAnimatorCoreContext
{
	GENERATED_BODY()

public:
	PROPERTYANIMATOR_API void SetMagnitude(float InMagnitude);
	float GetMagnitude() const
	{
		return Magnitude;
	}

	PROPERTYANIMATOR_API void SetAmplitudeMin(double InAmplitude);
	double GetAmplitudeMin() const
	{
		return AmplitudeMin;
	}

	PROPERTYANIMATOR_API void SetAmplitudeMax(double InAmplitude);
	double GetAmplitudeMax() const
	{
		return AmplitudeMax;
	}

	PROPERTYANIMATOR_API void SetFrequency(float InFrequency);
	float GetFrequency() const
	{
		return Frequency;
	}

	PROPERTYANIMATOR_API void SetTimeOffset(double InTimeOffset);
	double GetTimeOffset() const
	{
		return TimeOffset;
	}

protected:
	//~ Begin UPropertyAnimatorCoreContext
	virtual void OnAnimatedPropertyLinked() override;
	//~ End UPropertyAnimatorCoreContext

	/** Magnitude of the effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Animator", meta=(ClampMin="0.0", ClampMax="1.0"))
	float Magnitude = 1.f;

	/** The minimum value should be remapped to that values */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Animator")
	double AmplitudeMin = -1.f;

	/** The maximum value should be remapped to that values */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Animator")
	double AmplitudeMax = 1.f;

	/** Number of repetition every seconds for the effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Animator", meta=(ClampMin="0"))
	float Frequency = 1.f;

	/** Time offset variation for evaluation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Animator")
	double TimeOffset = 0.f;
};

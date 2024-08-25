// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animators/PropertyAnimatorCoreBase.h"
#include "PropertyAnimatorFloatBase.generated.h"

class UPropertyAnimatorFloatContext;

/**
 * Animate supported float properties with various options
 */
UCLASS(MinimalAPI, Abstract, AutoExpandCategories=("Animator"))
class UPropertyAnimatorFloatBase : public UPropertyAnimatorCoreBase
{
	GENERATED_BODY()

	friend class FPropertyAnimatorCoreEditorDetailCustomization;

public:
	PROPERTYANIMATOR_API void SetGlobalMagnitude(float InMagnitude);
	float GetGlobalMagnitude() const
	{
		return GlobalMagnitude;
	}

	PROPERTYANIMATOR_API void SetGlobalFrequency(float InFrequency);
	float GetGlobalFrequency() const
	{
		return GlobalFrequency;
	}

	PROPERTYANIMATOR_API void SetAccumulatedTimeOffset(double InOffset);
	double GetAccumulatedTimeOffset() const
	{
		return AccumulatedTimeOffset;
	}

	PROPERTYANIMATOR_API void SetRandomTimeOffset(bool bInOffset);
	bool GetRandomTimeOffset() const
	{
		return bRandomTimeOffset;
	}

	PROPERTYANIMATOR_API void SetSeed(int32 InSeed);
	int32 GetSeed() const
	{
		return Seed;
	}

protected:
	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif
	//~ End UObject

	virtual void OnMagnitudeChanged() {}
	virtual void OnGlobalFrequencyChanged() {}
	virtual void OnAccumulatedTimeOffsetChanged() {}
	virtual void OnSeedChanged() {}

	//~ Begin UPropertyAnimatorCoreBase
	virtual TSubclassOf<UPropertyAnimatorCoreContext> GetPropertyContextClass(const FPropertyAnimatorCoreData& InUnlinkedProperty) override;
	virtual bool IsPropertyDirectlySupported(const FPropertyAnimatorCoreData& InPropertyData) const override;
	virtual bool IsPropertyIndirectlySupported(const FPropertyAnimatorCoreData& InPropertyData) const override;
	virtual void EvaluateProperties(const FPropertyAnimatorCoreEvaluationParameters& InParameters) override;
	virtual void OnPropertyLinked(UPropertyAnimatorCoreContext* InLinkedProperty) override;
	//~ End UPropertyAnimatorCoreBase

	/** Evaluate and return float value for a property */
	virtual float Evaluate(double InTimeElapsed, const FPropertyAnimatorCoreData& InPropertyData, UPropertyAnimatorFloatContext* InOptions) const
	{
		return 0.f;
	}

	/** Global magnitude for the effect */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter, Getter, Category="Animator", meta=(ClampMin="0"))
	float GlobalMagnitude = 1.f;

	/** Global frequency multiplier */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter, Getter, Category="Animator", meta=(ClampMin="0"))
	float GlobalFrequency = 1.f;

	/** Use random time offset to add variation in animation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetRandomTimeOffset", Getter="GetRandomTimeOffset", Category="Animator")
	bool bRandomTimeOffset = false;

	/** Seed to generate per property time offset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Animator", meta=(EditCondition="bRandomTimeOffset", EditConditionHides))
	int32 Seed = 0;

	/** This time offset will be accumulated for each property for every round */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter, Getter, Category="Animator")
	double AccumulatedTimeOffset = 0.f;

private:
	/** Random stream for time offset */
	FRandomStream RandomStream = FRandomStream(Seed);
};
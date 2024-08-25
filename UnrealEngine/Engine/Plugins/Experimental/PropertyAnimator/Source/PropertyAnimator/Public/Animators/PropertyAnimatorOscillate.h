// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animators/PropertyAnimatorFloatBase.h"
#include "PropertyAnimatorOscillate.generated.h"

UENUM(BlueprintType)
enum class EPropertyAnimatorOscillateFunction : uint8
{
	Sine,
	Cosine,
	Square,
	InvertedSquare,
	Sawtooth,
	Triangle,
};

/**
 * Applies an additive regular oscillate movement with various options on supported float properties
 */
UCLASS(MinimalAPI, AutoExpandCategories=("Animator"))
class UPropertyAnimatorOscillate : public UPropertyAnimatorFloatBase
{
	GENERATED_BODY()

public:
	static constexpr const TCHAR* DefaultControllerName = TEXT("Oscillate");

	UPropertyAnimatorOscillate();

	PROPERTYANIMATOR_API void SetOscillateFunction(EPropertyAnimatorOscillateFunction InFunction);
	EPropertyAnimatorOscillateFunction GetOscillateFunction() const
	{
		return OscillateFunction;
	}

protected:
	//~ Begin UPropertyAnimatorFloatBase
	virtual float Evaluate(double InTimeElapsed, const FPropertyAnimatorCoreData& InPropertyData, UPropertyAnimatorFloatContext* InOptions) const override;
	//~ End UPropertyAnimatorFloatBase

	/** The oscillate function to feed current time elapsed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Animator")
	EPropertyAnimatorOscillateFunction OscillateFunction = EPropertyAnimatorOscillateFunction::Sine;
};
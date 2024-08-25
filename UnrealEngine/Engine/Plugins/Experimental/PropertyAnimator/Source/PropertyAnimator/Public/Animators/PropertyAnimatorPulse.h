// Copyright Epic Games, Inc. All Rights Reserved.

/** The wave function to feed current time elapsed */

#pragma once

#include "Animators/PropertyAnimatorFloatBase.h"
#include "PropertyAnimatorShared.h"
#include "PropertyAnimatorPulse.generated.h"

/**
 * Applies an additive pulse movement with various options on supported float properties
 */
UCLASS(MinimalAPI, AutoExpandCategories=("Animator"))
class UPropertyAnimatorPulse : public UPropertyAnimatorFloatBase
{
	GENERATED_BODY()

public:
	static constexpr const TCHAR* DefaultControllerName = TEXT("Pulse");

	UPropertyAnimatorPulse();

	PROPERTYANIMATOR_API void SetEasingFunction(EPropertyAnimatorEasingFunction InEasingFunction);
	EPropertyAnimatorEasingFunction GetEasingFunction() const
	{
		return EasingFunction;
	}

	PROPERTYANIMATOR_API void SetEasingType(EPropertyAnimatorEasingType InEasingType);
	EPropertyAnimatorEasingType GetEasingType() const
	{
		return EasingType;
	}

protected:
	//~ Begin UPropertyAnimatorFloatBase
	virtual float Evaluate(double InTimeElapsed, const FPropertyAnimatorCoreData& InPropertyData, UPropertyAnimatorFloatContext* InOptions) const override;
	//~ End UPropertyAnimatorFloatBase

	/** The easing function to use to modify the base effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Animator")
	EPropertyAnimatorEasingFunction EasingFunction = EPropertyAnimatorEasingFunction::Linear;

	/** The type of effect for easing function */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category="Animator", meta=(EditCondition="EasingFunction != EPropertyAnimatorEasingFunction::Line", EditConditionHides))
	EPropertyAnimatorEasingType EasingType = EPropertyAnimatorEasingType::InOut;
};
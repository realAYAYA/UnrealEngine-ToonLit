// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyAnimatorShared.h"
#include "PropertyAnimatorEasingParameters.generated.h"

USTRUCT()
struct FPropertyAnimatorEasingParameters
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Easing")
	double Amplitude = 1.0;

	/** Length of the easing in seconds */
	UPROPERTY(EditAnywhere, Category="Easing")
	double EasingTime = 1.0;

	UPROPERTY(EditAnywhere, Category="Easing")
	double OffsetX = 0.0;

	UPROPERTY(EditAnywhere, Category="Easing")
	double OffsetY = 0.0;

	UPROPERTY(EditAnywhere, Category="Easing")
	EPropertyAnimatorEasingType EasingType = EPropertyAnimatorEasingType::In;

	UPROPERTY(EditAnywhere, Category="Easing")
	EPropertyAnimatorEasingFunction EasingFunction  = EPropertyAnimatorEasingFunction::Sine;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animators/PropertyAnimatorFloatBase.h"
#include "PropertyAnimatorTime.generated.h"

/**
 * Applies an additive time movement with various options on supported float properties
 */
UCLASS(MinimalAPI, AutoExpandCategories=("Animator"))
class UPropertyAnimatorTime : public UPropertyAnimatorFloatBase
{
	GENERATED_BODY()

public:
	static constexpr const TCHAR* DefaultControllerName = TEXT("Time");

	UPropertyAnimatorTime();

protected:
	//~ Begin UPropertyAnimatorFloatBase
	virtual float Evaluate(double InTimeElapsed, const FPropertyAnimatorCoreData& InPropertyData, UPropertyAnimatorFloatContext* InOptions) const override;
	//~ End UPropertyAnimatorFloatBase
};
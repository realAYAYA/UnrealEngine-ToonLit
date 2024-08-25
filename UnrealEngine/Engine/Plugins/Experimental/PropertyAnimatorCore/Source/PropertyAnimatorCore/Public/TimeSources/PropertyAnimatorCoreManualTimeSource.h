// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyAnimatorCoreTimeSourceBase.h"
#include "PropertyAnimatorCoreManualTimeSource.generated.h"

UCLASS(MinimalAPI)
class UPropertyAnimatorCoreManualTimeSource : public UPropertyAnimatorCoreTimeSourceBase
{
	GENERATED_BODY()

public:
	UPropertyAnimatorCoreManualTimeSource()
		: UPropertyAnimatorCoreTimeSourceBase(TEXT("Manual"))
	{}

	PROPERTYANIMATORCORE_API void SetCustomTime(double InTime);
	double GetCustomTime() const
	{
		return CustomTime;
	}

	//~ Begin UPropertyAnimatorTimeSourceBase
	virtual double GetTimeElapsed() override;
	virtual bool IsTimeSourceReady() const override;
	//~ End UPropertyAnimatorTimeSourceBase

protected:
	/** Allows you to drive controllers with this float */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetCustomTime", Getter="GetCustomTime", Category="Animator")
	double CustomTime = 0.f;
};
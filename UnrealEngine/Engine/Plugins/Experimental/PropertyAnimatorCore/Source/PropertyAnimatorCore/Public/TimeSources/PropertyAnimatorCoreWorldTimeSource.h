// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyAnimatorCoreTimeSourceBase.h"
#include "PropertyAnimatorCoreWorldTimeSource.generated.h"

/**
 * Time source that follows world time,
 * Transient because it does not contain user saved data, can be recreated
 */
UCLASS(MinimalAPI, Transient)
class UPropertyAnimatorCoreWorldTimeSource : public UPropertyAnimatorCoreTimeSourceBase
{
	GENERATED_BODY()

public:
	UPropertyAnimatorCoreWorldTimeSource()
		: UPropertyAnimatorCoreTimeSourceBase(TEXT("World"))
	{}

	//~ Begin UPropertyAnimatorTimeSourceBase
	virtual double GetTimeElapsed() override;
	virtual bool IsTimeSourceReady() const override;
	//~ End UPropertyAnimatorTimeSourceBase
};
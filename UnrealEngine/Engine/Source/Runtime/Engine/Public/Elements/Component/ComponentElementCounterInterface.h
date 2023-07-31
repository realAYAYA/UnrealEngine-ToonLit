// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Framework/TypedElementCounter.h"
#include "ComponentElementCounterInterface.generated.h"

UCLASS()
class ENGINE_API UComponentElementCounterInterface : public UObject, public ITypedElementCounterInterface
{
	GENERATED_BODY()

public:
	virtual void IncrementCountersForElement(const FTypedElementHandle& InElementHandle, FTypedElementCounter& InOutCounter) override;
	virtual void DecrementCountersForElement(const FTypedElementHandle& InElementHandle, FTypedElementCounter& InOutCounter) override;
};

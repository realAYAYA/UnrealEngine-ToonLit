// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Framework/TypedElementCounter.h"
#include "ObjectElementCounterInterface.generated.h"

UCLASS()
class ENGINE_API UObjectElementCounterInterface : public UObject, public ITypedElementCounterInterface
{
	GENERATED_BODY()

public:
	virtual void IncrementCountersForElement(const FTypedElementHandle& InElementHandle, FTypedElementCounter& InOutCounter) override;
	virtual void DecrementCountersForElement(const FTypedElementHandle& InElementHandle, FTypedElementCounter& InOutCounter) override;

	static void IncrementCounterForObjectClass(const UObject* InObject, FTypedElementCounter& InOutCounter);
	static void DecrementCounterForObjectClass(const UObject* InObject, FTypedElementCounter& InOutCounter);
};

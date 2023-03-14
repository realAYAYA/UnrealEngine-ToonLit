// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Framework/TypedElementCounter.h"
#include "ActorElementCounterInterface.generated.h"

UCLASS()
class ENGINE_API UActorElementCounterInterface : public UObject, public ITypedElementCounterInterface
{
	GENERATED_BODY()

public:
	virtual void IncrementCountersForElement(const FTypedElementHandle& InElementHandle, FTypedElementCounter& InOutCounter) override;
	virtual void DecrementCountersForElement(const FTypedElementHandle& InElementHandle, FTypedElementCounter& InOutCounter) override;
};

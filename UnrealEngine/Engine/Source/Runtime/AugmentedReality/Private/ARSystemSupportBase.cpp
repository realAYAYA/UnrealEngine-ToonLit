// Copyright Epic Games, Inc. All Rights Reserved.

#include "ARSystemSupportBase.h"
#include "AugmentedRealityModule.h"
#include "ARPin.h"

bool FARSystemSupportBase::OnPinComponentToARPin(USceneComponent* ComponentToPin, UARPin* Pin)
{
	if (Pin == nullptr)
	{
		UE_LOG(LogAR, Warning, TEXT("OnPinComponentToARPin: Pin was null.  Doing nothing."));
		return false;
	}
	if (ComponentToPin == nullptr)
	{
		UE_LOG(LogAR, Warning, TEXT("OnPinComponentToARPin: Tried to pin null component to pin %s.  Doing nothing."), *Pin->GetDebugName().ToString());
		return false;
	}

	{
		if (UARPin* FindResult = FindARPinByComponent(ComponentToPin))
		{
			if (FindResult == Pin)
			{
				UE_LOG(LogAR, Warning, TEXT("OnPinComponentToARPin: Component %s is already pinned to pin %s.  Doing nothing."), *ComponentToPin->GetReadableName(), *Pin->GetDebugName().ToString());
				return true;
			}
			else
			{
				UE_LOG(LogAR, Warning, TEXT("OnPinComponentToARPin: Component %s is pinned to pin %s. Unpinning it from that pin first.  The pin will not be destroyed."), *ComponentToPin->GetReadableName(), *Pin->GetDebugName().ToString());
				FindResult->SetPinnedComponent(nullptr);
			}
		}

		Pin->SetPinnedComponent(ComponentToPin);

		return true;
	}
}

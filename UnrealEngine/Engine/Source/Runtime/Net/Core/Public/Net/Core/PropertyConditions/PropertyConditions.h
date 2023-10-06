// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "CoreMinimal.h"
#include "Delegates/IDelegateInstance.h"
#include "HAL/Platform.h"
#include "Templates/SharedPointer.h"
#include "UObject/CoreNet.h"
#include "UObject/ObjectKey.h"

class FOutputDevice;
class FRepChangedPropertyTracker;

namespace UE::Net::Private
{

class FNetPropertyConditionManager
{
public:
	NETCORE_API FNetPropertyConditionManager();
	NETCORE_API ~FNetPropertyConditionManager();

	static NETCORE_API FNetPropertyConditionManager& Get();

	NETCORE_API void SetPropertyActive(const FObjectKey ObjectKey, const uint16 RepIndex, const bool bActive);
	NETCORE_API void SetPropertyDynamicCondition(const FObjectKey ObjectKey, const uint16 RepIndex, const ELifetimeCondition Condition);

	NETCORE_API void NotifyObjectDestroyed(const FObjectKey ObjectKey);

	NETCORE_API TSharedPtr<FRepChangedPropertyTracker> FindOrCreatePropertyTracker(const FObjectKey ObjectKey);
	NETCORE_API TSharedPtr<FRepChangedPropertyTracker> FindPropertyTracker(const FObjectKey ObjectKey) const;

	NETCORE_API void LogMemory(FOutputDevice& Ar);

	static NETCORE_API void SetPropertyActiveOverride(IRepChangedPropertyTracker& Tracker, UObject* OwningObject, const uint16 RepIndex, const bool bIsActive);

private:
	void PostGarbageCollect();

	FDelegateHandle PostGarbageCollectHandle;

	TMap<FObjectKey, TSharedPtr<FRepChangedPropertyTracker>> PropertyTrackerMap;
};

}; // UE::Net::Private

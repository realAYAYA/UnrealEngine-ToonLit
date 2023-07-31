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

class NETCORE_API FNetPropertyConditionManager
{
public:
	FNetPropertyConditionManager();
	~FNetPropertyConditionManager();

	static FNetPropertyConditionManager& Get();

	void SetPropertyActive(const FObjectKey ObjectKey, const uint16 RepIndex, const bool bActive);
	void NotifyObjectDestroyed(const FObjectKey ObjectKey);

	TSharedPtr<FRepChangedPropertyTracker> FindOrCreatePropertyTracker(const FObjectKey ObjectKey);
	TSharedPtr<FRepChangedPropertyTracker> FindPropertyTracker(const FObjectKey ObjectKey) const;

	void LogMemory(FOutputDevice& Ar);

private:
	void PostGarbageCollect();

	FDelegateHandle PostGarbageCollectHandle;

	TMap<FObjectKey, TSharedPtr<FRepChangedPropertyTracker>> PropertyTrackerMap;
};

}; // UE::Net::Private
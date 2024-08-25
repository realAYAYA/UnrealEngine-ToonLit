// Copyright Epic Games, Inc. All Rights Reserved.

#include "NavigationObjectRepository.h"
#include "AI/Navigation/NavRelevantInterface.h"
#include "NavLinkCustomInterface.h"

void UNavigationObjectRepository::RegisterNavRelevantObject(INavRelevantInterface& NavRelevantObject)
{
#if DO_ENSURE // We don't want to execute the Find at all for targets where ensures are disabled
	if (!ensureMsgf(NavRelevantObjects.Find(&NavRelevantObject) == INDEX_NONE, TEXT("Same interface pointer can't be registered twice.")))
	{
		return;
	}
#endif

	NavRelevantObjects.Emplace(&NavRelevantObject);

	OnNavRelevantObjectRegistered.ExecuteIfBound(NavRelevantObject);
}

void UNavigationObjectRepository::UnregisterNavRelevantObject(INavRelevantInterface& NavRelevantObject)
{
	ensureMsgf(NavRelevantObjects.Remove(&NavRelevantObject) > 0, TEXT("Interface can't be removed since it was not registered or already unregistered)"));

	OnNavRelevantObjectUnregistered.ExecuteIfBound(NavRelevantObject);
}

void UNavigationObjectRepository::RegisterCustomNavLinkObject(INavLinkCustomInterface& CustomNavLinkObject)
{
#if DO_ENSURE // We don't want to execute the Find at all for targets where ensures are disabled
	if (!ensureMsgf(CustomLinkObjects.Find(&CustomNavLinkObject) == INDEX_NONE, TEXT("Same interface pointer can't be registered twice.")))
	{
		return;
	}
#endif

	CustomLinkObjects.Emplace(&CustomNavLinkObject);

	OnCustomNavLinkObjectRegistered.ExecuteIfBound(CustomNavLinkObject);
}

void UNavigationObjectRepository::UnregisterCustomNavLinkObject(INavLinkCustomInterface& CustomNavLinkObject)
{
	ensureMsgf(CustomLinkObjects.Remove(&CustomNavLinkObject) > 0, TEXT("Interface can't be removed since it was not registered or already unregistered)"));

	OnCustomNavLinkObjectUnregistered.ExecuteIfBound(CustomNavLinkObject);
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/Navigation/NavigationDirtyElement.h"
#include "AI/Navigation/NavigationTypes.h"

FNavigationDirtyElement::FNavigationDirtyElement(UObject* InOwner, INavRelevantInterface* InNavInterface, int32 InFlagsOverride /*= 0*/, const bool bUseWorldPartitionedDynamicMode /*= false*/)
	: Owner(InOwner)
	, NavInterface(InNavInterface)
	, FlagsOverride(InFlagsOverride)
	, PrevFlags(0)
	, PrevBounds(ForceInit)
	, bHasPrevData(false)
	, bInvalidRequest(false)
{
	if (bUseWorldPartitionedDynamicMode)
	{
		bIsFromVisibilityChange = FNavigationSystem::IsLevelVisibilityChanging(InOwner);
		bIsInBaseNavmesh = FNavigationSystem::IsInBaseNavmesh(InOwner);
	}
	else
	{
		bIsFromVisibilityChange = false;
		bIsInBaseNavmesh = false;
	}
}

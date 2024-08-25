// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/Box.h"
#include "UObject/WeakObjectPtr.h"

class INavRelevantInterface;

struct FNavigationDirtyElement
{
	/**
	 * If not empty and the associated navigation relevant object controls the dirty areas explicitly (i.e. ShouldSkipDirtyAreaOnAddOrRemove returns true),
	 * the list will be used to indicate the areas that need rebuilding.
	 * Otherwise, the default behavior, NavRelevant object's bounds will be used.
	 */
	TArray<FBox> ExplicitAreasToDirty;

	/** object owning this element */
	FWeakObjectPtr Owner;
	
	/** cached interface pointer */
	INavRelevantInterface* NavInterface;

	/** override for update flags */
	int32 FlagsOverride;
	
	/** flags of already existing entry for this actor */
	int32 PrevFlags;
	
	/** bounds of already existing entry for this actor */
	FBox PrevBounds;

	/** prev flags & bounds data are set */
	uint8 bHasPrevData : 1;

	/** request was invalidated while queued, use prev values to dirty area */
	uint8 bInvalidRequest : 1;

	/** requested during visibility change of the owning level (loading/unloading) */
	uint8 bIsFromVisibilityChange : 1;

	/** part of the base navmesh */
	uint8 bIsInBaseNavmesh : 1;

	FNavigationDirtyElement()
		: NavInterface(nullptr), FlagsOverride(0), PrevFlags(0), PrevBounds(ForceInit), bHasPrevData(false), bInvalidRequest(false), bIsFromVisibilityChange(false), bIsInBaseNavmesh(false)
	{
	}

	FNavigationDirtyElement(UObject* InOwner)
		: Owner(InOwner), NavInterface(nullptr), FlagsOverride(0), PrevFlags(0), PrevBounds(ForceInit), bHasPrevData(false), bInvalidRequest(false), bIsFromVisibilityChange(false), bIsInBaseNavmesh(false)
	{
	}

	ENGINE_API FNavigationDirtyElement(UObject* InOwner, INavRelevantInterface* InNavInterface, int32 InFlagsOverride = 0, const bool bUseWorldPartitionedDynamicMode = false);

	bool operator==(const FNavigationDirtyElement& Other) const 
	{ 
		return Owner == Other.Owner; 
	}

	bool operator==(const UObject*& OtherOwner) const 
	{ 
		return (Owner == OtherOwner);
	}

	FORCEINLINE friend uint32 GetTypeHash(const FNavigationDirtyElement& Info)
	{
		return GetTypeHash(Info.Owner);
	}
};

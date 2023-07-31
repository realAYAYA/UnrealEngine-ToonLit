// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NavigationOctreeController.h"
#include "NavigationDirtyAreasController.h"

struct FNavigationDirtyElement;

struct NAVIGATIONSYSTEM_API FNavigationDataHandler
{
	FNavigationOctreeController& OctreeController;
	FNavigationDirtyAreasController& DirtyAreasController;

	FNavigationDataHandler(FNavigationOctreeController& InOctreeController, FNavigationDirtyAreasController& InDirtyAreasController);

	void RemoveNavOctreeElementId(const FOctreeElementId2& ElementId, int32 UpdateFlags);
	FSetElementId RegisterNavOctreeElement(UObject& ElementOwner, INavRelevantInterface& ElementInterface, int32 UpdateFlags);
	void AddElementToNavOctree(const FNavigationDirtyElement& DirtyElement);

	/** Removes associated NavOctreeElement and invalidates associated pending updates. Also removes object from the list of children
	* of the NavigationParent, if any.
	* @param ElementOwner		Object for which we must remove associated NavOctreeElement
	* @param ElementInterface	Object associated NavRelevantInterface to access NavigationParent when registered as children
	* @param UpdateFlags		Flags indicating in which context the method is called to allow/forbid certain operations
	*
	* @return True if associated NavOctreeElement has been removed or pending update has been invalidated; false otherwise.
	*/
	bool UnregisterNavOctreeElement(UObject& ElementOwner, INavRelevantInterface& ElementInterface, int32 UpdateFlags);
	void UpdateNavOctreeElement(UObject& ElementOwner, INavRelevantInterface& ElementInterface, int32 UpdateFlags);
	void UpdateNavOctreeParentChain(UObject& ElementOwner, bool bSkipElementOwnerUpdate = false);
	bool UpdateNavOctreeElementBounds(UActorComponent& Comp, const FBox& NewBounds, const FBox& DirtyArea);	
	void FindElementsInNavOctree(const FBox& QueryBox, const FNavigationOctreeFilter& Filter, TArray<FNavigationOctreeElement>& Elements);
	bool ReplaceAreaInOctreeData(const UObject& Object, TSubclassOf<UNavArea> OldArea, TSubclassOf<UNavArea> NewArea, bool bReplaceChildClasses);
	void AddLevelCollisionToOctree(ULevel& Level);
	void RemoveLevelCollisionFromOctree(ULevel& Level);
	void UpdateActorAndComponentsInNavOctree(AActor& Actor);
	void ProcessPendingOctreeUpdates();
	void DemandLazyDataGathering(FNavigationRelevantData& ElementData);
};

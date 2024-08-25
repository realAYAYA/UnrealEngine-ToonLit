// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NavigationOctreeController.h"
#include "NavigationDirtyAreasController.h"

struct FNavigationDirtyElement;
class UNavArea;

struct FNavigationDataHandler
{
	FNavigationOctreeController& OctreeController;
	FNavigationDirtyAreasController& DirtyAreasController;

	NAVIGATIONSYSTEM_API FNavigationDataHandler(FNavigationOctreeController& InOctreeController, FNavigationDirtyAreasController& InDirtyAreasController);

	NAVIGATIONSYSTEM_API void ConstructNavOctree(const FVector& Origin, const float Radius, const ENavDataGatheringModeConfig DataGatheringMode, const float GatheringNavModifiersWarningLimitTime);

	NAVIGATIONSYSTEM_API void RemoveNavOctreeElementId(const FOctreeElementId2& ElementId, int32 UpdateFlags);
	NAVIGATIONSYSTEM_API FSetElementId RegisterNavOctreeElement(UObject& ElementOwner, INavRelevantInterface& ElementInterface, int32 UpdateFlags);
	NAVIGATIONSYSTEM_API void AddElementToNavOctree(const FNavigationDirtyElement& DirtyElement);

	/** Removes associated NavOctreeElement and invalidates associated pending updates. Also removes object from the list of children
	* of the NavigationParent, if any.
	* @param ElementOwner		Object for which we must remove associated NavOctreeElement
	* @param ElementInterface	Object associated NavRelevantInterface to access NavigationParent when registered as children
	* @param UpdateFlags		Flags indicating in which context the method is called to allow/forbid certain operations
	*
	* @return True if associated NavOctreeElement has been removed or pending update has been invalidated; false otherwise.
	*/
	NAVIGATIONSYSTEM_API bool UnregisterNavOctreeElement(UObject& ElementOwner, INavRelevantInterface& ElementInterface, int32 UpdateFlags);
	NAVIGATIONSYSTEM_API void UpdateNavOctreeElement(UObject& ElementOwner, INavRelevantInterface& ElementInterface, int32 UpdateFlags);
	NAVIGATIONSYSTEM_API void UpdateNavOctreeParentChain(UObject& ElementOwner, bool bSkipElementOwnerUpdate = false);
	NAVIGATIONSYSTEM_API bool UpdateNavOctreeElementBounds(UObject& Object, const FBox& NewBounds, const TConstArrayView<FBox> DirtyAreas);
	NAVIGATIONSYSTEM_API void FindElementsInNavOctree(const FBox& QueryBox, const FNavigationOctreeFilter& Filter, TArray<FNavigationOctreeElement>& Elements);
	NAVIGATIONSYSTEM_API bool ReplaceAreaInOctreeData(const UObject& Object, TSubclassOf<UNavArea> OldArea, TSubclassOf<UNavArea> NewArea, bool bReplaceChildClasses);
	NAVIGATIONSYSTEM_API void AddLevelCollisionToOctree(ULevel& Level);
	NAVIGATIONSYSTEM_API void RemoveLevelCollisionFromOctree(ULevel& Level);
	NAVIGATIONSYSTEM_API void UpdateActorAndComponentsInNavOctree(AActor& Actor);
	NAVIGATIONSYSTEM_API void ProcessPendingOctreeUpdates();
	NAVIGATIONSYSTEM_API void DemandLazyDataGathering(FNavigationRelevantData& ElementData);
	
	UE_DEPRECATED(5.4, "Use the overloaded version taking a list of areas as parameter instead.")
	NAVIGATIONSYSTEM_API bool UpdateNavOctreeElementBounds(UObject& Object, const FBox& NewBounds, const FBox& DirtyArea);
};

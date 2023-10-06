// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NavigationOctree.h"
#include "AI/Navigation/NavigationDirtyElement.h"

struct FNavigationOctreeController
{
	enum EOctreeUpdateMode
	{
		OctreeUpdate_Default = 0,						// regular update, mark dirty areas depending on exported content
		OctreeUpdate_Geometry = 1,						// full update, mark dirty areas for geometry rebuild
		OctreeUpdate_Modifiers = 2,						// quick update, mark dirty areas for modifier rebuild
		OctreeUpdate_Refresh = 4,						// update is used for refresh, don't invalidate pending queue
		OctreeUpdate_ParentChain = 8,					// update child nodes, don't remove anything
	};

	TSet<FNavigationDirtyElement> PendingOctreeUpdates;
	TSharedPtr<FNavigationOctree, ESPMode::ThreadSafe> NavOctree;
	/** Map of all objects that are tied to indexed navigation parent */
	TMultiMap<UObject*, FWeakObjectPtr> OctreeChildNodesMap;
	/** if set, navoctree updates are ignored, use with caution! */
	uint8 bNavOctreeLock : 1;

	NAVIGATIONSYSTEM_API void SetNavigationOctreeLock(bool bLock);
	NAVIGATIONSYSTEM_API bool HasPendingObjectNavOctreeId(UObject& Object) const;
	NAVIGATIONSYSTEM_API void RemoveObjectsNavOctreeId(const UObject& Object);
	NAVIGATIONSYSTEM_API void SetNavigableGeometryStoringMode(FNavigationOctree::ENavGeometryStoringMode NavGeometryMode);

	NAVIGATIONSYSTEM_API void Reset();

	NAVIGATIONSYSTEM_API const FNavigationOctree* GetOctree() const;
	NAVIGATIONSYSTEM_API FNavigationOctree* GetMutableOctree();
	NAVIGATIONSYSTEM_API const FOctreeElementId2* GetObjectsNavOctreeId(const UObject& Object) const;
	NAVIGATIONSYSTEM_API bool GetNavOctreeElementData(const UObject& NodeOwner, int32& DirtyFlags, FBox& DirtyBounds);
	NAVIGATIONSYSTEM_API const FNavigationRelevantData* GetDataForObject(const UObject& Object) const;
	NAVIGATIONSYSTEM_API FNavigationRelevantData* GetMutableDataForObject(const UObject& Object);
	NAVIGATIONSYSTEM_API bool HasObjectsNavOctreeId(const UObject& Object) const;
	NAVIGATIONSYSTEM_API bool IsNavigationOctreeLocked() const;
	/** basically says if navoctree has been created already */
	bool IsValid() const { return NavOctree.IsValid(); }
	NAVIGATIONSYSTEM_API bool IsValidElement(const FOctreeElementId2& ElementId) const;
	bool IsEmpty() const { return (IsValid() == false) || NavOctree->GetSizeBytes() == 0; }

private:
	static NAVIGATIONSYSTEM_API uint32 HashObject(const UObject& Object);
};

//----------------------------------------------------------------------//
// inlines
//----------------------------------------------------------------------//
FORCEINLINE uint32 FNavigationOctreeController::HashObject(const UObject& Object) 
{ 
	return FNavigationOctree::HashObject(Object);
}

FORCEINLINE_DEBUGGABLE const FOctreeElementId2* FNavigationOctreeController::GetObjectsNavOctreeId(const UObject& Object) const
{ 
	return NavOctree.IsValid()
		? NavOctree->ObjectToOctreeId.Find(HashObject(Object))
		: nullptr;
}

FORCEINLINE_DEBUGGABLE bool FNavigationOctreeController::HasObjectsNavOctreeId(const UObject& Object) const
{
	return NavOctree.IsValid() && (NavOctree->ObjectToOctreeId.Find(HashObject(Object)) != nullptr);
}

FORCEINLINE bool FNavigationOctreeController::HasPendingObjectNavOctreeId(UObject& Object) const 
{ 
	return PendingOctreeUpdates.Contains(FNavigationDirtyElement(&Object)); 
}

FORCEINLINE_DEBUGGABLE void FNavigationOctreeController::RemoveObjectsNavOctreeId(const UObject& Object)
{ 
	if (NavOctree.IsValid())
	{
		NavOctree->ObjectToOctreeId.Remove(HashObject(Object));
	}
}

FORCEINLINE const FNavigationOctree* FNavigationOctreeController::GetOctree() const
{ 
	return NavOctree.Get(); 
}

FORCEINLINE FNavigationOctree* FNavigationOctreeController::GetMutableOctree()
{ 
	return NavOctree.Get(); 
}

FORCEINLINE bool FNavigationOctreeController::IsNavigationOctreeLocked() const
{ 
	return bNavOctreeLock; 
}

FORCEINLINE void FNavigationOctreeController::SetNavigationOctreeLock(bool bLock) 
{ 
	bNavOctreeLock = bLock; 
}

FORCEINLINE bool FNavigationOctreeController::IsValidElement(const FOctreeElementId2& ElementId) const
{
	return IsValid() && NavOctree->IsValidElementId(ElementId);
}

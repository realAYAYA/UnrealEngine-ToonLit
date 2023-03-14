// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NavigationOctree.h"
#include "AI/Navigation/NavigationDirtyElement.h"

struct NAVIGATIONSYSTEM_API FNavigationOctreeController
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

	void SetNavigationOctreeLock(bool bLock);
	bool HasPendingObjectNavOctreeId(UObject& Object) const;
	void RemoveObjectsNavOctreeId(const UObject& Object);
	void SetNavigableGeometryStoringMode(FNavigationOctree::ENavGeometryStoringMode NavGeometryMode);

	void Reset();

	const FNavigationOctree* GetOctree() const;
	FNavigationOctree* GetMutableOctree();
	const FOctreeElementId2* GetObjectsNavOctreeId(const UObject& Object) const;
	bool GetNavOctreeElementData(const UObject& NodeOwner, int32& DirtyFlags, FBox& DirtyBounds);
	const FNavigationRelevantData* GetDataForObject(const UObject& Object) const;
	FNavigationRelevantData* GetMutableDataForObject(const UObject& Object);
	bool HasObjectsNavOctreeId(const UObject& Object) const;
	bool IsNavigationOctreeLocked() const;
	/** basically says if navoctree has been created already */
	bool IsValid() const { return NavOctree.IsValid(); }
	bool IsValidElement(const FOctreeElementId2& ElementId) const;
	bool IsEmpty() const { return (IsValid() == false) || NavOctree->GetSizeBytes() == 0; }

private:
	static uint32 HashObject(const UObject& Object);
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

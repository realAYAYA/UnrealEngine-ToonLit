// Copyright Epic Games, Inc. All Rights Reserved.

#include "Grid/PCGComponentOctree.h"
#include "PCGComponent.h"

FPCGComponentRef::FPCGComponentRef(UPCGComponent* InComponent, const FPCGComponentOctreeIDSharedRef& InIdShared)
	: IdShared(InIdShared)
{
	check(InComponent);

	Component = InComponent;

	UpdateBounds();
}

void FPCGComponentRef::UpdateBounds()
{
	check(Component);

	Bounds = Component->GetGridBounds();
}

FPCGComponentOctreeAndMap::FPCGComponentOctreeAndMap(const FVector& InOrigin, FVector::FReal InExtent)
	: Octree(InOrigin, InExtent)
{

}

void FPCGComponentOctreeAndMap::Reset(const FVector& InOrigin, FVector::FReal InExtent)
{
	FWriteScopeLock WriteLock(Lock);
	Octree = FPCGComponentOctree(InOrigin, InExtent);
	ComponentToIdMap.Empty();
}

bool FPCGComponentOctreeAndMap::Contains(const UPCGComponent* InComponent) const
{
	FReadScopeLock ReadLock(Lock);

	return ComponentToIdMap.Find(InComponent) != nullptr;
}

FBox FPCGComponentOctreeAndMap::GetBounds(const UPCGComponent* InComponent) const
{
	FBox Bounds(EForceInit::ForceInit);

	FReadScopeLock ReadLock(Lock);
	if (const FPCGComponentOctreeIDSharedRef* ElementIdPtr = ComponentToIdMap.Find(InComponent))
	{
		const FPCGComponentRef& ComponentRef = Octree.GetElementById((*ElementIdPtr)->Id);
		Bounds = ComponentRef.Bounds.GetBox();
	}

	return Bounds;
}

void FPCGComponentOctreeAndMap::AddOrUpdateComponent(UPCGComponent* InComponent, FBox& OutBounds, bool& bOutComponentHasChanged, bool& bOutComponentWasAdded)
{
	FWriteScopeLock WriteLock(Lock);

	FPCGComponentOctreeIDSharedRef* ElementIdPtr = ComponentToIdMap.Find(InComponent);

	if (!ElementIdPtr)
	{
		// Does not exist yet, add it.
		FPCGComponentOctreeIDSharedRef IdShared = MakeShared<FPCGComponentOctreeID>();
		FPCGComponentRef ComponentRef(InComponent, IdShared);
		OutBounds = ComponentRef.Bounds.GetBox();
		check(OutBounds.IsValid);

		// If the Component is already generated, it probably mean we are in loading. The component bounds and last
		// generated bounds should be the same.
		// If the bounds depends on other components on the owner however, it might not be the same, because of the registration order.
		// In this case, override the bounds by the last generated ones.
		FBox LastGeneratedBounds = InComponent->GetLastGeneratedBounds();
		if (InComponent->bGenerated && !LastGeneratedBounds.Equals(OutBounds))
		{
			OutBounds = LastGeneratedBounds;
			ComponentRef.Bounds = OutBounds;
		}

		Octree.AddElement(ComponentRef);

		bOutComponentHasChanged = true;
		bOutComponentWasAdded = true;

		// Store the shared ptr, because if we add/remove components in the octree, the id might change.
		// We need to make sure that we always have the latest id for the given component.
		ComponentToIdMap.Add(InComponent, MoveTemp(IdShared));
	}
	else
	{
		// It already exists, update it if the bounds changed.

		// Do a copy here.
		FPCGComponentRef ComponentRef = Octree.GetElementById((*ElementIdPtr)->Id);
		FBox PreviousBounds = ComponentRef.Bounds.GetBox();
		ComponentRef.UpdateBounds();
		OutBounds = ComponentRef.Bounds.GetBox();
		check(OutBounds.IsValid);

		// If bounds changed, remove and re-add to the octree
		if (!PreviousBounds.Equals(OutBounds))
		{
			Octree.RemoveElement((*ElementIdPtr)->Id);
			Octree.AddElement(ComponentRef);
			bOutComponentHasChanged = true;
		}
	}
}

bool FPCGComponentOctreeAndMap::RemapComponent(const UPCGComponent* InOldComponent, UPCGComponent* InNewComponent, bool& bOutBoundsHasChanged)
{
	// First verification we have the old component registered
	if (!Contains(InOldComponent))
	{
		return false;
	}

	// If so, lock again in write and recheck if it has not been remapped already.
	{
		FWriteScopeLock WriteLock(Lock);

		FPCGComponentOctreeIDSharedRef* ElementIdPtr = ComponentToIdMap.Find(InOldComponent);

		if (!ElementIdPtr)
		{
			// Nothing done
			return false;
		}

		FPCGComponentOctreeIDSharedRef ElementId = *ElementIdPtr;

		FPCGComponentRef ComponentRef = Octree.GetElementById(ElementId->Id);
		FBox PreviousBounds = ComponentRef.Bounds.GetBox();
		ComponentRef.Component = InNewComponent;
		ComponentRef.UpdateBounds();
		FBox Bounds = ComponentRef.Bounds.GetBox();
		check(Bounds.IsValid);

		// If bounds changed, we need to update the mapping
		bOutBoundsHasChanged = !Bounds.Equals(PreviousBounds);

		Octree.RemoveElement((*ElementIdPtr)->Id);
		Octree.AddElement(ComponentRef);
		ComponentToIdMap.Remove(InOldComponent);
		ComponentToIdMap.Add(InNewComponent, MoveTemp(ElementId));
	}

	return true;
}

bool FPCGComponentOctreeAndMap::RemoveComponent(UPCGComponent* InComponent)
{
	FWriteScopeLock WriteLock(Lock);

	FPCGComponentOctreeIDSharedRef* ElementIdPtr = ComponentToIdMap.Find(InComponent);

	if (!ElementIdPtr)
	{
		return false;
	}

	Octree.RemoveElement((*ElementIdPtr)->Id);
	ComponentToIdMap.Remove(InComponent);

	return true;
}

TSet<UPCGComponent*> FPCGComponentOctreeAndMap::GetAllComponents() const
{
	FReadScopeLock ReadLock(Lock);

	TSet<UPCGComponent*> Components;
	ComponentToIdMap.GetKeys(Components);

	return Components;
}

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/BoxSphereBounds.h"
#include "Math/GenericOctree.h"
#include "Misc/ScopeRWLock.h"
#include "UObject/ObjectPtr.h"
#include "Templates/SharedPointer.h"
#include "Templates/SharedPointerFwd.h"

template <typename ElementType, typename OctreeSemantics> class TOctree2;

class UPCGComponent;

struct FPCGComponentOctreeID : public TSharedFromThis<FPCGComponentOctreeID, ESPMode::ThreadSafe>
{
	FOctreeElementId2 Id;
};

using FPCGComponentOctreeIDSharedRef = TSharedRef<struct FPCGComponentOctreeID, ESPMode::ThreadSafe>;

struct FPCGComponentRef
{
	FPCGComponentRef(UPCGComponent* InComponent, const FPCGComponentOctreeIDSharedRef& InIdShared);

	void UpdateBounds();

	FPCGComponentOctreeIDSharedRef IdShared;
	TObjectPtr<UPCGComponent> Component;
	FBoxSphereBounds Bounds;
};

struct FPCGComponentRefSemantics
{
	enum { MaxElementsPerLeaf = 16 };
	enum { MinInclusiveElementsPerNode = 7 };
	enum { MaxNodeDepth = 12 };

	typedef TInlineAllocator<MaxElementsPerLeaf> ElementAllocator;

	FORCEINLINE static const FBoxSphereBounds& GetBoundingBox(const FPCGComponentRef& InVolume)
	{
		return InVolume.Bounds;
	}

	FORCEINLINE static const bool AreElementsEqual(const FPCGComponentRef& A, const FPCGComponentRef& B)
	{
		return A.Component == B.Component;
	}

	FORCEINLINE static void ApplyOffset(FPCGComponentRef& InVolume, const FVector& Offset)
	{
		InVolume.Bounds.Origin += Offset;
	}

	FORCEINLINE static void SetElementId(const FPCGComponentRef& Element, FOctreeElementId2 OctreeElementID)
	{
		Element.IdShared->Id = OctreeElementID;
	}
};

using FPCGComponentOctree = TOctree2<FPCGComponentRef, FPCGComponentRefSemantics> ;
using FPCGComponentToIdMap = TMap<UPCGComponent*, FPCGComponentOctreeIDSharedRef>;

class FPCGComponentOctreeAndMap
{
public:
	FPCGComponentOctreeAndMap() = default;
	FPCGComponentOctreeAndMap(const FVector& InOrigin, FVector::FReal InExtent);

	void Reset(const FVector& InOrigin, FVector::FReal InExtent);

	TSet<UPCGComponent*> GetAllComponents() const;

	template<typename IterateBoundsFunc>
	inline void FindElementsWithBoundsTest(const FBoxCenterAndExtent& BoxBounds, const IterateBoundsFunc& Func) const
	{
		FReadScopeLock ReadLock(Lock);
		return Octree.FindElementsWithBoundsTest(BoxBounds, Func);
	}

	bool Contains(const UPCGComponent* InComponent) const;
	FBox GetBounds(const UPCGComponent* InComponent) const;

	void AddOrUpdateComponent(UPCGComponent* InComponent, FBox& OutBounds, bool& bOutComponentHasChanged, bool& bOutComponentWasAdded);
	bool RemapComponent(const UPCGComponent* InOldComponent, UPCGComponent* InNewComponent, bool& bOutBoundsHasChanged);
	bool RemoveComponent(UPCGComponent* InComponent);

private:
	FPCGComponentOctree Octree;
	FPCGComponentToIdMap ComponentToIdMap;
	mutable FRWLock Lock;
};
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SmartObjectTypes.h"
#include "Templates/SharedPointer.h"
#include "Math/GenericOctree.h"
#include "SmartObjectOctree.generated.h"

typedef TSharedRef<struct FSmartObjectOctreeID, ESPMode::ThreadSafe> FSmartObjectOctreeIDSharedRef;

struct SMARTOBJECTSMODULE_API FSmartObjectOctreeID : public TSharedFromThis<FSmartObjectOctreeID, ESPMode::ThreadSafe>
{
	FOctreeElementId2 ID;
};

struct SMARTOBJECTSMODULE_API FSmartObjectOctreeElement
{
	FBoxCenterAndExtent Bounds;
	FSmartObjectHandle SmartObjectHandle;
	FSmartObjectOctreeIDSharedRef SharedOctreeID;

	FSmartObjectOctreeElement(const FBoxCenterAndExtent& Bounds, const FSmartObjectHandle SmartObjectHandle, const FSmartObjectOctreeIDSharedRef& SharedOctreeID);
};

struct FSmartObjectOctreeSemantics
{
	enum { MaxElementsPerLeaf = 16 };
	enum { MinInclusiveElementsPerNode = 7 };
	enum { MaxNodeDepth = 12 };

	typedef TInlineAllocator<MaxElementsPerLeaf> ElementAllocator;

	FORCEINLINE static const FBoxCenterAndExtent& GetBoundingBox(const FSmartObjectOctreeElement& Element)
	{
		return Element.Bounds;
	}

	FORCEINLINE static bool AreElementsEqual(const FSmartObjectOctreeElement& A, const FSmartObjectOctreeElement& B)
	{
		return A.SmartObjectHandle == B.SmartObjectHandle;
	}

	static void SetElementId(const FSmartObjectOctreeElement& Element, FOctreeElementId2 Id);
};

struct FSmartObjectOctree : TOctree2<FSmartObjectOctreeElement, FSmartObjectOctreeSemantics>
{
public:
	FSmartObjectOctree();
	FSmartObjectOctree(const FVector& Origin, float Radius);
	virtual ~FSmartObjectOctree();

	/** Add new node and initialize using SmartObject runtime data */
	void AddNode(const FBoxCenterAndExtent& Bounds, const FSmartObjectHandle SmartObjectHandle, const FSmartObjectOctreeIDSharedRef& SharedOctreeID);
	
	/** Updates element bounds remove/add operation */
	void UpdateNode(const FOctreeElementId2& Id, const FBox& NewBounds);

	/** Remove node */
	void RemoveNode(const FOctreeElementId2& Id);
};

USTRUCT()
struct SMARTOBJECTSMODULE_API FSmartObjectOctreeEntryData : public FSmartObjectSpatialEntryData
{
	GENERATED_BODY()

	FSmartObjectOctreeEntryData() : SharedOctreeID(MakeShareable(new FSmartObjectOctreeID())) {}

	FSmartObjectOctreeIDSharedRef SharedOctreeID;
};

UCLASS()
class SMARTOBJECTSMODULE_API USmartObjectOctree : public USmartObjectSpacePartition
{
	GENERATED_BODY()

protected:
	virtual FInstancedStruct Add(const FSmartObjectHandle Handle, const FBox& Bounds) override;
	virtual void Remove(const FSmartObjectHandle Handle, const FStructView& EntryData) override;
	virtual void Find(const FBox& QueryBox, TArray<FSmartObjectHandle>& OutResults) override;
	virtual void SetBounds(const FBox& Bounds) override;

private:
	FSmartObjectOctree SmartObjectOctree;
};
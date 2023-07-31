// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectOctree.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectOctree)

//----------------------------------------------------------------------//
// FSmartObjectOctreeElement
//----------------------------------------------------------------------//
FSmartObjectOctreeElement::FSmartObjectOctreeElement(const FBoxCenterAndExtent& InBounds, const FSmartObjectHandle InSmartObjectHandle, const FSmartObjectOctreeIDSharedRef& InSharedOctreeID)
	: Bounds(InBounds)
	, SmartObjectHandle(InSmartObjectHandle)
	, SharedOctreeID(InSharedOctreeID)
{
}

//----------------------------------------------------------------------//
// FSmartObjectOctree
//----------------------------------------------------------------------//
FSmartObjectOctree::FSmartObjectOctree()
	: FSmartObjectOctree(FVector::ZeroVector, 0)
{

}

FSmartObjectOctree::FSmartObjectOctree(const FVector& Origin, const float Radius)
	: TOctree2<FSmartObjectOctreeElement, FSmartObjectOctreeSemantics>(Origin, Radius)
{
}

FSmartObjectOctree::~FSmartObjectOctree()
{
}

void FSmartObjectOctree::AddNode(const FBoxCenterAndExtent& Bounds, const FSmartObjectHandle SmartObjectHandle, const FSmartObjectOctreeIDSharedRef& SharedOctreeID)
{
	AddElement(FSmartObjectOctreeElement(Bounds, SmartObjectHandle, SharedOctreeID));
}

void FSmartObjectOctree::UpdateNode(const FOctreeElementId2& Id, const FBox& NewBounds)
{
	FSmartObjectOctreeElement ElementCopy = GetElementById(Id);
	RemoveElement(Id);
	ElementCopy.Bounds = NewBounds;
	AddElement(ElementCopy);
}

void FSmartObjectOctree::RemoveNode(const FOctreeElementId2& Id)
{
	RemoveElement(Id);
}

//----------------------------------------------------------------------//
// FSmartObjectOctreeSemantics
//----------------------------------------------------------------------//
void FSmartObjectOctreeSemantics::SetElementId(const FSmartObjectOctreeElement& Element, const FOctreeElementId2 Id)
{
	Element.SharedOctreeID->ID = Id;
}

//----------------------------------------------------------------------//
// USmartObjectOctree
//----------------------------------------------------------------------//
void USmartObjectOctree::SetBounds(const FBox& Bounds)
{
	new(&SmartObjectOctree) FSmartObjectOctree(Bounds.GetCenter(), Bounds.GetExtent().Size2D());
}

FInstancedStruct USmartObjectOctree::Add(const FSmartObjectHandle Handle, const FBox& Bounds)
{
	const FSmartObjectOctreeEntryData EntryData;
	SmartObjectOctree.AddNode(Bounds, Handle, EntryData.SharedOctreeID);
	return FInstancedStruct::Make(EntryData);
}

void USmartObjectOctree::Remove(const FSmartObjectHandle Handle, const FStructView& EntryData)
{
	const FSmartObjectOctreeEntryData& OctreeEntryData = EntryData.GetMutable<FSmartObjectOctreeEntryData>();
	FSmartObjectOctreeID& SharedOctreeID = OctreeEntryData.SharedOctreeID.Get();
	if (SharedOctreeID.ID.IsValidId())
	{
		SmartObjectOctree.RemoveNode(SharedOctreeID.ID);
		SharedOctreeID.ID = {};
	}
}

void USmartObjectOctree::Find(const FBox& QueryBox, TArray<FSmartObjectHandle>& OutResults)
{
	SmartObjectOctree.FindElementsWithBoundsTest(QueryBox,
		[&OutResults](const FSmartObjectOctreeElement& Element)
		{
			OutResults.Add(Element.SmartObjectHandle);
		});
}

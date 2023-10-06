// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGContext.h"
#include "PCGElement.h"
#include "PCGModule.h"
#include "Data/PCGPointData.h"

/** 
* Convenience class to do point data partitioning, i.e. building a TMap<Value, TArray<point/point indices>>
* Uses CRTP to remove the need for virtual calls.
* Has some behavior options depending on the derived class.
* Currently supports time-slicing (caveat for rooting data, not done yet)
* Can easily write to a common output (for match & set cases)
* Can write to separate data (for partitioning)
* Supports multiple data (indices won't be useful though)
* 
* The intended implementation is that derived classes will implement the same functions (e.g. name hiding) and those will
* be appropriately called through using the derived class type.
* Methods that are not re-implemented/hidden will retain their default implementation, which is mostly trivial.
*/
template<typename Derived, typename KeyType>
class FPCGPointDataPartitionBase
{
public:
	struct Element
	{
		TArray<int32> PointIndices;
		UPCGPointData* PartitionData = nullptr;
	};

	FPCGPointDataPartitionBase() = default;
	FPCGPointDataPartitionBase(const TArrayView<KeyType>& InKeys)
	{
		for (const KeyType& Key : InKeys)
		{
			ElementMap.Emplace(Key, Element());
		}
	}

	Derived* This() { return static_cast<Derived*>(this); }
	const Derived* This() const { return static_cast<const Derived*>(this); }

	/** Overridable behavior methods */
	bool Initialize() { return true; }
	bool InitializeForPointData(const UPCGPointData* PointData) { return true; }
	void AddToPartitionData(Element* SelectedElement, const UPCGPointData* ParentPointData, const FPCGPoint& Point) {}
	void WriteToOutputData(UPCGPointData* OutPointData, Element* SelectedElement, const FPCGPoint& Point) {}
	Element* SelectPoint(const FPCGPoint& Point, int32 PointIndex) { return nullptr; }
	int32 TimeSlicingCheckFrequency() const { return 1024; }

	/** API */
	bool SelectPoints(FPCGContext& Context, const UPCGPointData* PointData, int32& InCurrentPointIndex, UPCGPointData* OutPointData);
	void Reset() { ElementMap.Reset(); }

	// Data access
	TMap<KeyType, Element> ElementMap;
};

template<typename Derived, typename KeyType>
bool FPCGPointDataPartitionBase<Derived, KeyType>::SelectPoints(FPCGContext& Context, const UPCGPointData* PointData, int32& InCurrentPointIndex, UPCGPointData* OutPointData)
{
	if (!PointData)
	{
		PCGE_LOG_C(Error, GraphAndLog, &Context, NSLOCTEXT("PCGPointDataPartition", "InputMissingData", "Missing input data"));
		return true;
	}

	if (InCurrentPointIndex == 0)
	{
		if (!This()->InitializeForPointData(PointData))
		{
			return true;
		}
	}

	int32 CurrentPointIndex = InCurrentPointIndex;
	int32 LastCheckpointIndex = InCurrentPointIndex;
	const int32 TimeSlicingCheckFrequency = This()->TimeSlicingCheckFrequency();

	const TArray<FPCGPoint>& Points = PointData->GetPoints();

	while (CurrentPointIndex < Points.Num())
	{
		const FPCGPoint& Point = Points[CurrentPointIndex];

		Element* SelectedElement = This()->SelectPoint(Point, CurrentPointIndex);
		if (SelectedElement)
		{
			SelectedElement->PointIndices.Add(CurrentPointIndex);
			This()->AddToPartitionData(SelectedElement, PointData, Point);
		}

		This()->WriteToOutputData(OutPointData, SelectedElement, Point);

		++CurrentPointIndex;

		if (CurrentPointIndex - LastCheckpointIndex >= TimeSlicingCheckFrequency)
		{
			if (Context.ShouldStop())
			{
				break;
			}
			else
			{
				LastCheckpointIndex = CurrentPointIndex;
			}
		}
	}

	InCurrentPointIndex = CurrentPointIndex;

	return (CurrentPointIndex == Points.Num());
}
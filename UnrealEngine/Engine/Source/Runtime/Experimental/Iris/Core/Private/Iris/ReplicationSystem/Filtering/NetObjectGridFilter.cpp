// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/Filtering/NetObjectGridFilter.h"
#include "Iris/ReplicationSystem/RepTag.h"
#include "Iris/ReplicationSystem/ReplicationProtocol.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/WorldLocations.h"
#include "Iris/Core/IrisLog.h"
#include "Iris/Core/IrisProfiler.h"

void UNetObjectGridFilter::Init(FNetObjectFilterInitParams& Params)
{
	Config = TStrongObjectPtr<UNetObjectGridFilterConfig>(CastChecked<UNetObjectGridFilterConfig>(Params.Config));
	checkf(Config.IsValid(), TEXT("Need config to operate."));

	AssignedObjectInfoIndices.Init(Params.MaxObjectCount);

	PerConnectionInfos.SetNum(Params.MaxConnectionCount + 1);

	WorldLocations = &Params.ReplicationSystem->GetWorldLocations();
}

void UNetObjectGridFilter::AddConnection(uint32 ConnectionId)
{
	PerConnectionInfos[ConnectionId] = {};
}

void UNetObjectGridFilter::RemoveConnection(uint32 ConnectionId)
{
	PerConnectionInfos[ConnectionId] = {};
}

bool UNetObjectGridFilter::AddObject(uint32 ObjectIndex, FNetObjectFilterAddObjectParams& Params)
{
	// We support either a world location in the state, tagged with RepTag_WorldLocation, or via the WorldLocations instance.
	UE::Net::FRepTagFindInfo WorldLocationTagInfo;
	bool bHasWorldLocation = false;
	if (WorldLocations->HasWorldLocation(ObjectIndex))
	{
		bHasWorldLocation = true;
		// Craft tag info that will let us know we need to retrieve the location from WorldLocations
		WorldLocationTagInfo.StateIndex = InvalidStateIndex;
		WorldLocationTagInfo.ExternalStateOffset = InvalidStateOffset;
	}
	else if (!UE::Net::FindRepTag(Params.Protocol, UE::Net::RepTag_WorldLocation, WorldLocationTagInfo))
	{
		return false;
	}

	if (!bHasWorldLocation && ((WorldLocationTagInfo.ExternalStateOffset >= MAX_uint16) || (WorldLocationTagInfo.StateIndex >= MAX_uint16)))
	{
		return false;
	}

	FObjectLocationInfo& ObjectLocationInfo = static_cast<FObjectLocationInfo&>(Params.OutInfo);
	ObjectLocationInfo.SetLocationStateOffset(WorldLocationTagInfo.ExternalStateOffset);
	ObjectLocationInfo.SetLocationStateIndex(WorldLocationTagInfo.StateIndex);

	const uint32 InfoIndex = AllocObjectInfo();
	ObjectLocationInfo.SetInfoIndex(InfoIndex);

	FPerObjectInfo& PerObjectInfo = ObjectInfos[InfoIndex];
	PerObjectInfo.ObjectIndex = ObjectIndex;

	// NetCullDistanceSqr is optional. 
	{
		UE::Net::FRepTagFindInfo NetCullDistanceSqrTagInfo;
		if (UE::Net::FindRepTag(Params.Protocol, UE::Net::RepTag_CullDistanceSqr, NetCullDistanceSqrTagInfo))
		{
			if ((NetCullDistanceSqrTagInfo.ExternalStateOffset < MAX_uint16) && (NetCullDistanceSqrTagInfo.StateIndex < MAX_uint16))
			{
				PerObjectInfo.CullDistanceSqrStateIndex = NetCullDistanceSqrTagInfo.StateIndex;
				PerObjectInfo.CullDistanceSqrStateOffset = NetCullDistanceSqrTagInfo.ExternalStateOffset;
			}
		}
	}

	AddCellInfoForObject(ObjectLocationInfo, Params.InstanceProtocol);
	
	if (PerObjectInfo.CullDistance > Config->MaxCullDistance)
	{
		// Too big an object. We expect it to be costly to move it across cells.
		RemoveObject(ObjectIndex, ObjectLocationInfo);
		return false;
	}

	return true;
}

void UNetObjectGridFilter::RemoveObject(uint32 ObjectIndex, const FNetObjectFilteringInfo& Info)
{
	const FObjectLocationInfo& ObjectLocationInfo = static_cast<const FObjectLocationInfo&>(Info);
	RemoveCellInfoForObject(ObjectLocationInfo);

	const uint32 InfoIndex = ObjectLocationInfo.GetInfoIndex();
	FreeObjectInfo(InfoIndex);
}

void UNetObjectGridFilter::UpdateObjects(FNetObjectFilterUpdateParams& Params)
{
	IRIS_PROFILER_SCOPE(UNetObjectGridFilter_UpdateObjects);

	for (SIZE_T ObjectIt = 0, ObjectEndIt = Params.ObjectCount; ObjectIt != ObjectEndIt; ++ObjectIt)
	{
		const uint32 ObjectIndex = Params.ObjectIndices[ObjectIt];

		const FObjectLocationInfo& ObjectLocationInfo = static_cast<const FObjectLocationInfo&>(Params.FilteringInfos[ObjectIndex]);
		const UE::Net::FReplicationInstanceProtocol* InstanceProtocol = Params.InstanceProtocols[ObjectIt];
		UpdateCellInfoForObject(ObjectLocationInfo, InstanceProtocol);
	}
}

void UNetObjectGridFilter::PreFilter(FNetObjectPreFilteringParams&)
{
	++FrameIndex;
}

void UNetObjectGridFilter::Filter(FNetObjectFilteringParams& Params)
{
	using namespace UE::Net;

	IRIS_PROFILER_SCOPE(UNetObjectGridFilter_Filter);

	// Update active cells for the connection
	FPerConnectionInfo& ConnectionInfo = PerConnectionInfos[Params.ConnectionId];
	TArray<FCellAndTimestamp, TInlineAllocator<32>> PrevCells = ConnectionInfo.RecentCells;
	ConnectionInfo.RecentCells.Reset();

	// Insert current cells for the views and remove any stale data from the previous cells.
	TArray<FCellAndTimestamp, TInlineAllocator<32>> NewCells;
	for (FReplicationView::FView& View : Params.View.Views)
	{
		FCellAndTimestamp CellAndTimestamp;
		CellAndTimestamp.Timestamp = FrameIndex;
		CalculateCellCoord(CellAndTimestamp.Cell, View.Pos);
		NewCells.AddUnique(CellAndTimestamp);

		for (const FCellAndTimestamp& PrevCell : PrevCells)
		{
			if ((PrevCell.Cell.X == CellAndTimestamp.Cell.X) & (PrevCell.Cell.Y == CellAndTimestamp.Cell.Y))
			{
				PrevCells.RemoveAtSwap(&PrevCell - PrevCells.GetData());
				break;
			}
		}
	}

	// Prune old cells and insert still relevant in the new list.
	{
		const uint32 MaxFrameCount = Config->ViewPosRelevancyFrameCount;
		for (const FCellAndTimestamp& PrevCell : PrevCells)
		{
			if ((FrameIndex - PrevCell.Timestamp) > MaxFrameCount)
			{
				continue;
			}

			NewCells.Add(PrevCell);
		}
	}

	// Store new cells
	ConnectionInfo.RecentCells = NewCells;

	// Only allow objects in any of the relevant cells to replicate.
	FNetBitArrayView AllowedObjects = Params.OutAllowedObjects;
	AllowedObjects.Reset();

	/**
	 * The algorithm will simply iterate over all relevant cells and set the bits
	 * in the AllowedObjects bitarray for all the objects present in the cell. An object can span multiple cells,
	 * but We assume it's cheaper to modify the bitarray than to create a new set with the unique
	 * object indices and then iterate over that. In both cases one do need to iterate over all
	 * objects in all relevant cells anyway and setting a bit should be faster than inserting into a set.
	 */
	for (const FCellAndTimestamp& CellAndTimestamp : NewCells)
	{
		if (FCellObjects* Objects = Cells.Find(CellAndTimestamp.Cell))
		{
			for (const uint32 ObjectIndex : Objects->ObjectIndices)
			{
				AllowedObjects.SetBit(ObjectIndex);
			}
		}
	}
}

uint32 UNetObjectGridFilter::AllocObjectInfo()
{
	uint32 Index = AssignedObjectInfoIndices.FindFirstZero();
	if (Index >= uint32(ObjectInfos.Num()))
	{
		constexpr int32 NumElementsPerChunk = ObjectInfosChunkSize/sizeof(FPerObjectInfo);
		ObjectInfos.Add(NumElementsPerChunk);
	}

	AssignedObjectInfoIndices.SetBit(Index);
	return Index;
}

void UNetObjectGridFilter::FreeObjectInfo(uint32 Index)
{
	AssignedObjectInfoIndices.ClearBit(Index);
	ObjectInfos[Index] = FPerObjectInfo();
}

void UNetObjectGridFilter::AddCellInfoForObject(const FObjectLocationInfo& ObjectLocationInfo, const UE::Net::FReplicationInstanceProtocol* InstanceProtocol)
{
	// Called for completely new objects
	FPerObjectInfo& PerObjectInfo = ObjectInfos[ObjectLocationInfo.GetInfoIndex()];
	PerObjectInfo.CullDistance = Config->DefaultCullDistance;
	UpdatePositionAndCullDistance(ObjectLocationInfo, PerObjectInfo, InstanceProtocol);

	FCellBox NewCellBox;
	CalculateCellBox(PerObjectInfo, NewCellBox);
	PerObjectInfo.CellBox = NewCellBox;

	const uint32 ObjectIndex = PerObjectInfo.ObjectIndex;

	// Add object to new cells.
	{
		FCellCoord Coord;
		for (int32 X = NewCellBox.MinX, EndX = NewCellBox.MaxX + 1; X < EndX; ++X)
		{
			Coord.X = X;
			for (int32 Y = NewCellBox.MinY, EndY = NewCellBox.MaxY + 1; Y < EndY; ++Y)
			{
				Coord.Y = Y;
				FCellObjects& Cell = Cells.FindOrAdd(Coord);
				Cell.ObjectIndices.Add(ObjectIndex);
			}
		}
	}
}

void UNetObjectGridFilter::RemoveCellInfoForObject(const FObjectLocationInfo& ObjectLocationInfo)
{
	FPerObjectInfo& PerObjectInfo = ObjectInfos[ObjectLocationInfo.GetInfoIndex()];

	const FCellBox CellBox = PerObjectInfo.CellBox;
	const uint32 ObjectIndex = PerObjectInfo.ObjectIndex;

	// Remove object from cells.
	{
		FCellCoord Coord;
		for (int32 X = CellBox.MinX, EndX = CellBox.MaxX + 1; X < EndX; ++X)
		{
			Coord.X = X;
			for (int32 Y = CellBox.MinY, EndY = CellBox.MaxY + 1; Y < EndY; ++Y)
			{
				Coord.Y = Y;
				FCellObjects* Cell = Cells.Find(Coord);
				checkSlow(Cell != nullptr);
				Cell->ObjectIndices.Remove(ObjectIndex);
			}
		}
	}
}

void UNetObjectGridFilter::UpdateCellInfoForObject(const FObjectLocationInfo& ObjectLocationInfo, const UE::Net::FReplicationInstanceProtocol* InstanceProtocol)
{
	FPerObjectInfo& PerObjectInfo = ObjectInfos[ObjectLocationInfo.GetInfoIndex()];

	UpdatePositionAndCullDistance(ObjectLocationInfo, PerObjectInfo, InstanceProtocol);

	const FCellBox PrevCellBox = PerObjectInfo.CellBox;
	FCellBox NewCellBox;
	CalculateCellBox(PerObjectInfo, NewCellBox);

	if (NewCellBox != PrevCellBox)
	{
		const uint32 ObjectIndex = PerObjectInfo.ObjectIndex;
		PerObjectInfo.CellBox = NewCellBox;

		// Special case for disjoint cells
		if (AreCellsDisjoint(NewCellBox, PrevCellBox))
		{
			// Remove object from previous cells.
			{
				FCellCoord Coord;
				for (int32 X = PrevCellBox.MinX, EndX = PrevCellBox.MaxX + 1; X < EndX; ++X)
				{
					Coord.X = X;
					for (int32 Y = PrevCellBox.MinY, EndY = PrevCellBox.MaxY + 1; Y < EndY; ++Y)
					{
						Coord.Y = Y;
						FCellObjects* Cell = Cells.Find(Coord);
						checkSlow(Cell != nullptr);
						Cell->ObjectIndices.Remove(ObjectIndex);
					}
				}
			}

			// Add object to new cells.
			{
				FCellCoord Coord;
				for (int32 X = NewCellBox.MinX, EndX = NewCellBox.MaxX + 1; X < EndX; ++X)
				{
					Coord.X = X;
					for (int32 Y = NewCellBox.MinY, EndY = NewCellBox.MaxY + 1; Y < EndY; ++Y)
					{
						Coord.Y = Y;
						FCellObjects& Cell = Cells.FindOrAdd(Coord);
						Cell.ObjectIndices.Add(ObjectIndex);
					}
				}
			}
		}
		// There's some overlap
		else
		{
			/**
			 * Naive algorithm to find out which cells to update.
			 * 
			 * 1. Calculate the cell overlap.
			 * 2. For each coord exclusive to the previous cell we will remove the object.
			 * 3. For each coord exclusive to the new cell we will add the object.
			 */
			FCellBox Overlap;
			Overlap.MinX = FPlatformMath::Max(NewCellBox.MinX, PrevCellBox.MinX);
			Overlap.MaxX = FPlatformMath::Min(NewCellBox.MaxX, PrevCellBox.MaxX);
			Overlap.MinY = FPlatformMath::Max(NewCellBox.MinY, PrevCellBox.MinY);
			Overlap.MaxY = FPlatformMath::Min(NewCellBox.MaxY, PrevCellBox.MaxY);
			
			// Remove object from previous cells.
			{
				FCellCoord Coord;
				for (int32 X = PrevCellBox.MinX, EndX = PrevCellBox.MaxX + 1; X < EndX; ++X)
				{
					Coord.X = X;
					for (int32 Y = PrevCellBox.MinY, EndY = PrevCellBox.MaxY + 1; Y < EndY; ++Y)
					{
						Coord.Y = Y;

						if (DoesCellContainCoord(Overlap, Coord))
						{
							continue;
						}

						FCellObjects* Cell = Cells.Find(Coord);
						checkSlow(Cell != nullptr);
						Cell->ObjectIndices.Remove(ObjectIndex);
					}
				}
			}

			// Add object to new cells.
			{
				FCellCoord Coord;
				for (int32 X = NewCellBox.MinX, EndX = NewCellBox.MaxX + 1; X < EndX; ++X)
				{
					Coord.X = X;
					for (int32 Y = NewCellBox.MinY, EndY = NewCellBox.MaxY + 1; Y < EndY; ++Y)
					{
						Coord.Y = Y;

						if (DoesCellContainCoord(Overlap, Coord))
						{
							continue;
						}

						FCellObjects& Cell = Cells.FindOrAdd(Coord);
						Cell.ObjectIndices.Add(ObjectIndex);
					}
				}
			}
		}
	}
}

void UNetObjectGridFilter::UpdatePositionAndCullDistance(const UNetObjectGridFilter::FObjectLocationInfo& ObjectLocationInfo, UNetObjectGridFilter::FPerObjectInfo& PerObjectInfo, const UE::Net::FReplicationInstanceProtocol* InstanceProtocol)
{
	TArrayView<const UE::Net::FReplicationInstanceProtocol::FFragmentData> FragmentDatas = MakeArrayView(InstanceProtocol->FragmentData, InstanceProtocol->FragmentCount);

	// Update position
	{
		if (ObjectLocationInfo.IsUsingWorldLocations())
		{
			PerObjectInfo.Position = WorldLocations->GetWorldLocation(PerObjectInfo.ObjectIndex);
		}
		else
		{
			const UE::Net::FReplicationInstanceProtocol::FFragmentData& FragmentData = FragmentDatas[ObjectLocationInfo.GetLocationStateIndex()];
			const uint8* LocationAddress = FragmentData.ExternalSrcBuffer + ObjectLocationInfo.GetLocationStateOffset();
			const float* LocationComponents = reinterpret_cast<const float*>(LocationAddress);
			PerObjectInfo.Position = FVector(LocationComponents[0], LocationComponents[1], LocationComponents[2]);
		}
	}

	// Optionally update cull distance
	if (PerObjectInfo.CullDistanceSqrStateOffset != InvalidStateOffset)
	{
		const UE::Net::FReplicationInstanceProtocol::FFragmentData& FragmentData = FragmentDatas[PerObjectInfo.CullDistanceSqrStateIndex];
		const uint8* CullDistanceSqrAddress = FragmentData.ExternalSrcBuffer + PerObjectInfo.CullDistanceSqrStateOffset;
		const float CullDistanceSqr = *reinterpret_cast<const float*>(CullDistanceSqrAddress);
		PerObjectInfo.CullDistance = FPlatformMath::Sqrt(CullDistanceSqr);
	}
}

void UNetObjectGridFilter::CalculateCellBox(const UNetObjectGridFilter::FPerObjectInfo& PerObjectInfo, UNetObjectGridFilter::FCellBox& OutCellBox)
{
	const float CullDistance = PerObjectInfo.CullDistance;
	const FVector Position = PerObjectInfo.Position;
	FVector MinPosition = Position - CullDistance;
	FVector MaxPosition = Position + CullDistance;

	MinPosition = MinPosition.ComponentMax(Config->MinPos);
	MaxPosition = MaxPosition.ComponentMin(Config->MaxPos);

	FCellBox CellBox;
	CellBox.MinX = FPlatformMath::FloorToInt(MinPosition.X/Config->CellSizeX);
	CellBox.MinY = FPlatformMath::FloorToInt(MinPosition.Y/Config->CellSizeY);
	CellBox.MaxX = FPlatformMath::FloorToInt(MaxPosition.X/Config->CellSizeX);
	CellBox.MaxY = FPlatformMath::FloorToInt(MaxPosition.Y/Config->CellSizeY);

	OutCellBox = CellBox;
}

void UNetObjectGridFilter::CalculateCellCoord(UNetObjectGridFilter::FCellCoord& OutCoord, const FVector& Pos)
{
	FCellCoord Coord;
	Coord.X = FPlatformMath::FloorToInt(Pos.X/Config->CellSizeX);
	Coord.Y = FPlatformMath::FloorToInt(Pos.Y/Config->CellSizeY);

	OutCoord = Coord;
}

bool UNetObjectGridFilter::AreCellsDisjoint(const UNetObjectGridFilter::FCellBox& A, const UNetObjectGridFilter::FCellBox& B)
{
	return (A.MinX > B.MaxX) | (A.MaxX < B.MinX) | (A.MinY > B.MaxY) | (A.MaxY < B.MinY);
}

bool UNetObjectGridFilter::DoesCellContainCoord(const UNetObjectGridFilter::FCellBox& Cell, const UNetObjectGridFilter::FCellCoord& Coord)
{
	return (Coord.X >= Cell.MinX) & (Coord.X <= Cell.MaxX) & (Coord.Y >= Cell.MinY) & (Coord.Y <= Cell.MaxY);
}

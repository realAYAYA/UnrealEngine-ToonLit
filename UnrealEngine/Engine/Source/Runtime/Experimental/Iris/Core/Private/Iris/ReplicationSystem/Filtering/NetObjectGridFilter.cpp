// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/Filtering/NetObjectGridFilter.h"
#include "Iris/ReplicationSystem/NetCullDistanceOverrides.h"
#include "Iris/ReplicationSystem/RepTag.h"
#include "Iris/ReplicationSystem/ReplicationProtocol.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/WorldLocations.h"
#include "Iris/Core/IrisLog.h"
#include "Iris/Core/IrisProfiler.h"

void UNetObjectGridFilter::OnInit(FNetObjectFilterInitParams& Params)
{
	Config = TStrongObjectPtr<UNetObjectGridFilterConfig>(CastChecked<UNetObjectGridFilterConfig>(Params.Config));
	checkf(Config.IsValid(), TEXT("Need config to operate."));

	AssignedObjectInfoIndices.Init(Params.MaxObjectCount);

	PerConnectionInfos.SetNum(Params.MaxConnectionCount + 1);

	NetCullDistanceOverrides = &Params.ReplicationSystem->GetNetCullDistanceOverrides();
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
	const bool bHasLocation = BuildObjectInfo(ObjectIndex, Params);

	if (!bHasLocation)
	{
		return false;
	}

	FObjectLocationInfo& ObjectLocationInfo = static_cast<FObjectLocationInfo&>(Params.OutInfo);
	
	const uint32 InfoIndex = AllocObjectInfo();
	ObjectLocationInfo.SetInfoIndex(InfoIndex);

	FPerObjectInfo& PerObjectInfo = ObjectInfos[InfoIndex];
	PerObjectInfo.ObjectIndex = ObjectIndex;

	AddCellInfoForObject(ObjectLocationInfo, Params.InstanceProtocol);
	
	if (PerObjectInfo.CullDistance > Config->MaxCullDistance)
	{
		// Too big an object. We expect it to be costly to move it across cells.
		RemoveObject(ObjectIndex, ObjectLocationInfo);
		//TODO: Should we log these objects so they can get flagged and properly set always relevant ?
		return false;
	}

	return true;
}

void UNetObjectGridFilter::RemoveObject(uint32 ObjectIndex, const FNetObjectFilteringInfo& Info)
{
	const FObjectLocationInfo& ObjectLocationInfo = static_cast<const FObjectLocationInfo&>(Info);
	RemoveCellInfoForObject(ObjectLocationInfo);

	OnObjectRemoved(ObjectIndex);

	const uint32 InfoIndex = ObjectLocationInfo.GetInfoIndex();
	FreeObjectInfo(InfoIndex);
}

void UNetObjectGridFilter::UpdateObjects(FNetObjectFilterUpdateParams& Params)
{
	for (SIZE_T ObjectIt = 0, ObjectEndIt = Params.ObjectCount; ObjectIt != ObjectEndIt; ++ObjectIt)
	{
		const uint32 ObjectIndex = Params.ObjectIndices[ObjectIt];

		const FObjectLocationInfo& ObjectLocationInfo = static_cast<const FObjectLocationInfo&>(Params.FilteringInfos[ObjectIndex]);
		const UE::Net::FReplicationInstanceProtocol* InstanceProtocol = Params.InstanceProtocols ? Params.InstanceProtocols[ObjectIt] : nullptr;
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
				PrevCells.RemoveAtSwap(static_cast<int32>(&PrevCell - PrevCells.GetData()));
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
	// Update position & culldistance
	{
		UpdateObjectInfo(PerObjectInfo, ObjectLocationInfo, InstanceProtocol);
	}

	// Optionally update cull distance
	if (NetCullDistanceOverrides->HasCullDistanceOverride(PerObjectInfo.ObjectIndex))
	{
		const float CullDistanceSqr = NetCullDistanceOverrides->GetCullDistanceSqr(PerObjectInfo.ObjectIndex);
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

	const int64 MinX = FPlatformMath::FloorToInt(MinPosition.X/Config->CellSizeX);
	const int64 MinY = FPlatformMath::FloorToInt(MinPosition.Y/Config->CellSizeY);
	const int64 MaxX = FPlatformMath::FloorToInt(MaxPosition.X/Config->CellSizeX);
	const int64 MaxY = FPlatformMath::FloorToInt(MaxPosition.Y/Config->CellSizeY);

	FCellBox CellBox;
	CellBox.MinX = static_cast<int32>(MinX);
	CellBox.MinY = static_cast<int32>(MinY);
	CellBox.MaxX = static_cast<int32>(MaxX);
	CellBox.MaxY = static_cast<int32>(MaxY);

	// Current large world max of 8796093022208.0 requires a cell size of at least around 4500 to not overflow an int32. 
	checkSlow(MinX == CellBox.MinX && MinY == CellBox.MinY && MaxX == CellBox.MaxX && MaxY == CellBox.MaxY);

	OutCellBox = CellBox;
}

void UNetObjectGridFilter::CalculateCellCoord(UNetObjectGridFilter::FCellCoord& OutCoord, const FVector& Pos)
{
	FCellCoord Coord;
	Coord.X = static_cast<int32>(FPlatformMath::FloorToInt(Pos.X/Config->CellSizeX));
	Coord.Y = static_cast<int32>(FPlatformMath::FloorToInt(Pos.Y/Config->CellSizeY));

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

//*************************************************************************************************
// UNetObjectGridWorldLocFilter
//*************************************************************************************************

void UNetObjectGridWorldLocFilter::OnInit(FNetObjectFilterInitParams& Params)
{
	Super::OnInit(Params);

	SetupFilterType(ENetFilterType::PrePoll_Raw);

	WorldLocations = &Params.ReplicationSystem->GetWorldLocations();
}

void UNetObjectGridWorldLocFilter::UpdateObjectInfo(UNetObjectGridFilter::FPerObjectInfo& PerObjectInfo, const UNetObjectGridFilter::FObjectLocationInfo& ObjectLocationInfo, const UE::Net::FReplicationInstanceProtocol* InstanceProtocol)
{
	check(ObjectLocationInfo.IsUsingWorldLocations());

	const UE::Net::FWorldLocations::FObjectInfo& CachedObjectInfo = WorldLocations->GetObjectInfo(PerObjectInfo.ObjectIndex);
	
	PerObjectInfo.Position = CachedObjectInfo.WorldLocation;
	PerObjectInfo.CullDistance = CachedObjectInfo.CullDistance;
}

bool UNetObjectGridWorldLocFilter::BuildObjectInfo(uint32 ObjectIndex, FNetObjectFilterAddObjectParams& Params)
{
	checkf(WorldLocations->HasInfoForObject(ObjectIndex), TEXT("UNetObjectGridWorldLocFilter only supports objects with valid world location data."));

	// Craft tag info that will let us know we need to retrieve the location from WorldLocations
	FObjectLocationInfo& ObjectLocationInfo = static_cast<FObjectLocationInfo&>(Params.OutInfo);
	ObjectLocationInfo.SetLocationStateOffset(InvalidStateOffset);
	ObjectLocationInfo.SetLocationStateIndex(InvalidStateIndex);

	return true;
}

//*************************************************************************************************
// UNetObjectGridFragmentLocFilter
//*************************************************************************************************

void UNetObjectGridFragmentLocFilter::OnInit(FNetObjectFilterInitParams& InitParams)
{
	Super::OnInit(InitParams);

	SetupFilterType(ENetFilterType::PostPoll_FragmentBased);
}

bool UNetObjectGridFragmentLocFilter::BuildObjectInfo(uint32 ObjectIndex, FNetObjectFilterAddObjectParams& Params)
{
	UE::Net::FRepTagFindInfo WorldLocationTagInfo;
	if (UE::Net::FindRepTag(Params.Protocol, UE::Net::RepTag_WorldLocation, WorldLocationTagInfo))
	{
		// Want to keep the memory footprint minimal so don't allow adding objects whose values would not fit.
		if (WorldLocationTagInfo.ExternalStateOffset >= MAX_uint16 || WorldLocationTagInfo.StateIndex >= MAX_uint16)
		{
			return false;
		}

		FObjectLocationInfo& ObjectLocationInfo = static_cast<FObjectLocationInfo&>(Params.OutInfo);
		ObjectLocationInfo.SetLocationStateOffset(static_cast<uint16>(WorldLocationTagInfo.ExternalStateOffset));
		ObjectLocationInfo.SetLocationStateIndex(static_cast<uint16>(WorldLocationTagInfo.StateIndex));

		// NetCullDistanceSqr is optional. 
		UE::Net::FRepTagFindInfo NetCullDistanceSqrTagInfo;
		if (UE::Net::FindRepTag(Params.Protocol, UE::Net::RepTag_CullDistanceSqr, NetCullDistanceSqrTagInfo))
		{
			if ((NetCullDistanceSqrTagInfo.ExternalStateOffset < MAX_uint16) && (NetCullDistanceSqrTagInfo.StateIndex < MAX_uint16))
			{
				FCullDistanceFragmentInfo FragmentInfo;
				FragmentInfo.CullDistanceSqrStateIndex = static_cast<uint16>(NetCullDistanceSqrTagInfo.StateIndex);
				FragmentInfo.CullDistanceSqrStateOffset = static_cast<uint16>(NetCullDistanceSqrTagInfo.ExternalStateOffset);

				CullDistanceFragments.Add(ObjectIndex, MoveTemp(FragmentInfo));
			}
		}

		return true;
	}

	return false;
}

void UNetObjectGridFragmentLocFilter::OnObjectRemoved(uint32 ObjectIndex)
{
	CullDistanceFragments.Remove(ObjectIndex);
}

void UNetObjectGridFragmentLocFilter::UpdateObjectInfo(UNetObjectGridFilter::FPerObjectInfo& PerObjectInfo, const UNetObjectGridFilter::FObjectLocationInfo& ObjectLocationInfo, const UE::Net::FReplicationInstanceProtocol* InstanceProtocol)
{
	check(ObjectLocationInfo.IsUsingWorldLocations() == false);
	check(InstanceProtocol);

	TArrayView<const UE::Net::FReplicationInstanceProtocol::FFragmentData> FragmentDatas = MakeArrayView(InstanceProtocol->FragmentData, InstanceProtocol->FragmentCount);

	// Update the location
	{
		const UE::Net::FReplicationInstanceProtocol::FFragmentData& FragmentData = FragmentDatas[ObjectLocationInfo.GetLocationStateIndex()];
		const uint8* LocationAddress = FragmentData.ExternalSrcBuffer + ObjectLocationInfo.GetLocationStateOffset();
		const FVector* Location = reinterpret_cast<const FVector*>(LocationAddress);
		PerObjectInfo.Position = *Location;
	}

	// Update the culldistance
	if( FCullDistanceFragmentInfo* CullDistanceFragmentInfo = CullDistanceFragments.Find(PerObjectInfo.ObjectIndex) )
	{
		const UE::Net::FReplicationInstanceProtocol::FFragmentData& FragmentData = FragmentDatas[CullDistanceFragmentInfo->CullDistanceSqrStateIndex];
		const uint8* CullDistanceSqrAddress = FragmentData.ExternalSrcBuffer + CullDistanceFragmentInfo->CullDistanceSqrStateOffset;
		const float CullDistanceSqr = *reinterpret_cast<const float*>(CullDistanceSqrAddress);
		PerObjectInfo.CullDistance = FPlatformMath::Sqrt(CullDistanceSqr);
	}
}


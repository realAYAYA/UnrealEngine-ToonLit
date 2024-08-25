// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectHashGrid.h"
#include "DebugRenderSceneProxy.h"
#include "Math/ColorList.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectHashGrid)

void USmartObjectHashGrid::Add(const FSmartObjectHandle Handle, const FBox& Bounds, FInstancedStruct& OutHandle)
{
	FSmartObjectHashGridEntryData GridEntryData;
	GridEntryData.CellLoc = HashGrid.Add(Handle, Bounds);
	
	OutHandle = FConstStructView::Make(GridEntryData);
}

void USmartObjectHashGrid::Remove(const FSmartObjectHandle Handle, FStructView EntryData)
{
	FSmartObjectHashGridEntryData& GridEntryData = EntryData.Get<FSmartObjectHashGridEntryData>();
	HashGrid.Remove(Handle, GridEntryData.CellLoc);
	GridEntryData.CellLoc = {};
}

void USmartObjectHashGrid::Find(const FBox& QueryBox, TArray<FSmartObjectHandle>& OutResults)
{
	HashGrid.QuerySmall(QueryBox, OutResults);
}

#if UE_ENABLE_DEBUG_DRAWING
void USmartObjectHashGrid::Draw(FDebugRenderSceneProxy* DebugProxy)
{
	const TSet<FSmartObjectHashGrid2D::FCell>& AllCells = HashGrid.GetCells();
	for (auto It(AllCells.CreateConstIterator()); It; ++It)
	{
		FBox CellBounds = HashGrid.CalcCellBounds(FSmartObjectHashGrid2D::FCellLocation(It->X, It->Y, It->Level));
		DebugProxy->Boxes.Emplace(CellBounds, GColorList.GetFColorByIndex(It->Level));
	}
}
#endif //UE_ENABLE_DEBUG_DRAWING


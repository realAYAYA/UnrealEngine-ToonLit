// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HierarchicalHashGrid2D.h"
#include "SmartObjectTypes.h"
#include "SmartObjectHashGrid.generated.h"

struct FSmartObjectHandle;

typedef THierarchicalHashGrid2D<2, 4, FSmartObjectHandle> FSmartObjectHashGrid2D;

USTRUCT()
struct SMARTOBJECTSMODULE_API FSmartObjectHashGridEntryData : public FSmartObjectSpatialEntryData
{
	GENERATED_BODY()

	FSmartObjectHashGrid2D::FCellLocation CellLoc;
};

UCLASS()
class SMARTOBJECTSMODULE_API USmartObjectHashGrid : public USmartObjectSpacePartition
{
	GENERATED_BODY()

protected:
	virtual FInstancedStruct Add(const FSmartObjectHandle Handle, const FBox& Bounds) override;
	virtual void Remove(const FSmartObjectHandle Handle, const FStructView& EntryData) override;
	virtual void Find(const FBox& QueryBox, TArray<FSmartObjectHandle>& OutResults) override;

#if UE_ENABLE_DEBUG_DRAWING
	virtual void Draw(FDebugRenderSceneProxy* DebugProxy) override;
#endif
private:
	FSmartObjectHashGrid2D HashGrid;
};

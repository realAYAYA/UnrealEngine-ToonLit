// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorldPartition/WorldPartitionBuilder.h"
#include "WorldPartitionSmartObjectCollectionBuilder.generated.h"

class ASmartObjectPersistentCollection;

/**
 * WorldPartitionBuilder dedicated to collect all smart object components from a world and store them in the collection.
 */
UCLASS()
class SMARTOBJECTSEDITORMODULE_API UWorldPartitionSmartObjectCollectionBuilder : public UWorldPartitionBuilder
{
	GENERATED_UCLASS_BODY()

protected:
	virtual bool RequiresCommandletRendering() const override { return false; }
	virtual ELoadingMode GetLoadingMode() const override { return IterativeCells; }
	virtual bool RunInternal(UWorld* World, const FCellInfo& InCellInfo, FPackageSourceControlHelper& PackageHelper) override;
	virtual bool PreRun(UWorld* World, FPackageSourceControlHelper& PackageHelper) override;
	virtual bool PostRun(UWorld* World, FPackageSourceControlHelper& PackageHelper, const bool bInRunSuccess) override;

	TArray<uint32> NumSmartObjectsBefore;
	TArray<uint32> OriginalContentsHash;
	uint32 NumSmartObjectsTotal = 0;

	bool bRemoveEmptyCollections = false;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "SmartObjectPersistentCollection.h"
#endif

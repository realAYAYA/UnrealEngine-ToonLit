// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SmartObjectCollection.h"
#include "WorldPartition/WorldPartitionBuilder.h"
#include "WorldPartitionSmartObjectCollectionBuilder.generated.h"

class ASmartObjectCollection;

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

	UPROPERTY(Transient)
	TObjectPtr<ASmartObjectCollection> MainCollection;

	uint32 NumSmartObjectsBefore = 0;
	uint32 NumSmartObjectsTotal = 0;
};
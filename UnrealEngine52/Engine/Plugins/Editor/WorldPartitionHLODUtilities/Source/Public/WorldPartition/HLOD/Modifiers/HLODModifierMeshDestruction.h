// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorldPartition/HLOD/HLODModifier.h"
#include "HLODModifierMeshDestruction.generated.h"


class FDestructionMeshMergeExtension;


UCLASS()
class WORLDPARTITIONHLODUTILITIES_API UWorldPartitionHLODModifierMeshDestruction : public UWorldPartitionHLODModifier
{
	GENERATED_UCLASS_BODY()

	virtual bool CanModifyHLOD(TSubclassOf<UHLODBuilder> InHLODBuilderClass) const;
	virtual void BeginHLODBuild(const FHLODBuildContext& InHLODBuildContext);
	virtual void EndHLODBuild(TArray<UActorComponent*>&InOutComponents);

private:
	FDestructionMeshMergeExtension* DestructionMeshMergeExtension;
};

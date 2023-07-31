// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Containers/Map.h"

#include "WorldPartition/WorldPartitionActorDesc.h"
#include "WorldPartition/DataLayer/DataLayersID.h"
#include "WorldPartition/HLOD/HLODStats.h"
#include "WorldPartition/HLOD/HLODSubActor.h"

class UHLODLayer;
class AWorldPartitionHLOD;

/**
 * ActorDesc for AWorldPartitionHLOD.
 */
class ENGINE_API FHLODActorDesc : public FWorldPartitionActorDesc
{
	friend class FHLODActorDescFactory;

public:
	typedef TMap<FName, int64>	FStats;

	inline const TArray<FHLODSubActorDesc>& GetSubActors() const { return HLODSubActors; }
	inline const FName GetSourceCellName() const { return SourceCellName; }
	inline const FName GetSourceHLODLayerName() const { return SourceHLODLayerName; }
	inline const FStats& GetStats() const { return HLODStats; }
	inline int64 GetStat(FName InStatName) const { return HLODStats.FindRef(InStatName); }

	int64 GetPackageSize() const;
	static int64 GetPackageSize(const AWorldPartitionHLOD* InHLODActor);

protected:
	//~ Begin FWorldPartitionActorDesc Interface.
	virtual void Init(const AActor* InActor) override;
	virtual bool Equals(const FWorldPartitionActorDesc* Other) const override;
	virtual void Serialize(FArchive& Ar) override;
	virtual bool IsRuntimeRelevant(const FActorContainerID& InContainerID) const override { return !bIsForcedNonSpatiallyLoaded; }
	//~ End FWorldPartitionActorDesc Interface.

	TArray<FHLODSubActorDesc> HLODSubActors;
	FName SourceCellName;
	FName SourceHLODLayerName;
	FStats HLODStats;
};
#endif

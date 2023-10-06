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
class FHLODActorDesc : public FWorldPartitionActorDesc
{
	friend class FHLODActorDescFactory;

public:
	typedef TMap<FName, int64>	FStats;

	inline const TArray<FGuid>& GetChildHLODActors() const { return ChildHLODActors; }
	inline const FName GetSourceHLODLayerName() const { return SourceHLODLayerName; }
	inline const FStats& GetStats() const { return HLODStats; }
	inline int64 GetStat(FName InStatName) const { return HLODStats.FindRef(InStatName); }

	ENGINE_API int64 GetPackageSize() const;
	static ENGINE_API int64 GetPackageSize(const AWorldPartitionHLOD* InHLODActor);

protected:
	//~ Begin FWorldPartitionActorDesc Interface.
	ENGINE_API virtual void Init(const AActor* InActor) override;
	ENGINE_API virtual bool Equals(const FWorldPartitionActorDesc* Other) const override;
	virtual uint32 GetSizeOf() const override { return sizeof(FHLODActorDesc); }
	ENGINE_API virtual void Serialize(FArchive& Ar) override;
	virtual bool IsRuntimeRelevant(const FActorContainerID& InContainerID) const override { return !bIsForcedNonSpatiallyLoaded; }
	virtual bool ShouldValidateRuntimeGrid() const override { return false; }
	//~ End FWorldPartitionActorDesc Interface.

	TArray<FGuid> ChildHLODActors;
	FName SourceHLODLayerName;
	FStats HLODStats;
};
#endif

// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "GameFramework/Actor.h"
#include "Containers/Array.h"
#include "Containers/Set.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "WorldPartition/HLOD/HLODSubActor.h"
#include "WorldPartition/HLOD/HLODStats.h"
#include "HLODActor.generated.h"

class UHLODLayer;
class UHLODSubsystem;

UCLASS(NotPlaceable)
class ENGINE_API AWorldPartitionHLOD : public AActor
{
	GENERATED_UCLASS_BODY()

	typedef TMap<FName, int64>	FStats;

public:
	void SetVisibility(bool bInVisible);

	inline FName GetSourceCellName() const { return SourceCellName; }
	inline uint32 GetLODLevel() const { return LODLevel; }

	virtual bool IsHLODRelevant() const override { return true; }

	bool DoesRequireWarmup() const { return bRequireWarmup; }

#if WITH_EDITOR
	void SetHLODComponents(const TArray<UActorComponent*>& InHLODComponents);

	void SetSubActors(const TArray<FHLODSubActor>& InSubActorMappings);
	const TArray<FHLODSubActor>& GetSubActors() const;

	void SetSubActorsHLODLayer(const UHLODLayer* InSubActorsHLODLayer);
	const UHLODLayer* GetSubActorsHLODLayer() const { return SubActorsHLODLayer; }

	void SetRequireWarmup(bool InRequireWarmup) { bRequireWarmup = InRequireWarmup; }

	void SetSourceCellName(FName InSourceCellName);
	inline void SetLODLevel(uint32 InLODLevel) { LODLevel = InLODLevel; }

	const FBox& GetHLODBounds() const;
	void SetHLODBounds(const FBox& InBounds);

	double GetMinVisibleDistance() const { return MinVisibleDistance; }
	void SetMinVisibleDistance(double InMinVisibleDistance) { MinVisibleDistance = InMinVisibleDistance; }

	void BuildHLOD(bool bForceBuild = false);
	uint32 GetHLODHash() const;

	const FStats& GetStats() const { return HLODStats; }
	int64 GetStat(FName InStatName) const { return HLODStats.FindRef(InStatName); }
	void SetStat(FName InStatName, int64 InStatValue) { HLODStats.Add(InStatName, InStatValue); }
	void ResetStats() { HLODStats.Reset(); }
#endif // WITH_EDITOR

protected:
	//~ Begin UObject Interface.
	virtual void Serialize(FArchive& Ar) override;
#if WITH_EDITOR
	virtual void PostLoad() override;
	virtual void RerunConstructionScripts() override;
	virtual bool CanEditChange(const FProperty* InProperty) const { return false; }
	virtual bool CanEditChangeComponent(const UActorComponent* Component, const FProperty* InProperty) const { return false; }
#endif
	//~ End UObject Interface.

	//~ Begin AActor Interface.
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool IsRuntimeOnly() const override { return true; }
#if WITH_EDITOR
	virtual TUniquePtr<class FWorldPartitionActorDesc> CreateClassActorDesc() const override;

	virtual void GetActorBounds(bool bOnlyCollidingComponents, FVector& Origin, FVector& BoxExtent, bool bIncludeFromChildActors) const override;
	virtual FBox GetStreamingBounds() const override;

	virtual bool ShouldImport(FString* ActorPropString, bool IsMovingLevel) override { return false; }
	virtual bool IsLockLocation() const { return true; }
	virtual bool IsUserManaged() const override { return false; }
#endif
	//~ End AActor Interface.

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TArray<FHLODSubActor> HLODSubActors;

	UPROPERTY()
	TObjectPtr<const UHLODLayer> SubActorsHLODLayer;

	UPROPERTY()
	FBox HLODBounds;

	UPROPERTY()
	double MinVisibleDistance;

	UPROPERTY()
	uint32 HLODHash;

	UPROPERTY()
	TMap<FName, int64> HLODStats;
#endif

	UPROPERTY()
	uint32 LODLevel;

	UPROPERTY()
	bool bRequireWarmup;

	UPROPERTY()
	TSoftObjectPtr<UWorldPartitionRuntimeCell> SourceCell_DEPRECATED;

	UPROPERTY()
	FName SourceCellName;
};

DEFINE_ACTORDESC_TYPE(AWorldPartitionHLOD, FHLODActorDesc);
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "Containers/Array.h"
#include "Containers/Set.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "WorldPartition/HLOD/HLODStats.h"

#if WITH_EDITOR
#include "UObject/ObjectSaveContext.h"
#endif // WITH_EDITOR

#include "HLODActor.generated.h"

class UHLODLayer;
class UWorldPartitionHLODSourceActors;

ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogHLODHash, Log, All);

UCLASS(NotPlaceable, MinimalAPI, HideCategories=(Rendering, Replication, Collision, Physics, Navigation, Networking, Input, Actor, LevelInstance, Cooking))
class AWorldPartitionHLOD : public AActor
{
	GENERATED_UCLASS_BODY()

	friend class FHLODActorDesc;
	typedef TMap<FName, int64> FStats;

public:
	ENGINE_API void SetVisibility(bool bInVisible);

	ENGINE_API const FGuid& GetSourceCellGuid() const;
	inline uint32 GetLODLevel() const { return LODLevel; }

	virtual bool IsHLODRelevant() const override { return true; }

	bool DoesRequireWarmup() const { return bRequireWarmup; }

#if WITH_EDITOR
	ENGINE_API void SetHLODComponents(const TArray<UActorComponent*>& InHLODComponents);

	ENGINE_API void SetSourceActors(UWorldPartitionHLODSourceActors* InSourceActors);
	ENGINE_API UWorldPartitionHLODSourceActors* GetSourceActors();
	ENGINE_API const UWorldPartitionHLODSourceActors* GetSourceActors() const;

	void SetRequireWarmup(bool InRequireWarmup) { bRequireWarmup = InRequireWarmup; }

	ENGINE_API void SetSourceCellGuid(const FGuid& InSourceCellGuid);
	inline void SetLODLevel(uint32 InLODLevel) { LODLevel = InLODLevel; }

	ENGINE_API const FBox& GetHLODBounds() const;
	ENGINE_API void SetHLODBounds(const FBox& InBounds);

	double GetMinVisibleDistance() const { return MinVisibleDistance; }
	void SetMinVisibleDistance(double InMinVisibleDistance) { MinVisibleDistance = InMinVisibleDistance; }

	ENGINE_API void BuildHLOD(bool bForceBuild = false);
	ENGINE_API uint32 GetHLODHash() const;

	ENGINE_API int64 GetStat(FName InStatName) const;
	void SetStat(FName InStatName, int64 InStatValue) { HLODStats.Add(InStatName, InStatValue); }
	void ResetStats() { HLODStats.Reset(); }

private:
	const FStats& GetStats() const { return HLODStats; }

#endif // WITH_EDITOR

protected:
	//~ Begin UObject Interface.
	ENGINE_API virtual void Serialize(FArchive& Ar) override;
	ENGINE_API virtual bool IsEditorOnly() const override;
	ENGINE_API virtual bool NeedsLoadForServer() const override;
	ENGINE_API virtual void PostLoad() override;
#if WITH_EDITOR
	ENGINE_API virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
	ENGINE_API virtual void RerunConstructionScripts() override;
	virtual bool CanEditChange(const FProperty* InProperty) const override { return false; }
	virtual bool CanEditChangeComponent(const UActorComponent* Component, const FProperty* InProperty) const override { return false; }
#endif
	//~ End UObject Interface.

	//~ Begin AActor Interface.
	ENGINE_API virtual void PreRegisterAllComponents() override;
	ENGINE_API virtual void BeginPlay() override;
	ENGINE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool IsRuntimeOnly() const override { return true; }
#if WITH_EDITOR
	ENGINE_API virtual TUniquePtr<class FWorldPartitionActorDesc> CreateClassActorDesc() const override;
	ENGINE_API virtual FBox GetStreamingBounds() const override;

	virtual bool ShouldImport(FStringView ActorPropString, bool IsMovingLevel) override { return false; }
	virtual bool IsLockLocation() const override { return true; }
	virtual bool IsUserManaged() const override { return false; }
#endif
	//~ End AActor Interface.

private:
#if WITH_EDITOR
	ENGINE_API void OnWorldCleanup(UWorld* InWorld, bool bSessionEnded, bool bCleanupResources);
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UWorldPartitionHLODSourceActors> SourceActors;

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
	FGuid SourceCellGuid;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TSoftObjectPtr<UWorldPartitionRuntimeCell> SourceCell_DEPRECATED;

	UPROPERTY()
	FName SourceCellName_DEPRECATED;

	UPROPERTY()
	TArray<FWorldPartitionRuntimeCellObjectMapping> HLODSubActors_DEPRECATED;

	UPROPERTY()
	TObjectPtr<const UHLODLayer> SubActorsHLODLayer_DEPRECATED;
#endif	
};

DEFINE_ACTORDESC_TYPE(AWorldPartitionHLOD, FHLODActorDesc);

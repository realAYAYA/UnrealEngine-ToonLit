// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "WorldPartition.h"
#include "WorldPartitionActorDescView.h"
#include "WorldPartition/ActorDescList.h"
#include "WorldPartition/WorldPartitionHandle.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"
#include "WorldPartition/WorldPartitionRuntimeCellOwner.h"
#include "WorldPartition/WorldPartitionActorDescViewProxy.h"
#include "WorldPartition/WorldPartitionStreamingGeneration.h"
#if WITH_EDITOR
#include "CookPackageSplitter.h"
#include "Misc/HierarchicalLogArchive.h"
#endif
#include "WorldPartitionRuntimeHash.generated.h"

class FActorDescViewMap;
class IStreamingGenerationContext;
struct FHierarchicalLogArchive;

UENUM()
enum class EWorldPartitionStreamingPerformance : uint8
{
	Good,
	Slow,
	Critical
};

UCLASS(Abstract)
class ENGINE_API URuntimeHashExternalStreamingObjectBase : public UObject, public IWorldPartitionRuntimeCellOwner
{
	GENERATED_BODY()

public:
	virtual void Initialize(UWorld* InOwningWorld, UWorld* InOuterWorld) 
	{ 
		OwningWorld = InOwningWorld;  
		OuterWorld = InOuterWorld;
	}

	//~ Begin IWorldPartitionRuntimeCellOwner Interface
	virtual UWorld* GetOwningWorld() const override { return OwningWorld.Get(); }
	virtual UWorld* GetOuterWorld() const override { return OuterWorld.Get(); }
	//~ End IWorldPartitionRuntimeCellOwner Interface

	virtual class UWorld* GetWorld() const override { return GetOwningWorld(); }

	virtual void ForEachStreamingCells(TFunctionRef<void(UWorldPartitionRuntimeCell&)> Func) {};
	
	virtual void OnStreamingObjectLoaded() {};

#if WITH_EDITOR
	virtual void PopulateGeneratorPackageForCook() {};
#endif

private:
	UPROPERTY();
	TSoftObjectPtr<UWorld> OwningWorld;

	UPROPERTY();
	TSoftObjectPtr<UWorld> OuterWorld;
};

UCLASS(Abstract, Config=Engine, AutoExpandCategories=(WorldPartition), Within = WorldPartition)
class ENGINE_API UWorldPartitionRuntimeHash : public UObject, public IWorldPartitionRuntimeCellOwner
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR
	virtual void SetDefaultValues() {}
	virtual bool SupportsHLODs() const { return false; }
	virtual bool PrepareGeneratorPackageForCook(TArray<UPackage*>& OutModifiedPackages) { return false; }
	virtual bool PopulateGeneratorPackageForCook(const TArray<FWorldPartitionCookPackage*>& PackagesToCook, TArray<UPackage*>& OutModifiedPackages) { return false; }
	virtual bool PopulateGeneratedPackageForCook(const FWorldPartitionCookPackage& PackagesToCook, TArray<UPackage*>& OutModifiedPackages) { return false; }
	virtual bool GenerateStreaming(class UWorldPartitionStreamingPolicy* StreamingPolicy, const IStreamingGenerationContext* StreamingGenerationContext, TArray<FString>* OutPackagesToGenerate) { return false; }
	virtual void FlushStreaming() {}
	virtual bool GenerateHLOD(ISourceControlHelper* SourceControlHelper, const IStreamingGenerationContext* StreamingGenerationContext, bool bCreateActorsOnly) { return false; }
	virtual void DrawPreview() const {}

	virtual URuntimeHashExternalStreamingObjectBase* StoreToExternalStreamingObject(UObject* StreamingObjectOuter, FName StreamingObjectName) { return nullptr; }

	virtual void DumpStateLog(FHierarchicalLogArchive& Ar);

	// PIE/Game methods
	void OnBeginPlay();
	void OnEndPlay();

protected:
	bool ConditionalRegisterAlwaysLoadedActorsForPIE(const FWorldPartitionActorDescView& ActorDescView, bool bIsMainWorldPartition, bool bIsMainContainer, bool bIsCellAlwaysLoaded);
#endif

public:
	class FStreamingSourceCells
	{
	public:
		void AddCell(const UWorldPartitionRuntimeCell* Cell, const FWorldPartitionStreamingSource& Source, const FSphericalSector& SourceShape);
		void Reset() { Cells.Reset(); }
		int32 Num() const { return Cells.Num(); }
		TSet<const UWorldPartitionRuntimeCell*>& GetCells() { return Cells; }

	private:
		TSet<const UWorldPartitionRuntimeCell*> Cells;
	};

	// Deprecated streaming interface
	UE_DEPRECATED(5.1, "GetAllStreamingCells is deprecated, use ForEachStreamingCells instead.")
	int32 GetAllStreamingCells(TSet<const UWorldPartitionRuntimeCell*>& Cells, bool bAllDataLayers = false, bool bDataLayersOnly = false, const TSet<FName>& InDataLayers = TSet<FName>()) const;

	UE_DEPRECATED(5.1, "GetStreamingCells is deprecated, use ForEachStreamingCells instead.")
	bool GetStreamingCells(const FWorldPartitionStreamingQuerySource& QuerySource, TSet<const UWorldPartitionRuntimeCell*>& OutCells) const;

	UE_DEPRECATED(5.1, "GetStreamingCells is deprecated, use ForEachStreamingCells instead.")
	bool GetStreamingCells(const TArray<FWorldPartitionStreamingSource>& Sources, UWorldPartitionRuntimeHash::FStreamingSourceCells& OutActivateCells, UWorldPartitionRuntimeHash::FStreamingSourceCells& OutLoadCells) const;

	//~ Begin IWorldPartitionRuntimeCellOwner Interface
	virtual UWorld* GetOwningWorld() const override { return GetWorld(); }
	virtual UWorld* GetOuterWorld() const override { return GetTypedOuter<UWorld>(); }
	//~ End IWorldPartitionRuntimeCellOwner Interface

	// Streaming interface
	virtual void ForEachStreamingCells(TFunctionRef<bool(const UWorldPartitionRuntimeCell*)> Func) const {}
	virtual void ForEachStreamingCellsQuery(const FWorldPartitionStreamingQuerySource& QuerySource, TFunctionRef<bool(const UWorldPartitionRuntimeCell*)> Func) const {}
	virtual void ForEachStreamingCellsSources(const TArray<FWorldPartitionStreamingSource>& Sources, TFunctionRef<bool(const UWorldPartitionRuntimeCell*, EStreamingSourceTargetState)> Func) const {}

	bool IsCellRelevantFor(bool bClientOnlyVisible) const;
	EWorldPartitionStreamingPerformance GetStreamingPerformance(const TSet<const UWorldPartitionRuntimeCell*>& CellToActivate) const;

	virtual URuntimeHashExternalStreamingObjectBase* StoreToExternalStreamingObject(UObject* Outer) { return nullptr; }
	virtual bool InjectExternalStreamingObject(URuntimeHashExternalStreamingObjectBase* ExternalStreamingObject) { return false; }
	virtual bool RemoveExternalStreamingObject(URuntimeHashExternalStreamingObjectBase* ExternalStreamingObject) { return false; }

	virtual bool Draw2D(class UCanvas* Canvas, const TArray<FWorldPartitionStreamingSource>& Sources, const FVector2D& PartitionCanvasSize, const FVector2D& Offset, FVector2D& OutUsedCanvasSize) const { return false; }
	virtual void Draw3D(const TArray<FWorldPartitionStreamingSource>& Sources) const {}
	virtual bool ContainsRuntimeHash(const FString& Name) const { return false; }

protected:
	virtual EWorldPartitionStreamingPerformance GetStreamingPerformanceForCell(const UWorldPartitionRuntimeCell* Cell) const { return EWorldPartitionStreamingPerformance::Good; }

private:
#if WITH_EDITOR
	void ForceExternalActorLevelReference(bool bForceExternalActorLevelReferenceForPIE);
#endif

protected:
#if WITH_EDITORONLY_DATA
	struct FAlwaysLoadedActorForPIE
	{
		FAlwaysLoadedActorForPIE(const FWorldPartitionReference& InReference, AActor* InActor)
			: Reference(InReference), Actor(InActor)
		{}

		FWorldPartitionReference Reference;
		AActor* Actor;
	};

	TArray<FAlwaysLoadedActorForPIE> AlwaysLoadedActorsForPIE;

public:
	mutable FActorDescList ModifiedActorDescListForPIE;
#endif
};
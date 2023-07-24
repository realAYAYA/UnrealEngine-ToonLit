// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/WorldPartitionActorDescView.h"
#include "WorldPartition/WorldPartitionStreamingSource.h"
#include "WorldPartition/WorldPartitionActorContainerID.h"
#include "WorldPartition/WorldPartitionRuntimeCellData.h"
#include "WorldPartition/WorldPartitionRuntimeCellInterface.h"
#include "WorldPartition/WorldPartitionRuntimeCellOwner.h"
#include "ProfilingDebugging/ProfilingHelpers.h"
#include "Misc/HierarchicalLogArchive.h"
#include "Algo/AnyOf.h"
#include "WorldPartitionRuntimeCell.generated.h"

class UDataLayerAsset;
class UDataLayerInstance;
struct FHierarchicalLogArchive;

enum class EWorldPartitionRuntimeCellVisualizeMode
{
	StreamingPriority,
	StreamingStatus
};

USTRUCT()
struct FWorldPartitionRuntimeCellObjectMapping
{
	GENERATED_USTRUCT_BODY()

	FWorldPartitionRuntimeCellObjectMapping()
#if WITH_EDITORONLY_DATA
		: Package(NAME_None)
		, Path(NAME_None)
		, ContainerTransform(FTransform::Identity)
		, ContainerPackage(NAME_None)
		, LoadedPath(NAME_None)
#endif
	{}

	FWorldPartitionRuntimeCellObjectMapping(FName InPackage, FName InPath, const FActorContainerID& InContainerID, const FTransform& InContainerTransform, FName InContainerPackage, FName InWorldPackage, const FGuid& InContentBundleGuid)
#if WITH_EDITORONLY_DATA
		: Package(InPackage)
		, Path(InPath)
		, ContainerID(InContainerID)
		, ContainerTransform(InContainerTransform)
		, ContainerPackage(InContainerPackage)
		, WorldPackage(InWorldPackage)
		, ContentBundleGuid(InContentBundleGuid)
		, LoadedPath(InPath)
#endif
	{}

#if WITH_EDITORONLY_DATA
	/** 
	 * The name of the package to load to resolve on disk (can contain a single actor or a data chunk)
	 */
	UPROPERTY()
	FName Package;

	/** 
	 * The complete name path of the contained object
	 */
	UPROPERTY()
	FName Path;

	/**
	 * ID of the owning container instance
	 */
	UPROPERTY()
	FActorContainerID ContainerID;

	/** 
	 * Transform of the owning container instance
	 */
	UPROPERTY()
	FTransform ContainerTransform;
		
	/**
	 * Package of the owning container instance
	 */
	UPROPERTY()
	FName ContainerPackage;

	/**
	 * Package of the World 
	 */
	UPROPERTY()
	FName WorldPackage;

	/**
	 * Content Bundle Id
	 */
	UPROPERTY()
	FGuid ContentBundleGuid;

	/**
	* Loaded actor path (when cooking or pie)
	* 
	* Depending on if the actor was part of a container instance or the main partition this will be the path
	* of the loaded or duplicated actor before it is moved into its runtime cell.
	* 
	* If the actor was part of the world partition this path should match the Path property.
	*/
	UPROPERTY()
	FName LoadedPath;
#endif
};

class UActorDescContainer;

/**
 * Cell State
 */
UENUM(BlueprintType)
enum class EWorldPartitionRuntimeCellState : uint8
{
	Unloaded,
	Loaded,
	Activated
};

USTRUCT()
struct FWorldPartitionRuntimeCellDebugInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FString Name;

	UPROPERTY()
	FName GridName;

	UPROPERTY()
	int64 CoordX = 0;

	UPROPERTY()
	int64 CoordY = 0;

	UPROPERTY()
	int64 CoordZ = 0;
};

static_assert(EWorldPartitionRuntimeCellState::Unloaded < EWorldPartitionRuntimeCellState::Loaded && EWorldPartitionRuntimeCellState::Loaded < EWorldPartitionRuntimeCellState::Activated, "Streaming Query code is dependent on this being true");

/**
 * Represents a PIE/Game streaming cell which points to external actor/data chunk packages
 */
UCLASS(Abstract)
class ENGINE_API UWorldPartitionRuntimeCell : public UObject, public IWorldPartitionCell
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
#endif

	virtual void Load() const PURE_VIRTUAL(UWorldPartitionRuntimeCell::Load,);
	virtual void Unload() const PURE_VIRTUAL(UWorldPartitionRuntimeCell::Unload,);
	virtual bool CanUnload() const PURE_VIRTUAL(UWorldPartitionRuntimeCell::CanUnload, return true;);
	virtual void Activate() const PURE_VIRTUAL(UWorldPartitionRuntimeCell::Activate,);
	virtual void Deactivate() const PURE_VIRTUAL(UWorldPartitionRuntimeCell::Deactivate,);
	virtual bool IsAddedToWorld() const PURE_VIRTUAL(UWorldPartitionRuntimeCell::IsAddedToWorld, return false;);
	virtual bool CanAddToWorld() const PURE_VIRTUAL(UWorldPartitionRuntimeCell::CanAddToWorld, return false;);
	virtual ULevel* GetLevel() const PURE_VIRTUAL(UWorldPartitionRuntimeCell::GetLevel, return nullptr;);
	virtual EWorldPartitionRuntimeCellState GetCurrentState() const PURE_VIRTUAL(UWorldPartitionRuntimeCell::GetCurrentState, return EWorldPartitionRuntimeCellState::Unloaded;);
	virtual FLinearColor GetDebugColor(EWorldPartitionRuntimeCellVisualizeMode VisualizeMode) const { static const FLinearColor DefaultColor = FLinearColor::Black.CopyWithNewOpacity(0.25f); return DefaultColor; }
	virtual bool IsAlwaysLoaded() const { return bIsAlwaysLoaded; }
	virtual void SetIsAlwaysLoaded(bool bInIsAlwaysLoaded) { bIsAlwaysLoaded = bInIsAlwaysLoaded; }
	virtual void SetPriority(int32 InPriority) { Priority = InPriority; }
	virtual void SetStreamingPriority(int32 InStreamingPriority) const PURE_VIRTUAL(UWorldPartitionRuntimeCell::SetStreamingPriority,);
	virtual EStreamingStatus GetStreamingStatus() const { return LEVEL_Unloaded; }
	virtual bool IsLoading() const { return false; }
	virtual const FString& GetDebugName() const { return DebugInfo.Name; }
	virtual bool IsDebugShown() const;
	virtual FName GetGridName() const { return DebugInfo.GridName; }
	bool GetClientOnlyVisible() const { return bClientOnlyVisible; }
	virtual FGuid const& GetContentBundleID() const { return ContentBundleID; }
	virtual TArray<FName> GetActors() const PURE_VIRTUAL(UWorldPartitionRuntimeCell::GetActors, return TArray<FName>(););

	/** Caches information on streaming source that will be used later on to sort cell. */
	bool ShouldResetStreamingSourceInfo() const;
	void ResetStreamingSourceInfo() const;
	void AppendStreamingSourceInfo(const FWorldPartitionStreamingSource& Source, const FSphericalSector& SourceShape) const;
	void MergeStreamingSourceInfo() const;
	int32 SortCompare(const UWorldPartitionRuntimeCell* Other, bool bCanUseSortingCache = true) const;

	//~Begin IWorldPartitionCell Interface
	virtual TArray<const UDataLayerInstance*> GetDataLayerInstances() const override;
	virtual bool ContainsDataLayer(const UDataLayerAsset* DataLayerAsset) const override;
	virtual bool ContainsDataLayer(const UDataLayerInstance* DataLayerInstance) const override;
	virtual bool HasDataLayers() const override { return !DataLayers.IsEmpty(); }
	virtual const TArray<FName>& GetDataLayers() const override  { return DataLayers; }
	virtual bool HasAnyDataLayer(const TSet<FName>& InDataLayers) const override
	{
		return Algo::AnyOf(DataLayers, [&InDataLayers](const FName& DataLayer) { return InDataLayers.Contains(DataLayer); });
	}
	virtual const FBox& GetContentBounds() const override;
	virtual FBox GetCellBounds() const override;
	virtual FName GetLevelPackageName() const override;
	virtual IWorldPartitionRuntimeCellOwner* GetCellOwner() const override { return CastChecked<IWorldPartitionRuntimeCellOwner>(GetOuter()); }
	//~End IWorldPartitionCell Interface

	bool GetBlockOnSlowLoading() const { return bBlockOnSlowLoading; }
#if WITH_EDITOR
	bool NeedsActorToCellRemapping() const;
	void SetBlockOnSlowLoading(bool bInBlockOnSlowLoading) { bBlockOnSlowLoading = bInBlockOnSlowLoading; }
	void SetClientOnlyVisible(bool bInClientOnlyVisible) { bClientOnlyVisible = bInClientOnlyVisible; }
	void SetDataLayers(const TArray<const UDataLayerInstance*>& InDataLayerInstances);
	void SetContentBundleUID(const FGuid& InContentBundleID) { ContentBundleID = InContentBundleID; }
	void SetDebugInfo(int64 InCoordX, int64 InCoordY, int64 InCoordZ, FName InGridName);
	void SetLevelPackageName(const FName& InLevelPackageName) { LevelPackageName = InLevelPackageName; }
	
	//~Begin IWorldPartitionCell Interface
	virtual TSet<FName> GetActorPackageNames() const override { return TSet<FName>(); }
	//~End IWorldPartitionCell Interface

	virtual void AddActorToCell(const FWorldPartitionActorDescView& ActorDescView, const FActorContainerID& InContainerID, const FTransform& InContainerTransform, const UActorDescContainer* InContainer) PURE_VIRTUAL(UWorldPartitionRuntimeCell::AddActorToCell,);
	virtual int32 GetActorCount() const PURE_VIRTUAL(UWorldPartitionRuntimeCell::GetActorCount, return 0;);

	// Cook methods
	virtual bool PrepareCellForCook(UPackage* InPackage) { return false; }
	virtual bool PopulateGeneratorPackageForCook(TArray<UPackage*>& OutModifiedPackages) PURE_VIRTUAL(UWorldPartitionRuntimeCell::PopulateGeneratorPackageForCook, return false;);
	virtual bool PopulateGeneratedPackageForCook(UPackage* InPackage, TArray<UPackage*>& OutModifiedPackages) PURE_VIRTUAL(UWorldPartitionRuntimeCell::PopulateGeneratedPackageForCook, return false;);
	virtual FString GetPackageNameToCreate() const PURE_VIRTUAL(UWorldPartitionRuntimeCell::GetPackageNameToCreate, return FString(""););

	void SetIsHLOD(bool bInIsHLOD) { bIsHLOD = bInIsHLOD; }

	virtual void DumpStateLog(FHierarchicalLogArchive& Ar);
#endif
	
	bool GetIsHLOD() const { return bIsHLOD; }
	
	FGuid GetGuid() const { return CellGuid; }
	void SetGuid(const FGuid& InCellGuid) { CellGuid = InCellGuid; }

#if !UE_BUILD_SHIPPING
	void SetDebugStreamingPriority(float InDebugStreamingPriority) { DebugStreamingPriority = InDebugStreamingPriority; }
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UActorContainer> UnsavedActorsContainer;
#endif

protected:
	FLinearColor GetDebugStreamingPriorityColor() const;

#if WITH_EDITOR
	void UpdateDebugName();
#endif

	UPROPERTY()
	bool bIsAlwaysLoaded;

private:
	UPROPERTY()
	TArray<FName> DataLayers;

	// Debug Info
	UPROPERTY()
	FWorldPartitionRuntimeCellDebugInfo DebugInfo;

	// Custom Priority
	UPROPERTY()
	int32 Priority;

	UPROPERTY()
	bool bClientOnlyVisible;

	UPROPERTY()
	bool bIsHLOD;

	UPROPERTY()
	bool bBlockOnSlowLoading;

	UPROPERTY()
	FGuid ContentBundleID;

protected:
	UPROPERTY()
	FGuid CellGuid;

#if !UE_BUILD_SHIPPING
	// Represents the streaming priority relative to other cells
	float DebugStreamingPriority;
#endif

#if WITH_EDITOR
	FName LevelPackageName;
#endif

public:
	UPROPERTY()
	TObjectPtr<UWorldPartitionRuntimeCellData> RuntimeCellData;
};

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
#include "ProfilingDebugging/ProfilingHelpers.h"
#include "Misc/HierarchicalLogArchive.h"
#include "Algo/AnyOf.h"
#include "UObject/ObjectMacros.h"
#include "WorldPartitionRuntimeCell.generated.h"

class UActorContainer;
class UDataLayerAsset;
class UDataLayerInstance;
class UWorldPartition;
class UDataLayerManager;
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
		, bIsEditorOnly(false)
#endif
	{}

	FWorldPartitionRuntimeCellObjectMapping(FName InPackage, FName InPath, const FActorContainerID& InContainerID, const FTransform& InContainerTransform, FName InContainerPackage, FName InWorldPackage, const FGuid& InActorInstanceGuid, bool bInIsEditorOnly)
#if WITH_EDITORONLY_DATA
		: Package(InPackage)
		, Path(InPath)
		, ContainerID(InContainerID)
		, ContainerTransform(InContainerTransform)
		, ContainerPackage(InContainerPackage)
		, WorldPackage(InWorldPackage)
		, ActorInstanceGuid(InActorInstanceGuid)
		, LoadedPath(InPath)
		, bIsEditorOnly(bInIsEditorOnly)
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
	 * GUID of the actor instance
	 */
	UPROPERTY()
	FGuid ActorInstanceGuid;

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

	UPROPERTY()
	bool bIsEditorOnly;
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
UCLASS(Abstract, MinimalAPI)
class UWorldPartitionRuntimeCell : public UObject, public IWorldPartitionCell
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR
	ENGINE_API virtual void PostDuplicate(bool bDuplicateForPIE) override;
#endif

	template<class T>
	T* GetTypedOuter() const
	{
		static_assert(!std::is_same<T, UWorld>::value, "Use GetOuterWorld instead");
		static_assert(!std::is_same<T, UWorldPartition>::value, "Use GetOuterWorld()->GetWorldPartition() instead");
		return Super::GetTypedOuter<T>();
	}

	ENGINE_API virtual void Load() const PURE_VIRTUAL(UWorldPartitionRuntimeCell::Load,);
	ENGINE_API virtual void Unload() const PURE_VIRTUAL(UWorldPartitionRuntimeCell::Unload,);
	ENGINE_API virtual bool CanUnload() const PURE_VIRTUAL(UWorldPartitionRuntimeCell::CanUnload, return true;);
	ENGINE_API virtual void Activate() const PURE_VIRTUAL(UWorldPartitionRuntimeCell::Activate,);
	ENGINE_API virtual void Deactivate() const PURE_VIRTUAL(UWorldPartitionRuntimeCell::Deactivate,);
	ENGINE_API virtual bool IsAddedToWorld() const PURE_VIRTUAL(UWorldPartitionRuntimeCell::IsAddedToWorld, return false;);
	ENGINE_API virtual bool CanAddToWorld() const PURE_VIRTUAL(UWorldPartitionRuntimeCell::CanAddToWorld, return false;);
	ENGINE_API virtual ULevel* GetLevel() const PURE_VIRTUAL(UWorldPartitionRuntimeCell::GetLevel, return nullptr;);
	ENGINE_API virtual EWorldPartitionRuntimeCellState GetCurrentState() const PURE_VIRTUAL(UWorldPartitionRuntimeCell::GetCurrentState, return EWorldPartitionRuntimeCellState::Unloaded;);
	virtual FLinearColor GetDebugColor(EWorldPartitionRuntimeCellVisualizeMode VisualizeMode) const { static const FLinearColor DefaultColor = FLinearColor::Black.CopyWithNewOpacity(0.25f); return DefaultColor; }
	virtual bool IsAlwaysLoaded() const { return bIsAlwaysLoaded; }
	virtual void SetIsAlwaysLoaded(bool bInIsAlwaysLoaded) { bIsAlwaysLoaded = bInIsAlwaysLoaded; }
	virtual void SetPriority(int32 InPriority) { Priority = InPriority; }
	ENGINE_API virtual void SetStreamingPriority(int32 InStreamingPriority) const PURE_VIRTUAL(UWorldPartitionRuntimeCell::SetStreamingPriority,);
	virtual EStreamingStatus GetStreamingStatus() const { return LEVEL_Unloaded; }
	UE_DEPRECATED(5.3, "IsLoading is deprecated.")
	virtual bool IsLoading() const { return false; }
	void SetClientOnlyVisible(bool bInClientOnlyVisible) { bClientOnlyVisible = bInClientOnlyVisible; }
	bool GetClientOnlyVisible() const { return bClientOnlyVisible; }
	virtual FGuid const& GetContentBundleID() const { return ContentBundleID; }
	ENGINE_API virtual TArray<FName> GetActors() const PURE_VIRTUAL(UWorldPartitionRuntimeCell::GetActors, return TArray<FName>(););

	//~Begin IWorldPartitionCell Interface
	ENGINE_API virtual TArray<const UDataLayerInstance*> GetDataLayerInstances() const override;
	ENGINE_API virtual bool ContainsDataLayer(const UDataLayerAsset* DataLayerAsset) const override;
	ENGINE_API virtual bool ContainsDataLayer(const UDataLayerInstance* DataLayerInstance) const override;
	virtual const TArray<FName>& GetDataLayers() const override  { return DataLayers; }
	virtual bool HasAnyDataLayer(const TSet<FName>& InDataLayers) const override
	{
		return Algo::AnyOf(DataLayers, [&InDataLayers](const FName& DataLayer) { return InDataLayers.Contains(DataLayer); });
	}
	ENGINE_API virtual FName GetLevelPackageName() const override;
	ENGINE_API virtual FString GetDebugName() const override;
	ENGINE_API virtual UWorld* GetOwningWorld() const override;
	ENGINE_API virtual UWorld* GetOuterWorld() const override;
	//~End IWorldPartitionCell Interface

	inline bool HasDataLayers() const { return !DataLayers.IsEmpty(); }

	ENGINE_API UDataLayerManager* GetDataLayerManager() const;
	ENGINE_API bool HasAnyDataLayerInEffectiveRuntimeState(EDataLayerRuntimeState InState) const;

	void SetBlockOnSlowLoading(bool bInBlockOnSlowLoading) { bBlockOnSlowLoading = bInBlockOnSlowLoading; }
	bool GetBlockOnSlowLoading() const { return bBlockOnSlowLoading; }
#if WITH_EDITOR
	ENGINE_API bool NeedsActorToCellRemapping() const;
	ENGINE_API void SetDataLayers(const TArray<const UDataLayerInstance*>& InDataLayerInstances);
	void SetContentBundleUID(const FGuid& InContentBundleID) { ContentBundleID = InContentBundleID; }
	void SetLevelPackageName(const FName& InLevelPackageName) { LevelPackageName = InLevelPackageName; }
	
	//~Begin IWorldPartitionCell Interface
	virtual TSet<FName> GetActorPackageNames() const override { return TSet<FName>(); }
	//~End IWorldPartitionCell Interface

	ENGINE_API virtual void AddActorToCell(const FWorldPartitionActorDescView& ActorDescView, const FActorContainerID& InContainerID, const FTransform& InContainerTransform, const UActorDescContainer* InContainer) PURE_VIRTUAL(UWorldPartitionRuntimeCell::AddActorToCell,);
	ENGINE_API virtual void Fixup() PURE_VIRTUAL(UWorldPartitionRuntimeCell::Fixup, );
	ENGINE_API virtual int32 GetActorCount() const PURE_VIRTUAL(UWorldPartitionRuntimeCell::GetActorCount, return 0;);

	// Cook methods
	virtual bool PrepareCellForCook(UPackage* InPackage) { return false; }
	ENGINE_API virtual bool PopulateGeneratorPackageForCook(TArray<UPackage*>& OutModifiedPackages) PURE_VIRTUAL(UWorldPartitionRuntimeCell::PopulateGeneratorPackageForCook, return false;);
	ENGINE_API virtual bool PopulateGeneratedPackageForCook(UPackage* InPackage, TArray<UPackage*>& OutModifiedPackages) PURE_VIRTUAL(UWorldPartitionRuntimeCell::PopulateGeneratedPackageForCook, return false;);
	ENGINE_API virtual FString GetPackageNameToCreate() const PURE_VIRTUAL(UWorldPartitionRuntimeCell::GetPackageNameToCreate, return FString(""););

	ENGINE_API virtual void DumpStateLog(FHierarchicalLogArchive& Ar) const;
#endif

	void SetIsHLOD(bool bInIsHLOD) { bIsHLOD = bInIsHLOD; }
	bool GetIsHLOD() const { return bIsHLOD; }
	
	const FGuid& GetGuid() const { return CellGuid; }
	void SetGuid(const FGuid& InCellGuid) { CellGuid = InCellGuid; }

	const FGuid& GetSourceCellGuid() const { return SourceCellGuid; }
	void SetSourceCellGuid(const FGuid& InSourceCellGuid) { SourceCellGuid = InSourceCellGuid; }

#if !UE_BUILD_SHIPPING
	void SetDebugStreamingPriority(float InDebugStreamingPriority) { DebugStreamingPriority = InDebugStreamingPriority; }
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UActorContainer> UnsavedActorsContainer;
#endif

protected:
	ENGINE_API FLinearColor GetDebugStreamingPriorityColor() const;

	UPROPERTY()
	bool bIsAlwaysLoaded;

private:
	UPROPERTY()
	TArray<FName> DataLayers;

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

	// Used by injected HLOD cells
	UPROPERTY()
	FGuid SourceCellGuid;

#if !UE_BUILD_SHIPPING
	// Represents the streaming priority relative to other cells
	float DebugStreamingPriority;
#endif

#if WITH_EDITOR
	FName LevelPackageName;
#endif

public:
	//~Begin UWorldPartitionRuntimeCellData Proxy
	inline bool ShouldResetStreamingSourceInfo() const { return RuntimeCellData->ShouldResetStreamingSourceInfo(); }
	inline void ResetStreamingSourceInfo() const { RuntimeCellData->ResetStreamingSourceInfo(); }
	inline void AppendStreamingSourceInfo(const FWorldPartitionStreamingSource& Source, const FSphericalSector& SourceShape) const { RuntimeCellData->AppendStreamingSourceInfo(Source, SourceShape); }
	inline void MergeStreamingSourceInfo() const { RuntimeCellData->MergeStreamingSourceInfo(); }
	ENGINE_API int32 SortCompare(const UWorldPartitionRuntimeCell* Other, bool bCanUseSortingCache = true) const;
	inline const FBox& GetContentBounds() const { return RuntimeCellData->GetContentBounds(); }
	inline FBox GetCellBounds() const { return RuntimeCellData->GetCellBounds(); }
	ENGINE_API virtual bool IsDebugShown() const;
	//~End UWorldPartitionRuntimeCellData Proxy

	UPROPERTY()
	TObjectPtr<UWorldPartitionRuntimeCellData> RuntimeCellData;
};

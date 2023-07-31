// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Optional.h"
#include "GameFramework/Actor.h"
#include "Templates/SubclassOf.h"
#include "UObject/LinkerInstancingContext.h"
#include "WorldPartition/WorldPartitionLog.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "WorldPartition/WorldPartitionStreamingSource.h"
#include "WorldPartition/WorldPartitionHandle.h"
#include "WorldPartition/ActorDescContainerCollection.h"
#include "WorldPartition/Cook/WorldPartitionCookPackageGenerator.h"

#if WITH_EDITOR
#include "WorldPartition/WorldPartitionActorLoaderInterface.h"
#include "WorldPartition/WorldPartitionEditorLoaderAdapter.h"
#include "PackageSourceControlHelper.h"
#include "CookPackageSplitter.h"
#endif

#include "WorldPartition.generated.h"

class FWorldPartitionActorDesc;
class UActorDescContainer;
class UWorldPartitionEditorHash;
class UWorldPartitionRuntimeCell;
class UWorldPartitionRuntimeHash;
class UWorldPartitionStreamingPolicy;
class IStreamingGenerationErrorHandler;
class FLoaderAdapterAlwaysLoadedActors;
class FLoaderAdapterActorList;
class FHLODActorDesc;
class UHLODLayer;
class UCanvas;
class ULevel;
class FAutoConsoleVariableRef;

struct IWorldPartitionStreamingSourceProvider;

enum class EWorldPartitionRuntimeCellState : uint8;
enum class EWorldPartitionStreamingPerformance : uint8;

enum class EWorldPartitionInitState
{
	Uninitialized,
	Initializing,
	Initialized,
	Uninitializing
};

#if WITH_EDITOR
/**
 * Interface for the world partition editor
 */
struct ENGINE_API IWorldPartitionEditor
{
	virtual void Refresh() {}
	virtual void Reconstruct() {}
};

class ENGINE_API ISourceControlHelper
{
public:
	virtual FString GetFilename(const FString& PackageName) const =0;
	virtual FString GetFilename(UPackage* Package) const =0;
	virtual bool Checkout(UPackage* Package) const =0;
	virtual bool Add(UPackage* Package) const =0;
	virtual bool Delete(const FString& PackageName) const =0;
	virtual bool Delete(UPackage* Package) const =0;
	virtual bool Save(UPackage* Package) const =0;
};
#endif

UCLASS(AutoExpandCategories=(WorldPartition))
class ENGINE_API UWorldPartition final : public UObject, public FActorDescContainerCollection, public IWorldPartitionCookPackageGenerator
{
	GENERATED_UCLASS_BODY()

	friend class FWorldPartitionActorDesc;
	friend class FWorldPartitionConverter;
	friend class UWorldPartitionConvertCommandlet;
	friend class FWorldPartitionEditorModule;
	friend class FWorldPartitionDetails;
	friend class FUnrealEdMisc;
	friend class UActorDescContainer;

public:
#if WITH_EDITOR
	static UWorldPartition* CreateOrRepairWorldPartition(AWorldSettings* WorldSettings, TSubclassOf<UWorldPartitionEditorHash> EditorHashClass = nullptr, TSubclassOf<UWorldPartitionRuntimeHash> RuntimeHashClass = nullptr);
#endif

	DECLARE_MULTICAST_DELEGATE_OneParam(FWorldPartitionInitializeDelegate, UWorldPartition*);
	
	UE_DEPRECATED(5.1, "Please use FWorldPartitionInitializedEvent& UWorld::OnWorldPartitionInitialized() instead.")
	FWorldPartitionInitializeDelegate OnWorldPartitionInitialized;

	UE_DEPRECATED(5.1, "Please use FWorldPartitionInitializedEvent& UWorld::OnWorldPartitionUninitialized() instead.")
	FWorldPartitionInitializeDelegate OnWorldPartitionUninitialized;

#if WITH_EDITOR
	TArray<FBox> GetUserLoadedEditorRegions() const;

public:
	void SetEnableStreaming(bool bInEnableStreaming);
	bool CanBeUsedByLevelInstance() const;
	void SetCanBeUsedByLevelInstance(bool bInCanBeUsedByLevelInstance);
	void OnEnableStreamingChanged();

private:
	void SavePerUserSettings();
		
	void OnGCPostReachabilityAnalysis();
	void OnPackageDirtyStateChanged(UPackage* Package);

	// PIE/Game
	void OnPreBeginPIE(bool bStartSimulate);
	void OnPrePIEEnded(bool bWasSimulatingInEditor);
	void OnCancelPIE();
	void OnBeginPlay();
	void OnEndPlay();

	// WorldDeletegates Events
	void OnWorldRenamed(UWorld* RenamedWorld);

	// ActorDescContainer Events
	void OnActorDescAdded(FWorldPartitionActorDesc* NewActorDesc);
	void OnActorDescRemoved(FWorldPartitionActorDesc* ActorDesc);
	void OnActorDescUpdating(FWorldPartitionActorDesc* ActorDesc);
	void OnActorDescUpdated(FWorldPartitionActorDesc* ActorDesc);

	bool GetInstancingContext(const FLinkerInstancingContext*& OutInstancingContext) const;
#endif

public:
	const FTransform& GetInstanceTransform() const;
	//~ End UActorDescContainer Interface

	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	virtual UWorld* GetWorld() const override;
	virtual bool ResolveSubobject(const TCHAR* SubObjectPath, UObject*& OutObject, bool bLoadIfExists) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	//~ End UObject Interface

#if WITH_EDITOR
	FName GetWorldPartitionEditorName() const;

	// Streaming generation
	bool GenerateStreaming(TArray<FString>* OutPackagesToGenerate = nullptr);
	bool GenerateContainerStreaming(const UActorDescContainer* ActorDescContainer, TArray<FString>* OutPackagesToGenerate = nullptr);
	void FlushStreaming();

	DECLARE_MULTICAST_DELEGATE_OneParam(FWorldPartitionGenerateStreamingDelegate, TArray<FString>*);
	FWorldPartitionGenerateStreamingDelegate OnPreGenerateStreaming;

	void RemapSoftObjectPath(FSoftObjectPath& ObjectPath);
	bool IsValidPackageName(const FString& InPackageName);

	// Begin Cooking
	void BeginCook(IWorldPartitionCookPackageContext& CookContext);
	DECLARE_MULTICAST_DELEGATE_OneParam(FWorldPartitionBeginCookDelegate, IWorldPartitionCookPackageContext&);
	FWorldPartitionBeginCookDelegate OnBeginCook;

	//~ Begin IWorldPartitionCookPackageGenerator Interface 
	virtual bool GatherPackagesToCook(IWorldPartitionCookPackageContext& CookContext) override;
	virtual bool PrepareGeneratorPackageForCook(IWorldPartitionCookPackageContext& CookContext, TArray<UPackage*>& OutModifiedPackages) override;
	virtual bool PopulateGeneratorPackageForCook(IWorldPartitionCookPackageContext& CookContext, const TArray<FWorldPartitionCookPackage*>& InPackagesToCook, TArray<UPackage*>& OutModifiedPackages) override;
	virtual bool PopulateGeneratedPackageForCook(IWorldPartitionCookPackageContext& CookContext, const FWorldPartitionCookPackage& InPackagesToCool, TArray<UPackage*>& OutModifiedPackages) override;
	//~ End IWorldPartitionCookPackageGenerator Interface 

	// End Cooking

	UE_DEPRECATED(5.1, "GetWorldBounds is deprecated, use GetEditorWorldBounds or GetRuntimeWorldBounds instead.")
	FBox GetWorldBounds() const { return GetRuntimeWorldBounds(); }

	FBox GetEditorWorldBounds() const;
	FBox GetRuntimeWorldBounds() const;
	
	UHLODLayer* GetDefaultHLODLayer() const { return DefaultHLODLayer; }
	void SetDefaultHLODLayer(UHLODLayer* InDefaultHLODLayer) { DefaultHLODLayer = InDefaultHLODLayer; }
	void GenerateHLOD(ISourceControlHelper* SourceControlHelper, bool bCreateActorsOnly);

	// Debugging
	void DrawRuntimeHashPreview();
	void DumpActorDescs(const FString& Path);

	void CheckForErrors(IStreamingGenerationErrorHandler* ErrorHandler) const;
	static void CheckForErrors(IStreamingGenerationErrorHandler* ErrorHandler, const UActorDescContainer* ActorDescContainer, bool bEnableStreaming, bool bIsChangelistValidation);

	void AppendAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const;

	DECLARE_MULTICAST_DELEGATE_OneParam(FActorDescContainerRegistrationDelegate, UActorDescContainer*);
	FActorDescContainerRegistrationDelegate OnActorDescContainerRegistered;
	FActorDescContainerRegistrationDelegate OnActorDescContainerUnregistered;
	UActorDescContainer* RegisterActorDescContainer(const FName& ContainerPackage);
	bool UnregisterActorDescContainer(UActorDescContainer* Container);
	void UninitializeActorDescContainers();

	// Actors pinning
	void PinActors(const TArray<FGuid>& ActorGuids);
	void UnpinActors(const TArray<FGuid>& ActorGuids);
	bool IsActorPinned(const FGuid& ActorGuid) const;

	void LoadLastLoadedRegions(const TArray<FBox>& EditorLastLoadedRegions);
	void LoadLastLoadedRegions();

	bool HasLoadedUserCreatedRegions() const { return !!NumUserCreatedLoadedRegions; }
	void OnUserCreatedRegionLoaded() { NumUserCreatedLoadedRegions++; }
	void OnUserCreatedRegionUnloaded() { check(HasLoadedUserCreatedRegions()); NumUserCreatedLoadedRegions--; }

	bool IsEnablingStreamingJustified() const { return bEnablingStreamingJustified; }
#endif

public:
	static bool IsSimulating(bool bIncludeTestEnableSimulationStreamingSource = true);

	void Initialize(UWorld* World, const FTransform& InTransform);
	bool IsInitialized() const;
	void Uninitialize();

	bool SupportsStreaming() const;
	bool IsStreamingEnabled() const;
	bool CanStream() const;
	bool IsServer() const;
	bool IsServerStreamingEnabled() const;
	bool IsServerStreamingOutEnabled() const;
	bool UseMakingVisibleTransactionRequests() const;
	bool UseMakingInvisibleTransactionRequests() const;

	bool IsMainWorldPartition() const;

	void Tick(float DeltaSeconds);
	void UpdateStreamingState();
	bool CanAddLoadedLevelToWorld(ULevel* InLevel) const;
	bool IsStreamingCompleted(const FWorldPartitionStreamingSource* InStreamingSource) const;
	bool IsStreamingCompleted(EWorldPartitionRuntimeCellState QueryState, const TArray<FWorldPartitionStreamingQuerySource>& QuerySources, bool bExactState) const;

	const TArray<FWorldPartitionStreamingSource>& GetStreamingSources() const;

	// Debugging
	bool CanDebugDraw() const;
	bool DrawRuntimeHash2D(UCanvas* Canvas, const FVector2D& PartitionCanvasSize, const FVector2D& Offset, FVector2D& OutUsedCanvasSize);
	void DrawRuntimeHash3D();
	void DrawRuntimeCellsDetails(UCanvas* Canvas, FVector2D& Offset);
	void DrawStreamingStatusLegend(UCanvas* Canvas, FVector2D& Offset);

	EWorldPartitionStreamingPerformance GetStreamingPerformance() const;

#if WITH_EDITORONLY_DATA
	UPROPERTY(DuplicateTransient)
	TObjectPtr<UWorldPartitionEditorHash> EditorHash;

	FLoaderAdapterAlwaysLoadedActors* AlwaysLoadedActors;
	FLoaderAdapterActorList* PinnedActors;

	IWorldPartitionEditor* WorldPartitionEditor;

private:
	/** Class of WorldPartitionStreamingPolicy to be used to manage world partition streaming. */
	UPROPERTY()
	TSubclassOf<UWorldPartitionStreamingPolicy> WorldPartitionStreamingPolicyClass;

	/** Used to know if it's the first time streaming is enabled on this world. */
	UPROPERTY()
	bool bStreamingWasEnabled;

	/** Used to know if we need to recheck if the user should enable streaming based on world size. */
	bool bShouldCheckEnableStreamingWarning;

	/** Whether Level Instance can reference this partition. */
	UPROPERTY(EditAnywhere, Category = "WorldPartitionSetup", AdvancedDisplay, meta = (EditConditionHides, HideEditConditionToggle, EditCondition = "!bEnableStreaming"))
	bool bCanBeUsedByLevelInstance;
#endif

public:
	UActorDescContainer* GetActorDescContainer() const { return ActorDescContainer; }

	UPROPERTY()
	TObjectPtr<UActorDescContainer> ActorDescContainer;

	UPROPERTY()
	TObjectPtr<UWorldPartitionRuntimeHash> RuntimeHash;

	UPROPERTY(Transient)
	TObjectPtr<UWorld> World;

	/** Enables streaming for this world. */
	UPROPERTY()
	bool bEnableStreaming;

private:
#if WITH_EDITOR
	bool bForceGarbageCollection;
	bool bForceGarbageCollectionPurge;
	bool bEnablingStreamingJustified;
	bool bIsPIE;
	int32 NumUserCreatedLoadedRegions;
#endif

#if WITH_EDITORONLY_DATA
	// Default HLOD layer
	UPROPERTY(EditAnywhere, Category=WorldPartitionSetup, meta = (DisplayName = "Default HLOD Layer", EditCondition="bEnableStreaming", EditConditionHides, HideEditConditionToggle))
	TObjectPtr<class UHLODLayer> DefaultHLODLayer;

	TArray<FWorldPartitionReference> LoadedSubobjects;

	TMap<FWorldPartitionReference, AActor*> DirtyActors;

	TSet<FString> GeneratedStreamingPackageNames;
#endif

	EWorldPartitionInitState InitState;
	TOptional<FTransform> InstanceTransform;

	mutable TOptional<bool> bCachedUseMakingInvisibleTransactionRequests;
	mutable TOptional<bool> bCachedUseMakingVisibleTransactionRequests;
	mutable TOptional<bool> bCachedIsServerStreamingEnabled;
	mutable TOptional<bool> bCachedIsServerStreamingOutEnabled;

	UPROPERTY(Transient)
	mutable TObjectPtr<UWorldPartitionStreamingPolicy> StreamingPolicy;

#if WITH_EDITORONLY_DATA
	FLinkerInstancingContext InstancingContext;

	FWorldPartitionReference WorldDataLayersActor;
#endif

#if WITH_EDITOR
	static int32 LoadingRangeBugItGo;
	static int32 EnableSimulationStreamingSource;
	static int32 WorldExtentToEnableStreaming;
	static bool DebugDedicatedServerStreaming;
	static FAutoConsoleVariableRef CVarLoadingRangeBugItGo;
	static FAutoConsoleVariableRef CVarEnableSimulationStreamingSource;
	static FAutoConsoleVariableRef CVarWorldExtentToEnableStreaming;
	static FAutoConsoleVariableRef CVarDebugDedicatedServerStreaming;
#endif

	static int32 EnableServerStreaming;
	static bool bEnableServerStreamingOut;
	static bool bUseMakingVisibleTransactionRequests;
	static bool bUseMakingInvisibleTransactionRequests;
	static FAutoConsoleVariableRef CVarEnableServerStreaming;
	static FAutoConsoleVariableRef CVarEnableServerStreamingOut;
	static FAutoConsoleVariableRef CVarUseMakingVisibleTransactionRequests;
	static FAutoConsoleVariableRef CVarUseMakingInvisibleTransactionRequests;

	void OnWorldMatchStarting();
	void OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld);
	void OnPostBugItGoCalled(const FVector& Loc, const FRotator& Rot);

	// Delegates registration
	void RegisterDelegates();
	void UnregisterDelegates();	

#if WITH_EDITOR
	void HashActorDesc(FWorldPartitionActorDesc* ActorDesc);
	void UnhashActorDesc(FWorldPartitionActorDesc* ActorDesc);
	void HashActorDescContainer(UActorDescContainer* ActorDescContainer);
	void UnhashActorDescContainer(UActorDescContainer* ActorDescContainer);

public:
	// Editor loader adapters management
	template <typename T, typename... ArgsType>
	UWorldPartitionEditorLoaderAdapter* CreateEditorLoaderAdapter(ArgsType&&... Args)
	{
		UWorldPartitionEditorLoaderAdapter* EditorLoaderAdapter = NewObject<UWorldPartitionEditorLoaderAdapter>(GetTransientPackage());
		EditorLoaderAdapter->SetLoaderAdapter(new T(Forward<ArgsType>(Args)...));
		RegisteredEditorLoaderAdapters.Add(EditorLoaderAdapter);
		return EditorLoaderAdapter;
	}

	void ReleaseEditorLoaderAdapter(UWorldPartitionEditorLoaderAdapter* EditorLoaderAdapter)
	{
		verify(RegisteredEditorLoaderAdapters.Remove(EditorLoaderAdapter) != INDEX_NONE);
		EditorLoaderAdapter->Release();
	}

	const TSet<TObjectPtr<UWorldPartitionEditorLoaderAdapter>>& GetRegisteredEditorLoaderAdapters() const
	{
		return RegisteredEditorLoaderAdapters;
	}

private:
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY(transient, NonTransactional)
	TSet<TObjectPtr<UWorldPartitionEditorLoaderAdapter>> RegisteredEditorLoaderAdapters;
#endif

#if !UE_BUILD_SHIPPING
	void GetOnScreenMessages(FCoreDelegates::FSeverityMessageMap& OutMessages);
#endif
	class AWorldPartitionReplay* Replay;

	friend class AWorldPartitionReplay;
	friend class UWorldPartitionStreamingPolicy;
};
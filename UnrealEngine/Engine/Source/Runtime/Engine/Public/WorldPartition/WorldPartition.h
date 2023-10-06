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
#include "WorldPartition/WorldPartitionStreamingGeneration.h"
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
class URuntimeHashExternalStreamingObjectBase;
class UWorldPartitionStreamingPolicy;
class IWorldPartitionCell;
class UDataLayerManager;
class IStreamingGenerationErrorHandler;
class FLoaderAdapterAlwaysLoadedActors;
class FLoaderAdapterActorList;
class FHLODActorDesc;
class UHLODLayer;
class UCanvas;
class ULevel;
class FAutoConsoleVariableRef;
class FWorldPartitionDraw2DContext;
class FContentBundleEditor;

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

UENUM()
enum class EWorldPartitionServerStreamingMode : uint8
{
	ProjectDefault = 0 UMETA(ToolTip = "Use project default (wp.Runtime.EnableServerStreaming)"),
	Disabled = 1 UMETA(ToolTip = "Server streaming is disabled"),
	Enabled = 2 UMETA(ToolTip = "Server streaming is enabled"),
	EnabledInPIE = 3 UMETA(ToolTip = "Server streaming is only enabled in PIE"),
};

UENUM()
enum class EWorldPartitionServerStreamingOutMode : uint8
{
	ProjectDefault = 0 UMETA(ToolTip = "Use project default (wp.Runtime.EnableServerStreamingOut)"),
	Disabled = 1 UMETA(ToolTip = "Server streaming out is disabled"),
	Enabled = 2 UMETA(ToolTip = "Server streaming out is enabled"),
};

#if WITH_EDITOR
/**
 * Interface for the world partition editor
 */
struct IWorldPartitionEditor
{
	virtual void Refresh() {}
	virtual void Reconstruct() {}
	virtual void FocusBox(const FBox& Box) const {}
};

class ISourceControlHelper
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

struct ENGINE_API FDirtyActor
{
	FDirtyActor()
		: ActorPtr(nullptr)
	{}

	FDirtyActor(AActor* InActor)
		: ActorPtr(InActor)
	{
	}

	FDirtyActor(const FWorldPartitionReference& InWorldPartitionRef, AActor* InActor)
		: WorldPartitionRef(InWorldPartitionRef)
		, ActorPtr(InActor)
	{
	}

	TOptional<FWorldPartitionReference> WorldPartitionRef;
	TWeakObjectPtr<AActor>	ActorPtr;		// TWeakObjectPtr is for undo support.

	bool operator == (const FDirtyActor& InDirtyActor) const
	{
		return WorldPartitionRef == InDirtyActor.WorldPartitionRef && ActorPtr == InDirtyActor.ActorPtr;
	}

	friend uint32 GetTypeHash(const FDirtyActor& InDirtyActor)
	{
		uint32 Hash = GetTypeHash(InDirtyActor.ActorPtr);
		if (InDirtyActor.WorldPartitionRef.IsSet())
		{
			Hash = HashCombine(Hash, GetTypeHash(InDirtyActor.WorldPartitionRef.GetValue()));
		}
		return Hash;
	}
};
#endif

UCLASS(AutoExpandCategories=(WorldPartition), MinimalAPI)
class UWorldPartition final : public UObject, public FActorDescContainerCollection, public IWorldPartitionCookPackageGenerator
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
	static ENGINE_API UWorldPartition* CreateOrRepairWorldPartition(AWorldSettings* WorldSettings, TSubclassOf<UWorldPartitionEditorHash> EditorHashClass = nullptr, TSubclassOf<UWorldPartitionRuntimeHash> RuntimeHashClass = nullptr);
	static ENGINE_API bool RemoveWorldPartition(AWorldSettings* WorldSettings);
#endif

#if WITH_EDITOR
	ENGINE_API TArray<FBox> GetUserLoadedEditorRegions() const;

public:
	ENGINE_API void SetEnableStreaming(bool bInEnableStreaming);

	UE_DEPRECATED(5.3, "CanBeUsedByLevelInstance is deprecated.")
	bool CanBeUsedByLevelInstance() const { return true; }
	UE_DEPRECATED(5.3, "SetCanBeUsedByLevelInstance is deprecated.")
	void SetCanBeUsedByLevelInstance(bool bInCanBeUsedByLevelInstance) {}

	ENGINE_API void OnEnableStreamingChanged();

private:
	ENGINE_API void SavePerUserSettings();
		
	ENGINE_API void OnGCPostReachabilityAnalysis();
	ENGINE_API void OnPackageDirtyStateChanged(UPackage* Package);

	// PIE/Game
	ENGINE_API void OnPreBeginPIE(bool bStartSimulate);
	ENGINE_API void OnPrePIEEnded(bool bWasSimulatingInEditor);
	ENGINE_API void OnCancelPIE();
	ENGINE_API void OnBeginPlay();
	ENGINE_API void OnEndPlay();

	// WorldDeletegates Events
	ENGINE_API void OnWorldRenamed(UWorld* RenamedWorld);

	// ActorDescContainer Events
	ENGINE_API void OnActorDescAdded(FWorldPartitionActorDesc* NewActorDesc);
	ENGINE_API void OnActorDescRemoved(FWorldPartitionActorDesc* ActorDesc);
	ENGINE_API void OnActorDescUpdating(FWorldPartitionActorDesc* ActorDesc);
	ENGINE_API void OnActorDescUpdated(FWorldPartitionActorDesc* ActorDesc);

	ENGINE_API bool GetInstancingContext(const FLinkerInstancingContext*& OutInstancingContext) const;
#endif

public:
	ENGINE_API const FTransform& GetInstanceTransform() const;
	//~ End UActorDescContainer Interface

	//~ Begin UObject Interface
#if WITH_EDITOR
	ENGINE_API virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif //WITH_EDITOR
	ENGINE_API virtual void Serialize(FArchive& Ar) override;
	ENGINE_API virtual UWorld* GetWorld() const override;
	ENGINE_API virtual bool ResolveSubobject(const TCHAR* SubObjectPath, UObject*& OutObject, bool bLoadIfExists) override;
	ENGINE_API virtual void BeginDestroy() override;
	static ENGINE_API void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	//~ End UObject Interface

	// Editor/Runtime conversions
	ENGINE_API bool ConvertEditorPathToRuntimePath(const FSoftObjectPath& InPath, FSoftObjectPath& OutPath) const;

#if WITH_EDITOR
	void SetInstanceTransform(const FTransform& InInstanceTransform) { InstanceTransform = InInstanceTransform; }
	ENGINE_API FName GetWorldPartitionEditorName() const;

	// Streaming generation
	bool CanGenerateStreaming() const { return !StreamingPolicy; }

	UE_DEPRECATED(5.3, "GenerateStreaming is deprecated, use GenerateStreaming with a param struct instead")
	ENGINE_API bool GenerateStreaming(TArray<FString>* OutPackagesToGenerate = nullptr);

	UE_DEPRECATED(5.3, "GenerateContainerStreaming is deprecated, use GenerateContainerStreaming with a param struct instead")
	ENGINE_API bool GenerateContainerStreaming(const UActorDescContainer* ActorDescContainer, TArray<FString>* OutPackagesToGenerate = nullptr);

	struct FGenerateStreamingParams
	{
		FGenerateStreamingParams()
		{}

		FStreamingGenerationActorDescCollection ActorDescCollection;
		TOptional<const FString> OutputLogPath;

		FGenerateStreamingParams& SetActorDescContainer(const UActorDescContainer* InActorDescContainer) 
		{ 
			ActorDescCollection.AddContainer(InActorDescContainer);
			return *this;
		}
		FGenerateStreamingParams& SetOutputLogPath(const FString& InOutputLogPath) { OutputLogPath = InOutputLogPath; return *this; }
	};

	struct FGenerateStreamingContext
	{
		FGenerateStreamingContext()
		{}

		 TArray<FString>* PackagesToGenerate = nullptr;
		 TOptional<FString> OutputLogFilename;

		FGenerateStreamingContext& SetPackagesToGenerate(TArray<FString>* InPackagesToGenerate) { PackagesToGenerate = InPackagesToGenerate; return *this; }
	};

	ENGINE_API bool GenerateStreaming(const FGenerateStreamingParams& InParams, FGenerateStreamingContext& InContext);
	ENGINE_API bool GenerateContainerStreaming(const FGenerateStreamingParams& InParams, FGenerateStreamingContext& InContext);

	ENGINE_API void FlushStreaming();
	ENGINE_API URuntimeHashExternalStreamingObjectBase* FlushStreamingToExternalStreamingObject(const FString& ExternalStreamingObjectName);

	// Event when world partition was enabled/disabled in the world
	DECLARE_MULTICAST_DELEGATE_OneParam(FWorldPartitionChangedEvent, UWorld*);
	static ENGINE_API FWorldPartitionChangedEvent WorldPartitionChangedEvent;

	DECLARE_MULTICAST_DELEGATE_OneParam(FWorldPartitionGenerateStreamingDelegate, TArray<FString>*);
	FWorldPartitionGenerateStreamingDelegate OnPreGenerateStreaming;

	ENGINE_API void RemapSoftObjectPath(FSoftObjectPath& ObjectPath) const;
	ENGINE_API bool IsValidPackageName(const FString& InPackageName);

	// Begin Cooking
	ENGINE_API void BeginCook(IWorldPartitionCookPackageContext& CookContext);
	DECLARE_MULTICAST_DELEGATE_OneParam(FWorldPartitionBeginCookDelegate, IWorldPartitionCookPackageContext&);
	FWorldPartitionBeginCookDelegate OnBeginCook;

	//~ Begin IWorldPartitionCookPackageGenerator Interface 
	ENGINE_API virtual bool GatherPackagesToCook(IWorldPartitionCookPackageContext& CookContext) override;
	ENGINE_API virtual bool PrepareGeneratorPackageForCook(IWorldPartitionCookPackageContext& CookContext, TArray<UPackage*>& OutModifiedPackages) override;
	ENGINE_API virtual bool PopulateGeneratorPackageForCook(IWorldPartitionCookPackageContext& CookContext, const TArray<FWorldPartitionCookPackage*>& InPackagesToCook, TArray<UPackage*>& OutModifiedPackages) override;
	ENGINE_API virtual bool PopulateGeneratedPackageForCook(IWorldPartitionCookPackageContext& CookContext, const FWorldPartitionCookPackage& InPackagesToCool, TArray<UPackage*>& OutModifiedPackages) override;
	ENGINE_API virtual UWorldPartitionRuntimeCell* GetCellForPackage(const FWorldPartitionCookPackage& PackageToCook) const override;
	//~ End IWorldPartitionCookPackageGenerator Interface 
	// End Cooking

	UE_DEPRECATED(5.1, "GetWorldBounds is deprecated, use GetEditorWorldBounds or GetRuntimeWorldBounds instead.")
	FBox GetWorldBounds() const { return GetRuntimeWorldBounds(); }

	ENGINE_API FBox GetEditorWorldBounds() const;
	ENGINE_API FBox GetRuntimeWorldBounds() const;
	
	UHLODLayer* GetDefaultHLODLayer() const { return DefaultHLODLayer; }
	void SetDefaultHLODLayer(UHLODLayer* InDefaultHLODLayer) { DefaultHLODLayer = InDefaultHLODLayer; }
	ENGINE_API void GenerateHLOD(ISourceControlHelper* SourceControlHelper, bool bCreateActorsOnly);

	// Debugging
	ENGINE_API void DrawRuntimeHashPreview();
	ENGINE_API void DumpActorDescs(const FString& Path);

	ENGINE_API void CheckForErrors(IStreamingGenerationErrorHandler* ErrorHandler) const;

	/* Struct of optional parameters passed to check for errors function. */
	struct FCheckForErrorsParams
	{
		ENGINE_API FCheckForErrorsParams();

		IStreamingGenerationErrorHandler* ErrorHandler;
		const UActorDescContainer* ActorDescContainer;
		bool bEnableStreaming;
		TMap<FGuid, const UActorDescContainer*> ActorGuidsToContainerMap;
	};

	UE_DEPRECATED(5.2, "CheckForErrors is deprecated, CheckForErrors with FCheckForErrorsParams should be used instead.")
	static ENGINE_API void CheckForErrors(IStreamingGenerationErrorHandler* ErrorHandler, const UActorDescContainer* ActorDescContainer, bool bEnableStreaming, bool bIsChangelistValidation);

	static ENGINE_API void CheckForErrors(const FCheckForErrorsParams& Params);

	using FStreamingGenerationErrorHandlerOverride = TFunction<IStreamingGenerationErrorHandler*(IStreamingGenerationErrorHandler* InErrorHandler)>;
	ENGINE_API inline static TOptional<FStreamingGenerationErrorHandlerOverride> StreamingGenerationErrorHandlerOverride;

	ENGINE_API void AppendAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const;

	struct FContainerRegistrationParams
	{
		FContainerRegistrationParams(FName InPackageName)
			: PackageName(InPackageName)
		{}

		/* The long package name of the container package on disk. */
		FName PackageName;

		/* Custom filter function used to filter actors descriptors. */
		TUniqueFunction<bool(const FWorldPartitionActorDesc*)> FilterActorDescFunc;
	};
	ENGINE_API UActorDescContainer* RegisterActorDescContainer(const FContainerRegistrationParams& InRegistrationParameters);
	ENGINE_API bool UnregisterActorDescContainer(UActorDescContainer* Container);
	ENGINE_API void UninitializeActorDescContainers();

	DECLARE_MULTICAST_DELEGATE_OneParam(FActorDescContainerRegistrationDelegate, UActorDescContainer*);
	FActorDescContainerRegistrationDelegate OnActorDescContainerRegistered;
	FActorDescContainerRegistrationDelegate OnActorDescContainerUnregistered;

	UE_DEPRECATED(5.3, "Use RegisterActorDescContainer with FContainerRegistrationParams instead.")
	UActorDescContainer* RegisterActorDescContainer(const FName& ContainerPackage) { return RegisterActorDescContainer(FContainerRegistrationParams(ContainerPackage)); }

	// Actors pinning
	ENGINE_API void PinActors(const TArray<FGuid>& ActorGuids);
	ENGINE_API void UnpinActors(const TArray<FGuid>& ActorGuids);
	ENGINE_API bool IsActorPinned(const FGuid& ActorGuid) const;

	ENGINE_API void LoadLastLoadedRegions(const TArray<FBox>& EditorLastLoadedRegions);
	ENGINE_API void LoadLastLoadedRegions();

	bool HasLoadedUserCreatedRegions() const { return !!NumUserCreatedLoadedRegions; }
	void OnUserCreatedRegionLoaded() { NumUserCreatedLoadedRegions++; }
	void OnUserCreatedRegionUnloaded() { check(HasLoadedUserCreatedRegions()); NumUserCreatedLoadedRegions--; }

	bool IsEnablingStreamingJustified() const { return bEnablingStreamingJustified; }

	const TSet<FDirtyActor>& GetDirtyActors() const { return DirtyActors; }
#endif

public:
	static ENGINE_API bool IsSimulating(bool bIncludeTestEnableSimulationStreamingSource = true);
	int32 GetStreamingStateEpoch() const { return StreamingStateEpoch; }

	ENGINE_API bool CanInitialize(UWorld* InWorld) const;
	ENGINE_API void Initialize(UWorld* World, const FTransform& InTransform);
	ENGINE_API bool IsInitialized() const;
	ENGINE_API void Update();
	ENGINE_API void Uninitialize();

	ENGINE_API bool SupportsStreaming() const;
	ENGINE_API bool IsStreamingEnabled() const;
	ENGINE_API bool CanStream() const;
	ENGINE_API bool IsServer() const;
	ENGINE_API bool IsServerStreamingEnabled() const;
	ENGINE_API bool IsServerStreamingOutEnabled() const;
	ENGINE_API bool UseMakingVisibleTransactionRequests() const;
	ENGINE_API bool UseMakingInvisibleTransactionRequests() const;

	ENGINE_API bool IsMainWorldPartition() const;

	ENGINE_API void Tick(float DeltaSeconds);
	ENGINE_API bool CanAddCellToWorld(const IWorldPartitionCell* InCell) const;
	ENGINE_API bool IsStreamingCompleted(const TArray<FWorldPartitionStreamingSource>* InStreamingSources) const;
	ENGINE_API bool IsStreamingCompleted(EWorldPartitionRuntimeCellState QueryState, const TArray<FWorldPartitionStreamingQuerySource>& QuerySources, bool bExactState) const;
	ENGINE_API bool GetIntersectingCells(const TArray<FWorldPartitionStreamingQuerySource>& InSources, TArray<const IWorldPartitionCell*>& OutCells) const;

	ENGINE_API bool InjectExternalStreamingObject(URuntimeHashExternalStreamingObjectBase* ExternalStreamingObject);
	ENGINE_API bool RemoveExternalStreamingObject(URuntimeHashExternalStreamingObjectBase* ExternalStreamingObject);

	ENGINE_API const TArray<FWorldPartitionStreamingSource>& GetStreamingSources() const;

	// Debugging
	ENGINE_API bool DrawRuntimeHash2D(FWorldPartitionDraw2DContext& DrawContext);
	ENGINE_API void DrawRuntimeHash3D();
	ENGINE_API void DrawRuntimeCellsDetails(UCanvas* Canvas, FVector2D& Offset);

	ENGINE_API void OnCellShown(const UWorldPartitionRuntimeCell* InCell);
	ENGINE_API void OnCellHidden(const UWorldPartitionRuntimeCell* InCell);

	ENGINE_API EWorldPartitionStreamingPerformance GetStreamingPerformance() const;

	ENGINE_API bool IsStreamingInEnabled() const;
	ENGINE_API void DisableStreamingIn();
	ENGINE_API void EnableStreamingIn();

	ENGINE_API UDataLayerManager* GetDataLayerManager() const;

	UE_DEPRECATED(5.3, "UpdateStreamingState is deprecated, use UWorldPartitionSubsystem::UpdateStreamingState instead.")
	void UpdateStreamingState() {}
	UE_DEPRECATED(5.3, "CanAddLoadedLevelToWorld is deprecated, use CanAddCellToWorld instead.")
	bool CanAddLoadedLevelToWorld(ULevel* InLevel) const { return true; }

#if WITH_EDITORONLY_DATA
	UPROPERTY(DuplicateTransient)
	TObjectPtr<UWorldPartitionEditorHash> EditorHash;

	FLoaderAdapterAlwaysLoadedActors* AlwaysLoadedActors;
	FLoaderAdapterActorList* ForceLoadedActors;
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
#endif

public:
	UActorDescContainer* GetActorDescContainer() const { return ActorDescContainer; }

	UPROPERTY(Transient)
	TObjectPtr<UActorDescContainer> ActorDescContainer;

	UPROPERTY()
	TObjectPtr<UWorldPartitionRuntimeHash> RuntimeHash;

	/** Enables streaming for this world. */
	UPROPERTY()
	bool bEnableStreaming;

	UPROPERTY(EditAnywhere, Category = WorldPartitionSetup, AdvancedDisplay, meta = (EditConditionHides, EditCondition = "bEnableStreaming", HideEditConditionToggle))
	EWorldPartitionServerStreamingMode ServerStreamingMode;

	UPROPERTY(EditAnywhere, Category = WorldPartitionSetup, AdvancedDisplay, meta = (EditConditionHides, EditCondition = "bEnableStreaming", HideEditConditionToggle))
	EWorldPartitionServerStreamingOutMode ServerStreamingOutMode;

private:
	TObjectPtr<UWorld> World;

#if WITH_EDITOR
	bool bForceGarbageCollection;
	bool bForceGarbageCollectionPurge;
	bool bEnablingStreamingJustified;
	bool bIsPIE;
	int32 NumUserCreatedLoadedRegions;
#endif

#if WITH_EDITORONLY_DATA
	// Default HLOD layer
	UPROPERTY(EditAnywhere, Category = WorldPartitionSetup, meta = (DisplayName = "Default HLOD Layer", EditCondition="bEnableStreaming", EditConditionHides, HideEditConditionToggle))
	TObjectPtr<class UHLODLayer> DefaultHLODLayer;

	TArray<FWorldPartitionReference> LoadedSubobjects;

	TSet<FDirtyActor> DirtyActors;

	TSet<FString> GeneratedStreamingPackageNames;

public:
	TOptional<bool> bOverrideEnableStreamingInEditor;

private:
#endif

	EWorldPartitionInitState InitState;
	TOptional<FTransform> InstanceTransform;

	// Defaults to true, can be set to false to temporarly disable Streaming in of new cells.
	bool bStreamingInEnabled;

	mutable TOptional<bool> bCachedUseMakingInvisibleTransactionRequests;
	mutable TOptional<bool> bCachedUseMakingVisibleTransactionRequests;
	mutable TOptional<bool> bCachedIsServerStreamingEnabled;
	mutable TOptional<bool> bCachedIsServerStreamingOutEnabled;

	UPROPERTY(Transient)
	TObjectPtr<UDataLayerManager> DataLayerManager;

	UPROPERTY(Transient)
	mutable TObjectPtr<UWorldPartitionStreamingPolicy> StreamingPolicy;

#if WITH_EDITORONLY_DATA
	FLinkerInstancingContext InstancingContext;
#endif

#if WITH_EDITOR
	static ENGINE_API int32 LoadingRangeBugItGo;
	static ENGINE_API int32 EnableSimulationStreamingSource;
	static ENGINE_API int32 WorldExtentToEnableStreaming;
	static ENGINE_API bool DebugDedicatedServerStreaming;
	static ENGINE_API FAutoConsoleVariableRef CVarLoadingRangeBugItGo;
	static ENGINE_API FAutoConsoleVariableRef CVarEnableSimulationStreamingSource;
	static ENGINE_API FAutoConsoleVariableRef CVarWorldExtentToEnableStreaming;
	static ENGINE_API FAutoConsoleVariableRef CVarDebugDedicatedServerStreaming;
#endif

	int32 StreamingStateEpoch;
	static ENGINE_API int32 GlobalEnableServerStreaming;
	static ENGINE_API bool bGlobalEnableServerStreamingOut;
	static ENGINE_API bool bUseMakingVisibleTransactionRequests;
	static ENGINE_API bool bUseMakingInvisibleTransactionRequests;
	static ENGINE_API FAutoConsoleVariableRef CVarEnableServerStreaming;
	static ENGINE_API FAutoConsoleVariableRef CVarEnableServerStreamingOut;
	static ENGINE_API FAutoConsoleVariableRef CVarUseMakingVisibleTransactionRequests;
	static ENGINE_API FAutoConsoleVariableRef CVarUseMakingInvisibleTransactionRequests;

	ENGINE_API void OnWorldMatchStarting();
	ENGINE_API void OnPostBugItGoCalled(const FVector& Loc, const FRotator& Rot);

	// Delegates registration
	ENGINE_API void RegisterDelegates();
	ENGINE_API void UnregisterDelegates();	

#if WITH_EDITOR
	ENGINE_API void HashActorDesc(FWorldPartitionActorDesc* ActorDesc);
	ENGINE_API void UnhashActorDesc(FWorldPartitionActorDesc* ActorDesc);
	void OnContentBundleRemovedContent(const FContentBundleEditor* ContentBundle);
	bool IsStreamingEnabledInEditor() const;

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
#endif

private:

#if WITH_EDITORONLY_DATA
	UPROPERTY(transient, NonTransactional)
	TSet<TObjectPtr<UWorldPartitionEditorLoaderAdapter>> RegisteredEditorLoaderAdapters;
#endif

#if !UE_BUILD_SHIPPING
	ENGINE_API void GetOnScreenMessages(FCoreDelegates::FSeverityMessageMap& OutMessages);
#endif
	class AWorldPartitionReplay* Replay;

	friend class AWorldPartitionReplay;
	friend class UWorldPartitionSubsystem;
};

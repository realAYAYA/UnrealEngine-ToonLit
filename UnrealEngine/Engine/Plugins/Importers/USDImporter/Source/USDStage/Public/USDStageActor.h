// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "Misc/SecureHash.h"
#include "Misc/ITransaction.h"
#include "Templates/Function.h"

#include "USDInfoCache.h"
#include "USDLevelSequenceHelper.h"
#include "USDListener.h"
#include "USDPrimTwin.h"
#include "USDSkeletalDataConversion.h"

#include "UsdWrappers/UsdStage.h"
#include "UsdWrappers/UsdPrim.h"
#include "UsdWrappers/SdfPath.h"

#include "USDStageActor.generated.h"

class ALevelSequenceActor;
class IMeshBuilderModule;
class ULevelSequence;
class UMaterial;
class UUsdAsset;
class UUsdAssetCache;
class UUsdTransactor;
enum class EMapChangeType : uint8;
enum class EUsdPurpose : int32;
struct FMeshDescription;
struct FUsdSchemaTranslationContext;

UCLASS( MinimalAPI )
class AUsdStageActor : public AActor
{
	GENERATED_BODY()

	friend struct FUsdStageActorImpl;
	friend class FUsdLevelSequenceHelperImpl;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "USD", meta = (RelativeToGameDir))
	FFilePath RootLayer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "USD")
	EUsdInitialLoadSet InitialLoadSet;

	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "USD" )
	EUsdInterpolationType InterpolationType;

	/**
	 * Whether to try to combine individual assets and components of the same type on a kind-per-kind basis,
	 * like multiple Mesh prims into a single Static Mesh
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "USD", meta = (Bitmask, BitmaskEnum="/Script/UnrealUSDWrapper.EUsdDefaultKind"))
	int32 KindsToCollapse;

	/**
	 * If enabled, when multiple mesh prims are collapsed into a single static mesh, identical material slots are merged into one slot.
	 * Otherwise, materials slots are simply appended to the list.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "USD")
	bool bMergeIdenticalMaterialSlots;

	/**
	 * If true, will cause us to collapse any point instancer prim into a single static mesh and static mesh component.
	 * If false, will cause us to use HierarchicalInstancedStaticMeshComponents to replicate the instancing behavior.
	 * Point instancers inside other point instancer prototypes are *always* collapsed into the prototype's static mesh.
	 */
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "USD" )
	bool bCollapseTopLevelPointInstancers;

	/* Only load prims with these specific purposes from the USD file */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "USD", meta = (Bitmask, BitmaskEnum="/Script/UnrealUSDWrapper.EUsdPurpose"))
	int32 PurposesToLoad;

	/** Try enabling Nanite for static meshes that are generated with at least this many triangles */
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "USD", meta = ( NoSpinbox = "true", UIMin = "0", ClampMin = "0" ))
	int32 NaniteTriangleThreshold;

	/** Specifies which set of shaders to use when parsing USD materials, in addition to the universal render context. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "USD")
	FName RenderContext;

	/** Specifies which material purbose to use when parsing USD material bindings, in addition to the "allPurpose" fallback. */
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "USD" )
	FName MaterialPurpose;

	// Describes what to add to the root bone animation within generated AnimSequences, if anything
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "USD" )
	EUsdRootMotionHandling RootMotionHandling = EUsdRootMotionHandling::NoAdditionalRootMotion;

public:
	UFUNCTION(BlueprintCallable, Category = "USD", meta = (CallInEditor = "true"))
	USDSTAGE_API void SetRootLayer(const FString& RootFilePath );

	UFUNCTION(BlueprintCallable, Category = "USD", meta = (CallInEditor = "true"))
	USDSTAGE_API void SetInitialLoadSet( EUsdInitialLoadSet NewLoadSet );

	UFUNCTION( BlueprintCallable, Category = "USD", meta = ( CallInEditor = "true" ) )
	USDSTAGE_API void SetInterpolationType( EUsdInterpolationType NewType );

	UFUNCTION( BlueprintCallable, Category = "USD", meta = ( CallInEditor = "true" ) )
	USDSTAGE_API void SetKindsToCollapse( int32 NewKindsToCollapse );

	UFUNCTION( BlueprintCallable, Category = "USD", meta = ( CallInEditor = "true" ) )
	USDSTAGE_API void SetMergeIdenticalMaterialSlots( bool bMerge );

	UFUNCTION( BlueprintCallable, Category = "USD", meta = ( CallInEditor = "true" ) )
	USDSTAGE_API void SetCollapseTopLevelPointInstancers( bool bCollapse );

	UFUNCTION(BlueprintCallable, Category = "USD", meta = (CallInEditor = "true"))
	USDSTAGE_API void SetPurposesToLoad( int32 NewPurposesToLoad );

	UFUNCTION( BlueprintCallable, Category = "USD", meta = ( CallInEditor = "true" ) )
	USDSTAGE_API void SetNaniteTriangleThreshold( int32 NewNaniteTriangleThreshold );

	UFUNCTION(BlueprintCallable, Category = "USD", meta = (CallInEditor = "true"))
	USDSTAGE_API void SetRenderContext( const FName& NewRenderContext );

	UFUNCTION( BlueprintCallable, Category = "USD", meta = ( CallInEditor = "true" ) )
	USDSTAGE_API void SetMaterialPurpose( const FName& NewMaterialPurpose );

	UFUNCTION( BlueprintCallable, Category = "USD", meta = ( CallInEditor = "true" ) )
	USDSTAGE_API void SetRootMotionHandling( EUsdRootMotionHandling NewHandlingStrategy );

	UFUNCTION(BlueprintCallable, Category = "USD", meta = (CallInEditor = "true"))
	USDSTAGE_API float GetTime() const { return Time; }

	UFUNCTION(BlueprintCallable, Category = "USD", meta = (CallInEditor = "true"))
	USDSTAGE_API void SetTime(float InTime);

	UFUNCTION( BlueprintCallable, Category = "USD", meta = ( CallInEditor = "true" ) )
	USDSTAGE_API ULevelSequence* GetLevelSequence() { return LevelSequence; }

	/**
	 * Gets the transient component that was generated for a prim with a given prim path.
	 * Warning: The lifetime of the component is managed by the AUsdStageActor, and it may be force-destroyed at any time (e.g. when closing the stage)
	 * @param PrimPath - Full path to the source prim, e.g. "/root_prim/my_prim"
	 * @return The corresponding spawned component. It may correspond to a parent prim, if the prim at PrimPath was collapsed. Nullptr if path is invalid.
	 */
	UFUNCTION(BlueprintCallable, Category = "USD", meta = (CallInEditor = "true"))
	USDSTAGE_API USceneComponent* GetGeneratedComponent( const FString& PrimPath );

	/**
	 * Gets the transient assets that were generated for a prim with a given prim path. Likely one asset (e.g. UStaticMesh), but can be multiple (USkeletalMesh, USkeleton, etc.)
	 * @param PrimPath - Full path to the source prim, e.g. "/root_prim/my_mesh"
	 * @return The corresponding generated assets. May be empty if path is invalid or if that prim led to no generated assets.
	 */
	UFUNCTION(BlueprintCallable, Category = "USD", meta = (CallInEditor = "true"))
	USDSTAGE_API TArray<UObject*> GetGeneratedAssets( const FString& PrimPath );

	/**
	 * Gets the path to the prim that was parsed to generate the given `Object`.
	 * @param Object - UObject to query with. Can be one of the transient components generated when a stage was opened, or something like a UStaticMesh.
	 * @return The path to the source prim, e.g. "/root_prim/some_prim". May be empty in case we couldn't find the source prim.
	 */
	UFUNCTION(BlueprintCallable, Category = "USD", meta = (CallInEditor = "true"))
	USDSTAGE_API FString GetSourcePrimPath( UObject* Object );

private:
	UPROPERTY(Category = UsdStageActor, VisibleAnywhere, BlueprintReadOnly, meta = (ExposeFunctionCategories = "Mesh,Rendering,Physics,Components|StaticMesh", AllowPrivateAccess = "true"))
	TObjectPtr<class USceneComponent> SceneComponent;

	/* TimeCode to evaluate the USD stage at */
	UPROPERTY(EditAnywhere, Category = "USD")
	float Time;

	UPROPERTY(VisibleAnywhere, Category = "USD", Transient)
	TObjectPtr<ULevelSequence> LevelSequence;

public:
	DECLARE_EVENT_OneParam( AUsdStageActor, FOnActorLoaded, AUsdStageActor* );
	USDSTAGE_API static FOnActorLoaded OnActorLoaded;

	DECLARE_EVENT( AUsdStageActor, FOnStageActorEvent );
	FOnStageActorEvent OnPreStageChanged;
	FOnStageActorEvent OnStageChanged;
	FOnStageActorEvent OnActorDestroyed;

	DECLARE_EVENT_TwoParams( AUsdStageActor, FOnPrimChanged, const FString&, bool );
	FOnPrimChanged OnPrimChanged;

	DECLARE_MULTICAST_DELEGATE(FOnUsdStageTimeChanged);
	FOnUsdStageTimeChanged OnTimeChanged;

public:
	AUsdStageActor();

	// Creates a brand new, memory-only USD stage and opens it
	USDSTAGE_API void NewStage();

	USDSTAGE_API void IsolateLayer( const UE::FSdfLayer& Layer );

	USDSTAGE_API void Reset() override;
	void Refresh() const;
	void ReloadAnimations();
	float GetTime() { return Time; }
	UUsdAssetCache* GetAssetCache() { return AssetCache; }
	TSharedPtr<FUsdInfoCache> GetInfoCache() { return InfoCache; }
	TMap< FString, TMap< FString, int32 > > GetMaterialToPrimvarToUVIndex() { return MaterialToPrimvarToUVIndex; }
	const UsdUtils::FBlendShapeMap& GetBlendShapeMap() { return BlendShapesByPath; }

public:
#if WITH_EDITOR
	virtual void PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent ) override;
	virtual void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
	virtual void PreEditChange( FProperty* PropertyThatWillChange ) override;
	virtual void PreEditUndo() override;
	void HandleTransactionStateChanged( const FTransactionContext& InTransactionContext, const ETransactionStateEventType InTransactionState );
#endif // WITH_EDITOR
	virtual void PostDuplicate( bool bDuplicateForPIE ) override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void Destroyed() override;
	virtual void PostActorCreated() override;
	virtual void PostRename( UObject* OldOuter, const FName OldName ) override;
	virtual void BeginDestroy() override;

	virtual void PostRegisterAllComponents() override;
	virtual void UnregisterAllComponents( bool bForReregister = false ) override;
	virtual void PostUnregisterAllComponents() override;

	void OnPreUsdImport( FString FilePath );
	void OnPostUsdImport( FString FilePath );

private:
	void OpenUsdStage();
	void CloseUsdStage();

	void LoadUsdStage();
	void UnloadUsdStage();

	UUsdPrimTwin* GetRootPrimTwin();

#if WITH_EDITOR
	void OnBeginPIE(bool bIsSimulating);
	void OnPostPIEStarted(bool bIsSimulating);
	void OnObjectsReplaced( const TMap<UObject*, UObject*>& ObjectReplacementMap );
	void OnLevelActorDeleted(AActor* DeletedActor);
#endif // WITH_EDITOR

	void UpdateSpawnedObjectsTransientFlag( bool bTransient );

	void OnUsdObjectsChanged( const UsdUtils::FObjectChangesByPath& InfoChanges, const UsdUtils::FObjectChangesByPath& ResyncChanges );
	void OnUsdPrimTwinDestroyed( const UUsdPrimTwin& UsdPrimTwin );

	void OnObjectPropertyChanged( UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent );
	void HandlePropertyChangedEvent( FPropertyChangedEvent& PropertyChangedEvent );
	bool HasAuthorityOverStage() const;
	void OnSkelAnimationBaked( const FString& SkelRootPrimPath );

private:
	// Note: This cannot be instanced: Read the comment in the constructor
	UPROPERTY( Transient )
	TObjectPtr<UUsdPrimTwin> RootUsdTwin;

	UPROPERTY( Transient )
	TSet< FString > PrimsToAnimate;

	UPROPERTY( Transient )
	TMap< TObjectPtr<UObject>, FString > ObjectsToWatch;

	UPROPERTY( VisibleAnywhere, Category = "USD", AdvancedDisplay )
	TObjectPtr<UUsdAssetCache> AssetCache;

	UPROPERTY( Transient )
	TObjectPtr<UUsdTransactor> Transactor;

public:
	// Loads the stage with RootLayer if its not loaded already, and returns the either the isolated stage (if any) or
	// the base stage
	USDSTAGE_API UE::FUsdStage& GetOrLoadUsdStage();

	// Returns either the isolated stage (if any) or the base stage
	USDSTAGE_API const UE::FUsdStage& GetUsdStage() const;

	// Always returns the base stage, regardless of whether we have an isolated stage or not
	USDSTAGE_API const UE::FUsdStage& GetBaseUsdStage() const;

	// Always returns the isolated stage, being an invalid stage in case we're not isolating anything
	USDSTAGE_API const UE::FUsdStage& GetIsolatedUsdStage() const;

	FUsdListener& GetUsdListener() { return UsdListener; }
	const FUsdListener& GetUsdListener() const { return UsdListener; }

	/** Control whether we respond to USD notices or not. Mostly used to prevent us from responding to them when we're writing data to the stage */
	USDSTAGE_API void StopListeningToUsdNotices();
	USDSTAGE_API void ResumeListeningToUsdNotices();
	USDSTAGE_API bool IsListeningToUsdNotices() const;

	/** Prevents writing back data to the USD stage whenever our LevelSequences are modified */
	USDSTAGE_API void StopMonitoringLevelSequence();
	USDSTAGE_API void ResumeMonitoringLevelSequence();
	USDSTAGE_API void BlockMonitoringLevelSequenceForThisTransaction();

	UUsdPrimTwin* GetOrCreatePrimTwin( const UE::FSdfPath& UsdPrimPath );
	UUsdPrimTwin* ExpandPrim( const UE::FUsdPrim& Prim, FUsdSchemaTranslationContext& TranslationContext );
	void UpdatePrim( const UE::FSdfPath& UsdPrimPath, bool bResync, FUsdSchemaTranslationContext& TranslationContext );

protected:
	/** Loads the asset for a single prim */
	void LoadAsset( FUsdSchemaTranslationContext& TranslationContext, const UE::FUsdPrim& Prim );

	/** Loads the assets for all prims from StartPrim and its children */
	void LoadAssets( FUsdSchemaTranslationContext& TranslationContext, const UE::FUsdPrim& StartPrim );

	void AnimatePrims();

private:
	/** Caches various information about prims that are expensive to query */
	TSharedPtr<FUsdInfoCache> InfoCache;

	/** Keep track of blend shapes so that we can map 'inbetween shapes' to their separate morph targets when animating */
	UsdUtils::FBlendShapeMap BlendShapesByPath;

	/**
	 * When parsing materials, we keep track of which primvar we mapped to which UV channel.
	 * When parsing meshes later, we use this data to place the correct primvar values in each UV channel.
	 * We keep this here as these are generated when the materials stored in AssetsCache are parsed, so it should accompany them
	 */
	TMap< FString, TMap< FString, int32 > > MaterialToPrimvarToUVIndex;

	// The main UsdStage that is currently opened
	UE::FUsdStage UsdStage;

	// Another stage that has as root layer one of the non-root local layers of UsdStage. This is the stage we'll
	// be displaying if it is valid, otherwise we'll be displaying UsdStage directly.
	UE::FUsdStage IsolatedStage;

	/**
	 * We use PostRegisterAllComponents and PostUnregisterAllComponents as main entry points to decide when to load/unload
	 * the USD stage. These are the three exceptions we must avoid though:
	 *  - We don't want to load/unload when duplicating into PIE as we want our duplicated actors/components to go with us;
	 *  - On the editor, the register/unregister functions are called from AActor::PostEditChangeProperty, and we obviously
	 *    don't want to load/unload the stage on every single property edit.
	 *  - We never want to load/unload actors and components on undo/redo: We always want to fetch them from the transaction buffer
	 */
	bool bIsTransitioningIntoPIE;
	bool bIsModifyingAProperty;
	bool bIsUndoRedoing;

	FUsdListener UsdListener;
	FUsdLevelSequenceHelper LevelSequenceHelper;

	FDelegateHandle OnRedoHandle;
	FThreadSafeCounter IsBlockedFromUsdNotices;

	/**
	 * Helps us know whether a transaction changed our RootLayer or not. We need this because we can only tag spawned
	 * transient actors and components after the initial actor/component spawning transaction has completed. Otherwise,
	 * the spawns will be replicated on each client in addition to the actors/components that they will spawn by themselves
	 * for opening the stage
	 */
	FFilePath OldRootLayer;
};

class USDSTAGE_API FScopedBlockNoticeListening final
{
public:
	explicit FScopedBlockNoticeListening( AUsdStageActor* InStageActor );
	~FScopedBlockNoticeListening();

	FScopedBlockNoticeListening() = delete;
	FScopedBlockNoticeListening( const FScopedBlockNoticeListening& ) = delete;
	FScopedBlockNoticeListening( FScopedBlockNoticeListening&& ) = delete;
	FScopedBlockNoticeListening& operator=( const FScopedBlockNoticeListening& ) = delete;
	FScopedBlockNoticeListening& operator=( FScopedBlockNoticeListening&& ) = delete;

private:
	TWeakObjectPtr<AUsdStageActor> StageActor;
};


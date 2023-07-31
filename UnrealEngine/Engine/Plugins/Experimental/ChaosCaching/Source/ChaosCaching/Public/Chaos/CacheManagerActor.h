// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/ParticleHandleFwd.h"
#include "Chaos/PBDRigidsEvolutionFwd.h"
#include "ChaosCache.h"
#include "Adapters/CacheAdapter.h"
#include "Engine/EngineTypes.h"
#include "Chaos/Core.h"
#include "GameFramework/Actor.h"

#include "CacheManagerActor.generated.h"

class UChaosCacheCollection;
class UPrimitiveComponent;

namespace Chaos
{
class FComponentCacheAdapter;
}

UENUM()
enum class ECacheMode : uint8
{
	None		UMETA(DisplayName = "Static Pose"),
	Play,		
	Record
};

UENUM()
enum class EStartMode : uint8
{
	Timed,
	Triggered,
};

namespace Chaos
{
	using FPhysicsSolver = FPBDRigidsSolver;
}

USTRUCT(BlueprintType)
struct CHAOSCACHING_API FObservedComponent
{
	GENERATED_BODY()

	FObservedComponent()
		: CacheName(NAME_None)
		, bIsSimulating(true)
		, Cache(nullptr)
		, BestFitAdapter(nullptr)
	{
	}

	FObservedComponent(const FObservedComponent& OtherComponent)
	{
		CacheName = OtherComponent.CacheName;
		bIsSimulating = OtherComponent.bIsSimulating;
		bTriggered = OtherComponent.bTriggered;
		AbsoluteTime = OtherComponent.AbsoluteTime;
		TimeSinceTrigger = OtherComponent.TimeSinceTrigger;
		Cache = OtherComponent.Cache;
		TickRecord = OtherComponent.TickRecord;
		BestFitAdapter = OtherComponent.BestFitAdapter;
		SoftComponentRef = OtherComponent.SoftComponentRef;
	}

	FObservedComponent& operator=(const FObservedComponent& OtherComponent)
	{
		CacheName = OtherComponent.CacheName;
		bIsSimulating = OtherComponent.bIsSimulating;
		bTriggered = OtherComponent.bTriggered;
		AbsoluteTime = OtherComponent.AbsoluteTime;
		TimeSinceTrigger = OtherComponent.TimeSinceTrigger;
		Cache = OtherComponent.Cache;
		TickRecord = OtherComponent.TickRecord;
		BestFitAdapter = OtherComponent.BestFitAdapter;
		SoftComponentRef = OtherComponent.SoftComponentRef;
		
		return *this;
	}

	/** Unique name for the cache, used as a key into the cache collection */
	UPROPERTY(EditAnywhere, Category = "Caching")
	FName CacheName;

	/** Deprecated hard object reference. Not working with sequencer and take recorder since
	 all the pointers from any other packages are cleared out. Use TSoftObjectPtr instead. */
	UE_DEPRECATED(5.1, "This property is going to be deleted. Use the SoftComponentRef instead")
	UPROPERTY()
	FComponentReference ComponentRef;

	/** The component observed by this object for either playback or recording */
	UPROPERTY(EditAnywhere, Category = "Caching", meta = (UseComponentPicker, AllowAnyActor))
	FSoftComponentReference SoftComponentRef;

	/** Capture of the initial state of the component before cache manager takes control. */
	UPROPERTY(EditAnywhere, Category = "Caching")
	bool bIsSimulating;

	/** Post serialize function to transfer datas from the deprecated TObjectPtr -> TSoftObjectPtr */
	void PostSerialize(const FArchive& Ar);

	/** Prepare runtime tick data for a new run */
	void ResetRuntimeData(const EStartMode ManagerStartMode);

	/** Gets the component from the internal component ref */
	UPrimitiveComponent* GetComponent();
	UPrimitiveComponent* GetComponent() const;

private:
	friend class AChaosCacheManager;

	bool         bTriggered;          // Whether the observed component is active
	Chaos::FReal AbsoluteTime;        // Time since BeginPlay
	Chaos::FReal TimeSinceTrigger;    // Time since our trigger happened
	UChaosCache* Cache;               // Cache to play - picked up in BeginPlay on the manager.
	FPlaybackTickRecord TickRecord;   // Tick record to track where we are in playback

	Chaos::FComponentCacheAdapter* BestFitAdapter; // Cache a pointer to the best adapter, required for cache random access handling.
};

struct FPerSolverData
{
	/* Handles to solver events to push/pull cache data */
	FDelegateHandle PreSolveHandle;
	FDelegateHandle PreBufferHandle;
	FDelegateHandle PostSolveHandle;
	FDelegateHandle TeardownHandle;

	/** List of the tick records for each playback index, tracks where the last tick was */
	TArray<FPlaybackTickRecord> PlaybackTickRecords;
	/** List of indices for components tagged for playback - avoids iterating non playback components */
	TArray<int32> PlaybackIndices;
	/** List of indices for components tagged for record - avoids iterating non record components */
	TArray<int32> RecordIndices;
	/** List of particles in the solver that are pending a kinematic update to be pushed back to their owner */
	TArray<Chaos::TPBDRigidParticleHandle<Chaos::FReal, 3>*> PendingKinematicUpdates;
};

UCLASS(Experimental)
class CHAOSCACHING_API AChaosCacheManager : public AActor
{
	GENERATED_BODY()

public:
	AChaosCacheManager(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * The Cache Collection asset to use for this observer. This can be used for playback and record simultaneously
	 * across multiple components depending on the settings for that component.
	 */
	UPROPERTY(EditInstanceOnly, Category = "Caching")
	TObjectPtr<UChaosCacheCollection> CacheCollection;

	/** How to use the cache - playback or record */
	UPROPERTY(EditAnywhere, Category = "Caching")
	ECacheMode CacheMode;

	/**
	* How to trigger the cache record or playback, timed will start counting at BeginPlay, Triggered will begin
	* counting from when the owning cache manager is requested to trigger the cache action
	* @see AChaosCacheManager::TriggerObservedComponent
	*/
	UPROPERTY(EditAnywhere, Category = "Caching")
	EStartMode StartMode;

	/**
	* Defines the (random access) time that represents the rest pose of the components managed by this cache.
	* When in Play mode, the components are set to the state provided by the caches at this evaluated time.
	*/
	UPROPERTY(EditAnywhere, Interp, BlueprintReadWrite, Category = "Caching", meta=(SequencerTrackClass="MovieSceneFloatTrack"))
	float StartTime;

	/** AActor interface */
	void TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;
	/** end AActor interface */

	/** UObject interface */
	virtual void Serialize(FArchive& Ar) override;
#if WITH_EDITOR
	friend class IChaosCachingEditorPlugin;
#endif

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	/** end UObject interface */

	/** Expose StartTime property to Sequencer. GetStartTime will be called on keys. */
	UFUNCTION(CallInEditor)
	void SetStartTime(float InStartTime);

	/** 
	 * Resets all components back to the world space transform they had when the cache for them was originally recorded
	 * if one is available
	 */
	UFUNCTION(BlueprintCallable, Category = "Caching")
	void ResetAllComponentTransforms();

	/**
	 * Resets the component at the specified index in the observed list back to the world space transform it had when the 
	 * cache for it was originally recorded if one is available
	 * @param InIndex Index of the component to reset
	 */
	UFUNCTION(BlueprintCallable, Category = "Caching")
	void ResetSingleTransform(int32 InIndex);

#if WITH_EDITOR
	/**
	 * Set the component at the specified index in the observed array to be the selected component in the outliner.
	 * This will also make that component's owner the selected actor in the outliner.
	 */
	void SelectComponent(int32 InIndex);
#endif

	/** Returns true if this cache manager is allowed to record caches. */
	bool CanRecord() const { return bCanRecord; }

	/** Initialize the cache adapters before playing/recording the cache. */
	void BeginEvaluate();
	
	/** Clean the cache adapters after playing/recording the cache.  */
	void EndEvaluate();

	/** Accessor to the manager observed components (read only) */
	const TArray<FObservedComponent>& GetObservedComponents() const {return ObservedComponents;}

	/** Accessor to the manager observed components (read/write) */
	TArray<FObservedComponent>& GetObservedComponents() {return ObservedComponents;}

protected:

	/** AActor interface */
	void BeginPlay() override;
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	/** End AActor interface */
	
	/**	Handles physics thread pre-solve (push kinematic data for components under playback) */
    void HandlePreSolve(Chaos::FReal InDt, Chaos::FPhysicsSolverEvents* InSolver);
   
    /** Handles physics thread pre-buffer (mark dirty kinematic particles) */
    void HandlePreBuffer(Chaos::FReal InDt, Chaos::FPhysicsSolverEvents* InSolver);
   
    /** Handles physics thread post-solve (record data for components under record) */
    void HandlePostSolve(Chaos::FReal InDt, Chaos::FPhysicsSolverEvents* InSolver);
   
    /** Handles solver teardown due to solver destruction / stream-out */
    void HandleTeardown(Chaos::FPhysicsSolverEvents* InSolver);
	
	/** Evaluates and sets state for all observed components at the specified time. */
	void OnStartFrameChanged(Chaos::FReal InT);

	/**
	 * Triggers a component to play or record.
	 * If the cache manager has an observed component entry for InComponent and it is a triggered entry
	 * this will begin the playback or record for that component, otherwise no action is taken.
	 * @param InComponent Component to trigger
	 */
	UFUNCTION(BlueprintCallable, Category = "Caching")
	void TriggerComponent(UPrimitiveComponent* InComponent);

	/**
	 * Triggers a component to play or record.
	 * Searches the observed component list for an entry matching InCacheName and triggers the
	 * playback or recording of the linked observed component
	 * @param InCacheName Cache name to trigger
	 */
	UFUNCTION(BlueprintCallable, Category = "Caching")
	void TriggerComponentByCache(FName InCacheName);

	/** Triggers the recording or playback of all observed components */
	UFUNCTION(BlueprintCallable, Category = "Caching")
	void TriggerAll();

	FObservedComponent* FindObservedComponent(UPrimitiveComponent* InComponent);
	FObservedComponent& AddNewObservedComponent(UPrimitiveComponent* InComponent);
	FObservedComponent& FindOrAddObservedComponent(UPrimitiveComponent* InComponent);
	void ClearObservedComponents();

	// Determines if the actor is allowed to record a cache.
	bool bCanRecord;

	// true if observed components are actively simulating, dynamically or kinematically.
	bool bIsSimulating;

private:
#if WITH_EDITOR
	void SetObservedComponentProperties(const ECacheMode& NewCacheMode);
#endif
	
	friend class UActorFactoryCacheManager; // Allows the actor factory to set up the observed list. See UActorFactoryCacheManager::PostSpawnActor

	using FTickObservedFunction = TUniqueFunction<void(UChaosCache*, FObservedComponent&, Chaos::FComponentCacheAdapter*)>;

	/**
	 * Helper function to apply a callable to observed components if they've been triggered, all of the Dt/time
	 * bookkeeping is handled in one place
	 * @param InIndices Index list of the observed components to update
	 * @param InDt Delta for the tick
	 * @param InCallable Callable to fire if the observed component is active
	 */
	void TickObservedComponents(const TArray<int32>& InIndices, Chaos::FReal InDt, FTickObservedFunction InCallable);

	/** List of observed objects and their caches */
	UPROPERTY(EditAnywhere, Category = "Caching")
	TArray<FObservedComponent> ObservedComponents;

	/** 1-1 list of adapters for the observed components, populated on BeginPlay */
	TArray<Chaos::FComponentCacheAdapter*> ActiveAdapters;

	/** List of particles returned by the adapter as requiring a kinematic update */
	TMap<Chaos::FPhysicsSolverEvents*, FPerSolverData> PerSolverData;

	/** Lists of currently open caches that need to be closed when complete */
	TArray<TTuple<FCacheUserToken, UChaosCache*>> OpenRecordCaches;
	TArray<TTuple<FCacheUserToken, UChaosCache*>> OpenPlaybackCaches;
};

UCLASS(Experimental)
class CHAOSCACHING_API AChaosCachePlayer : public AChaosCacheManager
{
	GENERATED_BODY()

public:
	AChaosCachePlayer(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

};

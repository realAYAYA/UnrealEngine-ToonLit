// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "UObject/WeakObjectPtr.h"
#include "Misc/CoreMisc.h"
#include "Misc/CoreDelegates.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "AI/Navigation/NavigationTypes.h"
#include "AI/Navigation/NavigationDirtyElement.h"
#include "NavigationSystemTypes.h"
#include "NavigationData.h"
#include "AI/NavigationSystemBase.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NavigationOctree.h"
#include "AI/NavigationSystemConfig.h"
#include "NavigationOctreeController.h"
#include "NavigationDirtyAreasController.h"
#include "Math/MovingWindowAverageFast.h"
#if WITH_EDITOR
#include "UnrealEdMisc.h"
#endif // WITH_EDITOR
#include "NavigationSystem.generated.h"


class AController;
class ANavMeshBoundsVolume;
class AWorldSettings;
class FEdMode;
class FNavDataGenerator;
class INavLinkCustomInterface;
class INavRelevantInterface;
class UCrowdManagerBase;
class UNavArea;
class UNavigationPath;
class UNavigationSystemModuleConfig;
struct FNavigationRelevantData;
struct FNavigationOctreeElement;

#if !UE_BUILD_SHIPPING
#define ALLOW_TIME_SLICE_DEBUG 1
#else
#define ALLOW_TIME_SLICE_DEBUG 0
#endif

/** delegate to let interested parties know that new nav area class has been registered */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnNavAreaChanged, const UClass* /*AreaClass*/);

/** Delegate to let interested parties know that Nav Data has been registered */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNavDataGenericEvent, ANavigationData*, NavData);

DECLARE_MULTICAST_DELEGATE(FOnNavigationInitDone);

namespace NavigationDebugDrawing
{
	extern const NAVIGATIONSYSTEM_API float PathLineThickness;
	extern const NAVIGATIONSYSTEM_API FVector PathOffset;
	extern const NAVIGATIONSYSTEM_API FVector PathNodeBoxExtent;
}

namespace FNavigationSystem
{	
	/** 
	 * Used to construct an ANavigationData instance for specified navigation data agent 
	 */
	typedef ANavigationData* (*FNavigationDataInstanceCreator)(UWorld*, const FNavDataConfig&);

	struct NAVIGATIONSYSTEM_API FCustomLinkOwnerInfo
	{
		FWeakObjectPtr LinkOwner;
		INavLinkCustomInterface* LinkInterface;

		FCustomLinkOwnerInfo() : LinkInterface(nullptr) {}
		FCustomLinkOwnerInfo(INavLinkCustomInterface* Link);

		bool IsValid() const { return LinkOwner.IsValid(); }
	};

	bool NAVIGATIONSYSTEM_API ShouldLoadNavigationOnClient(ANavigationData& NavData);

	void NAVIGATIONSYSTEM_API MakeAllComponentsNeverAffectNav(AActor& Actor);

#if ALLOW_TIME_SLICE_DEBUG
	const FName DebugTimeSliceDefaultSectionName = FName(TEXT("DefaultSection"));
#endif // ALLOW_TIME_SLICE_DEBUG
}

struct FNavigationSystemExec: public FSelfRegisteringExec
{
	//~ Begin FExec Interface
	virtual bool Exec(UWorld* Inworld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	//~ End FExec Interface
};

namespace ENavigationBuildLock
{
	enum Type
	{
		NoUpdateInEditor = 1 << 1,		// editor doesn't allow automatic updates
		NoUpdateInPIE = 1 << 2,			// PIE doesn't allow automatic updates
		InitialLock = 1 << 3,			// initial lock, release manually after levels are ready for rebuild (e.g. streaming)
		Custom = 1 << 4,
	};
}


// Use this just  before a call to TestTimeSliceFinished() to allow the time slice logging to use a debug name for the section of code being timed.
#if ALLOW_TIME_SLICE_DEBUG
#define MARK_TIMESLICE_SECTION_DEBUG(TIME_SLICER, TIME_SLICE_FNAME) \
static const FName TIME_SLICE_FNAME(TEXT(#TIME_SLICE_FNAME)); \
TIME_SLICER.DebugSetSectionName(TIME_SLICE_FNAME);
#else
#define MARK_TIMESLICE_SECTION_DEBUG(TIME_SLICER, TIME_SLICE_FNAME) ;
#endif

class NAVIGATIONSYSTEM_API FNavRegenTimeSlicer
{
public:
	/** Setup the initial values for a time slice. This can be called on an instance after TestTimeSliceFinished() has returned true and EndTimeSliceAndAdjustDuration() has been called */
	void SetupTimeSlice(double SliceDuration);

	/** 
	 *  Starts the time slice, this can be called multiple times as long as EndTimeSliceAndAdjustDuration() is called between each call.
	 *  StartTimeSlice should not be called after TestTimeSliceFinished() has returned true
	 */
	void StartTimeSlice();

	/** 
	 *  Useful when multiple sections of code need to be timesliced per frame using the same time slice duration that do not necessarily occur concurrently.
	 *  This ends the time sliced code section and adjusts the RemainingDuration based on the time used between calls to StartTimeSlice and the last call to TestTimeSliceFinished.
	 *  Note the actual time slice is not tested in this function. Thats done in TestTimeSliceFinished!
	 *  This can be called multiple times as long as StartTimeSlice() is called before EndTimeSliceAndAdjustDuration().
	 *  EndTimeSliceAndAdjustDuration can be called after TestTimeSliceFinished() has returned true in this case the RemainingDuration will just be zero
	 */
	void EndTimeSliceAndAdjustDuration();
	double GetStartTime() const { return StartTime; }
	bool TestTimeSliceFinished() const;

	//* Returns the cached result of calling TestTimeSliceFinished, false by default */
	bool IsTimeSliceFinishedCached() const { return bTimeSliceFinishedCached; }
	double GetRemainingDuration() const { return RemainingDuration; }
	double GetRemainingDurationFraction() const { return OriginalDuration > 0. ? RemainingDuration / OriginalDuration : 0.; }


#if ALLOW_TIME_SLICE_DEBUG
	/*
	 * Sets data used for debugging time slices that are taking too long to process.
	 * @param LongTimeSliceFunction function called when a time slice is taking too long to process.
	 * @param LongTimeSliceDuration when a time slice takes longer than this duration LongTimeSliceFunction will be called.
	 */
	void DebugSetLongTimeSliceData(TFunction<void(FName, double)> LongTimeSliceFunction, double LongTimeSliceDuration) const;
	void DebugResetLongTimeSliceFunction() const;

	/**
	 *  Sets the debug name for a time sliced section of code.
	 *  Do not call this directly use MARK_TIMESLICE_SECTION_DEBUG
	 */
	void DebugSetSectionName(FName InDebugSectionName) const
	{
		DebugSectionName = InDebugSectionName;
	}
#endif // ALLOW_TIME_SLICE_DEBUG

protected:
	double OriginalDuration = 0.;
	double RemainingDuration = 0.;
	double StartTime = 0.;
	mutable double TimeLastTested = 0.;
	mutable bool bTimeSliceFinishedCached = false;

#if ALLOW_TIME_SLICE_DEBUG
	mutable TFunction<void(FName, double)> DebugLongTimeSliceFunction;
	mutable double DebugLongTimeSliceDuration = 0.;
	mutable FName DebugSectionName = FNavigationSystem::DebugTimeSliceDefaultSectionName;
#endif
};

class NAVIGATIONSYSTEM_API FNavRegenTimeSliceManager
{
public:
	FNavRegenTimeSliceManager();

	void PushTileRegenTime(double NewTime) { MovingWindowTileRegenTime.PushValue(NewTime);  }

	double GetAverageTileRegenTime() const { return MovingWindowTileRegenTime.GetAverage();  }

	double GetAverageDeltaTime() const { return MovingWindowDeltaTime.GetAverage(); }

	bool DoTimeSlicedUpdate() const { return bDoTimeSlicedUpdate; }

	void CalcAverageDeltaTime(uint64 FrameNum);

	void CalcTimeSliceDuration(int32 NumTilesToRegen, const TArray<double>& CurrentTileRegenDurations);

	void SetMinTimeSliceDuration(double NewMinTimeSliceDuration);

	void SetMaxTimeSliceDuration(double NewMaxTimeSliceDuration);

	void SetMaxDesiredTileRegenDuration(float NewMaxDesiredTileRegenDuration);

	int32 GetNavDataIdx() const { return NavDataIdx;  }
	void SetNavDataIdx(int32 InNavDataIdx) { NavDataIdx = InNavDataIdx; }

	FNavRegenTimeSlicer& GetTimeSlicer() { return TimeSlicer; }
	const FNavRegenTimeSlicer& GetTimeSlicer() const { return TimeSlicer; }

protected:
	FNavRegenTimeSlicer TimeSlicer;

	/** Used to calculate the moving window average of the actual time spent inside functions used to regenerate a tile, this is processing time not actual time over multiple frames */
	FMovingWindowAverageFast<double, 256> MovingWindowTileRegenTime;

	/** Used to calculate the actual moving window delta time */
	FMovingWindowAverageFast<double, 256> MovingWindowDeltaTime;

	/** If there are enough tiles to process this in the Min Time Slice Duration */
	double MinTimeSliceDuration;

	/** The max Desired Time Slice Duration */
	double MaxTimeSliceDuration;

	uint64 FrameNumOld;

	/** The max real world desired time to Regen all the tiles in PendingDirtyTiles,
	 *  Note it could take longer than this, as the time slice is clamped per frame between
	 *	MinTimeSliceDuration and MaxTimeSliceDuration.
	 */
	float MaxDesiredTileRegenDuration;

	double TimeLastCall;

	int32 NavDataIdx;

	bool bDoTimeSlicedUpdate;
};

UCLASS(Within=World, config=Engine, defaultconfig)
class NAVIGATIONSYSTEM_API UNavigationSystemV1 : public UNavigationSystemBase
{
	GENERATED_BODY()

	friend UNavigationSystemModuleConfig;

public:
	UNavigationSystemV1(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual ~UNavigationSystemV1();

	UPROPERTY(Transient)
	TObjectPtr<ANavigationData> MainNavData;

	/** special navigation data for managing direct paths, not part of NavDataSet! */
	UPROPERTY(Transient)
	TObjectPtr<ANavigationData> AbstractNavData;

protected:
	/** If not None indicates which of navigation datas and supported agents are
	 * going to be used as the default ones. If navigation agent of this type does
	 * not exist or is not enabled then the first available nav data will be used
	 * as the default one */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = Navigation)
	FName DefaultAgentName;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = Navigation)
	TSoftClassPtr<UCrowdManagerBase> CrowdManagerClass;

	/** Should navigation system spawn default Navigation Data when there's none and there are navigation bounds present? */
	UPROPERTY(config, EditAnywhere, Category=NavigationSystem)
	uint32 bAutoCreateNavigationData:1;

	/** If true will try to spawn the navigation data instance in the sublevel with navigation bounds, if false it will spawn in the persistent level */
	UPROPERTY(config, EditAnywhere, Category = NavigationSystem)
	uint32 bSpawnNavDataInNavBoundsLevel:1;

	/** If false, will not create nav collision when connecting as a client */
	UPROPERTY(config, EditAnywhere, Category=NavigationSystem)
	uint32 bAllowClientSideNavigation:1;

	/** If true, games should ignore navigation data inside loaded sublevels */
	UPROPERTY(config, EditAnywhere, Category = NavigationSystem)
	uint32 bShouldDiscardSubLevelNavData:1;

	/** If true, will update navigation even when the game is paused */
	UPROPERTY(config, EditAnywhere, Category=NavigationSystem)
	uint32 bTickWhilePaused:1;

	/** gets set to true if gathering navigation data (like in navoctree) is required due to the need of navigation generation 
	 *	Is always true in Editor Mode. In other modes it depends on bRebuildAtRuntime of every required NavigationData class' CDO
	 */
	UPROPERTY()
	uint32 bSupportRebuilding : 1; 

public:
	/** if set to true will result navigation system not rebuild navigation until 
	 *	a call to ReleaseInitialBuildingLock() is called. Does not influence 
	 *	editor-time generation (i.e. does influence PIE and Game).
	 *	Defaults to false.*/
	UPROPERTY(config, EditAnywhere, Category=NavigationSystem)
	uint32 bInitialBuildingLocked:1;

	/** If set to true (default) navigation will be generated only within special navigation 
	 *	bounds volumes (like ANavMeshBoundsVolume). Set to false means navigation should be generated
	 *	everywhere.
	 */
	// @todo removing it from edition since it's currently broken and I'm not sure we want that at all
	// since I'm not sure we can make it efficient in a generic case
	//UPROPERTY(config, EditAnywhere, Category=NavigationSystem)
	uint32 bWholeWorldNavigable:1;

	/** false by default, if set to true will result in not caring about nav agent height 
	 *	when trying to match navigation data to passed in nav agent */
	UPROPERTY(config, EditAnywhere, Category=NavigationSystem)
	uint32 bSkipAgentHeightCheckWhenPickingNavData:1;

	/** Warnings are logged if exporting the navigation collision for an object exceed this vertex count.
	 * Use -1 to disable. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = NavigationSystem)
	int32 GeometryExportVertexCountWarningThreshold = 1000000;
	
protected:
	/** If set to true navigation will be generated only around registered "navigation enforcers"
	*	This has a range of consequences (including how navigation octree operates) so it needs to
	*	be a conscious decision.
	*	Once enabled results in whole world being navigable.
	*	@see RegisterNavigationInvoker
	*/
	UPROPERTY(EditDefaultsOnly, Category = "Navigation Enforcing", config)
	uint32 bGenerateNavigationOnlyAroundNavigationInvokers:1;

	/** Minimal time, in seconds, between active tiles set update */
	UPROPERTY(EditAnywhere, Category = "Navigation Enforcing", meta = (ClampMin = "0.1", UIMin = "0.1", EditCondition = "bGenerateNavigationOnlyAroundNavigationInvokers"), config)
	float ActiveTilesUpdateInterval;

	/** Sets how navigation data should be gathered when building collision information */
	UPROPERTY(EditDefaultsOnly, Category = "NavigationSystem", config)
	ENavDataGatheringModeConfig DataGatheringMode;

	/** -1 by default, if set to a positive value dirty areas with any dimensions in 2d over the threshold created at runtime will be logged */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = NavigationSystem, meta = (ClampMin = "-1.0", UIMin = "-1.0"))
	float DirtyAreaWarningSizeThreshold;

	/** -1.0f by default, if set to a positive value, all calls to GetNavigationData will be timed and compared to it. 
	*	Over the limit calls will be logged as warnings. 
	*	In seconds. Non-shipping build only.
	*/
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = NavigationSystem, meta = (ClampMin = "-1.0", UIMin = "-1.0"))
	float GatheringNavModifiersWarningLimitTime;

	/** List of agents types supported by this navigation system */
	UPROPERTY(config, EditAnywhere, Category = Agents)
	TArray<FNavDataConfig> SupportedAgents;

	/** NavigationSystem's properties in Project Settings define all possible supported agents,
	 *	but a specific navigation system can choose to support only a subset of agents. Set via 
	 *	NavigationSystemConfig */
	UPROPERTY(config, EditAnywhere, Category = Agents)
	FNavAgentSelector SupportedAgentsMask;

public:
	/** Bounds of tiles to be built */
	UPROPERTY(Transient)
	FBox BuildBounds;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ANavigationData>> NavDataSet;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ANavigationData>> NavDataRegistrationQueue;

	// List of pending navigation bounds update requests (add, remove, update size)
	TArray<FNavigationBoundsUpdateRequest> PendingNavBoundsUpdates;

 	UPROPERTY(/*BlueprintAssignable, */Transient)
	FOnNavDataGenericEvent OnNavDataRegisteredEvent;

	UPROPERTY(BlueprintAssignable, Transient, meta = (displayname = OnNavigationGenerationFinished))
	FOnNavDataGenericEvent OnNavigationGenerationFinishedDelegate;

	FOnNavigationInitDone OnNavigationInitDone;
	
private:
	TWeakObjectPtr<UCrowdManagerBase> CrowdManager;
	
	/** set to true when navigation processing was blocked due to missing nav bounds */
	uint32 bNavDataRemovedDueToMissingNavBounds : 1;

protected:	
	/** All areas where we build/have navigation */
	TSet<FNavigationBounds> RegisteredNavBounds;

private:
	TMap<AActor*, FNavigationInvoker> Invokers;
	/** Contains pre-digested and cached invokers' info. Generated by UpdateInvokers */
	TArray<FNavigationInvokerRaw> InvokerLocations;

	float NextInvokersUpdateTime;
	void UpdateInvokers();

	void DirtyTilesInBuildBounds();

public:
	//----------------------------------------------------------------------//
	// Blueprint functions
	//----------------------------------------------------------------------//

	UFUNCTION(BlueprintPure, Category = "AI|Navigation", meta = (WorldContext = "WorldContextObject"))
	static UNavigationSystemV1* GetNavigationSystem(UObject* WorldContextObject);
	
	/** Project a point onto the NavigationData */
	UFUNCTION(BlueprintPure, Category = "AI|Navigation", meta = (WorldContext = "WorldContextObject", DisplayName = "ProjectPointToNavigation", ScriptName = "ProjectPointToNavigation"))
	static bool K2_ProjectPointToNavigation(UObject* WorldContextObject, const FVector& Point, FVector& ProjectedLocation, ANavigationData* NavData, TSubclassOf<UNavigationQueryFilter> FilterClass, const FVector QueryExtent = FVector::ZeroVector);

	/** Generates a random location reachable from given Origin location.
	 *	@return Return Value represents if the call was successful */
	UFUNCTION(BlueprintPure, Category = "AI|Navigation", meta = (WorldContext = "WorldContextObject", DisplayName = "GetRandomReachablePointInRadius", ScriptName = "GetRandomReachablePointInRadius"))
	static bool K2_GetRandomReachablePointInRadius(UObject* WorldContextObject, const FVector& Origin, FVector& RandomLocation, float Radius, ANavigationData* NavData = NULL, TSubclassOf<UNavigationQueryFilter> FilterClass = NULL);

	/** Generates a random location in navigable space within given radius of Origin.
	 *	@return Return Value represents if the call was successful */
	UFUNCTION(BlueprintCallable, Category = "AI|Navigation", meta = (WorldContext = "WorldContextObject", DisplayName = "GetRandomLocationInNavigableRadius", ScriptName = "GetRandomLocationInNavigableRadius"))
	static bool K2_GetRandomLocationInNavigableRadius(UObject* WorldContextObject, const FVector& Origin, FVector& RandomLocation, float Radius, ANavigationData* NavData = NULL, TSubclassOf<UNavigationQueryFilter> FilterClass = NULL);
	
	/** Potentially expensive. Use with caution. Consider using UPathFollowingComponent::GetRemainingPathCost instead */
	UFUNCTION(BlueprintPure, Category="AI|Navigation", meta=(WorldContext="WorldContextObject" ) )
	static ENavigationQueryResult::Type GetPathCost(UObject* WorldContextObject, const FVector& PathStart, const FVector& PathEnd, float& PathCost, ANavigationData* NavData = NULL, TSubclassOf<UNavigationQueryFilter> FilterClass = NULL);

	/** Potentially expensive. Use with caution */
	UFUNCTION(BlueprintPure, Category="AI|Navigation", meta=(WorldContext="WorldContextObject" ) )
	static ENavigationQueryResult::Type GetPathLength(UObject* WorldContextObject, const FVector& PathStart, const FVector& PathEnd, float& PathLength, ANavigationData* NavData = NULL, TSubclassOf<UNavigationQueryFilter> FilterClass = NULL);

	UFUNCTION(BlueprintPure, Category="AI|Navigation", meta=(WorldContext="WorldContextObject" ) )
	static bool IsNavigationBeingBuilt(UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "AI|Navigation", meta = (WorldContext = "WorldContextObject"))
	static bool IsNavigationBeingBuiltOrLocked(UObject* WorldContextObject);

	/** Finds path instantly, in a FindPath Synchronously. 
	 *	@param PathfindingContext could be one of following: NavigationData (like Navmesh actor), Pawn or Controller. This parameter determines parameters of specific pathfinding query */
	UFUNCTION(BlueprintCallable, Category = "AI|Navigation", meta = (WorldContext="WorldContextObject"))
	static UNavigationPath* FindPathToLocationSynchronously(UObject* WorldContextObject, const FVector& PathStart, const FVector& PathEnd, AActor* PathfindingContext = NULL, TSubclassOf<UNavigationQueryFilter> FilterClass = NULL);

	/** Finds path instantly, in a FindPath Synchronously. Main advantage over FindPathToLocationSynchronously is that 
	 *	the resulting path will automatically get updated if goal actor moves more than TetherDistance away from last path node
	 *	@param PathfindingContext could be one of following: NavigationData (like Navmesh actor), Pawn or Controller. This parameter determines parameters of specific pathfinding query */
	UFUNCTION(BlueprintCallable, Category = "AI|Navigation", meta = (WorldContext="WorldContextObject"))
	static UNavigationPath* FindPathToActorSynchronously(UObject* WorldContextObject, const FVector& PathStart, AActor* GoalActor, float TetherDistance = 50.f, AActor* PathfindingContext = NULL, TSubclassOf<UNavigationQueryFilter> FilterClass = NULL);

	/** Performs navigation raycast on NavigationData appropriate for given Querier.
	 *	@param Querier if not passed default navigation data will be used
	 *	@param HitLocation if line was obstructed this will be set to hit location. Otherwise it contains SegmentEnd
	 *	@return true if line from RayStart to RayEnd was obstructed. Also, true when no navigation data present */
	UFUNCTION(BlueprintCallable, Category="AI|Navigation", meta=(WorldContext="WorldContextObject" ))
	static bool NavigationRaycast(UObject* WorldContextObject, const FVector& RayStart, const FVector& RayEnd, FVector& HitLocation, TSubclassOf<UNavigationQueryFilter> FilterClass = NULL, AController* Querier = NULL);

	/** will limit the number of simultaneously running navmesh tile generation jobs to specified number.
	 *	@param MaxNumberOfJobs gets trimmed to be at least 1. You cannot use this function to pause navmesh generation */
	UFUNCTION(BlueprintCallable, Category = "AI|Navigation")
	void SetMaxSimultaneousTileGenerationJobsCount(int32 MaxNumberOfJobs);
	
	/** Brings limit of simultaneous navmesh tile generation jobs back to Project Setting's default value */
	UFUNCTION(BlueprintCallable, Category = "AI|Navigation")
	void ResetMaxSimultaneousTileGenerationJobsCount();

	/** Registers given actor as a "navigation enforcer" which means navigation system will
	 *	make sure navigation is being generated in specified radius around it.
	 *	@note: you need NavigationSystem's GenerateNavigationOnlyAroundNavigationInvokers to be set to true
	 *		to take advantage of this feature
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Navigation")
	void RegisterNavigationInvoker(AActor* Invoker, float TileGenerationRadius = 3000, float TileRemovalRadius = 5000);

	/** Removes given actor from the list of active navigation enforcers.
	 *	@see RegisterNavigationInvoker for more details */
	UFUNCTION(BlueprintCallable, Category = "AI|Navigation")
	void UnregisterNavigationInvoker(AActor* Invoker);

	UFUNCTION(BlueprintCallable, Category = "AI|Navigation|Generation")
	void SetGeometryGatheringMode(ENavDataGatheringModeConfig NewMode);

	UFUNCTION(BlueprintCallable, Category = "AI|Navigation", meta=(DisplayName="ReplaceAreaInOctreeData"))
	bool K2_ReplaceAreaInOctreeData(const UObject* Object, TSubclassOf<UNavArea> OldArea, TSubclassOf<UNavArea> NewArea);

	FORCEINLINE bool IsActiveTilesGenerationEnabled() const{ return bGenerateNavigationOnlyAroundNavigationInvokers; }
	
	/** delegate type for events that dirty the navigation data ( Params: const FBox& DirtyBounds ) */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnNavigationDirty, const FBox&);
	/** called after navigation influencing event takes place*/
	static FOnNavigationDirty NavigationDirtyEvent;

	enum ERegistrationResult
	{
		RegistrationError,
		RegistrationFailed_DataPendingKill,			// means navigation data being registered is marked as pending kill
		RegistrationFailed_AgentAlreadySupported,	// this means that navigation agent supported by given nav data is already handled by some other, previously registered instance
		RegistrationFailed_AgentNotValid,			// given instance contains navmesh that doesn't support any of expected agent types, or instance doesn't specify any agent
		RegistrationFailed_NotSuitable,				// given instance had been considered unsuitable by current navigation system instance itself. NOTE: this value is not currently being used by the engine-supplied navigation system classes
		RegistrationSuccessful,
	};

	// EOctreeUpdateMode is deprecated. Use FNavigationOctreeController::EOctreeUpdateMode instead
	enum EOctreeUpdateMode
	{
		OctreeUpdate_Default = 0,						// regular update, mark dirty areas depending on exported content
		OctreeUpdate_Geometry = 1,						// full update, mark dirty areas for geometry rebuild
		OctreeUpdate_Modifiers = 2,						// quick update, mark dirty areas for modifier rebuild
		OctreeUpdate_Refresh = 4,						// update is used for refresh, don't invalidate pending queue
		OctreeUpdate_ParentChain = 8,					// update child nodes, don't remove anything
	};

	//~ Begin UObject Interface
	virtual void PostInitProperties() override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End UObject Interface

	virtual void Tick(float DeltaSeconds) override;	

	UWorld* GetWorld() const override { return GetOuterUWorld(); }

	UCrowdManagerBase* GetCrowdManager() const { return CrowdManager.Get(); }

protected:
	void CalcTimeSlicedUpdateData(TArray<double>& OutCurrentTimeSlicedBuildTaskDurations, TArray<bool>& OutIsTimeSlicingArray, bool& bOutAnyNonTimeSlicedGenerators, int32& OutNumTimeSlicedRemainingBuildTasks);

	/** spawn new crowd manager */
	virtual void CreateCrowdManager();

	/** Used to properly set navigation class for indicated agent and propagate information to other places
	 *	(like project settings) that may need this information 
	 */
	void SetSupportedAgentsNavigationClass(int32 AgentIndex, TSubclassOf<ANavigationData> NavigationDataClass);

public:
	//----------------------------------------------------------------------//
	//~ Begin Public querying Interface                                                                
	//----------------------------------------------------------------------//
	/** 
	 *	Synchronously looks for a path from @fLocation to @EndLocation for agent with properties @AgentProperties. NavData actor appropriate for specified 
	 *	FNavAgentProperties will be found automatically
	 *	@param ResultPath results are put here
	 *	@param NavData optional navigation data that will be used instead of the one that would be deducted from AgentProperties
	 *  @param Mode switch between normal and hierarchical path finding algorithms
	 */
	FPathFindingResult FindPathSync(const FNavAgentProperties& AgentProperties, FPathFindingQuery Query, EPathFindingMode::Type Mode = EPathFindingMode::Regular);

	/** 
	 *	Does a simple path finding from @StartLocation to @EndLocation on specified NavData. If none passed MainNavData will be used
	 *	Result gets placed in ResultPath
	 *	@param NavData optional navigation data that will be used instead main navigation data
	 *  @param Mode switch between normal and hierarchical path finding algorithms
	 */
	FPathFindingResult FindPathSync(FPathFindingQuery Query, EPathFindingMode::Type Mode = EPathFindingMode::Regular);

	/** 
	 *	Asynchronously looks for a path from @StartLocation to @EndLocation for agent with properties @AgentProperties. NavData actor appropriate for specified 
	 *	FNavAgentProperties will be found automatically
	 *	@param ResultDelegate delegate that will be called once query has been processed and finished. Will be called even if query fails - in such case see comments for delegate's params
	 *	@param NavData optional navigation data that will be used instead of the one that would be deducted from AgentProperties
	 *	@param PathToFill if points to an actual navigation path instance than this instance will be filled with resulting path. Otherwise a new instance will be created and 
	 *		used in call to ResultDelegate
	 *  @param Mode switch between normal and hierarchical path finding algorithms
	 *	@return request ID
	 */
	uint32 FindPathAsync(const FNavAgentProperties& AgentProperties, FPathFindingQuery Query, const FNavPathQueryDelegate& ResultDelegate, EPathFindingMode::Type Mode = EPathFindingMode::Regular);

	/** Removes query indicated by given ID from queue of path finding requests to process. */
	void AbortAsyncFindPathRequest(uint32 AsynPathQueryID);
	
	/** 
	 *	Synchronously check if path between two points exists
	 *  Does not return path object, but will run faster (especially in hierarchical mode)
	 *  @param Mode switch between normal and hierarchical path finding algorithms. @note Hierarchical mode ignores QueryFilter
	 *	@return true if path exists
	 */
	bool TestPathSync(FPathFindingQuery Query, EPathFindingMode::Type Mode = EPathFindingMode::Regular, int32* NumVisitedNodes = NULL) const;

	/** Finds random point in navigable space
	 *	@param ResultLocation Found point is put here
	 *	@param NavData If NavData == NULL then MainNavData is used.
	 *	@return true if any location found, false otherwise */
	bool GetRandomPoint(FNavLocation& ResultLocation, ANavigationData* NavData = NULL, FSharedConstNavQueryFilter QueryFilter = NULL);

	/** Finds random, reachable point in navigable space restricted to Radius around Origin
	 *	@param ResultLocation Found point is put here
	 *	@param NavData If NavData == NULL then MainNavData is used.
	 *	@return true if any location found, false otherwise */
	bool GetRandomReachablePointInRadius(const FVector& Origin, float Radius, FNavLocation& ResultLocation, ANavigationData* NavData = NULL, FSharedConstNavQueryFilter QueryFilter = NULL) const;

	/** Finds random, point in navigable space restricted to Radius around Origin. Resulting location is not tested for reachability from the Origin
	 *	@param ResultLocation Found point is put here
	 *	@param NavData If NavData == NULL then MainNavData is used.
	 *	@return true if any location found, false otherwise */
	bool GetRandomPointInNavigableRadius(const FVector& Origin, float Radius, FNavLocation& ResultLocation, ANavigationData* NavData = NULL, FSharedConstNavQueryFilter QueryFilter = NULL) const;
	
	/** Calculates a path from PathStart to PathEnd and retrieves its cost. 
	 *	@NOTE potentially expensive, so use it with caution */
	ENavigationQueryResult::Type GetPathCost(const FVector& PathStart, const FVector& PathEnd, float& PathCost, const ANavigationData* NavData = NULL, FSharedConstNavQueryFilter QueryFilter = NULL) const;

	/** Calculates a path from PathStart to PathEnd and retrieves its overestimated length.
	 *	@NOTE potentially expensive, so use it with caution */
	ENavigationQueryResult::Type GetPathLength(const FVector& PathStart, const FVector& PathEnd, float& PathLength, const ANavigationData* NavData = NULL, FSharedConstNavQueryFilter QueryFilter = NULL) const;

	/** Calculates a path from PathStart to PathEnd and retrieves its overestimated length and cost.
	 *	@NOTE potentially expensive, so use it with caution */
	ENavigationQueryResult::Type GetPathLengthAndCost(const FVector& PathStart, const FVector& PathEnd, float& PathLength, float& PathCost, const ANavigationData* NavData = NULL, FSharedConstNavQueryFilter QueryFilter = NULL) const;

	// @todo document
	bool ProjectPointToNavigation(const FVector& Point, FNavLocation& OutLocation, const FVector& Extent = INVALID_NAVEXTENT, const FNavAgentProperties* AgentProperties = NULL, FSharedConstNavQueryFilter QueryFilter = NULL)
	{
		return ProjectPointToNavigation(Point, OutLocation, Extent, AgentProperties != NULL ? GetNavDataForProps(*AgentProperties, Point) : GetDefaultNavDataInstance(FNavigationSystem::DontCreate), QueryFilter);
	}

	// @todo document
	bool ProjectPointToNavigation(const FVector& Point, FNavLocation& OutLocation, const FVector& Extent = INVALID_NAVEXTENT, const ANavigationData* NavData = NULL, FSharedConstNavQueryFilter QueryFilter = NULL) const;

	/** 
	 * Looks for NavData generated for specified movement properties and returns it. NULL if not found;
	 */
	virtual ANavigationData* GetNavDataForProps(const FNavAgentProperties& AgentProperties);

	/** 
	 * Looks for NavData generated for specified movement properties and returns it. NULL if not found; Const version.
	 */
	virtual const ANavigationData* GetNavDataForProps(const FNavAgentProperties& AgentProperties) const;

	/** Goes through all registered NavigationData instances and retrieves the one 
	 *	supporting agent named AgentName */
	virtual ANavigationData* GetNavDataForAgentName(const FName AgentName) const;

	/**
	 * Looks up NavData appropriate for specified movement properties and returns it. NULL if not found;
	 * This is the encouraged way of querying for the appropriate NavData. It makes no difference for NavigationSystemV1
	 *	(AgentLocation and Extent parameters not being used) but NaV2 will take advantage of it and all engine-level
	 * AI navigation code is going to use call this flavor.
	 */
	virtual ANavigationData* GetNavDataForProps(const FNavAgentProperties& AgentProperties, const FVector& AgentLocation, const FVector& Extent = INVALID_NAVEXTENT) const;

	/** Returns the world default navigation data instance. Creates one if it doesn't exist. */
	ANavigationData* GetDefaultNavDataInstance(FNavigationSystem::ECreateIfMissing CreateNewIfNoneFound);
	/** Returns the world default navigation data instance. */
	virtual INavigationDataInterface* GetMainNavData() const override { return Cast<INavigationDataInterface>(GetDefaultNavDataInstance()); }
	ANavigationData& GetMainNavDataChecked() const { check(MainNavData); return *MainNavData; }

	/** Set limiting bounds to be used when building navigation data. */
	virtual void SetBuildBounds(const FBox& Bounds) override;

	virtual FBox GetNavigableWorldBounds() const override;

	virtual bool ContainsNavData(const FBox& Bounds) const override;
	virtual FBox ComputeNavDataBounds() const override;
	virtual void AddNavigationDataChunk(class ANavigationDataChunkActor& DataChunkActor) override;
	virtual void RemoveNavigationDataChunk(class ANavigationDataChunkActor& DataChunkActor) override;
	virtual void FillNavigationDataChunkActor(const FBox& QueryBounds, class ANavigationDataChunkActor& DataChunkActor, FBox& OutTilesBounds) override;

	ANavigationData* GetDefaultNavDataInstance() const { return MainNavData; }

	ANavigationData* GetAbstractNavData() const { return AbstractNavData; }

	/** constructs a navigation data instance of specified NavDataClass, in passed Level
	 *	for supplied NavConfig. If Level == null and bSpawnNavDataInNavBoundsLevel == true
	 *	then the first volume actor in RegisteredNavBounds will be used to source the level. 
	 *	Otherwise the navdata instance will be spawned in NavigationSystem's world */
	virtual ANavigationData* CreateNavigationDataInstanceInLevel(const FNavDataConfig& NavConfig, ULevel* SpawnLevel);

	FSharedNavQueryFilter CreateDefaultQueryFilterCopy() const;

	/** Super-hacky safety feature for threaded navmesh building. Will be gone once figure out why keeping TSharedPointer to Navigation Generator doesn't 
	 *	guarantee its existence */
	bool ShouldGeneratorRun(const FNavDataGenerator* Generator) const;

	virtual bool IsNavigationBuilt(const AWorldSettings* Settings) const override;

	virtual bool IsThereAnywhereToBuildNavigation() const;

	bool ShouldGenerateNavigationEverywhere() const { return bWholeWorldNavigable; }

	bool ShouldAllowClientSideNavigation() const { return bAllowClientSideNavigation; }
	virtual bool ShouldLoadNavigationOnClient(ANavigationData* NavData = nullptr) const { return bAllowClientSideNavigation; }
	virtual bool ShouldDiscardSubLevelNavData(ANavigationData* NavData = nullptr) const { return bShouldDiscardSubLevelNavData; }

	FBox GetWorldBounds() const;
	
	FBox GetLevelBounds(ULevel* InLevel) const;

	bool IsNavigationRelevant(const AActor* TestActor) const;

	const TSet<FNavigationBounds>& GetNavigationBounds() const;

	static const FNavDataConfig& GetDefaultSupportedAgent();
	static const FNavDataConfig& GetBiggestSupportedAgent(const UWorld* World);
	
#if WITH_EDITOR
	static double GetWorldPartitionNavigationDataBuilderOverlap(const UWorld& World);
#endif
	
	const FNavDataConfig& GetDefaultSupportedAgentConfig() const;
	FORCEINLINE const TArray<FNavDataConfig>& GetSupportedAgents() const { return SupportedAgents; }
	void OverrideSupportedAgents(const TArray<FNavDataConfig>& NewSupportedAgents);
	void SetSupportedAgentsMask(const FNavAgentSelector& InSupportedAgentsMask);
	FNavAgentSelector GetSupportedAgentsMask() const { return SupportedAgentsMask; }

	virtual void ApplyWorldOffset(const FVector& InOffset, bool bWorldShift) override;

	/** checks if navigation/navmesh is dirty and needs to be rebuilt */
	bool IsNavigationDirty() const;

	/** checks if dirty navigation data can rebuild itself */
	bool CanRebuildDirtyNavigation() const;

	FORCEINLINE bool SupportsNavigationGeneration() const { return bSupportRebuilding; }

	static bool DoesPathIntersectBox(const FNavigationPath* Path, const FBox& Box, uint32 StartingIndex = 0, FVector* AgentExtent = NULL);
	static bool DoesPathIntersectBox(const FNavigationPath* Path, const FBox& Box, const FVector& AgentLocation, uint32 StartingIndex = 0, FVector* AgentExtent = NULL);
	
	//----------------------------------------------------------------------//
	// Active tiles
	//----------------------------------------------------------------------//
	virtual void RegisterInvoker(AActor& Invoker, float TileGenerationRadius, float TileRemovalRadius);
	virtual void UnregisterInvoker(AActor& Invoker);

	static void RegisterNavigationInvoker(AActor& Invoker, float TileGenerationRadius, float TileRemovalRadius);
	static void UnregisterNavigationInvoker(AActor& Invoker);

	const TArray<FNavigationInvokerRaw>& GetInvokerLocations() const { return InvokerLocations; }

	//----------------------------------------------------------------------//
	// Bookkeeping 
	//----------------------------------------------------------------------//
	
	// @todo document
	virtual void UnregisterNavData(ANavigationData* NavData);

	/** Traverses SupportedAgents and for all agents not supported (i.e. filtered
	 *	out by SupportedAgentsMask) checks if there's a currently registered
	 *	NavigationData instance for that agent, and if so it unregisters that agent */
	virtual void UnregisterUnusedNavData();

	/** Adds NavData to registration candidates queue - NavDataRegistrationQueue*/
	virtual void RequestRegistrationDeferred(ANavigationData& NavData);

protected:
	void ApplySupportedAgentsFilter();

	/** Processes all NavigationData instances in UWorld owning navigation system instance, and registers
	 *	all previously unregistered */
	void RegisterNavigationDataInstances();

	/** called in places where we need to spawn the NavOctree, but is checking additional conditions if we really want to do that
	 *	depending on navigation data setup among others 
	 *	@return true if NavOctree instance has been created, or if one is already present */
	virtual bool ConditionalPopulateNavOctree();

	/** called to instantiate NavigationSystem's NavOctree instance */
	virtual void ConstructNavOctree();

	/** Processes registration of candidates queues via RequestRegistration and stored in NavDataRegistrationQueue */
	virtual void ProcessRegistrationCandidates();

	/** registers CustomLinks awaiting registration in PendingCustomLinkRegistration */
	void ProcessCustomLinkPendingRegistration();

	/** used to apply updates of nav volumes in navigation system's tick */
	void PerformNavigationBoundsUpdate(const TArray<FNavigationBoundsUpdateRequest>& UpdateRequests);
	
	/** adds data to RegisteredNavBounds */
	void AddNavigationBounds(const FNavigationBounds& NewBounds);

	/** Searches for all valid navigation bounds in the world and stores them */
	virtual void GatherNavigationBounds();

	/** @return pointer to ANavigationData instance of given ID, or NULL if it was not found. Note it looks only through registered navigation data */
	ANavigationData* GetNavDataWithID(const uint16 NavDataID) const;

	static void RegisterComponentToNavOctree(UActorComponent* Comp);
	static void UnregisterComponentToNavOctree(UActorComponent* Comp);

public:
	virtual void ReleaseInitialBuildingLock();

	//----------------------------------------------------------------------//
	// navigation octree related functions
	//----------------------------------------------------------------------//
	static void OnComponentRegistered(UActorComponent* Comp);
	static void OnComponentUnregistered(UActorComponent* Comp);
	static void RegisterComponent(UActorComponent* Comp);
	static void UnregisterComponent(UActorComponent* Comp);
	static void OnActorRegistered(AActor* Actor);
	static void OnActorUnregistered(AActor* Actor);

	/** update navoctree entry for specified actor/component */
	static void UpdateActorInNavOctree(AActor& Actor);
	static void UpdateComponentInNavOctree(UActorComponent& Comp);
	/** update all navoctree entries for actor and its components */
	static void UpdateActorAndComponentsInNavOctree(AActor& Actor, bool bUpdateAttachedActors = true);
	/** update all navoctree entries for actor and its non scene components after root movement */
	static void UpdateNavOctreeAfterMove(USceneComponent* Comp);

protected:
	/** A helper function that gathers all actors attached to RootActor and fetches 
	 *	them back. The function does consider multi-level attachments. 
	 *	@param AttachedActors is getting reset at the beginning of the function. 
	 *		When done it is guaranteed to contain unique, non-null ptrs. It 
	 *		will not include RootActor.
	 *	@return the number of unique attached actors */
	static int32 GetAllAttachedActors(const AActor& RootActor, TArray<AActor*>& OutAttachedActors);

	/** updates navoctree information on actors attached to RootActor */
	static void UpdateAttachedActorsInNavOctree(AActor& RootActor);

public:
	/** removes all navoctree entries for actor and its components */
	static void ClearNavOctreeAll(AActor* Actor);
	/** updates bounds of all components implementing INavRelevantInterface */
	static void UpdateNavOctreeBounds(AActor* Actor);

	void AddDirtyArea(const FBox& NewArea, int32 Flags, const FName& DebugReason = NAME_None);
	void AddDirtyArea(const FBox& NewArea, int32 Flags, const TFunction<UObject*()>& ObjectProviderFunc, const FName& DebugReason = NAME_None);
	void AddDirtyAreas(const TArray<FBox>& NewAreas, int32 Flags, const FName& DebugReason = NAME_None);
	bool HasDirtyAreasQueued() const;
	int32 GetNumDirtyAreas() const;

	const FNavigationOctree* GetNavOctree() const { return DefaultOctreeController.GetOctree(); }
	FNavigationOctree* GetMutableNavOctree() { return DefaultOctreeController.GetMutableOctree(); }

	FORCEINLINE static uint32 HashObject(const UObject& Object)
	{
		return FNavigationOctree::HashObject(Object);
	}
	FORCEINLINE const FOctreeElementId2* GetObjectsNavOctreeId(const UObject& Object) const { return DefaultOctreeController.GetObjectsNavOctreeId(Object); }
	FORCEINLINE bool HasPendingObjectNavOctreeId(UObject* Object) const { return Object && DefaultOctreeController.HasPendingObjectNavOctreeId(*Object); }
	FORCEINLINE void RemoveObjectsNavOctreeId(const UObject& Object) { DefaultOctreeController.RemoveObjectsNavOctreeId(Object); }

	void RemoveNavOctreeElementId(const FOctreeElementId2& ElementId, int32 UpdateFlags);

	const FNavigationRelevantData* GetDataForObject(const UObject& Object) const;
	FNavigationRelevantData* GetMutableDataForObject(const UObject& Object);

	/** find all elements in navigation octree within given box (intersection) */
	void FindElementsInNavOctree(const FBox& QueryBox, const FNavigationOctreeFilter& Filter, TArray<FNavigationOctreeElement>& Elements);

	/** update single element in navoctree */
	void UpdateNavOctreeElement(UObject* ElementOwner, INavRelevantInterface* ElementInterface, int32 UpdateFlags);

	/** force updating parent node and all its children */
	void UpdateNavOctreeParentChain(UObject* ElementOwner, bool bSkipElementOwnerUpdate = false);

	/** update component bounds in navigation octree and mark only specified area as dirty, doesn't re-export component geometry */
	bool UpdateNavOctreeElementBounds(UActorComponent* Comp, const FBox& NewBounds, const FBox& DirtyArea);

	/** fetched Object's data from the octree and replaces occurences of OldArea with NewArea */
	bool ReplaceAreaInOctreeData(const UObject& Object, TSubclassOf<UNavArea> OldArea, TSubclassOf<UNavArea> NewArea, bool bReplaceChildClasses = false);

	//----------------------------------------------------------------------//
	// Custom navigation links
	//----------------------------------------------------------------------//
	virtual void RegisterCustomLink(INavLinkCustomInterface& CustomLink);
	void UnregisterCustomLink(INavLinkCustomInterface& CustomLink);
	
	static void RequestCustomLinkRegistering(INavLinkCustomInterface& CustomLink, UObject* OwnerOb);
	static void RequestCustomLinkUnregistering(INavLinkCustomInterface& CustomLink, UObject* ObjectOb);

	/** find custom link by unique ID */
	INavLinkCustomInterface* GetCustomLink(uint32 UniqueLinkId) const;

	/** updates custom link for all active navigation data instances */
	void UpdateCustomLink(const INavLinkCustomInterface* CustomLink);

	/** Return a Bounding Box containing the navlink points */
	static FBox ComputeCustomLinkBounds(const INavLinkCustomInterface& CustomLink);

	//----------------------------------------------------------------------//
	// Areas
	//----------------------------------------------------------------------//
	static void RequestAreaRegistering(UClass* NavAreaClass);
	static void RequestAreaUnregistering(UClass* NavAreaClass);

	/** find index in SupportedAgents array for given navigation data */
	int32 GetSupportedAgentIndex(const ANavigationData* NavData) const;

	/** find index in SupportedAgents array for agent type */
	int32 GetSupportedAgentIndex(const FNavAgentProperties& NavAgent) const;

	//----------------------------------------------------------------------//
	// Filters
	//----------------------------------------------------------------------//
	
	/** prepare descriptions of navigation flags in UNavigationQueryFilter class: using enum */
	void DescribeFilterFlags(UEnum* FlagsEnum) const;

	/** prepare descriptions of navigation flags in UNavigationQueryFilter class: using array */
	void DescribeFilterFlags(const TArray<FString>& FlagsDesc) const;

	/** removes cached filters from currently registered navigation data */
	void ResetCachedFilter(TSubclassOf<UNavigationQueryFilter> FilterClass);

	//----------------------------------------------------------------------//
	// building
	//----------------------------------------------------------------------//
	
	/** Triggers navigation building on all eligible navigation data. */
	virtual void Build();

	/** Cancels all currently running navigation builds */
	virtual void CancelBuild();

	// @todo document
	void OnPIEStart();
	// @todo document
	void OnPIEEnd();
	
	// @todo document
	FORCEINLINE bool IsNavigationBuildingLocked(uint8 Flags = ~0) const { return (NavBuildingLockFlags & Flags) != 0; }

	/** check if building is permanently locked to avoid showing navmesh building notify (due to queued dirty areas) */
	FORCEINLINE bool IsNavigationBuildingPermanentlyLocked() const
	{
		return (NavBuildingLockFlags & ~ENavigationBuildLock::InitialLock) != 0; 
	}

	/** check if navigation octree updates are currently ignored */
	FORCEINLINE bool IsNavigationOctreeLocked() const { return DefaultOctreeController.IsNavigationOctreeLocked(); }

	// @todo document
	UFUNCTION(BlueprintCallable, Category = "AI|Navigation")
	void OnNavigationBoundsUpdated(ANavMeshBoundsVolume* NavVolume);
	virtual void OnNavigationBoundsAdded(ANavMeshBoundsVolume* NavVolume);
	virtual void OnNavigationBoundsRemoved(ANavMeshBoundsVolume* NavVolume);

	/** determines whether any generator is performing navigation building actions at the moment */
	UE_DEPRECATED(4.26, "This function is deprecated.  Please use IsNavigationBuildInProgress")
	bool IsNavigationBuildInProgress(bool bCheckDirtyToo);

	/** determines whether any generator is performing navigation building actions at the moment, dirty areas are also checked */
	bool IsNavigationBuildInProgress();
	
	virtual void OnNavigationGenerationFinished(ANavigationData& NavData);

	/** Used to display "navigation building in progress" counter */
	int32 GetNumRemainingBuildTasks() const;

	/** Number of currently running tasks */
	int32 GetNumRunningBuildTasks() const;

protected:
	/** Sets up SuportedAgents and NavigationDataCreators. Override it to add additional setup, but make sure to call Super implementation */
	virtual void DoInitialSetup();

	/** Find or create abstract nav data */
	virtual void UpdateAbstractNavData();
	
	/** Called during ConditionalPopulateNavOctree and gives subclassess a chance
	 *	to influence what gets added */
	virtual void AddLevelToOctree(ULevel& Level);

	/** Called as part of UWorld::BeginTearingDown */
	virtual void OnBeginTearingDown(UWorld* World);
	
public:
	/** Called upon UWorld destruction to release what needs to be released */
	virtual void CleanUp(const FNavigationSystem::ECleanupMode Mode = FNavigationSystem::ECleanupMode::CleanupUnsafe) override;

	/** 
	 *	Called when owner-UWorld initializes actors
	 */
	virtual void OnInitializeActors() override;

	/** */
	virtual void OnWorldInitDone(FNavigationSystemRunMode Mode);

	/** Returns true if world has been initialized. */
	FORCEINLINE virtual bool IsWorldInitDone() const override { return bWorldInitDone; }
	FORCEINLINE bool IsInitialized() const { return bWorldInitDone; }

	FORCEINLINE FNavigationSystemRunMode GetRunMode() const { return OperationMode; }

	/** adds BSP collisions of currently streamed in levels to octree */
	void InitializeLevelCollisions();

	enum class ELockRemovalRebuildAction
	{
		Rebuild,
		RebuildIfNotInEditor,
		NoRebuild
	};
	void AddNavigationBuildLock(uint8 Flags);
	void RemoveNavigationBuildLock(uint8 Flags, const ELockRemovalRebuildAction RebuildAction = ELockRemovalRebuildAction::Rebuild);

	void SetNavigationOctreeLock(bool bLock);

	/** checks if auto-rebuilding navigation data is enabled. Defaults to bNavigationAutoUpdateEnabled
	*	value, but can be overridden per nav sys instance */
	virtual bool GetIsAutoUpdateEnabled() const { return bNavigationAutoUpdateEnabled; }

#if WITH_EDITOR
	/** allow editor to toggle whether seamless navigation building is enabled */
	static void SetNavigationAutoUpdateEnabled(bool bNewEnable, UNavigationSystemBase* InNavigationSystem);

	FORCEINLINE bool IsNavigationRegisterLocked() const { return NavUpdateLockFlags != 0; }
	FORCEINLINE bool IsNavigationUnregisterLocked() const { return NavUpdateLockFlags && !(NavUpdateLockFlags & ENavigationLockReason::AllowUnregister); }
	FORCEINLINE bool IsNavigationUpdateLocked() const { return IsNavigationRegisterLocked(); }
	FORCEINLINE void AddNavigationUpdateLock(uint8 Flags) { NavUpdateLockFlags |= Flags; }
	FORCEINLINE void RemoveNavigationUpdateLock(uint8 Flags) { NavUpdateLockFlags &= ~Flags; }

	void UpdateLevelCollision(ULevel* InLevel);

#endif // WITH_EDITOR

	FORCEINLINE bool IsSetUpForLazyGeometryExporting() const { return bGenerateNavigationOnlyAroundNavigationInvokers; }

	static UNavigationSystemV1* CreateNavigationSystem(UWorld* WorldOwner);

	static UNavigationSystemV1* GetCurrent(UWorld* World);
	static UNavigationSystemV1* GetCurrent(UObject* WorldContextObject);
	
	virtual void InitializeForWorld(UWorld& World, FNavigationSystemRunMode Mode) override;

	// Fetch the array of all nav-agent properties.
	void GetNavAgentPropertiesArray(TArray<FNavAgentProperties>& OutNavAgentProperties) const;

	static FORCEINLINE bool ShouldUpdateNavOctreeOnComponentChange()
	{
		return (bUpdateNavOctreeOnComponentChange && !bStaticRuntimeNavigation)
#if WITH_EDITOR
			|| (GIsEditor && !GIsPlayInEditorWorld)
#endif
			;
	}

	static FORCEINLINE bool IsNavigationSystemStatic()
	{
		return bStaticRuntimeNavigation
#if WITH_EDITOR
			&& !(GIsEditor && !GIsPlayInEditorWorld)
#endif
			;
	}

	/**	call with bEnableStatic == true to signal the NavigationSystem it doesn't need to store
	 *	any navigation-generation-related data at game runtime, because 
	 *	nothing is going to use it anyway. This will short-circuit all code related 
	 *	to navmesh rebuilding, so use it only if you have fully static navigation in 
	 *	your game. bEnableStatic = false will reset the mechanism.
	 *	Note: this is not a runtime switch. It's highly advisable not to call it manually and 
	 *	use UNavigationSystemModuleConfig.bStrictlyStatic instead */
	static void ConfigureAsStatic(bool bEnableStatic = true);

	static void SetUpdateNavOctreeOnComponentChange(bool bNewUpdateOnComponentChange);

	/** 
	 * Exec command handlers
	 */
	bool HandleCycleNavDrawnCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleCountNavMemCommand();
	
	//----------------------------------------------------------------------//
	// debug
	//----------------------------------------------------------------------//
	void CycleNavigationDataDrawn();

	FNavRegenTimeSliceManager& GetMutableNavRegenTimeSliceManager() { return NavRegenTimeSliceManager; }

protected:

	UPROPERTY()
	FNavigationSystemRunMode OperationMode;

	/** Queued async pathfinding queries to process in the next update. */
	TArray<FAsyncPathFindingQuery> AsyncPathFindingQueries;

	/** Queued async pathfinding results computed by the dedicated task in the last frame and ready to dispatch in the next update. */
	TArray<FAsyncPathFindingQuery> AsyncPathFindingCompletedQueries;

	/** Graph event that the main thread will wait for to synchronize with the async pathfinding task, if any. */
	FGraphEventRef AsyncPathFindingTask;

	/** Flag used by main thread to ask the async pathfinding task to stop and postpone remaining queries, if any. */
	TAtomic<bool> bAbortAsyncQueriesRequested;

	FCriticalSection NavDataRegistration;

	TMap<FNavAgentProperties, TWeakObjectPtr<ANavigationData> > AgentToNavDataMap;
	
	FNavigationOctreeController DefaultOctreeController;

	/** Map of all custom navigation links, that are relevant for path following */
	TMap<uint32, FNavigationSystem::FCustomLinkOwnerInfo> CustomLinksMap;

	FNavigationDirtyAreasController DefaultDirtyAreasController;

	// async queries
	FCriticalSection NavDataRegistrationSection;
	static FCriticalSection CustomLinkRegistrationSection;
	
#if WITH_EDITOR
	uint8 NavUpdateLockFlags;
#endif
	uint8 NavBuildingLockFlags;

	/** set of locking flags applied on startup of navigation system */
	uint8 InitialNavBuildingLockFlags;
	
	uint8 bInitialSetupHasBeenPerformed : 1;
	uint8 bInitialLevelsAdded : 1;
	uint8 bWorldInitDone : 1;
	/** set when the NavSys instance has been cleaned up. This is an irreversible state */
	uint8 bCleanUpDone : 1;
	uint8 bAsyncBuildPaused : 1; // mz@todo remove, replaced by bIsPIEActive and IsGameWorld
protected:

	/** cached navigable world bounding box*/
	mutable FBox NavigableWorldBounds;

	/** indicates which of multiple navigation data instances to draw*/
	int32 CurrentlyDrawnNavDataIndex;
	
#if !UE_BUILD_SHIPPING
	/** self-registering exec command to handle nav sys console commands */
	static FNavigationSystemExec ExecHandler;
#endif // !UE_BUILD_SHIPPING
	
	/** whether seamless navigation building is enabled */
	static bool bNavigationAutoUpdateEnabled;
	static bool bUpdateNavOctreeOnComponentChange;
	static bool bStaticRuntimeNavigation;
	static bool bIsPIEActive;

	static TMap<INavLinkCustomInterface*, FWeakObjectPtr> PendingCustomLinkRegistration;
	TSet<const UClass*> NavAreaClasses;

	FNavRegenTimeSliceManager NavRegenTimeSliceManager;

	/** delegate handler for PostLoadMap event */
	void OnPostLoadMap(UWorld* LoadedWorld);
#if WITH_EDITOR
	/** delegate handler for ActorMoved events */
	void OnActorMoved(AActor* Actor);
#endif
	/** delegate handler called when navigation is dirtied*/
	void OnNavigationDirtied(const FBox& Bounds);

	FDelegateHandle ReloadCompleteDelegateHandle;

	/** called to notify NavigaitonSystem about finished reload */
	virtual void OnReloadComplete(EReloadCompleteReason Reason);

	/** Registers given navigation data with this Navigation System.
	 *	@return RegistrationSuccessful if registration was successful, other results mean it failed
	 *	@see ERegistrationResult
	 */
	virtual ERegistrationResult RegisterNavData(ANavigationData* NavData);

	/** tries to register navigation area */
	void RegisterNavAreaClass(UClass* NavAreaClass);

	/** tries to unregister navigation area */
	void UnregisterNavAreaClass(UClass* NavAreaClass);

	void OnNavigationAreaEvent(UClass* AreaClass, ENavAreaEvent::Type Event);
	
 	FSetElementId RegisterNavOctreeElement(UObject* ElementOwner, INavRelevantInterface* ElementInterface, int32 UpdateFlags);
	void UnregisterNavOctreeElement(UObject* ElementOwner, INavRelevantInterface* ElementInterface, int32 UpdateFlags);
	
	/** read element data from navigation octree */
	bool GetNavOctreeElementData(const UObject& NodeOwner, int32& DirtyFlags, FBox& DirtyBounds);
	//bool GetNavOctreeElementData(UObject* NodeOwner, int32& DirtyFlags, FBox& DirtyBounds);

	/** Adds given element to NavOctree. No check for owner's validity are performed, 
	 *	nor its presence in NavOctree - function assumes callee responsibility 
	 *	in this regard **/
	void AddElementToNavOctree(const FNavigationDirtyElement& DirtyElement);

	void SetCrowdManager(UCrowdManagerBase* NewCrowdManager);

	/** Add BSP collision data to navigation octree */
	void AddLevelCollisionToOctree(ULevel* Level);
	
	/** Remove BSP collision data from navigation octree */
	void RemoveLevelCollisionFromOctree(ULevel* Level);

	virtual void SpawnMissingNavigationData();

	/** 
	 * Fills a mask indicating which navigation data associated to the supported agent mask are already instantiated.
	 * @param OutInstantiatedMask The mask that will represent already instantiated navigation data.
	 * @param InLevel If specified will be used to search for navigation data; will use the owning world otherwise.
	 * @return Number of instantiated navigation data.
	 */
	uint8 FillInstantiatedDataMask(TBitArray<>& OutInstantiatedMask, ULevel* InLevel = nullptr);

	/**
	 * Spawns missing navigation data.
	 * @param InInstantiatedMask The mask representing already instantiated navigation data.
	 * @param InLevel Level in which the new data must be added. See CreateNavigationDataInstanceInLevel doc.
	 */
	void SpawnMissingNavigationDataInLevel(const TBitArray<>& InInstantiatedMask, ULevel* InLevel = nullptr);

public:
	void DemandLazyDataGathering(FNavigationRelevantData& ElementData);

protected:
	virtual void RebuildDirtyAreas(float DeltaSeconds);

	// adds navigation bounds update request to a pending list
	void AddNavigationBoundsUpdateRequest(const FNavigationBoundsUpdateRequest& UpdateRequest);

	/** Triggers navigation building on all eligible navigation data. */
	virtual void RebuildAll(bool bIsLoadTime = false);
		 
	/** Handler for FWorldDelegates::LevelAddedToWorld event */
	 void OnLevelAddedToWorld(ULevel* InLevel, UWorld* InWorld);
	 
	/** Handler for FWorldDelegates::LevelRemovedFromWorld event */
	void OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld);

	/** Handler for FWorldDelegates::OnWorldPostActorTick event */
	void OnWorldPostActorTick(UWorld* World, ELevelTick TickType, float DeltaTime) { PostponeAsyncQueries(); }

	/** Adds given request to requests queue. Note it's to be called only on game thread only */
	void AddAsyncQuery(const FAsyncPathFindingQuery& Query);
		 
	/** spawns a non-game-thread task to process requests given in PathFindingQueries.
	 *	In the process PathFindingQueries gets copied. */
	void TriggerAsyncQueries(TArray<FAsyncPathFindingQuery>& PathFindingQueries);

	/** Processes pathfinding requests given in PathFindingQueries.*/
	void PerformAsyncQueries(TArray<FAsyncPathFindingQuery> PathFindingQueries);

	/** Broadcasts completion delegate for all completed async pathfinding requests. */
	void DispatchAsyncQueriesResults(const TArray<FAsyncPathFindingQuery>& PathFindingQueries) const;

	/**
	 * Requests the async pathfinding task to abort and waits for it to complete
	 * before resuming the main thread. Pathfind task will postpone remaining queries to next frame.
	 */
	void PostponeAsyncQueries();

	/** */
	virtual void DestroyNavOctree();

	/** Whether Navigation system needs to populate nav octree. 
	 *  Depends on runtime generation settings of each navigation data, always true in the editor
	 */
	bool RequiresNavOctree() const;

	/** Return "Strongest" runtime generation type required by registered navigation data objects
	 *  Depends on runtime generation settings of each navigation data, always ERuntimeGenerationType::Dynamic in the editor world
	 */
	ERuntimeGenerationType GetRuntimeGenerationType() const;

	void LogNavDataRegistrationResult(ERegistrationResult);

	/** Whether Navigation System is allowed to rebuild the navmesh
	 *  Depends on runtime generation settings of each navigation data, always true in the editor
	 */
	bool IsAllowedToRebuild() const;
	
	//----------------------------------------------------------------------//
	// new stuff
	//----------------------------------------------------------------------//
public:
	void VerifyNavigationRenderingComponents(const bool bShow);
	/** @param if InLevel is given then only navigation bounds from that level will be considered*/
	virtual int GetNavigationBoundsForNavData(const ANavigationData& NavData, TArray<FBox>& OutBounds, ULevel* InLevel = nullptr) const;
	static INavigationDataInterface* GetNavDataForActor(const AActor& Actor);
	virtual void Configure(const UNavigationSystemConfig& Config) override;
	virtual void AppendConfig(const UNavigationSystemConfig& NewConfig) override;
	
#if !UE_BUILD_SHIPPING
	void GetOnScreenMessages(TMultiMap<FCoreDelegates::EOnScreenMessageSeverity, FText>& OutMessages);
#endif // !UE_BUILD_SHIPPING

	//----------------------------------------------------------------------//
	// DEPRECATED
	//----------------------------------------------------------------------//
public:
	// Note that this function was only deprecated for blueprint in 5.1
	UE_DEPRECATED(4.22, "This version is deprecated.  Please use GetRandomLocationInNavigableRadius instead")
	UFUNCTION(BlueprintPure, Category = "AI|Navigation", meta = (WorldContext = "WorldContextObject", DisplayName = "GetRandomPointInNavigableRadius", ScriptName = "GetRandomPointInNavigableRadius", DeprecatedFunction, DeprecationMessage = "GetRandomPointInNavigableRadius is deprecated. Use GetRandomLocationInNavigableRadius instead"))
	static bool K2_GetRandomPointInNavigableRadius(UObject* WorldContextObject, const FVector& Origin, FVector& RandomLocation, float Radius, ANavigationData* NavData = NULL, TSubclassOf<UNavigationQueryFilter> FilterClass = NULL);
	
	UE_DEPRECATED(4.26, "This version of RemoveNavigationBuildLock is deprecated. Please use the new version")
	void RemoveNavigationBuildLock(uint8 Flags, bool bSkipRebuildInEditor) { RemoveNavigationBuildLock(Flags, bSkipRebuildInEditor ? ELockRemovalRebuildAction::RebuildIfNotInEditor : ELockRemovalRebuildAction::Rebuild);}
};

//----------------------------------------------------------------------//
// UNavigationSystemModuleConfig 
//----------------------------------------------------------------------//
UCLASS()
class NAVIGATIONSYSTEM_API UNavigationSystemModuleConfig : public UNavigationSystemConfig
{
	GENERATED_BODY()

protected:
	/** Whether at game runtime we expect any kind of dynamic navigation generation */
	UPROPERTY(EditAnywhere, Category = Navigation)
	uint32 bStrictlyStatic : 1;

	UPROPERTY(EditAnywhere, Category = Navigation)
	uint32 bCreateOnClient : 1;

	UPROPERTY(EditAnywhere, Category = Navigation)
	uint32 bAutoSpawnMissingNavData : 1;

	UPROPERTY(EditAnywhere, Category = Navigation)
	uint32 bSpawnNavDataInNavBoundsLevel : 1;

public:
	UNavigationSystemModuleConfig(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PostInitProperties() override;

	virtual UNavigationSystemBase* CreateAndConfigureNavigationSystem(UWorld& World) const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

protected:
#if WITH_EDITOR
	friend UNavigationSystemV1;
#endif // WITH_EDITOR
	void UpdateWithNavSysCDO(const UNavigationSystemV1& NavSysCDO);
};

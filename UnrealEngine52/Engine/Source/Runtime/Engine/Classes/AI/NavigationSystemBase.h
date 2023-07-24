// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "AI/Navigation/NavigationTypes.h"
#include "AI/NavigationModifier.h"
#include "Engine/World.h"
#include "NavigationSystemBase.generated.h"

class UNavigationSystemBase;
class UNavigationSystemConfig;
class AActor;
class UActorComponent;
class USceneComponent;
class INavigationDataInterface;
class IPathFollowingAgentInterface;
class AWorldSettings;
class ULevel;
class AController;
class UNavAreaBase;

ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogNavigation, Warning, All);
ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogNavigationDataBuild, Log, All);
ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogNavLink, Warning, All);
ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogAStar, Display, All);

UENUM()
enum class FNavigationSystemRunMode : uint8
{
	InvalidMode,
	GameMode,
	EditorMode,
	SimulationMode,
	PIEMode,
	InferFromWorldMode,
	EditorWorldPartitionBuildMode
};

namespace ENavigationLockReason
{
	enum Type
	{
		Unknown = 1 << 0,
		AllowUnregister = 1 << 1,

		MaterialUpdate = 1 << 2,
		LightingUpdate = 1 << 3,
		ContinuousEditorMove = 1 << 4,
		SpawnOnDragEnter = 1 << 5,
	};
}

class ENGINE_API FNavigationLockContext
{
public:
	FNavigationLockContext(ENavigationLockReason::Type Reason = ENavigationLockReason::Unknown, bool bApplyLock = true)
		: MyWorld(NULL), LockReason((uint8)Reason), bSingleWorld(false), bIsLocked(false)
	{
		if (bApplyLock)
		{
			LockUpdates();
		}
	}

	FNavigationLockContext(UWorld* InWorld, ENavigationLockReason::Type Reason = ENavigationLockReason::Unknown, bool bApplyLock = true)
		: MyWorld(InWorld), LockReason((uint8)Reason), bSingleWorld(true), bIsLocked(false)
	{
		if (bApplyLock)
		{
			LockUpdates();
		}
	}

	~FNavigationLockContext()
	{
		UnlockUpdates();
	}

private:
	UWorld* MyWorld;
	uint8 LockReason;
	uint8 bSingleWorld : 1;
	uint8 bIsLocked : 1;

	void LockUpdates();
	void UnlockUpdates();
};

namespace FNavigationSystem
{
	/** Creates an instance of NavigationSystem (class being specified by WorldSetting's NavigationSystemConfig)
	 *	A new instance will be created only if given WorldOwner doesn't have one yet.
	 *	The new instance will be assigned to the given WorldOwner (via SetNavigationSystem 
	 *	call) and depending on value of bInitializeForWorld the InitializeForWorld 
	 *	function will be called on the new NavigationSystem instance.
	 *	(@see UWorld.NavigationSystem)
	 *	@param RunMode if set to a valid value (other than FNavigationSystemRunMode::InvalidMode) 
	 *		will also configure the created NavigationSystem instance for that mode
	 *	@param NavigationSystemConfig is used to pick the navigation system's class and set it up. If null
	 *		then WorldOwner.WorldSettings.NavigationSystemConfig will be used
	 */
	ENGINE_API void AddNavigationSystemToWorld(UWorld& WorldOwner, const FNavigationSystemRunMode RunMode = FNavigationSystemRunMode::InvalidMode, UNavigationSystemConfig* NavigationSystemConfig = nullptr, const bool bInitializeForWorld = true, const bool bOverridePreviousNavSys = false);

	/** Discards all navigation data chunks in all sub-levels */
	ENGINE_API void DiscardNavigationDataChunks(UWorld& InWorld);

	ENGINE_API bool IsEditorRunMode(const FNavigationSystemRunMode Mode);
	
	template<typename TNavSys>
	FORCEINLINE TNavSys* GetCurrent(UWorld* World)
	{
		return World ? Cast<TNavSys>(World->GetNavigationSystem()) : (TNavSys*)nullptr;
	}

	template<typename TNavSys>
	FORCEINLINE const TNavSys* GetCurrent(const UWorld* World)
	{
		return World ? Cast<TNavSys>(World->GetNavigationSystem()) : (const TNavSys*)nullptr;
	}

	ENGINE_API UWorld* GetWorldFromContextObject(UObject* WorldContextObject);

	template<typename TNavSys>
	TNavSys* GetCurrent(UObject* WorldContextObject)
	{
		UWorld* World = GetWorldFromContextObject(WorldContextObject);
		return GetCurrent<TNavSys>(World);
	}

	ENGINE_API void UpdateActorData(AActor& Actor);
	ENGINE_API void UpdateComponentData(UActorComponent& Comp);
	ENGINE_API void UpdateActorAndComponentData(AActor& Actor, bool bUpdateAttachedActors = true);
	ENGINE_API void UpdateComponentDataAfterMove(USceneComponent& Comp);
	//ENGINE_API bool HasComponentData(UActorComponent& Comp);
	ENGINE_API void OnActorBoundsChanged(AActor& Actor);
	ENGINE_API void OnPostEditActorMove(AActor& Actor);
	ENGINE_API void OnComponentBoundsChanged(UActorComponent& Comp, const FBox& NewBounds, const FBox& DirtyArea);
	ENGINE_API void OnComponentTransformChanged(USceneComponent& Comp);

	ENGINE_API void OnActorRegistered(AActor& Actor);
	ENGINE_API void OnActorUnregistered(AActor& Actor);

	ENGINE_API void OnComponentRegistered(UActorComponent& Comp);
	ENGINE_API void OnComponentUnregistered(UActorComponent& Comp);
	
	ENGINE_API void RegisterComponent(UActorComponent& Comp);
	ENGINE_API void UnregisterComponent(UActorComponent& Comp);
	
	ENGINE_API void RemoveActorData(AActor& Actor);

	ENGINE_API bool HasComponentData(UActorComponent& Comp);
	
	ENGINE_API const FNavDataConfig& GetDefaultSupportedAgent();
	ENGINE_API const FNavDataConfig& GetBiggestSupportedAgent(const UWorld* World);
	ENGINE_API double GetWorldPartitionNavigationDataBuilderOverlap(const UWorld& World);

	ENGINE_API TSubclassOf<UNavAreaBase> GetDefaultWalkableArea();
	ENGINE_API TSubclassOf<UNavAreaBase> GetDefaultObstacleArea();

	/**	Retrieves the transform the Navigation System is using to convert coords
	 *	from FromCoordType to ToCoordType */
	ENGINE_API const FTransform& GetCoordTransform(const ENavigationCoordSystem::Type FromCoordType, const ENavigationCoordSystem::Type ToCoordType);
	UE_DEPRECATED(4.22, "FNavigationSystem::GetCoordTransformTo is deprecated. Use FNavigationSystem::GetCoordTransform instead")
	ENGINE_API const FTransform& GetCoordTransformTo(const ENavigationCoordSystem::Type CoordType);
	UE_DEPRECATED(4.22, "FNavigationSystem::GetCoordTransformFrom is deprecated. Use FNavigationSystem::GetCoordTransform instead")
	ENGINE_API const FTransform& GetCoordTransformFrom(const ENavigationCoordSystem::Type CoordType);

	ENGINE_API bool WantsComponentChangeNotifies();

	ENGINE_API INavigationDataInterface* GetNavDataForActor(const AActor& Actor);
	ENGINE_API TSubclassOf<AActor> GetDefaultNavDataClass();

	ENGINE_API void VerifyNavigationRenderingComponents(UWorld& World, const bool bShow);
	ENGINE_API void Build(UWorld& World);

#if WITH_EDITOR
	ENGINE_API void OnPIEStart(UWorld& World);
	ENGINE_API void OnPIEEnd(UWorld& World);
	ENGINE_API void SetNavigationAutoUpdateEnabled(const bool bNewEnable, UNavigationSystemBase* InNavigationSystem);
	ENGINE_API void UpdateLevelCollision(ULevel& Level);
#endif // WITH_EDITOR

	enum class ECleanupMode : uint8
	{
		CleanupWithWorld,
		CleanupUnsafe,
	};

	// pathfollowing
	ENGINE_API bool IsFollowingAPath(const AController& Controller);
	ENGINE_API void StopMovement(const AController& Controller);
	ENGINE_API IPathFollowingAgentInterface* FindPathFollowingAgentForActor(const AActor& Actor);

	DECLARE_DELEGATE_OneParam(FActorBasedSignature, AActor& /*Actor*/);
	DECLARE_DELEGATE_OneParam(FActorComponentBasedSignature, UActorComponent& /*Comp*/);
	DECLARE_DELEGATE_OneParam(FSceneComponentBasedSignature, USceneComponent& /*Comp*/);
	DECLARE_DELEGATE_OneParam(FWorldBasedSignature, UWorld& /*World*/);
	DECLARE_DELEGATE_OneParam(FLevelBasedSignature, ULevel& /*Level*/);
	DECLARE_DELEGATE_OneParam(FControllerBasedSignature, const AController& /*Controller*/);
	DECLARE_DELEGATE_TwoParams(FNavigationAutoUpdateEnableSignature, const bool /*bNewEnable*/, UNavigationSystemBase* /*InNavigationSystem*/);
	DECLARE_DELEGATE_RetVal_OneParam(bool, FBoolControllerBasedSignature, const AController& /*Controller*/);
	DECLARE_DELEGATE_RetVal_OneParam(bool, FBoolActorComponentBasedSignature, UActorComponent& /*Comp*/);
	DECLARE_DELEGATE_RetVal(TSubclassOf<UNavAreaBase>, FNavAreaBasedSignature);
	DECLARE_DELEGATE_RetVal(const FNavDataConfig&, FNavDataConfigBasedSignature);
	DECLARE_DELEGATE_RetVal_OneParam(const FNavDataConfig&, FNavDataConfigAndWorldSignature, const UWorld* /*World*/);
	DECLARE_DELEGATE_RetVal_OneParam(double, FDoubleWorldBasedSignature, const UWorld& /*World*/);
	DECLARE_DELEGATE_TwoParams(FWorldByteBasedSignature, UWorld& /*World*/, uint8 /*Flags*/);
	DECLARE_DELEGATE_TwoParams(FActorBooleBasedSignature, AActor& /*Actor*/, bool /*bUpdateAttachedActors*/);
	DECLARE_DELEGATE_ThreeParams(FComponentBoundsChangeSignature, UActorComponent& /*Comp*/, const FBox& /*NewBounds*/, const FBox& /*DirtyArea*/)
	DECLARE_DELEGATE_RetVal_OneParam(INavigationDataInterface*, FNavDataForPropsSignature, const FNavAgentProperties& /*AgentProperties*/);
	DECLARE_DELEGATE_RetVal_OneParam(INavigationDataInterface*, FNavDataForActorSignature, const AActor& /*Actor*/);
	DECLARE_DELEGATE_RetVal(TSubclassOf<AActor>, FNavDataClassFetchSignature);
	DECLARE_DELEGATE_TwoParams(FWorldBoolBasedSignature, UWorld& /*World*/, const bool /*bShow*/);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnNavigationInitSignature, const UNavigationSystemBase&);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnNavAreaGenericEvent, const UWorld&, const UClass*);
}


UCLASS(Abstract, config = Engine, defaultconfig, Transient)
class ENGINE_API UNavigationSystemBase : public UObject
{
	GENERATED_BODY()

public:
	virtual ~UNavigationSystemBase(){}

	virtual void Tick(float DeltaSeconds) PURE_VIRTUAL(UNavigationSystemBase::Tick, );
	virtual void CleanUp(const FNavigationSystem::ECleanupMode Mode) PURE_VIRTUAL(UNavigationSystemBase::CleanUp, );
	virtual void Configure(const UNavigationSystemConfig& Config) PURE_VIRTUAL(UNavigationSystemBase::Configure, );
	/** Called when there's a need to extend current navigation system's config with information in NewConfig */
	virtual void AppendConfig(const UNavigationSystemConfig& NewConfig) PURE_VIRTUAL(UNavigationSystemBase::AppendConfig, );

	/**
	*	Called when owner-UWorld initializes actors
	*/
	virtual void OnInitializeActors() {}

	virtual bool IsNavigationBuilt(const AWorldSettings* Settings) const { return false; }

	virtual void ApplyWorldOffset(const FVector& InOffset, bool bWorldShift) PURE_VIRTUAL(UNavigationSystemBase::ApplyWorldOffset, );

	virtual void InitializeForWorld(UWorld& World, FNavigationSystemRunMode Mode) PURE_VIRTUAL(UNavigationSystemBase::InitializeForWorld, );

	/** 
	 *	If you're using NavigationSysstem module consider calling 
	 *	FNavigationSystem::GetCurrent<UNavigationSystemV1>()->GetDefaultNavDataInstance 
	 *	instead.
	 */
	virtual INavigationDataInterface* GetMainNavData() const { return nullptr; }

	UE_DEPRECATED(4.20, "GetMainNavData is deprecated. Use FNavigationSystem::GetCurrent<UNavigationSystemV1>()->GetDefaultNavDataInstance instead")
	INavigationDataInterface* GetMainNavData(int) { return nullptr; }

	virtual void SetBuildBounds(const FBox& Bounds) PURE_VIRTUAL(UNavigationSystemBase::SetBuildBounds, );

	virtual FBox GetNavigableWorldBounds() const PURE_VIRTUAL(UNavigationSystemBase::GetNavigableWorldBounds, return FBox(ForceInit););
	
	virtual bool ContainsNavData(const FBox& Bounds) const PURE_VIRTUAL(UNavigationSystemBase::ContainsNavData, return false;);
	virtual FBox ComputeNavDataBounds() const PURE_VIRTUAL(UNavigationSystemBase::GetNavigableWorldBounds, return FBox(ForceInit););
	
	virtual void AddNavigationDataChunk(class ANavigationDataChunkActor& DataChunkActor) {}
	virtual void RemoveNavigationDataChunk(class ANavigationDataChunkActor& DataChunkActor) {}
	virtual void FillNavigationDataChunkActor(const FBox& QueryBounds, class ANavigationDataChunkActor& DataChunkActor, FBox& OutTilesBounds) {}

	virtual bool IsWorldInitDone() const PURE_VIRTUAL(UNavigationSystemBase::IsWorldInitDone, return false;);

	static FNavigationSystem::FOnNavigationInitSignature& OnNavigationInitStartStaticDelegate();
	static FNavigationSystem::FOnNavigationInitSignature& OnNavigationInitDoneStaticDelegate();
	static FNavigationSystem::FOnNavAreaGenericEvent& OnNavAreaRegisteredDelegate();
	static FNavigationSystem::FOnNavAreaGenericEvent& OnNavAreaUnregisteredDelegate();

protected:
	/**	Sets the Transform the Navigation System will use when converting from FromCoordType
	 *	to ToCoordType
	 *	@param bAddInverse if true (default) will also set coord transform in 
	 *		the reverse order using Transform.Inverse() */
	static void SetCoordTransform(const ENavigationCoordSystem::Type FromCoordType, const ENavigationCoordSystem::Type ToCoordType, const FTransform& Transform, bool bAddInverse = true);
	UE_DEPRECATED(4.22, "FNavigationSystem::SetCoordTransformTo is deprecated. Use FNavigationSystem::SetCoordTransform instead")
	static void SetCoordTransformTo(const ENavigationCoordSystem::Type CoordType, const FTransform& Transform);
	UE_DEPRECATED(4.22, "FNavigationSystem::SetCoordTransformFrom is deprecated. Use FNavigationSystem::SetCoordTransform instead")
	static void SetCoordTransformFrom(const ENavigationCoordSystem::Type CoordType, const FTransform& Transform);
	static void SetWantsComponentChangeNotifies(const bool bEnable);
	static void SetDefaultWalkableArea(TSubclassOf<UNavAreaBase> InAreaClass);
	static void SetDefaultObstacleArea(TSubclassOf<UNavAreaBase> InAreaClass);

	static void ResetEventDelegates();
	static FNavigationSystem::FActorBasedSignature& UpdateActorDataDelegate();
	static FNavigationSystem::FActorComponentBasedSignature& UpdateComponentDataDelegate();
	static FNavigationSystem::FSceneComponentBasedSignature& UpdateComponentDataAfterMoveDelegate();
	static FNavigationSystem::FActorBasedSignature& OnActorBoundsChangedDelegate();
	static FNavigationSystem::FActorBasedSignature& OnPostEditActorMoveDelegate();
	static FNavigationSystem::FSceneComponentBasedSignature& OnComponentTransformChangedDelegate();
	static FNavigationSystem::FActorBasedSignature& OnActorRegisteredDelegate();
	static FNavigationSystem::FActorBasedSignature& OnActorUnregisteredDelegate();
	static FNavigationSystem::FActorComponentBasedSignature& OnComponentRegisteredDelegate();
	static FNavigationSystem::FActorComponentBasedSignature& OnComponentUnregisteredDelegate();
	static FNavigationSystem::FActorComponentBasedSignature& RegisterComponentDelegate();
	static FNavigationSystem::FActorComponentBasedSignature& UnregisterComponentDelegate();
	static FNavigationSystem::FActorBasedSignature& RemoveActorDataDelegate();
	static FNavigationSystem::FBoolActorComponentBasedSignature& HasComponentDataDelegate();
	static FNavigationSystem::FNavDataConfigBasedSignature& GetDefaultSupportedAgentDelegate();
	static FNavigationSystem::FNavDataConfigAndWorldSignature& GetBiggestSupportedAgentDelegate();
	static FNavigationSystem::FActorBooleBasedSignature& UpdateActorAndComponentDataDelegate();
	static FNavigationSystem::FComponentBoundsChangeSignature& OnComponentBoundsChangedDelegate();
	static FNavigationSystem::FNavDataForActorSignature& GetNavDataForActorDelegate();
	static FNavigationSystem::FNavDataClassFetchSignature& GetDefaultNavDataClassDelegate();
	static FNavigationSystem::FWorldBoolBasedSignature& VerifyNavigationRenderingComponentsDelegate();
	static FNavigationSystem::FWorldBasedSignature& BuildDelegate();
#if WITH_EDITOR
	static FNavigationSystem::FWorldBasedSignature& OnPIEStartDelegate();
	static FNavigationSystem::FWorldBasedSignature& OnPIEEndDelegate();
	static FNavigationSystem::FLevelBasedSignature& UpdateLevelCollisionDelegate();
	static FNavigationSystem::FNavigationAutoUpdateEnableSignature& SetNavigationAutoUpdateEnableDelegate();
	static FNavigationSystem::FWorldByteBasedSignature& AddNavigationUpdateLockDelegate();
	static FNavigationSystem::FWorldByteBasedSignature& RemoveNavigationUpdateLockDelegate();
	static FNavigationSystem::FDoubleWorldBasedSignature& GetWorldPartitionNavigationDataBuilderOverlapDelegate();
#endif // WITH_EDITOR
};


class ENGINE_API IPathFollowingManagerInterface
{
protected:
	static FNavigationSystem::FControllerBasedSignature& StopMovementDelegate();
	static FNavigationSystem::FBoolControllerBasedSignature& IsFollowingAPathDelegate();
};

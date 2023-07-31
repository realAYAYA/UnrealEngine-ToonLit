// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "Debug/DebugDrawComponent.h"
#include "DebugRenderSceneProxy.h"
#include "ZoneGraphTypes.h"
#include "MassZoneGraphNavigationFragments.h"
#include "MassNavigationTestingActor.generated.h"

class AZoneGraphData;
class UZoneGraphTestingComponent;
class UZoneGraphSubsystem;
class AMassNavigationTestingActor;

#if UE_ENABLE_DEBUG_DRAWING
class MASSNAVIGATIONEDITOR_API FMassNavigationTestingSceneProxy final : public FDebugRenderSceneProxy
{
public:
	FMassNavigationTestingSceneProxy(const UPrimitiveComponent& InComponent);
	
	virtual SIZE_T GetTypeHash() const override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual uint32 GetMemoryFootprint(void) const override;
};
#endif // UE_ENABLE_DEBUG_DRAWING

/** Component for testing MassMovement functionality. */
UCLASS(ClassGroup = Debug)
class MASSNAVIGATIONEDITOR_API UMassNavigationTestingComponent : public UDebugDrawComponent
{
	GENERATED_BODY()
public:
	UMassNavigationTestingComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	virtual void OnRegister() override;
	virtual void OnUnregister() override;

	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

#if UE_ENABLE_DEBUG_DRAWING
	virtual FDebugRenderSceneProxy* CreateDebugSceneProxy() override;
#endif

	void UpdateTests();
	void PinLane();
	void ClearPinnedLane();

protected:

#if WITH_EDITOR
	void OnZoneGraphDataBuildDone(const struct FZoneGraphBuildData& BuildData);
#endif
	void OnZoneGraphDataChanged(const AZoneGraphData* ZoneGraphData);

#if WITH_EDITOR
	FDelegateHandle OnDataChangedHandle;
#endif
	FDelegateHandle OnDataAddedHandle;
	FDelegateHandle OnDataRemovedHandle;

	UPROPERTY(Transient)
	TObjectPtr<UZoneGraphSubsystem> ZoneGraph;

	UPROPERTY(Transient)
	FZoneGraphLaneLocation LaneLocation;

	UPROPERTY(Transient)
	FZoneGraphLaneLocation GoalLaneLocation;

	UPROPERTY(EditAnywhere, Category = Default);
	FVector SearchExtent;

	UPROPERTY(EditAnywhere, Category = Default);
	float AnticipationDistance = 50.0f;

	UPROPERTY(EditAnywhere, Category = Default);
	float AgentRadius = 40.0f;

	UPROPERTY(EditAnywhere, Category = Default);
	bool bHasSpecificEndPoint = true;

	UPROPERTY(EditAnywhere, Category = Default);
	FZoneGraphTagFilter QueryFilter;

	UPROPERTY(EditAnywhere, Category = Default, meta = (MakeEditWidget=true))
	FVector GoalPosition;

	FZoneGraphLaneHandle PinnedLane;
	
	FMassZoneGraphCachedLaneFragment CachedLane;
	TArray<FMassZoneGraphShortPathFragment> ShortPaths;
};

/** Debug actor to visually test zone graph. */
UCLASS(hidecategories = (Actor, Input, Collision, Rendering, Replication, Partition, HLOD, Cooking))
class MASSNAVIGATIONEDITOR_API AMassNavigationTestingActor : public AActor
{
	GENERATED_BODY()
public:
	AMassNavigationTestingActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
#endif // WITH_EDITOR

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Default")
	void PinLane();
	
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Default")
	void ClearPinnedLane();

protected:
	UPROPERTY(Category = Default, VisibleAnywhere, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMassNavigationTestingComponent> DebugComp;
};

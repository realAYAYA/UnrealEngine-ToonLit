// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "Debug/DebugDrawComponent.h"
#include "DebugRenderSceneProxy.h"
#include "NavCorridor.h"
#include "NavCorridorTestingComponent.generated.h"

class UNavCorridorTestingComponent;
class ANavCorridorTestingActor;

/** Component for testing AI Locomotion functionality. */
UCLASS(ClassGroup = Custom, hidecategories = (Physics, Collision, Rendering, Cooking, Lighting, Navigation, Tags, HLOD, Mobile, AssetUserData, Activation))
class NAVCORRIDOR_API UNavCorridorTestingComponent : public UDebugDrawComponent
{
	GENERATED_BODY()
public:
	UNavCorridorTestingComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostLoad();
#endif // WITH_EDITOR

	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

#if UE_ENABLE_DEBUG_DRAWING
	FDebugRenderSceneProxy* CreateDebugSceneProxy() override;
#endif // UE_ENABLE_DEBUG_DRAWING

	void UpdateTests();

protected:
	void UpdateNavData();
	
	UPROPERTY(EditAnywhere, Category = "Test")
	FNavAgentProperties NavAgentProps;

	UPROPERTY(EditAnywhere, Category = "Test")
	TSubclassOf<class UNavigationQueryFilter> FilterClass;

	/** If true, finds path to Goal actor. */
	UPROPERTY(EditAnywhere, Category = "Test");
	bool bFindCorridorToGoal = true;

	/** If true, finds nearest path location on Goal actor corridor. */
	UPROPERTY(EditAnywhere, Category = "Test");
	bool bFollowPathOnGoalCorridor = false;

	UPROPERTY(EditAnywhere, Category = "Test");
	float FollowLookAheadDistance = 200.0f;
	
	UPROPERTY(EditAnywhere, Category = "Test")
	TObjectPtr<AActor> GoalActor = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ANavigationData> NavData;

	UPROPERTY(EditAnywhere, Category = "Test")
	FNavCorridorParams CorridorParams;

	UPROPERTY(EditAnywhere, Category = "Test");
	bool bUpdateParametersFromWidth = false;

	UPROPERTY(EditAnywhere, Category = "Test")
	float PathOffset = 40.0f;

	UPROPERTY(transient, VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	float PathfindingTimeUs = 0.0f;

	UPROPERTY(transient, VisibleAnywhere, BlueprintReadOnly, Category = "Profile")
	float CorridorTimeUs = 0.0f;
	
	FNavPathSharedPtr Path;
	FNavCorridor Corridor;

	/** Location used to track of the target actor moves. */
	FVector LastTargetLocation;

	FNavCorridorLocation NearestPathLocation;
	FNavCorridorLocation LookAheadPathLocation;
	FVector ClampedLookAheadLocation;
};

/** Debug actor to visually test zone graph. */
UCLASS(hidecategories = (Actor, Input, Collision, Rendering, Replication, Partition, HLOD, Cooking))
class NAVCORRIDOR_API ANavCorridorTestingActor : public AActor
{
	GENERATED_BODY()
public:
	ANavCorridorTestingActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
#endif // WITH_EDITOR

protected:
	UPROPERTY(Category = Default, VisibleAnywhere, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNavCorridorTestingComponent> DebugComp;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "Components/ActorComponent.h"
#include "StructView.h"
#include "InstancedStruct.h"
#include "GameplayActuationState.h"
#include "GameplayTaskTypes.h"
#include "EngineDefines.h"
#include "VisualLogger/VisualLoggerDebugSnapshotInterface.h"
#include "GameplayActuationComponent.generated.h"

class UGameplayTask;

/**
 * Component holding current actuation state, and functionality to create transitions between GameplayTasks. 
 */
UCLASS(ClassGroup = GameplayTasks, hidecategories = (Object, LOD, Lighting, Transform, Sockets, TextureStreaming), editinlinenew, meta = (BlueprintSpawnableComponent))
class GAMEPLAYINTERACTIONSMODULE_API UGameplayActuationComponent : public UActorComponent, public IVisualLoggerDebugSnapshotInterface
{
	GENERATED_BODY()

public:

	UGameplayActuationComponent(const FObjectInitializer& ObjectInitializer);

	/** Returns current actuation state. */
	FConstStructView GetActuationState() const
	{
		static FGameplayActuationStateBase Default;
		return ActuationState.IsValid() ? FConstStructView(ActuationState) : FConstStructView::Make(Default);
	}

	/**
	 * Attempts to make a transition task based on current state, and given next state.
	 * @return valid task if one can be created, or nullptr if no valid transition can be made.
	 */
	UGameplayTask* TryMakeTransitionTask(const FConstStructView NextState);

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#if ENABLE_VISUAL_LOG
	virtual void GrabDebugSnapshot(FVisualLogEntry* Snapshot) const override;
#endif

	/** Current actuation state */
	UPROPERTY()
	FInstancedStruct ActuationState; 

	/** ID of the GameplayTask whose state was last stored in ActuationState */
	uint32 LastStateOwnerID = 0;

	/** Transition descriptors */
	UPROPERTY(Category="Actuation", EditAnywhere, meta=(BaseStruct="/Script/GameplayInteractionsModule.GameplayTransitionDesc"));
	TArray<FInstancedStruct> Transitions;

	/** If true, allow transition tasks. */
	UPROPERTY(Category="Actuation", EditAnywhere);
	bool bEnableTransitions = true;

#if WITH_GAMEPLAYTASK_DEBUG	
	struct FTrajectoryPoint
	{
		FTrajectoryPoint() = default;
		FTrajectoryPoint(const FVector& InLocation, const FColor& InColor)
			: Location(InLocation)
			, Color(InColor)
		{
		}
		
		FVector Location = FVector::ZeroVector;
		FColor Color = FColor::White;
	};

	TArray<FTrajectoryPoint> DebugTrajectory;

	int32 StateCounter = 0;
#endif
};

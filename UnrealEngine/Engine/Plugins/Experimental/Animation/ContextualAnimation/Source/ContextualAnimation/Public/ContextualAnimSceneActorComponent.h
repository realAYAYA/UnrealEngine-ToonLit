// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ContextualAnimTypes.h"
#include "ActorComponents/IKRigInterface.h"
#include "Components/PrimitiveComponent.h"
#include "ContextualAnimSceneActorComponent.generated.h"

class AActor;
class FPrimitiveSceneProxy;
class UAnimInstance;
class UAnimMontage;
class UAnimSequenceBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FContextualAnimSceneActorCompDelegate, class UContextualAnimSceneActorComponent*, SceneActorComponent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FContextualAnimPlayMontageNotifyBeginDelegate, class UContextualAnimSceneActorComponent*, SceneActorComponent, FName, NotifyName);

USTRUCT(BlueprintType)
struct FContextualAnimWarpTarget
{
	GENERATED_BODY()

	FContextualAnimWarpTarget() = default;
	FContextualAnimWarpTarget(const FName InRole, const FName InWarpTargetName, const FTransform& InTargetTransform)
		: Role(InRole)
		, TargetName(InWarpTargetName)
		, TargetLocation(InTargetTransform.GetLocation())
		, TargetRotation(InTargetTransform.GetRotation())
	{
	}

	UPROPERTY(EditAnywhere, Category = "Default")
	FName Role = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Default")
	FName TargetName = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Default")
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Default")
	FQuat TargetRotation = FQuat::Identity;
};

/** Base struct for replicated data with a rep counter */
USTRUCT()
struct FContextualAnimRepData
{
	GENERATED_BODY()

	/** Auto increment counter to ensure replication even if the data is the same */
	UPROPERTY()
	uint8 RepCounter = 0;

	void IncrementRepCounter();
	bool IsValid() const { return RepCounter != 0; }
};

/** Used to replicate start/stop contextual anim events */
USTRUCT()
struct FContextualAnimRepBindingsData : public FContextualAnimRepData
{
	GENERATED_BODY()

	UPROPERTY()
	FContextualAnimSceneBindings Bindings;

	UPROPERTY()
	TArray<FContextualAnimWarpPoint> WarpPoints;

	UPROPERTY()
	TArray<FContextualAnimWarpTarget> ExternalWarpTargets;

	void Reset()
	{
		RepCounter = 0;
		Bindings.Reset();
		WarpPoints.Reset();
		ExternalWarpTargets.Reset();
	}
};

/** Used to replicate a late join event */
USTRUCT()
struct FContextualAnimRepLateJoinData : public FContextualAnimRepData
{
	GENERATED_BODY()

	/** Actor that is joining the interaction */
	UPROPERTY()
	TObjectPtr<AActor> Actor = nullptr;

	/** Role in the interaction the actor is gonna play */
	UPROPERTY()
	FName Role = NAME_None;

	UPROPERTY()
	TArray<FContextualAnimWarpPoint> WarpPoints;

	UPROPERTY()
	TArray<FContextualAnimWarpTarget> ExternalWarpTargets;

	void Reset()
	{
		RepCounter = 0;
		Actor = nullptr;
		Role = NAME_None;
		WarpPoints.Reset();
		ExternalWarpTargets.Reset();
	}
};

/** Used to transition events */
USTRUCT()
struct FContextualAnimRepTransitionData : public FContextualAnimRepData
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 Id = 0;

	UPROPERTY()
	uint8 SectionIdx = 0;

	UPROPERTY()
	uint8 AnimSetIdx = 0;

	UPROPERTY()
	TArray<FContextualAnimWarpPoint> WarpPoints;

	UPROPERTY()
	TArray<FContextualAnimWarpTarget> ExternalWarpTargets;

	void Reset()
	{
		RepCounter = 0;
		Id = 0;
		SectionIdx = 0;
		AnimSetIdx = 0;
		WarpPoints.Reset();
		ExternalWarpTargets.Reset();
	}
};

UCLASS(meta = (BlueprintSpawnableComponent))
class CONTEXTUALANIMATION_API UContextualAnimSceneActorComponent : public UPrimitiveComponent, public IIKGoalCreatorInterface
{
	GENERATED_BODY()

public:

	/** Event that happens when the actor owner of this component joins an scene */
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FContextualAnimSceneActorCompDelegate OnJoinedSceneDelegate;

	/** Event that happens when the actor owner of this component leave an scene */
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FContextualAnimSceneActorCompDelegate OnLeftSceneDelegate;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FContextualAnimPlayMontageNotifyBeginDelegate OnPlayMontageNotifyBeginDelegate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	TObjectPtr<class UContextualAnimSceneAsset> SceneAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bEnableDebug;

	UContextualAnimSceneActorComponent(const FObjectInitializer& ObjectInitializer);

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

	virtual void AddIKGoals_Implementation(TMap<FName, FIKRigGoal>& OutGoals) override;

	const FContextualAnimSceneBindings& GetBindings() const { return Bindings; };

	UFUNCTION(BlueprintPure, Category = "Contextual Anim|Scene Actor Component")
	bool IsInActiveScene() const;

	/** Called when the actor owner of this component joins an scene */
	UFUNCTION(BlueprintCallable, Category = "Contextual Anim|Scene Actor Component")
	void OnJoinedScene(const FContextualAnimSceneBindings& InBindings);

	/** Called from the scene instance when the actor owner of this component leave an scene */
	UFUNCTION(BlueprintCallable, Category = "Contextual Anim|Scene Actor Component")
	void OnLeftScene();

	UFUNCTION(BlueprintPure, Category = "Contextual Anim|Scene Actor Component")
	const TArray<FContextualAnimIKTarget>& GetIKTargets() const { return IKTargets; }

	UFUNCTION(BlueprintPure, Category = "Contextual Anim|Scene Actor Component")
	const FContextualAnimIKTarget& GetIKTargetByGoalName(FName GoalName) const;

	UFUNCTION(BlueprintCallable, Category = "Contextual Anim|Scene Actor Component")
	bool StartContextualAnimScene(const FContextualAnimSceneBindings& InBindings);
	
	UFUNCTION(BlueprintCallable, Category = "Contextual Anim|Scene Actor Component")
	bool LateJoinContextualAnimScene(AActor* Actor, FName Role);
	
	UFUNCTION(BlueprintCallable, Category = "Contextual Anim|Scene Actor Component")
	bool TransitionContextualAnimScene(FName SectionName);
	
	UFUNCTION(BlueprintCallable, Category = "Contextual Anim|Scene Actor Component")
	bool TransitionSingleActor(int32 SectionIdx, int32 AnimSetIdx);

	bool StartContextualAnimScene(const FContextualAnimSceneBindings& InBindings, const TArray<FContextualAnimWarpTarget>& ExternalWarpTargets);
	bool LateJoinContextualAnimScene(AActor* Actor, FName Role, const TArray<FContextualAnimWarpTarget>& ExternalWarpTargets);
	bool TransitionContextualAnimScene(FName SectionName, const TArray<FContextualAnimWarpTarget>& ExternalWarpTargets);
	bool TransitionContextualAnimScene(FName SectionName, int32 AnimSetIdx, const TArray<FContextualAnimWarpTarget>& ExternalWarpTargets);
	bool TransitionSingleActor(int32 SectionIdx, const TArray<FContextualAnimWarpTarget>& ExternalWarpTargets);
	bool TransitionSingleActor(int32 SectionIdx, int32 AnimSetIdx, const TArray<FContextualAnimWarpTarget>& ExternalWarpTargets);

	UFUNCTION(BlueprintCallable, Category = "Contextual Anim|Scene Actor Component")
	void EarlyOutContextualAnimScene();

	bool IsOwnerLocallyControlled() const;

protected:

	/** 
	 * Replicated copy of the bindings so we can start the action on simulated proxies 
	 * This gets replicated only from the initiator of the action and then set on all the other members of the interaction
	 */
	UPROPERTY(Transient, ReplicatedUsing = OnRep_Bindings)
	FContextualAnimRepBindingsData RepBindings;

	UPROPERTY(Transient, ReplicatedUsing = OnRep_LateJoinData)
	FContextualAnimRepLateJoinData RepLateJoinData;

	UPROPERTY(Transient, ReplicatedUsing = OnRep_TransitionData)
	FContextualAnimRepTransitionData RepTransitionData;

	UPROPERTY(Transient, ReplicatedUsing = OnRep_RepTransitionSingleActor)
	FContextualAnimRepTransitionData RepTransitionSingleActorData;

	/**
	 * Bindings for the interaction we are currently playing.
	 * Used to update IK, keep montage in sync, disable/enable collision between actors etc
	 */
	UPROPERTY(Transient)
	FContextualAnimSceneBindings Bindings;

	/** List of IKTarget for this frame */
	UPROPERTY(Transient)
	TArray<FContextualAnimIKTarget> IKTargets;

	struct FCharacterProperties
	{
		bool bIgnoreClientMovementErrorChecksAndCorrection = false;
		bool bAllowPhysicsRotationDuringAnimRootMotion = false;
		bool bUseControllerDesiredRotation = false;
		bool bOrientRotationToMovement = false;
		EMovementMode MovementMode = EMovementMode::MOVE_Walking;
		TArray<TTuple<ECollisionChannel, ECollisionResponse>> CollisionResponses;
	};
	FCharacterProperties CharacterPropertiesBackup;

	void UpdateIKTargets();

	/** 
	 * Event called right before owner's mesh ticks the pose when we are in a scene instance and IK Targets are required. 
	 * Used to update IK Targets before animation need them 
	 */
	UFUNCTION()
	virtual void OnTickPose(class USkinnedMeshComponent* SkinnedMeshComponent, float DeltaTime, bool bNeedsValidRootMotion);

	UFUNCTION()
	void OnRep_Bindings();

	UFUNCTION()
	void OnRep_LateJoinData();

	UFUNCTION()
	void OnRep_RepTransitionSingleActor();

	UFUNCTION()
	void OnRep_TransitionData();

	void SetIgnoreCollisionWithActor(AActor& Actor, bool bValue) const;
	void SetIgnoreCollisionWithOtherActors(bool bValue) const;
	
	virtual void SetCollisionState(const FContextualAnimSceneBinding& Binding);
	virtual void RestoreCollisionState(const FContextualAnimSceneBinding& Binding);

	void SetMovementState(const FContextualAnimSceneBinding& Binding, EMovementMode DesiredMoveMode);
	void RestoreMovementState(const FContextualAnimSceneBinding& Binding);

	UFUNCTION()
	void OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnPlayMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointNotifyPayload);

	// @TODO: These two functions are going to replace OnJoinedScene and OnLeftScene
	// main different is that these new functions are taking care of animation playback too

	void JoinScene(const FContextualAnimSceneBindings& InBindings, const TArray<FContextualAnimWarpPoint> WarpPoints, const TArray<FContextualAnimWarpTarget>& ExternalWarpTargets);

	virtual void OnJoinScene(const FContextualAnimSceneBinding& Binding);

	void LeaveScene();

	virtual bool CanLeaveScene(const FContextualAnimSceneBinding& Binding);

	virtual void OnLeaveScene(const FContextualAnimSceneBinding& Binding);

	void LateJoinScene(const FContextualAnimSceneBindings& InBindings, int32 SectionIdx, int32 AnimSetIdx, const TArray<FContextualAnimWarpPoint>& WarpPoints, const TArray<FContextualAnimWarpTarget>& ExternalWarpTargets);

	virtual void OnLateJoinScene(const FContextualAnimSceneBinding& Binding, int32 SectionIdx, int32 AnimSetIdx);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerStartContextualAnimScene(const FContextualAnimSceneBindings& InBindings);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerEarlyOutContextualAnimScene();

	virtual void PlayAnimation_Internal(UAnimSequenceBase* Animation, float StartTime, bool bSyncPlaybackTime);

	virtual void AddOrUpdateWarpTargets(int32 SectionIdx, int32 AnimSetIdx, const TArray<FContextualAnimWarpPoint>& WarpPoints, const TArray<FContextualAnimWarpTarget>& ExternalWarpTargets);

	void HandleTransitionSelf(int32 NewSectionIdx, int32 NewAnimSetIdx, const TArray<FContextualAnimWarpPoint>& WarpPoints, const TArray<FContextualAnimWarpTarget>& ExternalWarpTargets);

	void HandleTransitionEveryone(int32 NewSectionIdx, int32 NewAnimSetIdx, const TArray<FContextualAnimWarpPoint>& WarpPoints, const TArray<FContextualAnimWarpTarget>& ExternalWarpTargets);

	virtual void OnTransitionScene(const FContextualAnimSceneBinding& Binding, int32 SectionIdx, int32 AnimSetIdx);

	virtual void OnTransitionSingleActor(const FContextualAnimSceneBinding& Binding, int32 SectionIdx, int32 AnimSetIdx);

	void OtherActorLeftScene(AActor& Actor);

private:

	TArray<TWeakObjectPtr<const UAnimMontage>, TInlineAllocator<5>> AnimsPlayed;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif

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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FContextualAnimSceneActorCompDelegate, class UContextualAnimSceneActorComponent*, SceneActorComponent);

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	TObjectPtr<class UContextualAnimSceneAsset> SceneAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bEnableDebug;

	UContextualAnimSceneActorComponent(const FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

	virtual void AddIKGoals_Implementation(TMap<FName, FIKRigGoal>& OutGoals) override;

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

	void EarlyOutContextualAnimScene();

	bool IsOwnerLocallyControlled() const;

protected:

	/** 
	 * Replicated copy of the bindings so we can start the action on simulated proxies 
	 * This gets replicated only from the initiator of the action and then set on all the other members of the interaction
	 */
	UPROPERTY(Transient, ReplicatedUsing = OnRep_Bindings)
	FContextualAnimSceneBindings RepBindings;

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
	};
	FCharacterProperties CharacterPropertiesBackup;

	void UpdateIKTargets();

	/** 
	 * Event called right before owner's mesh ticks the pose when we are in a scene instance and IK Targets are required. 
	 * Used to update IK Targets before animation need them 
	 */
	UFUNCTION()
	void OnTickPose(class USkinnedMeshComponent* SkinnedMeshComponent, float DeltaTime, bool bNeedsValidRootMotion);

	UFUNCTION()
	void OnRep_Bindings(const FContextualAnimSceneBindings& LastRepBindings);

	void SetIgnoreCollisionWithOtherActors(bool bValue) const;

	UFUNCTION()
	void OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);

	// @TODO: These two functions are going to replace OnJoinedScene and OnLeftScene
	// main different is that these new functions are taking care of animation playback too

	void JoinScene(const FContextualAnimSceneBindings& InBindings);

	void LeaveScene();

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerStartContextualAnimScene(const FContextualAnimSceneBindings& InBindings);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerEarlyOutContextualAnimScene();

private:

	bool bRegistered = false;

};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif

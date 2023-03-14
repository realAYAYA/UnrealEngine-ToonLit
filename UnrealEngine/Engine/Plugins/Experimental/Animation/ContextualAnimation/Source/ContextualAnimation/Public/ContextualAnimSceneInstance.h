// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameFramework/Actor.h"
#include "ContextualAnimTypes.h"
#include "ContextualAnimSceneInstance.generated.h"

struct FAnimMontageInstance;
struct FContextualAnimTrack;
class UContextualAnimSceneInstance;
class UContextualAnimSceneActorComponent;
class UContextualAnimSceneAsset;
class UAnimInstance;
class USkeletalMeshComponent;
class UAnimMontage;
class UWorld;

/** Delegate to notify external objects when this is scene is completed */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnContextualAnimSceneEnded, class UContextualAnimSceneInstance*, SceneInstance);

/** Delegate to notify external objects when an actor join this scene */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnContextualAnimSceneActorJoined, class UContextualAnimSceneInstance*, SceneInstance, AActor*, Actor);

/** Delegate to notify external objects when an actor left this scene */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnContextualAnimSceneActorLeft, class UContextualAnimSceneInstance*, SceneInstance, AActor*, Actor);

/** Delegate to notify external objects about anim notify events */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnContextualAnimSceneNotify, class UContextualAnimSceneInstance*, SceneInstance, AActor*, Actor, FName, NotifyName);

/** Instance of a contextual animation scene */
UCLASS(BlueprintType, Blueprintable)
class CONTEXTUALANIMATION_API UContextualAnimSceneInstance : public UObject
{
	GENERATED_BODY()

public:

	friend class UContextualAnimManager;
	friend class FContextualAnimViewModel;

	/**
	 * Delegate to notify once the scene play time reaches the duration defined by the longest played montage of the selected section.
	 * This delegate should be used if one or more montages have 'bEnableAutoBlendOut' set to 'false'.
	 */
	UPROPERTY(BlueprintAssignable)
	FOnContextualAnimSceneEnded OnSectionEndTimeReached;

	/**
	 * Delegate to notify external objects when this is scene is completed after all montages played by the scene section blended out.
	 * Will not be broadcasted if one or more montages have 'bEnableAutoBlendOut' set to 'false'.
	 */
	UPROPERTY(BlueprintAssignable)
	FOnContextualAnimSceneEnded OnSceneEnded;

	/** Delegate to notify external objects when an actor join */
	UPROPERTY(BlueprintAssignable)
	FOnContextualAnimSceneActorJoined OnActorJoined;

	/** Delegate to notify external objects when an actor leave */
	UPROPERTY(BlueprintAssignable)
	FOnContextualAnimSceneActorLeft OnActorLeft;

	/** Delegate to notify external objects when an animation hits a 'PlayMontageNotify' or 'PlayMontageNotifyWindow' begin */
	UPROPERTY(BlueprintAssignable)
	FOnContextualAnimSceneNotify OnNotifyBegin;

	/** Delegate to notify external objects when an animation hits a 'PlayMontageNotify' or 'PlayMontageNotifyWindow' end */
	UPROPERTY(BlueprintAssignable)
	FOnContextualAnimSceneNotify OnNotifyEnd;

	UContextualAnimSceneInstance(const FObjectInitializer& ObjectInitializer);

	virtual UWorld* GetWorld() const override;

	void Tick(float DeltaTime);

	/** Resolve initial alignment and start playing animation for all actors */
	void Start();

	/**
	 * Tells current scene instance to transition to a different section.
	 * @note The method assumes that selection criteria were applied through bindings creation before calling.
	 * @return True if scene was able to transition all bindings to the new section, false otherwise.
	 */
	bool ForceTransitionToSection(const int32 SectionIdx, const int32 AnimSetIdx, const TArray<FContextualAnimSetPivot>& Pivots);
		
	/** Force all the actors to leave the scene */
	void Stop();

	bool IsDonePlaying() const;

	/** Whether the supplied actor is part of this scene */
	bool IsActorInThisScene(const AActor* Actor) const;

	const UContextualAnimSceneAsset& GetSceneAsset() const { return *SceneAsset; }
	const FContextualAnimSceneBindings& GetBindings() const { return Bindings; }
	FContextualAnimSceneBindings& GetBindings() { return Bindings; }
	const FContextualAnimSceneBinding* FindBindingByActor(const AActor* Actor) const { return Bindings.FindBindingByActor(Actor); }
	const FContextualAnimSceneBinding* FindBindingByRole(const FName& Role) const { return Bindings.FindBindingByRole(Role); }

	UFUNCTION(BlueprintCallable, Category = "Contextual Anim|Scene Instance")
	AActor* GetActorByRole(FName Role) const;

protected:

	/**
	 * Tells the scene actor to join the scene (play animation)
	 * @return Duration of the playing animation, or MIN_flt if not playing one.
	 */
	float Join(FContextualAnimSceneBinding& Binding);

	/** Tells the scene actor to leave the scene (stop animation) */
	void Leave(FContextualAnimSceneBinding& Binding);

	float TransitionTo(FContextualAnimSceneBinding& Binding, const FContextualAnimTrack& AnimTrack);

	TArray<FContextualAnimSetPivot>& GetMutablePivots() { return AlignmentSectionToScenePivotList; }
	void SetPivots(const TArray<FContextualAnimSetPivot>& Pivots) { AlignmentSectionToScenePivotList = Pivots; }

	/** Helper function to set ignore collision between the supplied actor and all the other actors in this scene */
	void SetIgnoreCollisionWithOtherActors(AActor* Actor, bool bValue) const;

	UFUNCTION()
	void OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnNotifyBeginReceived(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointNotifyPayload);

	UFUNCTION()
	void OnNotifyEndReceived(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointNotifyPayload);

private:

	/** Scene asset this instance was created from */
	UPROPERTY()
	TObjectPtr<const UContextualAnimSceneAsset> SceneAsset;

	UPROPERTY()
	FContextualAnimSceneBindings Bindings;

	TArray<FContextualAnimSetPivot> AlignmentSectionToScenePivotList;

	/**
	 * Remaining scene section duration initially computed based on the longest animation duration from all actors that joined the scene.
	 * Delegate 'OnSectionEndTimeReached' is broadcasted once value reaches 0.
	 */
	float RemainingDuration;

	/** Helper to play an AnimSequenceBase as montage. If Animation is not a montage it plays it as dynamic montage  */
	UAnimMontage* PlayAnimation(UAnimInstance& AnimInstance, UAnimSequenceBase& Animation);
};
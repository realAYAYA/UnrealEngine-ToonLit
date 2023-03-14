// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTask.h"
#include "ContextualAnimTypes.h"
#include "GameplayInteractionsTypes.h"
#include "GameplayTaskTransition.h"
#include "GameplayActuationComponent.h"
#include "GameplayActuationStateProvider.h"
#include "GameplayTask_PlayContextualAnim.generated.h"

class UContextualAnimSceneInstance;
class UContextualAnimSceneAsset;
class AActor;
class UGameplayActuationComponent;


UENUM(BlueprintType)
enum class EPlayContextualAnimExitMode : uint8
{
	DefaultExit,	// uses ExitSectionName if specified and available otherwise fallback on Teleport
	FastExit,		// uses FastExitSectionName if specified and available otherwise fallback on Teleport
	Teleport		// teleports actor to safe location defined at interaction pivot
};


/** Status describing current ticking state. */
UENUM(BlueprintType)
enum class EPlayContextualAnimStatus : uint8
{
	Unset,				// Status not set.
	Playing,			// Section is currently playing.
	DonePlaying,		// Section was played successfully.
	Failed,				// Section failed to play.
};


/**
 * Contextual Animation actuation state
 */
USTRUCT()
struct GAMEPLAYINTERACTIONSMODULE_API FGameplayActuationState_ContextualAnim : public FGameplayActuationStateBase
{
	GENERATED_BODY()

protected:
	virtual void OnStateDeactivated(FConstStructView NextState)  override;

public:
	UPROPERTY()
	FName InteractorRole = FName();

	UPROPERTY()
	TObjectPtr<AActor> InteractableObject = nullptr;

	UPROPERTY()
	FName InteractableObjectRole = FName();

	UPROPERTY()
	FName SectionName = FName();
	
	UPROPERTY()
	FName ExitSectionName = FName();
	
	UPROPERTY()
	TObjectPtr<const UContextualAnimSceneAsset> SceneAsset = nullptr;

	UPROPERTY()
	TObjectPtr<UContextualAnimSceneInstance> SceneInstance = nullptr;
};


/**
 * Contextual animation as enter transition
 */
USTRUCT(BlueprintType)
struct GAMEPLAYINTERACTIONSMODULE_API FGameplayTransitionDesc_EnterContextualAnim : public FGameplayTransitionDesc
{
	GENERATED_BODY()

protected:
	virtual UGameplayTask* MakeTransitionTask(const FMakeGameplayTransitionTaskContext& Context) const override; 
};


/**
 * Contextual animation as exit transition
 */
USTRUCT(BlueprintType)
struct GAMEPLAYINTERACTIONSMODULE_API FGameplayTransitionDesc_ExitContextualAnim : public FGameplayTransitionDesc
{
	GENERATED_BODY()

protected:
	virtual UGameplayTask* MakeTransitionTask(const FMakeGameplayTransitionTaskContext& Context) const override; 
};


/**
 * Simulated GameplayTask that starts a contextual anim scene on multiple actors
 */
UCLASS()
class GAMEPLAYINTERACTIONSMODULE_API UGameplayTask_PlayContextualAnim : public UGameplayTask, public IGameplayTaskTransition, public IGameplayActuationStateProvider
{
	GENERATED_BODY()

public:
	explicit UGameplayTask_PlayContextualAnim(const FObjectInitializer& ObjectInitializer);

	DECLARE_DELEGATE_OneParam(FSectionDoneDelegate, const int32 SectionIdx);

	UFUNCTION(BlueprintCallable, Category="Gameplay|Tasks", meta = (DisplayName="PlayContextualAnim", BlueprintInternalUseOnly = "TRUE"))
	static UGameplayTask_PlayContextualAnim* PlayContextualAnim(
		AActor* Interactor,
		const FName InteractorRole,
		AActor* InteractableObject,
		const FName InteractableObjectRole,
		const FName SectionName,
		const FName ExitSectionName,
		const UContextualAnimSceneAsset* SceneAsset
		);

	static UGameplayTask_PlayContextualAnim* CreateContextualAnimTransition(
		AActor* Interactor,
		const FName InteractorRole,
		AActor* InteractableObject,
		const FName InteractableObjectRole,
		const FName SectionName,
		const FName ExitSectionName,
		const UContextualAnimSceneAsset* SceneAsset
		);

	UFUNCTION(BlueprintCallable, Category="Gameplay|Tasks")
	void SetExit(EPlayContextualAnimExitMode ExitMode, FName NewExitSectionName);

	UFUNCTION(BlueprintCallable, Category="Gameplay|Tasks")
	EPlayContextualAnimStatus GetStatus() const { return Status; }

	static bool CreateBindings(const UContextualAnimSceneAsset& SceneAsset, const FContextualAnimStartSceneParams& SceneParams, FContextualAnimSceneBindings& OutBindings);

protected:
	virtual void InitSimulatedTask(UGameplayTasksComponent& InGameplayTasksComponent) override;
	virtual void Activate() override;

#if ENABLE_VISUAL_LOG
	virtual void TickTask(float DeltaTime) override;
#endif
	
	virtual void OnDestroy(bool bInOwnerFinished) override;
	virtual FString GetDebugString() const override;

	/** IGameplayActuationStateProvider */
	virtual FConstStructView GetActuationState() const override
	{
		return FConstStructView::Make(ActuationState);
	}

	/** IGameplayTaskTransition */
	virtual FGameplayTransitionCompleted& GetTransitionCompleted() override { return OnTransitionCompleted; };

	UFUNCTION()
	void OnSectionEndTimeReached(UContextualAnimSceneInstance* SceneInstance);

	void SharedInitAndApply();
	void TransitionToSection();

	FGameplayActuationState_ContextualAnim ActuationState;
	
	FGameplayTransitionCompleted OnTransitionCompleted;

	UPROPERTY(BlueprintAssignable)
	FGenericGameplayTaskDelegate OnRequestFailed;

	UPROPERTY(BlueprintAssignable)
	FGameplayTaskActuationCompleted OnCompleted;

	UPROPERTY(Replicated)
	int32 SectionIdx = INDEX_NONE;

	UPROPERTY(Replicated)
	int32 AnimSetIdx = INDEX_NONE;

	UPROPERTY(Replicated)
	TArray<FTransform> Pivots;

	UPROPERTY(Replicated)
	TObjectPtr<const UContextualAnimSceneAsset> SceneAsset = nullptr;

	UPROPERTY(Replicated)
	FName InteractorRole = FName();

	UPROPERTY(Replicated)
	TObjectPtr<AActor> InteractableObject = nullptr;

	UPROPERTY(Replicated)
	FName InteractableObjectRole = FName();

	UPROPERTY(Replicated)
	FName ExitSectionName = FName();

	UPROPERTY()
	TObjectPtr<UContextualAnimSceneInstance> SceneInstance = nullptr;

	UPROPERTY()
	TObjectPtr<const UGameplayActuationComponent> ActuationComponent = nullptr;

	/** Rebuilt locally from replicated data */
	UPROPERTY()
	FContextualAnimStartSceneParams SceneParams;

	UPROPERTY()
	FTransform SafeExitPoint = FTransform::Identity;

	UPROPERTY()
	EPlayContextualAnimStatus Status = EPlayContextualAnimStatus::Unset;

	UPROPERTY()
	bool bTeleportOnTaskEnd = false;
};

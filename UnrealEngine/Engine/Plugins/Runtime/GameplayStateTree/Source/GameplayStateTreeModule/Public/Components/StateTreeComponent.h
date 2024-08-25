// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BrainComponent.h"
#include "GameplayTaskOwnerInterface.h"
#include "IStateTreeSchemaProvider.h"
#include "StateTreeReference.h"
#include "StateTreeInstanceData.h"
#include "UObject/Package.h"
#include "StateTreeComponent.generated.h"

enum class EStateTreeRunStatus : uint8;
struct FGameplayTag;
struct FStateTreeEvent;
struct FStateTreeExecutionContext;

class UStateTree;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStateTreeRunStatusChanged, EStateTreeRunStatus, StateTreeRunStatus);

UCLASS(ClassGroup = AI, HideCategories = (Activation, Collision), meta = (BlueprintSpawnableComponent))
class GAMEPLAYSTATETREEMODULE_API UStateTreeComponent : public UBrainComponent, public IGameplayTaskOwnerInterface, public IStateTreeSchemaProvider
{
	GENERATED_BODY()
public:
	UStateTreeComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// BEGIN UActorComponent overrides
	virtual void InitializeComponent() override;
	virtual void UninitializeComponent() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	// END UActorComponent overrides

	// BEGIN UBrainComponent overrides
	virtual void StartLogic() override;
	virtual void RestartLogic() override;
	virtual void StopLogic(const FString& Reason)  override;
	virtual void Cleanup() override;
	virtual void PauseLogic(const FString& Reason) override;
	virtual EAILogicResuming::Type ResumeLogic(const FString& Reason)  override;
	virtual bool IsRunning() const override;
	virtual bool IsPaused() const override;
	// END UBrainComponent overrides

	// BEGIN IGameplayTaskOwnerInterface
	virtual UGameplayTasksComponent* GetGameplayTasksComponent(const UGameplayTask& Task) const override;
	virtual AActor* GetGameplayTaskOwner(const UGameplayTask* Task) const override;
	virtual AActor* GetGameplayTaskAvatar(const UGameplayTask* Task) const override;
	virtual uint8 GetGameplayTaskDefaultPriority() const override;
	virtual void OnGameplayTaskInitialized(UGameplayTask& Task) override;
	// END IGameplayTaskOwnerInterface

	// BEGIN IStateTreeSchemaProvider
	TSubclassOf<UStateTreeSchema> GetSchema() const override;
	// END

	/**
	 * Sets whether the State Tree is started automatically on being play.
	 * This function sets the bStartLogicAutomatically property, and should be used mostly from constructions sscripts.
	 * If you wish to start the logic manually, call StartLogic(). 
	 */
	UFUNCTION(BlueprintCallable, Category = "Gameplay|StateTree")
	void SetStartLogicAutomatically(const bool bInStartLogicAutomatically);
	 
	/** Sends event to the running StateTree. */
	UFUNCTION(BlueprintCallable, Category = "Gameplay|StateTree")
	void SendStateTreeEvent(const FStateTreeEvent& Event);

	/** Sends event to the running StateTree. */
	void SendStateTreeEvent(const FGameplayTag Tag, const FConstStructView Payload = FConstStructView(), const FName Origin = FName());

	/** Returns the current run status of the StateTree. */
	UFUNCTION(BlueprintPure, Category = "Gameplay|StateTree")
	EStateTreeRunStatus GetStateTreeRunStatus() const;

	/** Called when the run status of the StateTree has changed */
	UPROPERTY(BlueprintAssignable, Category = "Gameplay|StateTree")
	FStateTreeRunStatusChanged OnStateTreeRunStatusChanged;

#if WITH_GAMEPLAY_DEBUGGER
	virtual FString GetDebugInfoString() const override;
#endif // WITH_GAMEPLAY_DEBUGGER

protected:

#if WITH_EDITORONLY_DATA
	virtual void PostLoad() override;
#endif
	
	virtual bool SetContextRequirements(FStateTreeExecutionContext& Context, bool bLogErrors = false);
	
	virtual bool CollectExternalData(const FStateTreeExecutionContext& Context, const UStateTree* StateTree, TArrayView<const FStateTreeExternalDataDesc> Descs, TArrayView<FStateTreeDataView> OutDataViews) const;
	
#if WITH_EDITORONLY_DATA
	UE_DEPRECATED(5.1, "This property has been deprecated. Use StateTreeReference instead.")
	UPROPERTY()
	TObjectPtr<UStateTree> StateTree_DEPRECATED;
#endif

	UPROPERTY(EditAnywhere, Category = AI, meta=(Schema="/Script/GameplayStateTreeModule.StateTreeComponentSchema"))
	FStateTreeReference StateTreeRef;

	/** If true, the StateTree logic is started on begin play. Otherwise StartLogic() needs to be called. */
	UPROPERTY(EditAnywhere, Category = AI)
	bool bStartLogicAutomatically = true;

	UPROPERTY(Transient)
	FStateTreeInstanceData InstanceData;

	/** if set, state tree execution is allowed */
	uint8 bIsRunning : 1;

	/** if set, execution requests will be postponed */
	uint8 bIsPaused : 1;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "StateTreeExecutionContext.h"
#include "Tasks/AITask.h"
#endif

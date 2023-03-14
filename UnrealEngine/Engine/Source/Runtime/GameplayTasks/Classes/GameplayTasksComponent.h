// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "UObject/CoreNet.h"
#include "UObject/ScriptInterface.h"
#include "Components/ActorComponent.h"
#include "EngineDefines.h"
#include "GameplayTaskOwnerInterface.h"
#include "GameplayTask.h"
#include "GameplayTaskResource.h"
#include "VisualLogger/VisualLoggerDebugSnapshotInterface.h"
#include "GameplayTasksComponent.generated.h"

class AActor;
class Error;
class FOutBunch;
class UActorChannel;

enum class EGameplayTaskEvent : uint8
{
	Add,
	Remove,
};

UENUM()
enum class EGameplayTaskRunResult : uint8
{
	/** When tried running a null-task*/
	Error,
	Failed,
	/** Successfully registered for running, but currently paused due to higher priority tasks running */
	Success_Paused,
	/** Successfully activated */
	Success_Active,
	/** Successfully activated, but finished instantly */
	Success_Finished,
};

struct FGameplayTaskEventData
{
	EGameplayTaskEvent Event;
	UGameplayTask& RelatedTask;

	FGameplayTaskEventData(EGameplayTaskEvent InEvent, UGameplayTask& InRelatedTask)
		: Event(InEvent), RelatedTask(InRelatedTask)
	{

	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnClaimedResourcesChangeSignature, FGameplayResourceSet, NewlyClaimed, FGameplayResourceSet, FreshlyReleased);

/**
*	The core ActorComponent for interfacing with the GameplayAbilities System
*/
UCLASS(ClassGroup = GameplayTasks, hidecategories = (Object, LOD, Lighting, Transform, Sockets, TextureStreaming), editinlinenew, meta = (BlueprintSpawnableComponent))
class GAMEPLAYTASKS_API UGameplayTasksComponent : public UActorComponent, public IGameplayTaskOwnerInterface, public IVisualLoggerDebugSnapshotInterface
{
	GENERATED_BODY()

private:

	friend struct FEventLock;
	int32 EventLockCounter;

	uint8 bInEventProcessingInProgress : 1;

public:
	/** Set to indicate that GameplayTasksComponent needs immediate replication. @TODO could just use ForceReplication(), but this allows initial implementation to be game specific. */
	UPROPERTY()
	uint8 bIsNetDirty:1;

protected:
	/** Indicates what's the highest priority among currently running tasks */
	uint8 TopActivePriority;

	/** Resources used by currently active tasks */
	FGameplayResourceSet CurrentlyClaimedResources;

	/** Tasks that run on simulated proxies */
	const TArray<UGameplayTask*>& GetSimulatedTasks()
	{
		return SimulatedTasks;
	}

	UE_DEPRECATED(5.1, "This will be removed in future versions. Use AddSimulatedTask or RemoveSimulatedTask to modify the array")
	TArray<UGameplayTask*>& GetSimulatedTasks_Mutable();

	/** Remove all current tasks and register the one in the passed array. It's optimal to use Add/Remove of single tasks directly if possible.*/
	void SetSimulatedTasks(const TArray<UGameplayTask*>& NewSimulatedTasks);

	/** Add a new simulated task. Returns true if the task was added to the list. Returns false if the task was already registered. */
	bool AddSimulatedTask(UGameplayTask* NewTask);

	/** Remove an existing simulated task */
	void RemoveSimulatedTask(UGameplayTask* NewTask);

	UPROPERTY()
	TArray<TObjectPtr<UGameplayTask>> TaskPriorityQueue;
	
	/** Transient array of events whose main role is to avoid
	 *	long chain of recurrent calls if an activated/paused/removed task 
	 *	wants to push/pause/kill other tasks.
	 *	Note: this TaskEvents is assumed to be used in a single thread */
	TArray<FGameplayTaskEventData> TaskEvents;

	/** Array of currently active UGameplayTask that require ticking */
	UPROPERTY()
	TArray<TObjectPtr<UGameplayTask>> TickingTasks;

	/** All known tasks (processed by this component) referenced for GC */
	UPROPERTY(transient)
	TArray<TObjectPtr<UGameplayTask>> KnownTasks;

public:
	UPROPERTY(BlueprintReadWrite, Category = "Gameplay Tasks")
	FOnClaimedResourcesChangeSignature OnClaimedResourcesChange;

	UGameplayTasksComponent(const FObjectInitializer& ObjectInitializer);
	
	UFUNCTION()
	void OnRep_SimulatedTasks(const TArray<UGameplayTask*>& PreviousSimulatedTasks);

	virtual void GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const override;
	virtual bool ReplicateSubobjects(UActorChannel *Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags) override;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void UpdateShouldTick();

	/** retrieves information whether this component should be ticking taken current
	*	activity into consideration*/
	virtual bool GetShouldTick() const;
	
	/** processes the task and figures out if it should get triggered instantly or wait
	 *	based on task's RequiredResources, Priority and ResourceOverlapPolicy */
	void AddTaskReadyForActivation(UGameplayTask& NewTask);

	void RemoveResourceConsumingTask(UGameplayTask& Task);
	void EndAllResourceConsumingTasksOwnedBy(const IGameplayTaskOwnerInterface& TaskOwner);

	bool FindAllResourceConsumingTasksOwnedBy(const IGameplayTaskOwnerInterface& TaskOwner, TArray<UGameplayTask*>& FoundTasks) const;
	
	/** finds first resource-consuming task of given name */
	UGameplayTask* FindResourceConsumingTaskByName(const FName TaskInstanceName) const;

	bool HasActiveTasks(UClass* TaskClass) const;

	FORCEINLINE FGameplayResourceSet GetCurrentlyUsedResources() const { return CurrentlyClaimedResources; }

	// BEGIN IGameplayTaskOwnerInterface
	virtual UGameplayTasksComponent* GetGameplayTasksComponent(const UGameplayTask& Task) const { return const_cast<UGameplayTasksComponent*>(this); }
	virtual AActor* GetGameplayTaskOwner(const UGameplayTask* Task) const override { return GetOwner(); }
	virtual AActor* GetGameplayTaskAvatar(const UGameplayTask* Task) const override { return GetOwner(); }
	virtual void OnGameplayTaskActivated(UGameplayTask& Task) override;
	virtual void OnGameplayTaskDeactivated(UGameplayTask& Task) override;
	// END IGameplayTaskOwnerInterface

	// ActorComponent overrides
	virtual void ReadyForReplication() override;

	UFUNCTION(BlueprintCallable, DisplayName="Run Gameplay Task", meta=(ScriptName="RunGameplayTask"), Category = "Gameplay Tasks", meta = (AutoCreateRefTerm = "AdditionalRequiredResources, AdditionalClaimedResources", AdvancedDisplay = "AdditionalRequiredResources, AdditionalClaimedResources"))
	static EGameplayTaskRunResult K2_RunGameplayTask(TScriptInterface<IGameplayTaskOwnerInterface> TaskOwner, UGameplayTask* Task, uint8 Priority, TArray<TSubclassOf<UGameplayTaskResource> > AdditionalRequiredResources, TArray<TSubclassOf<UGameplayTaskResource> > AdditionalClaimedResources);

	static EGameplayTaskRunResult RunGameplayTask(IGameplayTaskOwnerInterface& TaskOwner, UGameplayTask& Task, uint8 Priority, FGameplayResourceSet AdditionalRequiredResources, FGameplayResourceSet AdditionalClaimedResources);
	
#if WITH_GAMEPLAYTASK_DEBUG
	FString GetTickingTasksDescription() const;
	FString GetKnownTasksDescription() const;
	FString GetTasksPriorityQueueDescription() const;
	static FString GetTaskStateName(EGameplayTaskState Value);
#endif // WITH_GAMEPLAYTASK_DEBUG
	using GameplayTaskContainerType = decltype(TickingTasks);
	GameplayTaskContainerType::TConstIterator GetTickingTaskIterator() const;
	GameplayTaskContainerType::TConstIterator GetKnownTaskIterator() const;
	GameplayTaskContainerType::TConstIterator GetPriorityQueueIterator() const;
	GameplayTaskContainerType::TConstIterator GetSimulatedTaskIterator() const;

#if ENABLE_VISUAL_LOG
	virtual void GrabDebugSnapshot(FVisualLogEntry* Snapshot) const override;
	void DescribeSelfToVisLog(struct FVisualLogEntry* Snapshot) const;
#endif // ENABLE_VISUAL_LOG

protected:
	struct FEventLock
	{
		FEventLock(UGameplayTasksComponent* InOwner);
		~FEventLock();

	protected:
		UGameplayTasksComponent* Owner;
	};

	void RequestTicking();
	void ProcessTaskEvents();
	void UpdateTaskActivations();

	void SetCurrentlyClaimedResources(FGameplayResourceSet NewClaimedSet);

private:
	/** called when a task gets ended with an external call, meaning not coming from UGameplayTasksComponent mechanics */
	void OnTaskEnded(UGameplayTask& Task);

	void AddTaskToPriorityQueue(UGameplayTask& NewTask);
	void RemoveTaskFromPriorityQueue(UGameplayTask& Task);

	FORCEINLINE bool CanProcessEvents() const { return !bInEventProcessingInProgress && (EventLockCounter == 0); }

	void SetSimulatedTasksNetDirty();

	/** Tasks that run on simulated proxies */
	UPROPERTY(ReplicatedUsing = OnRep_SimulatedTasks)
	TArray<TObjectPtr<UGameplayTask>> SimulatedTasks;

};

typedef UGameplayTasksComponent::GameplayTaskContainerType::TConstIterator FConstGameplayTaskIterator;

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningAgentsManagerComponent.h"
#include "LearningArray.h"
#include "LearningAgentsController.generated.h"

/**
 * A controller provides a method for injecting actions into the learning agents system from some other existing
 * behavior, e.g. we may want to gather demonstrations from a human or AI behavior tree controlling our agent(s)
 * for imitation learning purposes.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class LEARNINGAGENTS_API ULearningAgentsController : public ULearningAgentsManagerComponent
{
	GENERATED_BODY()

public:

	// These constructors/destructors are needed to make forward declarations happy
	ULearningAgentsController();
	ULearningAgentsController(FVTableHelper& Helper);
	virtual ~ULearningAgentsController();

	/** Initializes this object to be used with the given agent interactor. */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void SetupController(ULearningAgentsInteractor* InInteractor);

	/**
	 * During this event, you should set the actions of your agents.
	 * @param AgentIds The list of agent ids to set actions for.
	 * @see LearningAgentsActions.h for the list of available actions.
	 * @see ULearningAgentsInteractor::GetAgent to get the agent corresponding to each id.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "LearningAgents")
	void SetActions(const TArray<int32>& AgentIds);

	/**
	 * Call this function when it is time to gather all the actions for your agents. This should be called roughly 
	 * whenever you are calling ULearningAgentsInteractor::EncodeObservations. This will call this controller's SetActions event.
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void EncodeActions();

	/**
	 * Calls EncodeObservations, followed by EncodeActions, followed by DecodeActions
	 */
	UFUNCTION(BlueprintCallable, Category = "LearningAgents")
	void RunController();

protected:

	/**
	 * Gets the agent interactor associated with this component.
	 * @param AgentClass The class to cast the agent interactor to (in blueprint).
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (DeterminesOutputType = "InteractorClass"))
	ULearningAgentsInteractor* GetInteractor(const TSubclassOf<ULearningAgentsInteractor> InteractorClass) const;

private:

	/** The agent interactor this controller is associated with. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "LearningAgents")
	TObjectPtr<ULearningAgentsInteractor> Interactor;

	/** Temp buffers used to record the set of agents that are valid for encoding */
	TArray<int32> ValidAgentIds;
	UE::Learning::FIndexSet ValidAgentSet;
	TBitArray<> ValidAgentStatus;
};

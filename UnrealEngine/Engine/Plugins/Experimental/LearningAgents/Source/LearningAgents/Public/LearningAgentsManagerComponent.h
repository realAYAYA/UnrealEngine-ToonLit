// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"

#include "LearningArray.h"
#include "Containers/Array.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"

#include "LearningAgentsManagerComponent.generated.h"

class ALearningAgentsManager;
class ULearningAgentsHelper;

/**
 * Base class for components which can be attached to an ALearningAgentsManager.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class LEARNINGAGENTS_API ULearningAgentsManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	// These constructors/destructors are needed to make forward declarations happy
	ULearningAgentsManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	ULearningAgentsManagerComponent(FVTableHelper& Helper);
	virtual ~ULearningAgentsManagerComponent();

	virtual void PostInitProperties() override;

	/**
	 * During this event, any additional logic required for when agents are added can be executed.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "LearningAgents")
	void AgentsAdded(const TArray<int32>& AgentIds);

	/**
	 * During this event, any additional logic required for when agents are removed can be executed.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "LearningAgents")
	void AgentsRemoved(const TArray<int32>& AgentIds);

	/**
	 * During this event, any additional logic required for when agents are reset can be executed.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "LearningAgents")
	void AgentsReset(const TArray<int32>& AgentIds);

	/**
	 * Called whenever agents are added to the parent ALearningAgentsManager object.
	 * @param AgentIds Array of agent ids which have been added
	 */
	virtual void OnAgentsAdded(const TArray<int32>& AgentIds);

	/**
	 * Called whenever agents are removed from the parent ALearningAgentsManager object.
	 * @param AgentIds Array of agent ids which have been removed
	 */
	virtual void OnAgentsRemoved(const TArray<int32>& AgentIds);

	/**
	 * Called whenever agents are reset on the parent ALearningAgentsManager object.
	 * @param AgentIds Array of agent ids which have been reset
	 */
	virtual void OnAgentsReset(const TArray<int32>& AgentIds);

	/** Returns true if this component has been setup. Otherwise, false. */
	UFUNCTION(BlueprintPure, Category = "LearningAgents")
	bool IsSetup() const;

// ----- Blueprint Convenience Functions -----
protected:

	/**
	 * Gets the agent with the given id from the manager. Calling this from blueprint with the appropriate AgentClass
	 * will automatically cast the object to the given type. If not in a blueprint, you should call the manager's
	 * GetAgent methods directly.
	 * @param AgentId The id of the agent to get.
	 * @param AgentClass The class to cast the agent object to (in blueprint).
	 * @return The agent object.
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (AgentId = "-1", DeterminesOutputType = "AgentClass"))
	UObject* GetAgent(const int32 AgentId, const TSubclassOf<UObject> AgentClass) const;

	/**
	 * Gets the agents associated with a set of ids from the manager. Calling this from blueprint with the appropriate 
	 * AgentClass will automatically cast the object to the given type. If not in a blueprint, you should call the 
	 * manager's GetAgents method directly.
	 * @param AgentIds The ids of the agents to get.
	 * @param AgentClass The class to cast the agent objects to (in blueprint).
	 * @param OutAgents The output array of agent objects.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (DeterminesOutputType = "AgentClass", DynamicOutputParam = "OutAgents"))
	void GetAgents(const TArray<int32>& AgentIds, const TSubclassOf<UObject> AgentClass, TArray<UObject*>& OutAgents) const;

	/**
	 * Gets all added agents from the manager. Calling this from blueprint with the appropriate AgentClass will 
	 * automatically cast the object to the given type.
	 * @param AgentClass The class to cast the agent objects to (in blueprint).
	 * @param OutAgents The output array of agent objects.
	 * @param OutAgentIds The output array of agent ids.
	 */
	UFUNCTION(BlueprintPure = false, Category = "LearningAgents", meta = (DeterminesOutputType = "AgentClass", DynamicOutputParam = "OutAgents"))
	void GetAllAgents(TArray<UObject*>& OutAgents, TArray<int32>& OutAgentIds, const TSubclassOf<UObject> AgentClass) const;

	/**
	 * Gets the agent manager associated with this component.
	 * @param AgentClass The class to cast the agent manager to (in blueprint).
	 */
	UFUNCTION(BlueprintPure, Category = "LearningAgents", meta = (DeterminesOutputType = "AgentManagerClass"))
	ALearningAgentsManager* GetAgentManager(const TSubclassOf<ALearningAgentsManager> AgentManagerClass) const;

// ----- Non-blueprint public interface -----
public:

	/** Gets the agent corresponding to the given id. */
	const UObject* GetAgent(const int32 AgentId) const;

	/** Gets the agent corresponding to the given id. */
	UObject* GetAgent(const int32 AgentId);

	/** Checks if the component has the given agent id */
	bool HasAgent(const int32 AgentId) const;

	/** Gets the associated agent manager assuming it exists. */
	const ALearningAgentsManager* GetAgentManager() const;

	/** Gets the associated agent manager assuming it exists. */
	ALearningAgentsManager* GetAgentManager();

	/** Checks if the component has an agent manager. */
	bool HasAgentManager() const;

// ----- Helpers -----
public:

	/**
	 * Used by objects derived from ULearningAgentsHelper to add themselves to this component during their creation.
	 * You shouldn't need to call this directly.
	 */
	void AddHelper(TObjectPtr<ULearningAgentsHelper> Object);

protected:

	/** True if this component has been setup. Otherwise, false. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "LearningAgents")
	bool bIsSetup = false;

	/** The associated manager this component is attached to. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "LearningAgents")
	TObjectPtr<ALearningAgentsManager> Manager;

	/** The list of current helper objects. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "LearningAgents")
	TArray<TObjectPtr<ULearningAgentsHelper>> HelperObjects;
};

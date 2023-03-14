// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityManager.h"
#include "MassProcessingTypes.h"
#include "Async/TaskGraphInterfaces.h"
#include "MassCommandBuffer.h"
#include "MassProcessorDependencySolver.h"
#include "MassProcessor.generated.h"


struct FMassProcessingPhaseConfig;
class UMassCompositeProcessor;
struct FMassDebugger;

enum class EProcessorCompletionStatus : uint8
{
	Invalid,
	Threaded,
	Postponed,
	Done
};

USTRUCT()
struct FMassProcessorExecutionOrder
{
	GENERATED_BODY()

	/** Determines which processing group this processor will be placed in. Leaving it empty ("None") means "top-most group for my ProcessingPhase" */
	UPROPERTY(EditAnywhere, Category = Processor, config)
	FName ExecuteInGroup = FName();

	UPROPERTY(EditAnywhere, Category = Processor, config)
	TArray<FName> ExecuteBefore;

	UPROPERTY(EditAnywhere, Category = Processor, config)
	TArray<FName> ExecuteAfter;
};


UCLASS(abstract, EditInlineNew, CollapseCategories, config = Mass, defaultconfig, ConfigDoNotCheckDefaults)
class MASSENTITY_API UMassProcessor : public UObject
{
	GENERATED_BODY()
public:
	UMassProcessor();
	UMassProcessor(const FObjectInitializer& ObjectInitializer);

	virtual void Initialize(UObject& Owner) {}
	virtual FGraphEventRef DispatchProcessorTasks(const TSharedPtr<FMassEntityManager>& EntityManager, FMassExecutionContext& ExecutionContext, const FGraphEventArray& Prerequisites = FGraphEventArray());

	EProcessorExecutionFlags GetExecutionFlags() const { return (EProcessorExecutionFlags)ExecutionFlags; }

	/** Whether this processor should execute according the CurrentExecutionFlags parameters */
	bool ShouldExecute(const EProcessorExecutionFlags CurrentExecutionFlags) const { return (GetExecutionFlags() & CurrentExecutionFlags) != EProcessorExecutionFlags::None; }
	void CallExecute(FMassEntityManager& EntityManager, FMassExecutionContext& Context);

	bool AllowDuplicates() const { return bAllowDuplicates; }

	virtual void DebugOutputDescription(FOutputDevice& Ar, int32 Indent = 0) const;
	virtual FString GetProcessorName() const { return GetName(); }

	//----------------------------------------------------------------------//
	// Ordering functions 
	//----------------------------------------------------------------------//
	virtual EMassProcessingPhase GetProcessingPhase() const { return ProcessingPhase; }
	virtual void SetProcessingPhase(EMassProcessingPhase Phase) { ProcessingPhase = Phase; }
	bool DoesRequireGameThreadExecution() const { return bRequiresGameThreadExecution; }
	
	const FMassProcessorExecutionOrder& GetExecutionOrder() const { return ExecutionOrder; }

	/** By default fetches requirements declared entity queries registered via RegisterQuery. Processors can override 
	 *	this function to supply additional requirements */
	virtual void ExportRequirements(FMassExecutionRequirements& OutRequirements) const;

	const FMassSubsystemRequirements& GetProcessorRequirements() const { return ProcessorRequirements; }

	/** Adds Query to RegisteredQueries list. Query is required to be a member variable of this processor. Not meeting
	 *  this requirement will cause check failure and the query won't be registered. */
	void RegisterQuery(FMassEntityQuery& Query);

	bool ShouldAutoAddToGlobalList() const { return bAutoRegisterWithProcessingPhases; }
#if WITH_EDITOR
	bool ShouldShowUpInSettings() const { return ShouldAutoAddToGlobalList() || bCanShowUpInSettings; }
#endif // WITH_EDITORONLY_DATA

	/** Sets bAutoRegisterWithProcessingPhases. Setting it to true will result in this processor class being always 
	 * instantiated to be automatically evaluated every frame. @see UMassProcessingPhaseManager
	 * Note that calling this function is only valid on CDOs. Calling it on a regular instance will fail an ensure and 
	 * have no other effect, i.e. CDO's value won't change */
	void SetShouldAutoRegisterWithGlobalList(const bool bAutoRegister);

#if CPUPROFILERTRACE_ENABLED
	FString StatId;
#endif
	
protected:
	virtual void ConfigureQueries() PURE_VIRTUAL(UMassProcessor::ConfigureQueries);
	virtual void PostInitProperties() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) PURE_VIRTUAL(UMassProcessor::Execute);

protected:
	/** Whether this processor should be executed on StandAlone or Server or Client */
	UPROPERTY(EditAnywhere, Category = "Pipeline", meta = (Bitmask, BitmaskEnum = "/Script/MassEntity.EProcessorExecutionFlags"), config)
	int32 ExecutionFlags;

	/** Processing phase this processor will be automatically run as part of. */
	UPROPERTY(EditDefaultsOnly, Category = Processor, config)
	EMassProcessingPhase ProcessingPhase = EMassProcessingPhase::PrePhysics;

	/** Configures when this given processor can be executed in relation to other processors and processing groups, within its processing phase. */
	UPROPERTY(EditDefaultsOnly, Category = Processor, config)
	FMassProcessorExecutionOrder ExecutionOrder;

	/** Configures whether this processor should be automatically included in the global list of processors executed every tick (see ProcessingPhase and ExecutionOrder). */
	UPROPERTY(EditDefaultsOnly, Category = Processor, config)
	bool bAutoRegisterWithProcessingPhases = true;

	/** meant as a class property, make sure to set it in subclass' constructor. Controls whether there can be multiple 
	 *  instances of a given class in a single FMassRuntimePipeline */
	bool bAllowDuplicates = false;

	UPROPERTY(EditDefaultsOnly, Category = Processor, config)
	bool bRequiresGameThreadExecution = false;

#if WITH_EDITORONLY_DATA
	/** Used to permanently remove a given processor class from PipeSetting's listing. Used primarily for test-time 
	 *  processor classes, but can also be used by project-specific code to prune the processor list. */
	UPROPERTY(config)
	bool bCanShowUpInSettings = true;
#endif // WITH_EDITORONLY_DATA

	friend UMassCompositeProcessor;
	friend FMassDebugger;

	/** A query representing elements this processor is accessing in Execute function outside of query execution */
	FMassSubsystemRequirements ProcessorRequirements;

private:
	/** Stores processor's queries registered via RegisterQuery. 
	 *  @note that it's safe to store pointers here since RegisterQuery does verify that a given registered query is 
	 *  a member variable of a given processor */
	TArray<FMassEntityQuery*> OwnedQueries;
};


UCLASS()
class MASSENTITY_API UMassCompositeProcessor : public UMassProcessor
{
	GENERATED_BODY()

	friend FMassDebugger;
public:
	struct FDependencyNode
	{
		FName Name;
		UMassProcessor* Processor = nullptr;
		TArray<int32> Dependencies;
	};

public:
	UMassCompositeProcessor();

	void SetChildProcessors(TArray<UMassProcessor*>&& InProcessors);

	virtual void Initialize(UObject& Owner) override;
	virtual void DebugOutputDescription(FOutputDevice& Ar, int32 Indent = 0) const override;
	virtual void SetProcessingPhase(EMassProcessingPhase Phase) override;

	void SetGroupName(FName NewName);
	FName GetGroupName() const { return GroupName; }

	virtual void CopyAndSort(const FMassProcessingPhaseConfig& PhaseConfig, const FString& DependencyGraphFileName = FString());
	void SetProcessors(TArrayView<UMassProcessor*> InProcessorInstances, const FString& DependencyGraphFileName = FString());

	/** adds SubProcessor to an appropriately named group. If RequestedGroupName == None then SubProcessor
	 *  will be added directly to ChildPipeline. If not then the indicated group will be searched for in ChildPipeline 
	 *  and if it's missing it will be created and AddGroupedProcessor will be called recursively */
	void AddGroupedProcessor(FName RequestedGroupName, UMassProcessor& SubProcessor);

	virtual FGraphEventRef DispatchProcessorTasks(const TSharedPtr<FMassEntityManager>& EntityManager, FMassExecutionContext& ExecutionContext, const FGraphEventArray& Prerequisites = FGraphEventArray()) override;

	bool IsEmpty() const { return ChildPipeline.IsEmpty(); }

	virtual FString GetProcessorName() const override { return GroupName.ToString(); }

protected:
	virtual void ConfigureQueries() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	/**
	 *  Adds processors in OrderedProcessors to ChildPipeline and builds flat processing graph that's being used in multithreaded mode.
	 */
	void Populate(TConstArrayView<FProcessorDependencySolver::FOrderInfo> OrderedProcessors);

	/** RequestedGroupName can indicate a multi-level group name, like so: A.B.C
	 *  We need to extract the highest-level group name ('A' in the example), and see if it already exists. 
	 *  If not, create it. 
	 *  @param RequestedGroupName name of the group for which we want to find or create the processor.
	 *  @param OutRemainingGroupName contains the group name after cutting the high-level group. In the used example it
	 *    will contain "B.C". This value is then used to recursively create subgroups */
	UMassCompositeProcessor* FindOrAddGroupProcessor(FName RequestedGroupName, FString* OutRemainingGroupName = nullptr);

protected:
	UPROPERTY(VisibleAnywhere, Category=Mass)
	FMassRuntimePipeline ChildPipeline;

	/** Group name that will be used when resolving processor dependencies and grouping */
	UPROPERTY()
	FName GroupName;

	TArray<FDependencyNode> ProcessingFlatGraph;

	struct FProcessorCompletion
	{
		FGraphEventRef CompletionEvent;
		EProcessorCompletionStatus Status = EProcessorCompletionStatus::Invalid;

		bool IsDone() const 
		{
			return Status == EProcessorCompletionStatus::Done || (CompletionEvent.IsValid() && CompletionEvent->IsComplete());
		}

		void Wait()
		{
			if (CompletionEvent.IsValid())
			{
				CompletionEvent->Wait();
			}
		}
	};
	TArray<FProcessorCompletion> CompletionStatus;
};

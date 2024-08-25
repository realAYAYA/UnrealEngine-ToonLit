// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextScheduleGraphTask.h"
#include "Scheduler/AnimNextSchedulePortTask.h"
#include "Scheduler/AnimNextScheduleExternalTask.h"
#include "Scheduler/AnimNextScheduleParamScopeTask.h"
#include "Scheduler/AnimNextScheduleExternalParamTask.h"
#include "Tasks/Task.h"
#include "Scheduler/IAnimNextScheduleTermInterface.h"
#include "AnimNextSchedule.generated.h"

class UAnimNextGraph;
class UAnimNextParameterBlock;
class UAnimNextSchedule;
class UAnimNextSchedulerWorldSubsystem;
class UAnimNextComponent;
struct FAnimNextSchedulerEntry;
struct FAnimNextParam;

namespace UE::AnimNext
{
	struct FScheduler;
	struct FSchedulerImpl;
	struct FScheduleResult;
	struct FScheduleContext;
	struct FScheduleInstanceData;
	struct FScheduleTickFunction;
}

namespace UE::AnimNext::UncookedOnly
{
	class FModule;
	struct FUtils;
}

USTRUCT()
struct FAnimNextScheduleEntryTerm
{
	GENERATED_BODY()

	FAnimNextScheduleEntryTerm() = default;
	
	FAnimNextScheduleEntryTerm(FName InName, const FAnimNextParamType& InType, EScheduleTermDirection InDirection)
		: Name(InName)
		, Type(InType)
		, Direction(InDirection)
	{}

	UPROPERTY(EditAnywhere, Category = "Term")
	FName Name;

	UPROPERTY(EditAnywhere, Category = "Term")
	FAnimNextParamType Type;

	UPROPERTY(EditAnywhere, Category = "Term")
	EScheduleTermDirection Direction = EScheduleTermDirection::Input;
};

UCLASS(MinimalAPI, EditInlineNew, Abstract)
class UAnimNextScheduleEntry : public UObject
{
	GENERATED_BODY()
};

UCLASS(MinimalAPI, DisplayName="Graph")
class UAnimNextScheduleEntry_AnimNextGraph : public UAnimNextScheduleEntry
{
	GENERATED_BODY()

private:
	friend struct UE::AnimNext::UncookedOnly::FUtils;

	// UObject interface
	virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps) override;

	// The graph to run by default
	UPROPERTY(EditAnywhere, Category = "Graph")
	TObjectPtr<UAnimNextGraph> Graph = nullptr;

	// Parameter to get the graph from dynamically
	UPROPERTY(EditAnywhere, Category = "Graph", meta = (CustomWidget = "ParamName", AllowedParamType = "TObjectPtr<UAnimNextGraph>", AllowNone))
	FName DynamicGraph;

	// An optional entry point to use when running the supplied graph
	UPROPERTY(EditAnywhere, Category = "Graph", meta = (CustomWidget = "ParamName", AllowedParamType = "FName", AllowNone))
	FName EntryPoint;

	// All parameters that are required by this graph to run (only required if dynamic as static graph params can be discovered by the compiler)
	UPROPERTY(EditAnywhere, Category = "Graph", meta = (CustomWidget = "ParamName"))
	TArray<FAnimNextParam> RequiredParameters;

	// The intermediate terms used by the graph
	UPROPERTY(EditAnywhere, Category = "Graph")
	TArray<FAnimNextScheduleEntryTerm> Terms;
};

UCLASS(MinimalAPI, DisplayName="Port")
class UAnimNextScheduleEntry_Port : public UAnimNextScheduleEntry
{
	GENERATED_BODY()

private:
	friend struct UE::AnimNext::UncookedOnly::FUtils;

	// The type of the port to use
	UPROPERTY(EditAnywhere, Category = "Port", meta = (ShowDisplayNames))
	TSubclassOf<UAnimNextSchedulePort> Port;

	// The intermediate terms used by this port
	UPROPERTY(EditAnywhere, Category = "Port")
	TArray<FAnimNextScheduleEntryTerm> Terms;
};

UCLASS(MinimalAPI, DisplayName="External")
class UAnimNextScheduleEntry_ExternalTask : public UAnimNextScheduleEntry
{
	GENERATED_BODY()

private:
	friend struct UE::AnimNext::UncookedOnly::FUtils;

	// The external task we wrap
	UPROPERTY(EditAnywhere, Category = "External Task", meta = (CustomWidget = "ParamName", AllowedParamType = "FAnimNextExternalTaskBinding"))
	FName ExternalTask;
};

UCLASS(MinimalAPI, DisplayName="Scope")
class UAnimNextScheduleEntry_ParamScope : public UAnimNextScheduleEntry
{
	GENERATED_BODY()

private:
	friend struct UE::AnimNext::UncookedOnly::FUtils;

	// UObject interface
	virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps) override;

	// The scope to use
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (CustomWidget = "ParamName", AllowedParamType = "FAnimNextScope"))
	FName Scope;

	// Parameters to apply in this scope
	UPROPERTY(EditAnywhere, Category = "Parameters")
	TArray<TObjectPtr<UAnimNextParameterBlock>> ParameterBlocks;

	// Entries that are part of this scope
	UPROPERTY(EditAnywhere, Category = "Parameters", Instanced)
	TArray<TObjectPtr<UAnimNextScheduleEntry>> SubEntries;
};

// This entry is inserted only by the compilation machinery
UCLASS(MinimalAPI, DisplayName="External Parameters [INTERNAL]")
class UAnimNextScheduleEntry_ExternalParams : public UAnimNextScheduleEntry
{
	GENERATED_BODY()

private:
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	
	// Parameter sources to use
	UPROPERTY()
	TArray<FAnimNextScheduleExternalParameterSource> ParameterSources;

	// Whether the task can run on a worker thread
	UPROPERTY()
	bool bThreadSafe = false;

};

// TEMP: opcode in schedule
UENUM()
enum class EAnimNextScheduleScheduleOpcode : uint8
{
	None,
	RunGraphTask,			// Operand = Task Index
	BeginRunExternalTask,	// Operand = Task Index
	EndRunExternalTask,		// Operand = Task Index
	RunPort,				// Operand = Port Index
	RunParamScopeEntry,		// Operand = Scope Index
	RunParamScopeExit,		// Operand = Scope Index
	RunExternalParamTask,	// Operand = Task Index
	PrerequisiteTask,		// Operand = Task Index
	PrerequisiteBeginExternalTask, // Operand = Task Index
	PrerequisiteEndExternalTask, // Operand = Task Index
	PrerequisiteScopeEntry,	// Operand = Scope Index
	PrerequisiteScopeExit,	// Operand = Scope Index
	PrerequisiteExternalParamTask, // Operand = Task Index
	Exit,					// Operand = 0
};

// TEMP: Bytecode instruction in schedule
USTRUCT()
struct FAnimNextScheduleInstruction
{
	GENERATED_BODY()

	UPROPERTY()
	EAnimNextScheduleScheduleOpcode Opcode = EAnimNextScheduleScheduleOpcode::None;

	UPROPERTY()
	int32 Operand = INDEX_NONE;
};

UENUM()
enum class EAnimNextScheduleInitMethod : uint8
{
	// Do not perform any initial update, set up data structures only
	None,

	// Set up data structures, perform an initial update and then pause
	InitializeAndPause,

	// Set up data structures, perform an initial update and then pause in editor only, otherwise act like InitializeAndRun
	InitializeAndPauseInEditor,

	// Set up data structures then continue updating
	InitializeAndRun
};

UCLASS()
class ANIMNEXT_API UAnimNextSchedule : public UObject
{
	GENERATED_BODY()

private:
	friend struct UE::AnimNext::FScheduler;
	friend struct UE::AnimNext::FSchedulerImpl;
	friend struct UE::AnimNext::FScheduleContext;
	friend struct UE::AnimNext::FScheduleInstanceData;
	friend struct UE::AnimNext::FScheduleTickFunction;
	friend struct FAnimNextSchedulerEntry;
	friend class UAnimNextComponent;
	friend class UAnimNextSchedulerWorldSubsystem;
	friend class UE::AnimNext::UncookedOnly::FModule;
	friend struct UE::AnimNext::UncookedOnly::FUtils;

	// UObject interface
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
	UE_DEPRECATED(5.4, "Implement the version that takes FAssetRegistryTagsContext instead.")
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps) override;

	void HandlePackageDone(const FEndLoadPackageContext& Context);

	// Compile the editor data into a compact runtime representation
	void CompileSchedule();
#endif

#if WITH_EDITORONLY_DATA
	// Function hook used to compile in editor/cooker
	static TUniqueFunction<void(UAnimNextSchedule*)> CompileFunction;

	// Function hook used to expose to asset registry
	static TUniqueFunction<void(const UAnimNextSchedule*, FAssetRegistryTagsContext)> GetAssetRegistryTagsFunction;
	
	// Editor only
	// TODO: move this into an editor only subobject
	// TODO: this is currently only a linear list, we want it to be a graph
	UPROPERTY(EditAnywhere, Category = "Schedule", Instanced)
	TArray<TObjectPtr<UAnimNextScheduleEntry>> Entries;
#endif

	// TEMP: Instructions derived from the entries above
	UPROPERTY(NonTransactional)
	TArray<FAnimNextScheduleInstruction> Instructions;

	// TEMP: Tasks derived from the entries above
	UPROPERTY(NonTransactional)
	TArray<FAnimNextScheduleGraphTask> GraphTasks;

	// TEMP: Ports derived from the entries above
	UPROPERTY(NonTransactional)
	TArray<FAnimNextSchedulePortTask> Ports;

	// TEMP: External tasks derived from the entries above
	UPROPERTY(NonTransactional)
	TArray<FAnimNextScheduleExternalTask> ExternalTasks;

	// TEMP: Parameter scope entry tasks derived from the entries above
	UPROPERTY(NonTransactional)
	TArray<FAnimNextScheduleParamScopeEntryTask> ParamScopeEntryTasks;

	// TEMP: Parameter scope exit tasks derived from the entries above
	UPROPERTY(NonTransactional)
	TArray<FAnimNextScheduleParamScopeExitTask> ParamScopeExitTasks;

	// TEMP: External parameter tasks derived from the entries above
	UPROPERTY(NonTransactional)
	TArray<FAnimNextScheduleExternalParamTask> ExternalParamTasks;
	
	// TEMP: Data for intermediates, defined as a property bag
	UPROPERTY(NonTransactional)
	FInstancedPropertyBag IntermediatesData;

	// TEMP: Count of total number of parameter scopes that this schedule needs to execute
	UPROPERTY(NonTransactional)
	uint32 NumParameterScopes = 0;

	// TEMP: Count of total number of tick functions that this schedule needs to execute
	UPROPERTY(NonTransactional)
	uint32 NumTickFunctions = 0;
};
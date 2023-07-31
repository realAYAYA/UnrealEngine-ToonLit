// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MassArchetypeTypes.h"
#include "MassEntityQuery.h"
#include "MassProcessorDependencySolver.h"
#include "MassProcessingTypes.h"
#include "MassProcessor.h"
#include "Types/SlateEnums.h"
#include "MassDebugger.h"


class UMassProcessor;
struct FMassArchetypeHandle;
struct FMassDebuggerModel;
struct FMassEntityManager;

enum class EMassDebuggerSelectionMode : uint8
{
	None,
	Processor,
	Archetype,
	// @todo future:
	// Fragment
	MAX
};

enum class EMassDebuggerProcessorSelection : uint8
{
	None,
	Selected,
	MAX
};

enum class EMassDebuggerProcessingGraphNodeSelection : uint8
{
	None,
	WaitFor,
	Block,
	MAX
};


struct FMassDebuggerQueryData
{
	FMassDebuggerQueryData(const FMassEntityQuery& Query, const FText& InLabel);
	FMassDebuggerQueryData(const FMassSubsystemRequirements& SubsystemRequirements, const FText& InLabel);

	FMassExecutionRequirements ExecutionRequirements;
	FText Label;

	int32 GetTotalBitsUsedCount();
	bool IsEmpty() const;
}; 

struct FMassDebuggerArchetypeData
{
	FMassDebuggerArchetypeData(const FMassArchetypeHandle& ArchetypeHandle);

	FMassArchetypeCompositionDescriptor Composition;

	/** Hash of the Compositions. */
	uint32 CompositionHash = 0;
	/** Combined hash of composition and shared fragments. */
	uint32 FullHash = 0;

	/** Archetype statistics */
	UE::Mass::Debug::FArchetypeStats ArchetypeStats;

	/** Child debugger data (same as parent, but changed in some way) */
	TArray<TSharedPtr<FMassDebuggerArchetypeData>> Children;
	/** Parent debugger data. */
	TWeakPtr<FMassDebuggerArchetypeData> Parent;


	/** Index in FMassDebuggerModel::CachedArchetypes */
	int32 Index = INDEX_NONE;
	/** Display label */
	FText Label;
	/** Display label */
	FText LabelLong;
	/** Display label tooltip */
	FText LabelTooltip;
	/** FullHash as a display string */
	FText HashLabel;
	/** Primary debug name, used for grouping derived archetypes. */
	FString PrimaryDebugName;

	/** True if the archetype is selected. */
	bool bIsSelected = false;

	int32 GetTotalBitsUsedCount() const;
};

struct FMassDebuggerProcessorData
{
	FMassDebuggerProcessorData(const UMassProcessor& InProcessor);
	FMassDebuggerProcessorData(const FMassEntityManager& EntitySubsystem, UMassProcessor& InProcessor, const TMap<FMassArchetypeHandle, TSharedPtr<FMassDebuggerArchetypeData>>& InTransientArchetypesMap);

private:
	void SetProcessor(const UMassProcessor& InProcessor);

public:
	FString Name;
	FString Label;
	uint32 ProcessorHash = 0; 
	TSharedPtr<FMassDebuggerQueryData> ProcessorRequirements;
	TArray<TSharedPtr<FMassDebuggerQueryData>> Queries;
	TArray<TSharedPtr<FMassDebuggerArchetypeData>> ValidArchetypes;
	
	EMassDebuggerProcessorSelection Selection = EMassDebuggerProcessorSelection::None;
};

struct FMassDebuggerProcessingGraphNode
{
	FMassDebuggerProcessingGraphNode(const TSharedPtr<FMassDebuggerProcessorData>& InProcessorData, const UMassCompositeProcessor::FDependencyNode& InProcessorNode);
	
	FText GetLabel() const;

	TSharedPtr<FMassDebuggerProcessorData> ProcessorData;
	TArray<int32> WaitForNodes;
	TArray<int32> BlockNodes;
	EMassDebuggerProcessingGraphNodeSelection GraphNodeSelection = EMassDebuggerProcessingGraphNodeSelection::None;
};

struct FMassDebuggerProcessingGraph
{
	FMassDebuggerProcessingGraph(const FMassDebuggerModel& DebuggerModel, UMassCompositeProcessor& InGraphOwner);

	FString Label;
	TArray<FMassDebuggerProcessingGraphNode> GraphNodes;
};


struct FMassDebuggerEnvironment
{
	explicit FMassDebuggerEnvironment(const TSharedRef<const FMassEntityManager>& InEntityManager)
		: EntityManager(InEntityManager), World(InEntityManager->GetWorld())
	{}

	bool operator==(const FMassDebuggerEnvironment& Other) const { return EntityManager == Other.EntityManager; }

	FString GetDisplayName() const;
	const FMassEntityManager* GetEntityManager() const;
	bool IsWorldValid() const { return World.IsValid(); }
	
	TWeakPtr<const FMassEntityManager> EntityManager;
	TWeakObjectPtr<UWorld> World;
};


struct FMassDebuggerModel
{
	DECLARE_MULTICAST_DELEGATE(FOnRefresh);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnProcessorsSelected, TConstArrayView<TSharedPtr<FMassDebuggerProcessorData>>, ESelectInfo::Type);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnArchetypesSelected, TConstArrayView<TSharedPtr<FMassDebuggerArchetypeData>>, ESelectInfo::Type);

	void SetEnvironment(const TSharedPtr<FMassDebuggerEnvironment>& Item);

	void RefreshAll();

	void SelectProcessor(TSharedPtr<FMassDebuggerProcessorData>& Processor);
	void SelectProcessors(TArrayView<TSharedPtr<FMassDebuggerProcessorData>> Processors, ESelectInfo::Type SelectInfo);
	void ClearProcessorSelection();

	void SelectArchetypes(TArray<TSharedPtr<FMassDebuggerArchetypeData>> SelectedArchetypes, ESelectInfo::Type SelectInfo);
	void ClearArchetypeSelection();

	bool IsCurrentEnvironment(const FMassDebuggerEnvironment& InEnvironment) const { return Environment && *Environment.Get() == InEnvironment; }
	bool IsCurrentEnvironmentValid() const { return Environment && Environment->EntityManager.IsValid(); }

	void CacheArchetypesData(TMap<FMassArchetypeHandle, TSharedPtr<FMassDebuggerArchetypeData>>& OutTransientArchetypesMap); 
	void CacheProcessorsData(const TMap<FMassArchetypeHandle, TSharedPtr<FMassDebuggerArchetypeData>>& InTransientArchetypesMap);
	void CacheProcessingGraphs();

	float MinDistanceToSelectedArchetypes(const TSharedPtr<FMassDebuggerArchetypeData>& InArchetypeData) const;

	FText GetDisplayName() const;

	void MarkAsStale();
	bool IsStale() const;

	const TSharedPtr<FMassDebuggerProcessorData>& GetProcessorDataChecked(const UMassProcessor& Processor) const;

protected:
	void StoreArchetypes(const FMassEntityManager& EntitySubsystem, TMap<FMassArchetypeHandle, TSharedPtr<FMassDebuggerArchetypeData>>& OutTransientArchetypesMap);

	void ResetSelectedArchetypes();
	void ResetSelectedProcessors();

public:
	FOnRefresh OnRefreshDelegate;
	FOnProcessorsSelected OnProcessorsSelectedDelegate;
	FOnArchetypesSelected OnArchetypesSelectedDelegate;

	EMassDebuggerSelectionMode SelectionMode = EMassDebuggerSelectionMode::None;

	TSharedPtr<FMassDebuggerEnvironment> Environment;
	TArray<TSharedPtr<FMassDebuggerProcessorData>> CachedProcessors;
	TArray<TSharedPtr<FMassDebuggerProcessorData>> SelectedProcessors;

	TArray<TSharedPtr<FMassDebuggerArchetypeData>> CachedArchetypes;
	TArray<TSharedPtr<FMassDebuggerArchetypeData>> SelectedArchetypes;

	TArray<TSharedPtr<FMassDebuggerProcessingGraph>> CachedProcessingGraphs;

	TMap<FMassArchetypeHandle, TSharedPtr<FMassDebuggerArchetypeData>> HandleToArchetypeMap;

	TArray<TArray<float>> ArchetypeDistances;

	FString EnvironmentDisplayName;
};


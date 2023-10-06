// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassProcessingTypes.h"
#include "MassEntityTypes.h"
#include "MassArchetypeTypes.h"
#include "Containers/StaticArray.h"


class UMassProcessor;

namespace EMassAccessOperation
{
	constexpr uint32 Read = 0;
	constexpr uint32 Write = 1;
	constexpr uint32 MAX = 2;
};

template<typename T>
struct TMassExecutionAccess
{
	T Read;
	T Write;

	T& operator[](const uint32 OpIndex)
	{
		check(OpIndex <= EMassAccessOperation::MAX);
		return OpIndex == EMassAccessOperation::Read ? Read : Write;
	}

	const T& operator[](const uint32 OpIndex) const
	{
		check(OpIndex <= EMassAccessOperation::MAX);
		return OpIndex == EMassAccessOperation::Read ? Read : Write;
	}

	TConstArrayView<T> AsArrayView() const { return MakeArrayView(&Read, 2); }

	bool IsEmpty() const { return Read.IsEmpty() && Write.IsEmpty(); }
};

struct MASSENTITY_API FMassExecutionRequirements
{
	void Append(const FMassExecutionRequirements& Other);
	void CountResourcesUsed();
	int32 GetTotalBitsUsedCount();
	bool IsEmpty() const;
	FMassArchetypeCompositionDescriptor AsCompositionDescriptor() const;

	TMassExecutionAccess<FMassFragmentBitSet> Fragments;
	TMassExecutionAccess<FMassChunkFragmentBitSet> ChunkFragments;
	TMassExecutionAccess<FMassSharedFragmentBitSet> SharedFragments;
	TMassExecutionAccess<FMassExternalSubsystemBitSet> RequiredSubsystems;
	FMassTagBitSet RequiredAllTags;
	FMassTagBitSet RequiredAnyTags;
	FMassTagBitSet RequiredNoneTags;
	int32 ResourcesUsedCount = INDEX_NONE;
};

struct FMassProcessorDependencySolver
{
private:
	struct FNode
	{
		FNode(const FName InName, UMassProcessor* InProcessor, const int32 InNodeIndex = INDEX_NONE) 
			: Name(InName), Processor(InProcessor), NodeIndex(InNodeIndex)
		{}

		bool IsGroup() const { return Processor == nullptr; }
		void IncreaseWaitingNodesCount(TArrayView<FNode> InAllNodes);

		FName Name = TEXT("");
		UMassProcessor* Processor = nullptr;
		TArray<int32> OriginalDependencies;
		TArray<int32> TransientDependencies;
		TArray<FName> ExecuteBefore;
		TArray<FName> ExecuteAfter;
		FMassExecutionRequirements Requirements;
		int32 NodeIndex = INDEX_NONE;
		/** indicates how often given node can be found in dependencies sequence for other nodes  */
		int32 TotalWaitingNodes = 0;
		/** 
		 * indicates how deep within dependencies graph this give node is, or in other words, what's the longest sequence 
		 * from this node to a dependency-less "parent" node 
		 */
		int32 SequencePositionIndex = 0;
		TArray<int32> SubNodeIndices;
		TArray<FMassArchetypeHandle> ValidArchetypes;
	};

	struct FResourceUsage
	{
		FResourceUsage(const TArray<FNode>& InAllNodes);

		bool CanAccessRequirements(const FMassExecutionRequirements& TestedRequirements, const TArray<FMassArchetypeHandle>& InArchetypes) const;
		void SubmitNode(const int32 NodeIndex, FNode& InOutNode);

	private:
		struct FResourceUsers
		{
			TArray<int32> Users;
		};
		
		struct FResourceAccess
		{
			TArray<FResourceUsers> Access;
		};
		
		FMassExecutionRequirements Requirements;
		TMassExecutionAccess<FResourceAccess> FragmentsAccess;
		TMassExecutionAccess<FResourceAccess> ChunkFragmentsAccess;
		TMassExecutionAccess<FResourceAccess> SharedFragmentsAccess;
		TMassExecutionAccess<FResourceAccess> RequiredSubsystemsAccess;
		TConstArrayView<FNode> AllNodesView;

		template<typename TBitSet>
		void HandleElementType(TMassExecutionAccess<FResourceAccess>& ElementAccess
			, const TMassExecutionAccess<TBitSet>& TestedRequirements, FMassProcessorDependencySolver::FNode& InOutNode, const int32 NodeIndex);

		template<typename TBitSet>
		static bool CanAccess(const TMassExecutionAccess<TBitSet>& StoredElements, const TMassExecutionAccess<TBitSet>& TestedElements);

		/** Determines whether any of the Elements' (i.e. Fragment, Tag,...) users operate on any of the archetypes given via InArchetypes */
		bool HasArchetypeConflict(TMassExecutionAccess<FResourceAccess> ElementAccess, const TArray<FMassArchetypeHandle>& InArchetypes) const;
	};

public:
	/** Optionally returned by ResolveDependencies and contains information about processors that have been pruned and 
	 *  other potentially useful bits. To be used in a transient fashion. */
	struct FResult
	{
		FString DependencyGraphFileName;
		TArray<TSubclassOf<UMassProcessor>> PrunedProcessorClasses;
		int32 MaxSequenceLength = 0;
		uint32 ArchetypeDataVersion = 0;

		void Reset()
		{
			PrunedProcessorClasses.Reset();
			MaxSequenceLength = 0;
			ArchetypeDataVersion = 0;
		}
	};

	MASSENTITY_API FMassProcessorDependencySolver(TArrayView<UMassProcessor* const> InProcessors, const bool bIsGameRuntime = true);
	MASSENTITY_API void ResolveDependencies(TArray<FMassProcessorOrderInfo>& OutResult, TSharedPtr<FMassEntityManager> EntityManager = nullptr, FResult* InOutOptionalResult = nullptr);

	MASSENTITY_API static void CreateSubGroupNames(FName InGroupName, TArray<FString>& SubGroupNames);

	/** Determines whether the dependency solving that produced InResult will produce different results if run with a given EntityManager */
	static bool IsResultUpToDate(const FResult& InResult, TSharedPtr<FMassEntityManager> EntityManager);

	bool IsSolvingForSingleThread() const { return bSingleThreadTarget; }

protected:
	// note that internals are protected rather than private to support unit testing

	/**
	 * Traverses InOutIndicesRemaining in search of the first RootNode's node that has no dependencies left. Once found 
	 * the node's index gets added to OutNodeIndices, removed from dependency lists from all other nodes and the function 
	 * quits.
	 * @return 'true' if a dependency-less node has been found and added to OutNodeIndices; 'false' otherwise.
	 */
	bool PerformSolverStep(FResourceUsage& ResourceUsage, TArray<int32>& InOutIndicesRemaining, TArray<int32>& OutNodeIndices);
	
	int32 CreateNodes(UMassProcessor& Processor);
	void BuildDependencies();
	void Solve(TArray<FMassProcessorOrderInfo>& OutResult);
	void LogNode(const FNode& Node, int Indent = 0);
	
	// @todo due to fundamental change to how nodes are organized the graph generation needs reimplementation
	// friend struct FDumpGraphDependencyUtils;
	// void DumpGraph(FArchive& LogFile) const;

	TArrayView<UMassProcessor* const> Processors;
	bool bAnyCyclesDetected = false;
	/**
	 * indicates whether we're generating processor order to be run in single- or multi-threaded environment (usually
	 * this meas Dedicated Server vs Any other configuration). In Single-Threaded mode we can skip a bunch of expensive, 
	 * fine tunning tests.
	 * @Note: currently the value depends on MASS_DO_PARALLEL and there's no way to configure it otherwise, but there's 
	 * nothing inherently stopping us from letting users configure it.
	 */
	const bool bSingleThreadTarget = bool(!MASS_DO_PARALLEL);
	const bool bGameRuntime = true;
	FString DependencyGraphFileName;
	TArray<FNode> AllNodes;
	TMap<FName, int32> NodeIndexMap;
};
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassProcessingTypes.h"
#include "MassEntityTypes.h"
#include "Containers/StaticArray.h"


class UMassProcessor;

enum class EDependencyNodeType : uint8
{
	Invalid,
	Processor,
	GroupStart,
	GroupEnd
};

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

	TMassExecutionAccess<FMassFragmentBitSet> Fragments;
	TMassExecutionAccess<FMassChunkFragmentBitSet> ChunkFragments;
	TMassExecutionAccess<FMassSharedFragmentBitSet> SharedFragments;
	TMassExecutionAccess<FMassExternalSubsystemBitSet> RequiredSubsystems;
	FMassTagBitSet RequiredAllTags;
	FMassTagBitSet RequiredAnyTags;
	FMassTagBitSet RequiredNoneTags;
	int32 ResourcesUsedCount = INDEX_NONE;
};

struct FProcessorDependencySolver
{
private:
	struct FNode
	{
		FNode(const FName InName, UMassProcessor* InProcessor, const int32 InNodeIndex = INDEX_NONE) 
			: Name(InName), Processor(InProcessor), NodeIndex(InNodeIndex)
		{}

		FName Name = TEXT("");
		UMassProcessor* Processor = nullptr;
		TArray<int32> OriginalDependencies;
		TArray<int32> TransientDependencies;
		TArray<FName> ExecuteBefore;
		TArray<FName> ExecuteAfter;
		FMassExecutionRequirements Requirements;
		int32 NodeIndex = INDEX_NONE;
		TArray<int32> SubNodeIndices;

		bool IsGroup() const { return Processor == nullptr; }
	};

	struct FResourceUsage
	{
		FResourceUsage();

		bool CanAccessRequirements(const FMassExecutionRequirements& TestedRequirements) const;
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

		template<typename TBitSet>
		static void HandleElementType(TMassExecutionAccess<FResourceAccess>& ElementAccess
			, const TMassExecutionAccess<TBitSet>& TestedRequirements, FProcessorDependencySolver::FNode& InOutNode, const int32 NodeIndex);

		template<typename TBitSet>
		static bool CanAccess(const TMassExecutionAccess<TBitSet>& StoredElements, const TMassExecutionAccess<TBitSet>& TestedElements);
	};

public:
	struct FOrderInfo
	{
		FName Name = TEXT("");
		UMassProcessor* Processor = nullptr;
		EDependencyNodeType NodeType = EDependencyNodeType::Invalid;
		TArray<FName> Dependencies;
	};

	MASSENTITY_API FProcessorDependencySolver(TArrayView<UMassProcessor*> InProcessors, const FName Name, const FString& InDependencyGraphFileName = FString());
	MASSENTITY_API void ResolveDependencies(TArray<FOrderInfo>& OutResult);

	MASSENTITY_API static void CreateSubGroupNames(FName InGroupName, TArray<FString>& SubGroupNames);

protected:
	// note that internals are protected rather than private to support unit testing

	/**
	 * Traverses InOutIndicesRemaining in search of the first RootNode's node that has no dependencies left. Once found 
	 * the node's index gets added to OutNodeIndices, removed from dependency lists from all other nodes and the function 
	 * quits.
	 * @return 'true' if a dependency-less node has been found and added to OutNodeIndices; 'false' otherwise.
	 */
	bool PerformSolverStep(FResourceUsage& ResourceUsage, TArray<int32>& InOutIndicesRemaining, TArray<int32>& OutNodeIndices);
	
	void CreateNodes(UMassProcessor& Processor);
	void BuildDependencies();
	void Solve(TArray<FProcessorDependencySolver::FOrderInfo>& OutResult);
	void LogNode(const FNode& Node, int Indent = 0);
	
	// @todo due to fundamental change to how nodes are organized the graph generation needs reimplementation
	// friend struct FDumpGraphDependencyUtils;
	// void DumpGraph(FArchive& LogFile) const;

	TArrayView<UMassProcessor*> Processors;
	bool bAnyCyclesDetected = false;
	FString DependencyGraphFileName;
	FName CollectionName;
	TArray<FNode> AllNodes;
	TMap<FName, int32> NodeIndexMap;
};
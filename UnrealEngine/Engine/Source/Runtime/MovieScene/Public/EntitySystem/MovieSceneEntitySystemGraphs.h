// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/TaskGraphInterfaces.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/BitArray.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/SortedMap.h"
#include "Containers/SparseArray.h"
#include "Containers/UnrealString.h"
#include "EntitySystem/MovieSceneEntitySystemDirectedGraph.h"
#include "EntitySystem/MovieSceneEntitySystemTypes.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Math/NumericLimits.h"
#include "Misc/AssertionMacros.h"
#include "Templates/Casts.h"
#include "Templates/SharedPointer.h"
#include "Templates/SubclassOf.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"

#include "MovieSceneEntitySystemGraphs.generated.h"

class FReferenceCollector;
class UMovieSceneEntitySystem;
class UMovieSceneEntitySystemLinker;
template <typename FuncType> class TFunctionRef;

namespace UE
{
namespace MovieScene
{

	struct FSystemSubsequentTasks;
	struct FSystemTaskPrerequisites;


} // namespace MovieScene
} // namespace UE


USTRUCT()
struct FMovieSceneEntitySystemGraphNode
{
	GENERATED_BODY();

	FMovieSceneEntitySystemGraphNode()
		: System(nullptr)
	{}

	explicit FMovieSceneEntitySystemGraphNode(UMovieSceneEntitySystem* InSystem)
		: System(InSystem)
	{}

	TSharedPtr<UE::MovieScene::FSystemTaskPrerequisites> Prerequisites;
	TSharedPtr<UE::MovieScene::FSystemTaskPrerequisites> SubsequentTasks;

	UPROPERTY()
	TObjectPtr<UMovieSceneEntitySystem> System;
};

USTRUCT()
struct FMovieSceneEntitySystemGraphNodes
{
	GENERATED_BODY()

	void AddStructReferencedObjects(FReferenceCollector& Collector) const;

	TSparseArray<FMovieSceneEntitySystemGraphNode> Array;
};
template<>
struct TStructOpsTypeTraits<FMovieSceneEntitySystemGraphNodes> : public TStructOpsTypeTraitsBase2<FMovieSceneEntitySystemGraphNodes>
{
	enum { WithAddStructReferencedObjects = true };
};


USTRUCT()
struct MOVIESCENE_API FMovieSceneEntitySystemGraph
{
	using FDirectionalEdge = FMovieSceneEntitySystemDirectedGraph::FDirectionalEdge;

	GENERATED_BODY()

	void AddPrerequisite(UMovieSceneEntitySystem* Upstream, UMovieSceneEntitySystem* Downstream);

	void AddReference(UMovieSceneEntitySystem* FromReference, UMovieSceneEntitySystem* ToReference);

	void RemoveReference(UMovieSceneEntitySystem* FromReference, UMovieSceneEntitySystem* ToReference);

	/** Olog(n) time */
	template<typename Allocator>
	void GatherReferencesFrom(const UMovieSceneEntitySystem* FromReference, TArray<UMovieSceneEntitySystem*, Allocator>& OutReferences)
	{
		IterateReferences(FromReference, [&OutReferences](UMovieSceneEntitySystem* System){ OutReferences.Add(System); });
	}

	template<typename Iter>
	void IterateReferencesFrom(const UMovieSceneEntitySystem* FromReference, Iter&& Iterator)
	{
		check(GetGraphID(FromReference) != TNumericLimits<uint16>::Max());
		for (const FDirectionalEdge& Edge : ReferenceGraph.GetEdgesFrom(GetGraphID(FromReference)))
		{
			Iterator(Nodes.Array[Edge.ToNode]);
		}
	}

	/** O(n) time */
	template<typename Allocator>
	void GatherReferencesTo(const UMovieSceneEntitySystem* ToReference, TArray<UMovieSceneEntitySystem*, Allocator>& OutReferences)
	{
		IterateReferences(ToReference, [&OutReferences](UMovieSceneEntitySystem* System){ OutReferences.Add(System); });
	}
	template<typename Iter>
	void IterateReferencesTo(const UMovieSceneEntitySystem* ToReference, Iter&& Iterator)
	{
		const uint16 ToNode = GetGraphID(ToReference);
		check(ToNode != TNumericLimits<uint16>::Max());
		for (const FDirectionalEdge& Edge : ReferenceGraph.GetEdges())
		{
			if (Edge.ToNode == ToNode)
			{
				Iterator(Nodes.Array[Edge.FromNode]);
			}
		}
	}

	bool IsEmpty() const
	{
		return Nodes.Array.Num() == 0;
	}

	bool HasReferencesTo(const UMovieSceneEntitySystem* ToReference) const
	{
		return ReferenceGraph.HasEdgeTo(GetGraphID(ToReference));
	}

	bool HasReferencesFrom(const UMovieSceneEntitySystem* FromReference) const
	{
		return ReferenceGraph.HasEdgeFrom(GetGraphID(FromReference));
	}

	void AddSystem(UMovieSceneEntitySystem* InSystem);

	int32 NumSubsequents(UMovieSceneEntitySystem* InSystem) const;

	void RemoveSystem(UMovieSceneEntitySystem* InSystem);

	int32 RemoveIrrelevantSystems(UMovieSceneEntitySystemLinker* Linker);

	void Shutdown();

	void ExecutePhase(UE::MovieScene::ESystemPhase Phase, UMovieSceneEntitySystemLinker* Linker, FGraphEventArray& OutTasks);

	void IteratePhase(UE::MovieScene::ESystemPhase Phase, TFunctionRef<void(UMovieSceneEntitySystem*)> InIter);

	TArray<UMovieSceneEntitySystem*> GetSystems() const;

	template<typename SystemType>
	SystemType* FindSystemOfType() const
	{
		return CastChecked<SystemType>(FindSystemOfType(SystemType::StaticClass()), ECastCheckedType::NullAllowed);
	}

	UMovieSceneEntitySystem* FindSystemOfType(TSubclassOf<UMovieSceneEntitySystem> InClassType) const;

	void DebugPrint() const;

	FString ToString() const;

private:

	// Implementation function that means we don't need to #include the entity system
	static uint16 GetGraphID(const UMovieSceneEntitySystem* InSystem);

	void UpdateCache();

	template<typename ArrayType>
	void ExecutePhase(const ArrayType& RetrieveEntries, UMovieSceneEntitySystemLinker* Linker, FGraphEventArray& OutTasks);

private:
	friend UE::MovieScene::FSystemSubsequentTasks;

	TArray<uint16, TInlineAllocator<4>>  SpawnPhase;
	TArray<uint16, TInlineAllocator<8>>  InstantiationPhase;
	TArray<uint16, TInlineAllocator<16>> EvaluationPhase;
	TArray<uint16, TInlineAllocator<2>>  FinalizationPhase;

	UPROPERTY()
	FMovieSceneEntitySystemGraphNodes Nodes;

	FMovieSceneEntitySystemDirectedGraph FlowGraph;
	FMovieSceneEntitySystemDirectedGraph ReferenceGraph;

	uint32 SerialNumber = 0;
	uint32 PreviousSerialNumber = 0;
	uint32 ReentrancyGuard = 0;
};
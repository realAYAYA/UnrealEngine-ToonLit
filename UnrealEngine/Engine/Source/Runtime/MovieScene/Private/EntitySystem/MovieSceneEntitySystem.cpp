// Copyright Epic Games, Inc. All Rights Reserved.

#include "EntitySystem/MovieSceneEntitySystem.h"
#include "EntitySystem/MovieSceneEntitySystemTypes.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneComponentRegistry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneEntitySystem)

namespace UE
{
namespace MovieScene
{

struct FSystemDependencyGraph
{
	uint16 GetGraphID(UClass* Class)
	{
		const uint16* Existing = GraphIDsByClass.Find(Class->GetFName());
		if (Existing)
		{
			return *Existing;
		}

		const int32 NewIndex = Nodes.Add(FNode::FromClass(Class));
		check(NewIndex >= 0 && NewIndex < TNumericLimits<uint16>::Max());

		const uint16 NewGraphID = static_cast<uint16>(NewIndex);

		ImplicitPrerequisites.AllocateNode(NewGraphID);
		ImplicitSubsequents.AllocateNode(NewGraphID);

		GraphIDsByClass.Add(Class->GetFName(), NewGraphID);

		return NewGraphID;
	}

	uint16 GetGraphID(FComponentTypeID InComponentType)
	{
		const uint16* Existing = GraphIDsByComponent.Find(InComponentType);
		if (Existing)
		{
			return *Existing;
		}

		const int32 NewIndex = Nodes.Add(FNode::FromComponent(InComponentType));
		check(NewIndex >= 0 && NewIndex < TNumericLimits<uint16>::Max());

		const uint16 NewGraphID = static_cast<uint16>(NewIndex);

		ImplicitPrerequisites.AllocateNode(NewGraphID);
		ImplicitSubsequents.AllocateNode(NewGraphID);

		GraphIDsByComponent.Add(InComponentType, NewGraphID);

		return NewGraphID;
	}

	void SetupDependencies(UMovieSceneEntitySystem* ThisSystem, UMovieSceneEntitySystemLinker* Linker)
	{
		using FDirectionalEdge = FMovieSceneEntitySystemDirectedGraph::FDirectionalEdge;

		const uint16 FromNode = ThisSystem->GetGlobalDependencyGraphID();

		// Set up prerequisites
		for (FDirectionalEdge Edge : ImplicitPrerequisites.GetEdgesFrom(FromNode))
		{
			FNode Node = Nodes[Edge.ToNode];

			// Follow edges from components
			if (Node.WriteComponentType)
			{
				for (FDirectionalEdge ComponentEdge : ImplicitPrerequisites.GetEdgesFrom(Edge.ToNode))
				{
					// Components shouldn't be connected to other components
					SetupPrereq(Nodes[ComponentEdge.ToNode].Class.Get(), ThisSystem, Linker);
				}
			}
			else
			{
				SetupPrereq(Node.Class.Get(), ThisSystem, Linker);
			}
		}

		// Set up subsequents
		for (FDirectionalEdge Edge : ImplicitSubsequents.GetEdgesFrom(FromNode))
		{
			FNode Node = Nodes[Edge.ToNode];

			// Follow edges from components
			if (Node.WriteComponentType)
			{
				for (FDirectionalEdge ComponentEdge : ImplicitSubsequents.GetEdgesFrom(Edge.ToNode))
				{
					// Components shouldn't be connected to other components
					SetupSubsequent(ThisSystem, Nodes[ComponentEdge.ToNode].Class.Get(), Linker);
				}
			}
			else
			{
				SetupSubsequent(ThisSystem, Node.Class.Get(), Linker);
			}
		}
	}

	void SetupPrereq(UClass* PrereqType, UMovieSceneEntitySystem* ThisSystem, UMovieSceneEntitySystemLinker* Linker)
	{
		if (PrereqType)
		{
			if (UMovieSceneEntitySystem* TargetSystem = Linker->FindSystem(PrereqType))
			{
				Linker->SystemGraph.AddPrerequisite(TargetSystem, ThisSystem);
			}
		}
	}
	void SetupSubsequent(UMovieSceneEntitySystem* ThisSystem, UClass* SubsequentType, UMovieSceneEntitySystemLinker* Linker)
	{
		if (SubsequentType)
		{
			if (UMovieSceneEntitySystem* TargetSystem = Linker->FindSystem(SubsequentType))
			{
				Linker->SystemGraph.AddPrerequisite(ThisSystem, TargetSystem);
			}
		}
	}

	void MakeRelationship(uint16 UpstreamGraphID, uint16 DownstreamGraphID)
	{
		ImplicitSubsequents.MakeEdge(UpstreamGraphID, DownstreamGraphID);
		ImplicitPrerequisites.MakeEdge(DownstreamGraphID, UpstreamGraphID);
	}

	UClass* ClassFromGraphID(uint16 GraphID) const
	{
		check(Nodes.IsValidIndex(GraphID));
		return Nodes[GraphID].Class.Get();
	}

	uint16 NumGraphIDs() const
	{
		return Nodes.Num();
	}

private:

	TMap<FName, uint16> GraphIDsByClass;
	TMap<FComponentTypeID, uint16> GraphIDsByComponent;

	struct FNode
	{
		static FNode FromClass(UClass* InClass)
		{
			return FNode{ InClass, FComponentTypeID::Invalid() };
		}
		static FNode FromComponent(FComponentTypeID InComponent)
		{
			return FNode{ nullptr, InComponent };
		}

		TWeakObjectPtr<UClass> Class;
		FComponentTypeID WriteComponentType;
	};

	TArray<FNode> Nodes;
	FMovieSceneEntitySystemDirectedGraph ImplicitPrerequisites;
	FMovieSceneEntitySystemDirectedGraph ImplicitSubsequents;
};

FSystemDependencyGraph GlobalDependencyGraph;

TMap<FName, TStatId> SystemStats;

} // namespace MovieScene
} // namespace UE

UMovieSceneEntitySystem::UMovieSceneEntitySystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	using namespace UE::MovieScene;

	bSystemIsEnabled = true;
	SystemCategories = EEntitySystemCategory::Unspecified;

	Phase = ESystemPhase::Evaluation;
	GraphID = TNumericLimits<uint16>::Max();

	if (!GetClass()->HasAnyClassFlags(CLASS_Abstract))
	{
		GlobalDependencyGraphID = GlobalDependencyGraph.GetGraphID(GetClass());
	}
	else
	{
		GlobalDependencyGraphID = MAX_uint16;
	}

#if STATS || ENABLE_STATNAMEDEVENTS
	const TStatId* ExistingStat = SystemStats.Find(GetClass()->GetFName());

	if (ExistingStat)
	{
		StatID = *ExistingStat;
	}
	else
	{
#if STATS
		TStatId NewStatID = FDynamicStats::CreateStatId<STAT_GROUP_TO_FStatGroup(STATGROUP_MovieSceneECS)>(GetClass()->GetName());
#else
		// Just use the base UObject stat ID if we only have named events
		TStatId NewStatID = GetStatID(true /* bForDeferredUse */);
#endif

		StatID = NewStatID;
		SystemStats.Add(GetClass()->GetFName(), NewStatID);
	}
#endif
}

UMovieSceneEntitySystem::~UMovieSceneEntitySystem()
{
}

void UMovieSceneEntitySystem::DefineImplicitPrerequisite(TSubclassOf<UMovieSceneEntitySystem> UpstreamSystemType, TSubclassOf<UMovieSceneEntitySystem> DownstreamSystemType)
{
	using namespace UE::MovieScene;

	const uint16 UpstreamGlobalDependencyGraphID   = GlobalDependencyGraph.GetGraphID(UpstreamSystemType.Get());
	const uint16 DownstreamGlobalDependencyGraphID = GlobalDependencyGraph.GetGraphID(DownstreamSystemType.Get());

	GlobalDependencyGraph.MakeRelationship(UpstreamGlobalDependencyGraphID, DownstreamGlobalDependencyGraphID);
}

void UMovieSceneEntitySystem::DefineComponentProducer(TSubclassOf<UMovieSceneEntitySystem> ThisClassType, FComponentTypeID ComponentType)
{
	using namespace UE::MovieScene;

	const uint16 UpstreamGlobalDependencyGraphID   = GlobalDependencyGraph.GetGraphID(ThisClassType.Get());
	const uint16 DownstreamGlobalDependencyGraphID = GlobalDependencyGraph.GetGraphID(ComponentType);

	GlobalDependencyGraph.MakeRelationship(UpstreamGlobalDependencyGraphID, DownstreamGlobalDependencyGraphID);
}

void UMovieSceneEntitySystem::DefineComponentConsumer(TSubclassOf<UMovieSceneEntitySystem> ThisClassType, FComponentTypeID ComponentType)
{
	using namespace UE::MovieScene;

	const uint16 UpstreamGlobalDependencyGraphID   = GlobalDependencyGraph.GetGraphID(ComponentType);
	const uint16 DownstreamGlobalDependencyGraphID = GlobalDependencyGraph.GetGraphID(ThisClassType.Get());

	GlobalDependencyGraph.MakeRelationship(UpstreamGlobalDependencyGraphID, DownstreamGlobalDependencyGraphID);
}

void UMovieSceneEntitySystem::LinkRelevantSystems(UMovieSceneEntitySystemLinker* InLinker)
{
	using namespace UE::MovieScene;

	for (uint16 GraphID = 0; GraphID < GlobalDependencyGraph.NumGraphIDs(); ++GraphID)
	{
		if (InLinker->HasLinkedSystem(GraphID))
		{
			continue;
		}

		UClass* Class = GlobalDependencyGraph.ClassFromGraphID(GraphID);
		UMovieSceneEntitySystem* SystemCDO = Class ? Cast<UMovieSceneEntitySystem>(Class->GetDefaultObject()) : nullptr;

		if (SystemCDO && InLinker->GetSystemFilter().CheckSystem(SystemCDO))
		{
			SystemCDO->ConditionalLinkSystem(InLinker);
		}
	}
}

void UMovieSceneEntitySystem::LinkCategorySystems(UMovieSceneEntitySystemLinker* InLinker, UE::MovieScene::EEntitySystemCategory InCategory)
{
	using namespace UE::MovieScene;

	for (uint16 GraphID = 0; GraphID < GlobalDependencyGraph.NumGraphIDs(); ++GraphID)
	{
		if (InLinker->HasLinkedSystem(GraphID))
		{
			continue;
		}

		UClass* Class = GlobalDependencyGraph.ClassFromGraphID(GraphID);
		if (Class == UMovieSceneEntitySystem::StaticClass())
		{
			// Ignore the base class
			continue;
		}

		UMovieSceneEntitySystem* SystemCDO = Class ? Cast<UMovieSceneEntitySystem>(Class->GetDefaultObject()) : nullptr;
		if (SystemCDO && EnumHasAnyFlags(SystemCDO->SystemCategories, InCategory))
		{
			InLinker->LinkSystemIfAllowed(Class);
		}
	}
}

void UMovieSceneEntitySystem::LinkAllSystems(UMovieSceneEntitySystemLinker* InLinker)
{
	using namespace UE::MovieScene;

	for (uint16 GraphID = 0; GraphID < GlobalDependencyGraph.NumGraphIDs(); ++GraphID)
	{
		if (InLinker->HasLinkedSystem(GraphID))
		{
			continue;
		}

		UClass* Class = GlobalDependencyGraph.ClassFromGraphID(GraphID);
		// Ignore the base class
		if (Class && Class != UMovieSceneEntitySystem::StaticClass())
		{
			InLinker->LinkSystemIfAllowed(Class);
		}
	}
}

UE::MovieScene::EEntitySystemCategory UMovieSceneEntitySystem::RegisterCustomSystemCategory()
{
	using namespace UE::MovieScene;

	static EEntitySystemCategory NextCustomCategory = EEntitySystemCategory::Custom;

	EEntitySystemCategory Result = NextCustomCategory;
	NextCustomCategory = (EEntitySystemCategory)((uint32)NextCustomCategory << 1);
	check(NextCustomCategory != EEntitySystemCategory::Last);
	return Result;
}

bool UMovieSceneEntitySystem::IsRelevant(UMovieSceneEntitySystemLinker* InLinker) const
{
	if (RelevantComponent && InLinker->EntityManager.ContainsComponent(RelevantComponent))
	{
		return true;
	}

	return IsRelevantImpl(InLinker);
}

bool UMovieSceneEntitySystem::IsRelevantImpl(UMovieSceneEntitySystemLinker* InLinker) const
{
	return false;
}

void UMovieSceneEntitySystem::ConditionalLinkSystem(UMovieSceneEntitySystemLinker* InLinker) const
{
	check(HasAnyFlags(RF_ClassDefaultObject));
	ConditionalLinkSystemImpl(InLinker);
}

void UMovieSceneEntitySystem::ConditionalLinkSystemImpl(UMovieSceneEntitySystemLinker* InLinker) const
{
	if (IsRelevant(InLinker))
	{
		InLinker->LinkSystem(GetClass());
	}
}

void UMovieSceneEntitySystem::Enable()
{
	bSystemIsEnabled = true;
}

void UMovieSceneEntitySystem::Disable()
{
	bSystemIsEnabled = false;
}

void UMovieSceneEntitySystem::TagGarbage()
{
	OnTagGarbage();
}

void UMovieSceneEntitySystem::CleanTaggedGarbage()
{
	OnCleanTaggedGarbage();
}

bool UMovieSceneEntitySystem::IsReadyForFinishDestroy()
{
	return Linker == nullptr;
}

void UMovieSceneEntitySystem::Abandon()
{
	Linker = nullptr;
	GraphID = TNumericLimits<uint16>::Max();
}

void UMovieSceneEntitySystem::FinishDestroy()
{
	checkf(!Linker, TEXT("System being destroyed without Unlink being called"));

	Super::FinishDestroy();
}

void UMovieSceneEntitySystem::Run(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	if (!bSystemIsEnabled)
	{
		return;
	}

#if STATS || ENABLE_STATNAMEDEVENTS
	FScopeCycleCounter Scope(StatID);
#endif

	checkf(Linker != nullptr, TEXT("Attempting to evaluate a system that has been unlinked!"));

	// We may have erroneously linked a system we should have done, but we must not run it in this case
	if (!Linker->GetSystemFilter().CheckSystem(this))
	{
		return;
	}

	Linker->EntityManager.IncrementSystemSerial();

	UE_LOG(LogMovieSceneECS, Verbose, TEXT("Running moviescene system for phase %d: %s"), (int32)Phase, *GetName());
	OnRun(InPrerequisites, Subsequents);
}

void UMovieSceneEntitySystem::Link(UMovieSceneEntitySystemLinker* InLinker)
{
	using namespace UE::MovieScene;

	check(GraphID != TNumericLimits<uint16>::Max());

	Linker = InLinker;
	OnLink();

	GlobalDependencyGraph.SetupDependencies(this, InLinker);
	Linker->SystemLinked(this);
}

void UMovieSceneEntitySystem::Unlink()
{
	if (GraphID != TNumericLimits<uint16>::Max())
	{
		Linker->SystemGraph.RemoveSystem(this);
	}

	OnUnlink();

	Linker->SystemUnlinked(this);
	Linker = nullptr;
}


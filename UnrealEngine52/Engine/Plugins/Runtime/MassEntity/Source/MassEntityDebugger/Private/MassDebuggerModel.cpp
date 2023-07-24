// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassDebuggerModel.h"
#include "MassProcessor.h"
#include "MassEntityManager.h"
#include "MassEntityQuery.h"
#include "Engine/Engine.h"
#include "MassDebugger.h"
#include "MassDebuggerSettings.h"
#include "UObject/UObjectIterator.h"
#include "Containers/UnrealString.h"

#define LOCTEXT_NAMESPACE "SMassDebugger"

namespace UE::Mass::Debugger::Private
{
	template<typename TBitSet>
	int32 BitSetDistance(const TBitSet& A, const TBitSet& B)
	{
		return (A - B).CountStoredTypes() + (B - A).CountStoredTypes();
	}

	float CalcArchetypeBitDistance(FMassDebuggerArchetypeData& A, FMassDebuggerArchetypeData& B)
	{
		int32 TotalLength = A.Composition.CountStoredTypes() +  B.Composition.CountStoredTypes();

		check(TotalLength > 0);

		return float(BitSetDistance(A.Composition.Fragments, B.Composition.Fragments)
			+ BitSetDistance(A.Composition.Tags, B.Composition.Tags)
			+ BitSetDistance(A.Composition.ChunkFragments, B.Composition.ChunkFragments)
			+ BitSetDistance(A.Composition.SharedFragments, B.Composition.SharedFragments)) 
			/ TotalLength;
	}

	void MakeDisplayName(const FString& InName, FString& OutDisplayName)
	{
		OutDisplayName = InName;
		if (GET_MASSDEBUGGER_CONFIG_VALUE(bStripMassPrefix) == false)
		{
			return;
		}
		
		OutDisplayName.RemoveFromStart(TEXT("Mass"), ESearchCase::CaseSensitive);
	}

	uint32 CalcProcessorHash(const UMassProcessor& Processor)
	{
		return PointerHash(&Processor);
	}

	/** We're ignoring all the CDO processors (since as such are not being run at runtime) as well ass processors owned
	 *  by a CDO, for the very same reason. */
	bool IsDebuggableProcessor(const UWorld* ContextWorld, const UMassProcessor& Processor)
	{
		return IsValid(&Processor)
			&& Processor.HasAnyFlags(RF_ClassDefaultObject) == false
			&& Processor.GetWorld() == ContextWorld 
			// checking ContextWorld is a cheaper way of supporting the declared behavior, since if there is a world then
			// the processors are definitelly not CDO owned (by design). Is there is no world we need to check specifically.
			&& (ContextWorld != nullptr || Processor.GetOuter()->HasAnyFlags(RF_ClassDefaultObject) == false);
	}
} // namespace UE::Mass::Debugger::Private

//----------------------------------------------------------------------//
// FMassDebuggerEnvironment
//----------------------------------------------------------------------//
FString FMassDebuggerEnvironment::GetDisplayName() const
{
	FString DisplayName;

#if WITH_MASSENTITY_DEBUG
	if (const FMassEntityManager* EntityManagerPtr = GetEntityManager())
	{
		DisplayName += EntityManagerPtr->DebugGetName();
		if (DisplayName.Len())
		{
			DisplayName += TEXT(" - ");
		}
	}
#endif // WITH_MASSENTITY_DEBUG
	
	const UWorld* WorldPtr = World.Get();
	DisplayName += WorldPtr ? WorldPtr->GetPathName() : TEXT("No World");
	return DisplayName;
}

const FMassEntityManager* FMassDebuggerEnvironment::GetEntityManager() const
{
	return EntityManager.Pin().Get();
}

//----------------------------------------------------------------------//
// FMassDebuggerQueryData
//----------------------------------------------------------------------//
FMassDebuggerQueryData::FMassDebuggerQueryData(const FMassEntityQuery& Query, const FText& InLabel)
	: Label(InLabel)
{
#if WITH_MASSENTITY_DEBUG
	FMassDebugger::GetQueryExecutionRequirements(Query, ExecutionRequirements);
#endif // WITH_MASSENTITY_DEBUG
}

FMassDebuggerQueryData::FMassDebuggerQueryData(const FMassSubsystemRequirements& SubsystemRequirements, const FText& InLabel)
	: Label(InLabel)
{
#if WITH_MASSENTITY_DEBUG
	SubsystemRequirements.ExportRequirements(ExecutionRequirements);
#endif // WITH_MASSENTITY_DEBUG
}

int32 FMassDebuggerQueryData::GetTotalBitsUsedCount() 
{
	return ExecutionRequirements.GetTotalBitsUsedCount();
}

bool FMassDebuggerQueryData::IsEmpty() const
{
	return ExecutionRequirements.IsEmpty();
}

//----------------------------------------------------------------------//
// FMassDebuggerProcessorData
//----------------------------------------------------------------------//
FMassDebuggerProcessorData::FMassDebuggerProcessorData(const UMassProcessor& InProcessor)
{
	SetProcessor(InProcessor);
#if WITH_MASSENTITY_DEBUG
	TConstArrayView<FMassEntityQuery*> ProcessorQueries = FMassDebugger::GetProcessorQueries(InProcessor);

	ProcessorRequirements = MakeShareable(new FMassDebuggerQueryData(InProcessor.GetProcessorRequirements(), LOCTEXT("MassProcessorRequirementsLabel", "Processor Requirements")));

	Queries.Reserve(ProcessorQueries.Num());
	for (const FMassEntityQuery* Query : ProcessorQueries)
	{
		check(Query);
		Queries.Add(MakeShareable(new FMassDebuggerQueryData(*Query, LOCTEXT("MassEntityQueryLabel", "Query"))));
	}
#endif // WITH_MASSENTITY_DEBUG
}

FMassDebuggerProcessorData::FMassDebuggerProcessorData(const FMassEntityManager& EntityManager, UMassProcessor& InProcessor, const TMap<FMassArchetypeHandle, TSharedPtr<FMassDebuggerArchetypeData>>& InTransientArchetypesMap)
{
	SetProcessor(InProcessor);
#if WITH_MASSENTITY_DEBUG
	TConstArrayView<FMassEntityQuery*> ProcessorQueries = FMassDebugger::GetUpToDateProcessorQueries(EntityManager, InProcessor);

	ProcessorRequirements = MakeShareable(new FMassDebuggerQueryData(InProcessor.GetProcessorRequirements(), LOCTEXT("MassProcessorRequirementsLabel", "Processor Requirements")));

	Queries.Reserve(ProcessorQueries.Num());
	for (const FMassEntityQuery* Query : ProcessorQueries)
	{
		check(Query);
		Queries.Add(MakeShareable(new FMassDebuggerQueryData(*Query, LOCTEXT("MassEntityQueryLabel", "Query"))));

		for (const FMassArchetypeHandle& ArchetypeHandle : Query->GetArchetypes())
		{
			ValidArchetypes.Add(InTransientArchetypesMap.FindChecked(ArchetypeHandle));
		}
	}
#endif // WITH_MASSENTITY_DEBUG
}

void FMassDebuggerProcessorData::SetProcessor(const UMassProcessor& InProcessor)
{
	Name = InProcessor.GetProcessorName();
	UE::Mass::Debugger::Private::MakeDisplayName(Name, Label);

	ProcessorHash = UE::Mass::Debugger::Private::CalcProcessorHash(InProcessor);
}

//----------------------------------------------------------------------//
// FMassDebuggerArchetypeData
//----------------------------------------------------------------------//
FMassDebuggerArchetypeData::FMassDebuggerArchetypeData(const FMassArchetypeHandle& ArchetypeHandle)
{
#if WITH_MASSENTITY_DEBUG
	Composition = FMassDebugger::GetArchetypeComposition(ArchetypeHandle);

	// @todo should ensure we're using same hashing as the EntityManager here
	CompositionHash = Composition.CalculateHash();
	FullHash = CompositionHash;

	FString FullHashAsString;
	BytesToHexLower(reinterpret_cast<const uint8*>(&FullHash), sizeof(FullHash), FullHashAsString);
	HashLabel = FText::FromString(FullHashAsString);

	FMassDebugger::GetArchetypeEntityStats(ArchetypeHandle, ArchetypeStats);

	const TConstArrayView<FName> DebugNames = FMassDebugger::GetArchetypeDebugNames(ArchetypeHandle);

	if (DebugNames.IsEmpty())
	{
		// This archetype has no associated debug names, use hash as name.
		FString HashAsString; 
		BytesToHexLower(reinterpret_cast<const uint8*>(&CompositionHash), sizeof(CompositionHash), HashAsString);
		PrimaryDebugName = HashAsString;

		// Use first fragment as name
		if (FMassFragmentBitSet::FIndexIterator It = Composition.Fragments.GetIndexIterator())
		{
			const FName FirstStructName = Composition.Fragments.DebugGetStructTypeName(*It);
			TStringBuilder<256> StringBuilder;
			StringBuilder.Append(FirstStructName.ToString());
			StringBuilder.Append(TEXT("..."));
			Label = FText::FromString(StringBuilder.ToString());
		}
		else
		{
			Label = FText::FromString(HashAsString);
		}
		
		LabelLong = Label;
	}
	else
	{
		PrimaryDebugName = DebugNames[0].ToString();

		TStringBuilder<256> StringBuilder;

		// Short label for lists
		StringBuilder.Reset();
		StringBuilder.Append(DebugNames[0].ToString());
		if (DebugNames.Num() > 1)
		{
			StringBuilder.Append(TEXT("..."));
		}
		Label = FText::FromString(StringBuilder.ToString());

		// Longer label for info display
		StringBuilder.Reset();
		for (int i = 0; i < DebugNames.Num(); i++)
		{
			if (i > 0)
			{
				StringBuilder.Append(TEXT(", "));
			}
			StringBuilder.Append(DebugNames[i].ToString());
		}
		LabelLong = FText::FromString(StringBuilder.ToString());

		// Label tooltip
		StringBuilder.Reset();
		for (int i = 0; i < DebugNames.Num(); i++)
		{
			if (i > 0)
			{
				StringBuilder.Append(TEXT("\n"));
			}
			StringBuilder.Append(DebugNames[i].ToString());
		}
		LabelTooltip = FText::FromString(StringBuilder.ToString());
	}

#endif // WITH_MASSENTITY_DEBUG
}

int32 FMassDebuggerArchetypeData::GetTotalBitsUsedCount() const
{
	return Composition.CountStoredTypes();
}

//----------------------------------------------------------------------//
// FMassDebuggerProcessingGraphNode
//----------------------------------------------------------------------//
FMassDebuggerProcessingGraphNode::FMassDebuggerProcessingGraphNode(const TSharedPtr<FMassDebuggerProcessorData>& InProcessorData, const UMassCompositeProcessor::FDependencyNode& InProcessorNode)
	: ProcessorData(InProcessorData)
{
	if (InProcessorNode.Processor == nullptr)
	{
		return;
	}

	WaitForNodes = InProcessorNode.Dependencies;
}

FText FMassDebuggerProcessingGraphNode::GetLabel() const
{
	if (ProcessorData.IsValid())
	{
		return FText::FromString(ProcessorData->Label);
	}

	return LOCTEXT("InvalidProcessor", "Invalid");
}

//----------------------------------------------------------------------//
// FMassDebuggerProcessingGraph
//----------------------------------------------------------------------//
FMassDebuggerProcessingGraph::FMassDebuggerProcessingGraph(const FMassDebuggerModel& DebuggerModel, UMassCompositeProcessor& InGraphOwner)
{
	Label = InGraphOwner.GetProcessorName();
#if WITH_MASSENTITY_DEBUG
	TConstArrayView<UMassCompositeProcessor::FDependencyNode> ProcessingGraph = FMassDebugger::GetProcessingGraph(InGraphOwner);

	if (ProcessingGraph.Num() > 0)
	{
		GraphNodes.Reserve(ProcessingGraph.Num());
		for (const UMassCompositeProcessor::FDependencyNode& Node : ProcessingGraph)
		{
			check(Node.Processor);
			const TSharedPtr<FMassDebuggerProcessorData>& ProcessorData = DebuggerModel.GetProcessorDataChecked(*Node.Processor);
			check(ProcessorData.IsValid());
			GraphNodes.Add(FMassDebuggerProcessingGraphNode(ProcessorData, Node));
		}
	}
	// it's possible for the graph to be empty if InGraphOwner has been populated for a single-thread execution.
	// See if there are any processors owned by InGraphOwner.
	else if (InGraphOwner.IsEmpty() == false)
	{
		TConstArrayView<TObjectPtr<UMassProcessor>> HostedProcessors = FMassDebugger::GetHostedProcessors(InGraphOwner);
		for (const TObjectPtr<UMassProcessor>& Processor : HostedProcessors)
		{
			check(Processor);
			const TSharedPtr<FMassDebuggerProcessorData>& ProcessorData = DebuggerModel.GetProcessorDataChecked(*Processor);
			check(ProcessorData.IsValid());
			GraphNodes.Add(FMassDebuggerProcessingGraphNode(ProcessorData));
		}

		// if we have processors, but the flat processing graph is empty, it means it's a single-threaded composite processor
		bSingleTheadGraph = true;
	}
#endif // WITH_MASSENTITY_DEBUG
}

//----------------------------------------------------------------------//
// FMassDebuggerModel
//----------------------------------------------------------------------//
FMassDebuggerModel::FMassDebuggerModel()
{
#if WITH_MASSENTITY_DEBUG
	OnEntitySelectedHandle = FMassDebugger::OnEntitySelectedDelegate.AddRaw(this, &FMassDebuggerModel::OnEntitySelected);
#endif // WITH_MASSENTITY_DEBUG
}

FMassDebuggerModel::~FMassDebuggerModel()
{
#if WITH_MASSENTITY_DEBUG
	if (OnEntitySelectedHandle.IsValid())
	{
		FMassDebugger::OnEntitySelectedDelegate.Remove(OnEntitySelectedHandle);
	}
#endif // WITH_MASSENTITY_DEBUG
}

void FMassDebuggerModel::SetEnvironment(const TSharedPtr<FMassDebuggerEnvironment>& Item)
{
	if (Item)
	{
		Environment = Item;
		EnvironmentDisplayName = Item->GetDisplayName();
	}
	else
	{
		Environment = nullptr;
		EnvironmentDisplayName.Reset();
	}

	RefreshAll();
}

void FMassDebuggerModel::RefreshAll()
{
	TMap<FMassArchetypeHandle, TSharedPtr<FMassDebuggerArchetypeData>> TransientArchetypesMap;

	CacheArchetypesData(TransientArchetypesMap);
	CacheProcessorsData(TransientArchetypesMap);
	CacheProcessingGraphs();

	ClearArchetypeSelection();

	OnRefreshDelegate.Broadcast();
}

void FMassDebuggerModel::SelectProcessor(TSharedPtr<FMassDebuggerProcessorData>& Processor)
{
	SelectProcessors(MakeArrayView(&Processor, 1), ESelectInfo::Direct);
}

void FMassDebuggerModel::SelectProcessors(TArrayView<TSharedPtr<FMassDebuggerProcessorData>> Processors, ESelectInfo::Type SelectInfo)
{
	SelectionMode = EMassDebuggerSelectionMode::Processor;

	ResetSelectedProcessors();
	ResetSelectedArchetypes();

	SelectedProcessors = Processors;

	for (TSharedPtr<FMassDebuggerProcessorData>& ProcessorData : SelectedProcessors)
	{
		check(ProcessorData.IsValid());
		ProcessorData->Selection = EMassDebuggerProcessorSelection::Selected;

		for (TSharedPtr<FMassDebuggerArchetypeData>& ArchetypeData : ProcessorData->ValidArchetypes)
		{
			SelectedArchetypes.AddUnique(ArchetypeData);
			ArchetypeData->bIsSelected = true;
		}
	}

	OnProcessorsSelectedDelegate.Broadcast(SelectedProcessors, SelectInfo);
}

void FMassDebuggerModel::ClearProcessorSelection()
{
	SelectionMode = EMassDebuggerSelectionMode::None;

	ResetSelectedProcessors();

	OnProcessorsSelectedDelegate.Broadcast(SelectedProcessors, ESelectInfo::Direct);
}

void FMassDebuggerModel::SelectArchetypes(TArrayView<TSharedPtr<FMassDebuggerArchetypeData>> InSelectedArchetypes, ESelectInfo::Type SelectInfo)
{
	ResetSelectedProcessors();
	ResetSelectedArchetypes();

	SelectionMode = EMassDebuggerSelectionMode::Archetype;

	SelectedArchetypes = InSelectedArchetypes;

	for (TSharedPtr<FMassDebuggerProcessorData>& ProcessorData : CachedProcessors)
	{
		check(ProcessorData.IsValid());
		for (const TSharedPtr<FMassDebuggerArchetypeData>& ArchetypeData : InSelectedArchetypes)
		{
			if (ProcessorData->ValidArchetypes.Find(ArchetypeData) != INDEX_NONE)
			{
				ProcessorData->Selection = EMassDebuggerProcessorSelection::Selected;
				SelectedProcessors.Add(ProcessorData);
				break;
			}
		}
	}

	OnArchetypesSelectedDelegate.Broadcast(SelectedArchetypes, SelectInfo);
}

void FMassDebuggerModel::ClearArchetypeSelection()
{
	SelectionMode = EMassDebuggerSelectionMode::None;

	ResetSelectedArchetypes();
	OnArchetypesSelectedDelegate.Broadcast(SelectedArchetypes, ESelectInfo::Direct);
}

void FMassDebuggerModel::CacheProcessorsData(const TMap<FMassArchetypeHandle, TSharedPtr<FMassDebuggerArchetypeData>>& InTransientArchetypesMap)
{
	CachedProcessors.Reset();

	UWorld* World = Environment ? Environment->World.Get() : nullptr;
	const FMassEntityManager* EntityManager = Environment ? Environment->GetEntityManager() : nullptr;

	if (EntityManager)
	{
		for (FThreadSafeObjectIterator It(UMassProcessor::StaticClass()); It; ++It)
		{
			UMassProcessor* Processor = Cast<UMassProcessor>(*It);
			if (Processor 
				&& Cast<UMassCompositeProcessor>(Processor) == nullptr
				&& UE::Mass::Debugger::Private::IsDebuggableProcessor(World, *Processor))
			{
				CachedProcessors.Add(MakeShareable(new FMassDebuggerProcessorData(*EntityManager, *Processor, InTransientArchetypesMap)));
			}
		}
	}
	else
	{
		for (FThreadSafeObjectIterator It(UMassProcessor::StaticClass()); It; ++It)
		{
			UMassProcessor* Processor = Cast<UMassProcessor>(*It);
			if (Processor 
				&& Cast<UMassCompositeProcessor>(Processor) == nullptr
				&& UE::Mass::Debugger::Private::IsDebuggableProcessor(World, *Processor))
			{
				CachedProcessors.Add(MakeShareable(new FMassDebuggerProcessorData(*Processor)));
			}
		}
	}

	CachedProcessors.Sort([](const TSharedPtr<FMassDebuggerProcessorData>& A, const TSharedPtr<FMassDebuggerProcessorData>& B)
	{
		return A->Label < B->Label;
	});
}

void FMassDebuggerModel::CacheProcessingGraphs()
{
	CachedProcessingGraphs.Reset();

	UWorld* World = Environment ? Environment->World.Get() : nullptr;
	for (FThreadSafeObjectIterator It(UMassProcessor::StaticClass()); It; ++It)
	{
		UMassCompositeProcessor* Processor = Cast<UMassCompositeProcessor>(*It);
		if (Processor && UE::Mass::Debugger::Private::IsDebuggableProcessor(World, *Processor))
		{
			CachedProcessingGraphs.Add(MakeShareable(new FMassDebuggerProcessingGraph(*this, *Processor)));
		}
	}	
}

void FMassDebuggerModel::CacheArchetypesData(TMap<FMassArchetypeHandle, TSharedPtr<FMassDebuggerArchetypeData>>& OutTransientArchetypesMap)
{
	CachedAllArchetypes.Reset();
	CachedArchetypeRepresentatives.Reset();

	if (Environment)
	{
		if (const FMassEntityManager* EntityManager = Environment->GetEntityManager())
		{
			StoreArchetypes(*EntityManager, OutTransientArchetypesMap);
		}
	}
}

float FMassDebuggerModel::MinDistanceToSelectedArchetypes(const TSharedPtr<FMassDebuggerArchetypeData>& InArchetypeData) const
{
	float MinDistance = MAX_flt;
	for (const TSharedPtr<FMassDebuggerArchetypeData>& SelectedArchetype : SelectedArchetypes)
	{
		MinDistance = FMath::Min(MinDistance, ArchetypeDistances[SelectedArchetype->Index][InArchetypeData->Index]);
	}
	return MinDistance;
}

void FMassDebuggerModel::StoreArchetypes(const FMassEntityManager& EntityManager, TMap<FMassArchetypeHandle, TSharedPtr<FMassDebuggerArchetypeData>>& OutTransientArchetypesMap)
{
#if WITH_MASSENTITY_DEBUG
	TArray<FMassArchetypeHandle> ArchetypeHandles = FMassDebugger::GetAllArchetypes(EntityManager);

	CachedAllArchetypes.Reset(ArchetypeHandles.Num());

	int32 MaxBitsUsed = 0;

	// @todo build an archetype handle map
	for (FMassArchetypeHandle& ArchetypeHandle : ArchetypeHandles)
	{
		FMassDebuggerArchetypeData* ArchetypeDataPtr = new FMassDebuggerArchetypeData(ArchetypeHandle);
		ArchetypeDataPtr->Index = CachedAllArchetypes.Add(MakeShareable(ArchetypeDataPtr));
		OutTransientArchetypesMap.Add(ArchetypeHandle, CachedAllArchetypes.Last());

		MaxBitsUsed = FMath::Max(MaxBitsUsed, ArchetypeDataPtr->GetTotalBitsUsedCount());
	}
#endif // WITH_MASSENTITY_DEBUG

	// calculate distances
	ArchetypeDistances.Reset();
	ArchetypeDistances.AddDefaulted(CachedAllArchetypes.Num());
	for (int i = 0; i < CachedAllArchetypes.Num(); ++i)
	{
		ArchetypeDistances[i].AddDefaulted(CachedAllArchetypes.Num());
	}

	for (int i = 0; i < CachedAllArchetypes.Num(); ++i)
	{
		for (int k = i + 1; k < CachedAllArchetypes.Num(); ++k)
		{
			const float Distance = UE::Mass::Debugger::Private::CalcArchetypeBitDistance(*CachedAllArchetypes[i].Get(), *CachedAllArchetypes[k].Get());
			ArchetypeDistances[i][k] = Distance;
			ArchetypeDistances[k][i] = Distance;
		}
	}

	// Add archetypes that share same primary name under the same entry. 
	TMap<FString, TSharedPtr<FMassDebuggerArchetypeData>> ArchetypeNameMap;
	for (TSharedPtr<FMassDebuggerArchetypeData>& ArchetypeData : CachedAllArchetypes)
	{
		if (const TSharedPtr<FMassDebuggerArchetypeData>* Representative = ArchetypeNameMap.Find(ArchetypeData->PrimaryDebugName))
		{
			(*Representative)->Children.Add(ArchetypeData);
			ArchetypeData->Parent = *Representative;
		}
		else
		{
			ArchetypeNameMap.Add(ArchetypeData->PrimaryDebugName, ArchetypeData);
		}
	}

	for (auto& KeyValue : ArchetypeNameMap)
	{
		CachedArchetypeRepresentatives.Add(KeyValue.Value);
	}
}

FText FMassDebuggerModel::GetDisplayName() const
{
	if (!Environment)
	{
		return LOCTEXT("PickEnvironment", "Pick Environment");
	}
	else if (IsStale())
	{
		return FText::FromString(FString::Printf(TEXT("(%s) %s")
				, *(LOCTEXT("StaleEnvironmentPrefix", "Stale").ToString())
				, *EnvironmentDisplayName));
	}

	return FText::FromString(Environment->GetDisplayName());
}

void FMassDebuggerModel::MarkAsStale()
{
	if (Environment)
	{
		Environment->World = nullptr;
	}
}

bool FMassDebuggerModel::IsStale() const 
{ 
	return Environment.IsValid() == false || (Environment->NeedsValidWorld() == true && Environment->IsWorldValid() == false);
}

const TSharedPtr<FMassDebuggerProcessorData>& FMassDebuggerModel::GetProcessorDataChecked(const UMassProcessor& Processor) const
{
	check(CachedProcessors.Num());

	const uint32 ProcessorHash = UE::Mass::Debugger::Private::CalcProcessorHash(Processor);

	const TSharedPtr<FMassDebuggerProcessorData>* DataFound = CachedProcessors.FindByPredicate([ProcessorHash](const TSharedPtr<FMassDebuggerProcessorData>& Element)
		{
			return Element->ProcessorHash == ProcessorHash;
		});

	check(DataFound);
	return *DataFound;
}

void FMassDebuggerModel::ResetSelectedArchetypes()
{
	for (TSharedPtr<FMassDebuggerArchetypeData>& ArchetypeData : SelectedArchetypes)
	{
		ArchetypeData->bIsSelected = false;
	}
	SelectedArchetypes.Reset();
}

void FMassDebuggerModel::ResetSelectedProcessors()
{
	// using CachedProcessors instead of SelectedProcessors to be on the safe side
	for (TSharedPtr<FMassDebuggerProcessorData>& ProcessorData : CachedProcessors)
	{
		check(ProcessorData.IsValid());
		ProcessorData->Selection = EMassDebuggerProcessorSelection::None;
	}
	SelectedProcessors.Reset();
}

void FMassDebuggerModel::OnEntitySelected(const FMassEntityManager& EntityManager, const FMassEntityHandle EntityHandle)
{
	if (!Environment || Environment->GetEntityManager() != &EntityManager)
	{
		// not the entity manager we're debugging right now
		return;
	}
	
	const FMassArchetypeHandle ArchetypeHandle = EntityManager.GetArchetypeForEntity(EntityHandle);
	if (ArchetypeHandle.IsValid() == false)
	{
		return;
	}

#if WITH_MASSENTITY_DEBUG
	const uint32 ArchetypeHash = FMassDebugger::GetArchetypeComposition(ArchetypeHandle).CalculateHash();
#else
	const uint32 ArchetypeHash = 0;
#endif // WITH_MASSENTITY_DEBUG
	TSharedPtr<FMassDebuggerArchetypeData>* DebuggerArchetypeData = CachedAllArchetypes.FindByPredicate([ArchetypeHash](const TSharedPtr<FMassDebuggerArchetypeData>& Element)
		{
			return Element.IsValid() && Element->CompositionHash == ArchetypeHash;
		});

	if (DebuggerArchetypeData)
	{
		SelectArchetypes(MakeArrayView(DebuggerArchetypeData, 1), ESelectInfo::Direct);
	}
}

#undef LOCTEXT_NAMESPACE

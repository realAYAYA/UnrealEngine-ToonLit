// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassProcessorDependencySolver.h"
#include "MassProcessingTypes.h"
#include "MassProcessor.h"
#include "Logging/MessageLog.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "Mass"

namespace UE::Mass::Private
{
	FString NameViewToString(TConstArrayView<FName> View)
	{
		if (View.Num() == 0)
		{
			return TEXT("[]");
		}
		FString ReturnVal = FString::Printf(TEXT("[%s"), *View[0].ToString());
		for (int i = 1; i < View.Num(); ++i)
		{
			ReturnVal += FString::Printf(TEXT(", %s"), *View[i].ToString());
		}
		return ReturnVal + TEXT("]");
	}

	bool DoArchetypeContainersOverlap(TConstArrayView<FMassArchetypeHandle> A, const TArray<FMassArchetypeHandle>& B)
	{
		for (const FMassArchetypeHandle& HandleA : A)
		{
			if (B.Contains(HandleA))
			{
				return true;
			}
		}
		return false;
	}
}

//----------------------------------------------------------------------//
//  FMassExecutionRequirements
//----------------------------------------------------------------------//
void FMassExecutionRequirements::Append(const FMassExecutionRequirements& Other)
{
	for (int i = 0; i < EMassAccessOperation::MAX; ++i)
	{
		Fragments[i] += Other.Fragments[i];
		ChunkFragments[i] += Other.ChunkFragments[i];
		SharedFragments[i] += Other.SharedFragments[i];
		RequiredSubsystems[i] += Other.RequiredSubsystems[i];
	}
	RequiredAllTags += Other.RequiredAllTags;
	RequiredAnyTags += Other.RequiredAnyTags;
	RequiredNoneTags += Other.RequiredNoneTags;

	// signal that it requires recalculation;
	ResourcesUsedCount = INDEX_NONE;
}

void FMassExecutionRequirements::CountResourcesUsed()
{
	ResourcesUsedCount = 0;

	for (int i = 0; i < EMassAccessOperation::MAX; ++i)
	{
		ResourcesUsedCount += Fragments[i].CountStoredTypes();
		ResourcesUsedCount += ChunkFragments[i].CountStoredTypes();
		ResourcesUsedCount += SharedFragments[i].CountStoredTypes();
		ResourcesUsedCount += RequiredSubsystems[i].CountStoredTypes();
	}
}

int32 FMassExecutionRequirements::GetTotalBitsUsedCount()
{
	CountResourcesUsed();

	return ResourcesUsedCount + RequiredAllTags.CountStoredTypes()
		+ RequiredAnyTags.CountStoredTypes() + RequiredNoneTags.CountStoredTypes();
}

bool FMassExecutionRequirements::IsEmpty() const
{
	return Fragments.IsEmpty() && ChunkFragments.IsEmpty() 
		&& SharedFragments.IsEmpty() && RequiredSubsystems.IsEmpty()
		&& RequiredAllTags.IsEmpty() && RequiredAnyTags.IsEmpty() && RequiredNoneTags.IsEmpty();
}

FMassArchetypeCompositionDescriptor FMassExecutionRequirements::AsCompositionDescriptor() const
{
	return FMassArchetypeCompositionDescriptor(Fragments.Read + Fragments.Write
		, RequiredAllTags + RequiredAnyTags
		, ChunkFragments.Read + ChunkFragments.Write
		, SharedFragments.Read + SharedFragments.Write);
}

//----------------------------------------------------------------------//
//  FProcessorDependencySolver::FResourceUsage
//----------------------------------------------------------------------//
FMassProcessorDependencySolver::FResourceUsage::FResourceUsage(const TArray<FNode>& InAllNodes)
	: AllNodesView(InAllNodes)
{
	for (int i = 0; i < EMassAccessOperation::MAX; ++i)
	{
		FragmentsAccess[i].Access.AddZeroed(FMassFragmentBitSet::GetMaxNum());
		ChunkFragmentsAccess[i].Access.AddZeroed(FMassChunkFragmentBitSet::GetMaxNum());
		SharedFragmentsAccess[i].Access.AddZeroed(FMassSharedFragmentBitSet::GetMaxNum());
		RequiredSubsystemsAccess[i].Access.AddZeroed(FMassExternalSubsystemBitSet::GetMaxNum());
	}
}

template<typename TBitSet>
void FMassProcessorDependencySolver::FResourceUsage::HandleElementType(TMassExecutionAccess<FResourceAccess>& ElementAccess
	, const TMassExecutionAccess<TBitSet>& TestedRequirements, FMassProcessorDependencySolver::FNode& InOutNode, const int32 NodeIndex)
{
	using UE::Mass::Private::DoArchetypeContainersOverlap;

	// for every bit set in TestedRequirements we do the following:
	// 1. For every read-only requirement we make InOutNode depend on the currently stored Writer of this resource
	//    - note that this operation is not destructive, meaning we don't destructively consume the data, since all 
	//      subsequent read access to the given resource will also depend on the Writer
	//    - note 2: we also fine tune what we store as a dependency for InOutNode by checking if InOutNode's archetype
	//      overlap with whoever the current Writer is 
	//    - this will result in InOutNode wait for the current Writer to finish before starting its own work and 
	//      that's exactly what we need to do to avoid accessing data while it's potentially being written
	// 2. For every read-write requirement we make InOutNode depend on all the readers and writers currently stored. 
	//    - once that's done we clean currently stored Readers and Writers since every subsequent operation on this 
	//      resource will be blocked by currently considered InOutNode (as the new Writer)
	//    - again, we do check corresponding archetype collections overlap
	//    - similarly to the read operation waiting on write operations in pt 1. we want to hold off the write 
	//      operations to be performed by InOutNode until all currently registered (and conflicting) writers and readers 
	//      are done with their operations 
	// 3. For all accessed resources we store information that InOutNode is accessing it
	//    - we do this so that the following nodes know that they'll have to wait for InOutNode if an access 
	//      conflict arises. 

	// 1. For every read only requirement we make InOutNode depend on the currently stored Writer of this resource
	for (auto It = TestedRequirements.Read.GetIndexIterator(); It; ++It)
	{
		for (int32 UserIndex : ElementAccess.Write.Access[*It].Users)
		{
			if (DoArchetypeContainersOverlap(AllNodesView[UserIndex].ValidArchetypes, InOutNode.ValidArchetypes))
			{
				InOutNode.OriginalDependencies.Add(UserIndex);
			}
		}
	}

	// 2. For every read-write requirement we make InOutNode depend on all the readers and writers currently stored. 
	for (auto It = TestedRequirements.Write.GetIndexIterator(); It; ++It)
	{
		for (int32 i = ElementAccess.Read.Access[*It].Users.Num() - 1; i >= 0; --i)
		{
			const int32 UserIndex = ElementAccess.Read.Access[*It].Users[i];
			if (DoArchetypeContainersOverlap(AllNodesView[UserIndex].ValidArchetypes, InOutNode.ValidArchetypes))
			{	
				InOutNode.OriginalDependencies.Add(UserIndex);
				ElementAccess.Read.Access[*It].Users.RemoveAtSwap(i);
			}
		}

		for (int32 i = ElementAccess.Write.Access[*It].Users.Num() - 1; i >= 0; --i)
		{
			const int32 UserIndex = ElementAccess.Write.Access[*It].Users[i];
			if (DoArchetypeContainersOverlap(AllNodesView[UserIndex].ValidArchetypes, InOutNode.ValidArchetypes))
			{
				InOutNode.OriginalDependencies.Add(UserIndex);
				ElementAccess.Write.Access[*It].Users.RemoveAtSwap(i);
			}
		}
	}

	// 3. For all accessed resources we store information that InOutNode is accessing it
	for (auto It = TestedRequirements.Read.GetIndexIterator(); It; ++It)
	{
		// mark Element at index indicated by It as being used in mode EMassAccessOperation(i) by NodeIndex
		ElementAccess.Read.Access[*It].Users.Add(NodeIndex);
	}
	for (auto It = TestedRequirements.Write.GetIndexIterator(); It; ++It)
	{
		// mark Element at index indicated by It as being used in mode EMassAccessOperation(i) by NodeIndex
		ElementAccess.Write.Access[*It].Users.Add(NodeIndex);
	}
}

template<typename TBitSet>
bool FMassProcessorDependencySolver::FResourceUsage::CanAccess(const TMassExecutionAccess<TBitSet>& StoredElements, const TMassExecutionAccess<TBitSet>& TestedElements)
{
	// see if there's an overlap of tested write operations with existing read & write operations, as well as 
	// tested read operations with existing write operations
	
	return !(
		// if someone's already writing to what I want to write
		TestedElements.Write.HasAny(StoredElements.Write)
		// or if someone's already reading what I want to write
		|| TestedElements.Write.HasAny(StoredElements.Read)
		// or if someone's already writing what I want to read
		|| TestedElements.Read.HasAny(StoredElements.Write)
	);
}

bool FMassProcessorDependencySolver::FResourceUsage::HasArchetypeConflict(TMassExecutionAccess<FResourceAccess> ElementAccess, const TArray<FMassArchetypeHandle>& InArchetypes) const
{
	using UE::Mass::Private::DoArchetypeContainersOverlap;

	// this function is being run when we've already determined there's an access conflict on given ElementsAccess,
	// meaning whoever's asking is trying to access Elements that are already being used. We can still grant access 
	// though provided that none of the current users of Element access the same archetypes the querier does (as provided 
	// by InArchetypes).
	// @todo this operation could be even more efficient and precise if we tracked which operation (read/write) and which
	// specific Element were conflicting and the we could limit the check to that. That would however significantly 
	// complicate the code and would require a major refactor to keep things clean.
	for (int i = 0; i < EMassAccessOperation::MAX; ++i)
	{
		for (const FResourceUsers& Resource : ElementAccess[i].Access)
		{
			for (const int32 UserIndex : Resource.Users)
			{
				if (DoArchetypeContainersOverlap(AllNodesView[UserIndex].ValidArchetypes, InArchetypes))
				{
					return true;
				}
			}
		}
	}
	return false;
}

bool FMassProcessorDependencySolver::FResourceUsage::CanAccessRequirements(const FMassExecutionRequirements& TestedRequirements, const TArray<FMassArchetypeHandle>& InArchetypes) const
{
	bool bCanAccess = (CanAccess<FMassFragmentBitSet>(Requirements.Fragments, TestedRequirements.Fragments) || !HasArchetypeConflict(FragmentsAccess, InArchetypes))
		&& (CanAccess<FMassChunkFragmentBitSet>(Requirements.ChunkFragments, TestedRequirements.ChunkFragments) || !HasArchetypeConflict(ChunkFragmentsAccess, InArchetypes))
		&& (CanAccess<FMassSharedFragmentBitSet>(Requirements.SharedFragments, TestedRequirements.SharedFragments) || !HasArchetypeConflict(SharedFragmentsAccess, InArchetypes))
		&& CanAccess<FMassExternalSubsystemBitSet>(Requirements.RequiredSubsystems, TestedRequirements.RequiredSubsystems);

	return bCanAccess;
}

void FMassProcessorDependencySolver::FResourceUsage::SubmitNode(const int32 NodeIndex, FNode& InOutNode)
{
	HandleElementType<FMassFragmentBitSet>(FragmentsAccess, InOutNode.Requirements.Fragments, InOutNode, NodeIndex);
	HandleElementType<FMassChunkFragmentBitSet>(ChunkFragmentsAccess, InOutNode.Requirements.ChunkFragments, InOutNode, NodeIndex);
	HandleElementType<FMassSharedFragmentBitSet>(SharedFragmentsAccess, InOutNode.Requirements.SharedFragments, InOutNode, NodeIndex);
	HandleElementType<FMassExternalSubsystemBitSet>(RequiredSubsystemsAccess, InOutNode.Requirements.RequiredSubsystems, InOutNode, NodeIndex);

	Requirements.Append(InOutNode.Requirements);
}

//----------------------------------------------------------------------//
//  FProcessorDependencySolver::FNode
//----------------------------------------------------------------------//
void FMassProcessorDependencySolver::FNode::IncreaseWaitingNodesCount(TArrayView<FMassProcessorDependencySolver::FNode> InAllNodes)
{
	// cycle-protection check. If true it means we have a cycle and the whole algorithm result will be unreliable 
	if (TotalWaitingNodes >= FMath::Square(InAllNodes.Num()))
	{
		return;
	}

	++TotalWaitingNodes;

	for (const int32 DependencyIndex : OriginalDependencies)
	{
		check(&InAllNodes[DependencyIndex] != this);
		InAllNodes[DependencyIndex].IncreaseWaitingNodesCount(InAllNodes);
	}
}
//----------------------------------------------------------------------//
//  FProcessorDependencySolver
//----------------------------------------------------------------------//
FMassProcessorDependencySolver::FMassProcessorDependencySolver(TArrayView<UMassProcessor*> InProcessors, const bool bIsGameRuntime)
	: Processors(InProcessors)
	, bGameRuntime(bIsGameRuntime)
{}

bool FMassProcessorDependencySolver::PerformSolverStep(FResourceUsage& ResourceUsage, TArray<int32>& InOutIndicesRemaining, TArray<int32>& OutNodeIndices)
{
	int32 AcceptedNodeIndex = INDEX_NONE;
	int32 FallbackAcceptedNodeIndex = INDEX_NONE;

	for (int32 i = 0; i < InOutIndicesRemaining.Num(); ++i)
	{
		const int32 NodeIndex = InOutIndicesRemaining[i];
		if (AllNodes[NodeIndex].TransientDependencies.Num() == 0)
		{
			// if we're solving dependencies for a single thread use we don't need to fine-tune the order based on resources nor archetypes
			if (bSingleThreadTarget || ResourceUsage.CanAccessRequirements(AllNodes[NodeIndex].Requirements, AllNodes[NodeIndex].ValidArchetypes))
			{
				AcceptedNodeIndex = NodeIndex;
				break;
			}
			else if (FallbackAcceptedNodeIndex == INDEX_NONE)
			{
				// if none of the nodes left can "cleanly" execute (i.e. without conflicting with already stored nodes)
				// we'll just pick this one up and go with it. 
				FallbackAcceptedNodeIndex = NodeIndex;
			}
		}
	}

	if (AcceptedNodeIndex != INDEX_NONE || FallbackAcceptedNodeIndex != INDEX_NONE)
	{
		const int32 NodeIndex = AcceptedNodeIndex != INDEX_NONE ? AcceptedNodeIndex : FallbackAcceptedNodeIndex;

		FNode& Node = AllNodes[NodeIndex];

		// Note that this is not an unexpected event and will happen during every dependency solving. It's a part 
		// of the algorithm. We initially look for all the things we can run without conflicting with anything else. 
		// But that can't last forever, at some point we'll end up in a situation where every node left waits for 
		// something that has been submitted already. Then we just pick one of the waiting ones (the one indicated by 
		// FallbackAcceptedNodeIndex), "run it" and proceed.
		UE_CLOG(AcceptedNodeIndex == INDEX_NONE, LogMass, Verbose, TEXT("No dependency-free node can be picked, due to resource requirements. Picking %s as the next node.")
			, *Node.Name.ToString());

		ResourceUsage.SubmitNode(NodeIndex, Node);
		InOutIndicesRemaining.RemoveSingle(NodeIndex);
		OutNodeIndices.Add(NodeIndex);
		for (const int32 DependencyIndex : Node.OriginalDependencies)
		{
			Node.SequencePositionIndex = FMath::Max(Node.SequencePositionIndex, AllNodes[DependencyIndex].SequencePositionIndex);
		}
		++Node.SequencePositionIndex;

		for (const int32 RemainingNodeIndex : InOutIndicesRemaining)
		{
			AllNodes[RemainingNodeIndex].TransientDependencies.RemoveSingleSwap(NodeIndex, /*bAllowShrinking=*/false);
		}
		
		return true;
	}

	return false;
}

void FMassProcessorDependencySolver::CreateSubGroupNames(FName InGroupName, TArray<FString>& SubGroupNames)
{
	// the function will convert composite group name into a series of progressively more precise group names
	// so "A.B.C" will result in ["A", "A.B", "A.B.C"]

	SubGroupNames.Reset();
	FString GroupNameAsString = InGroupName.ToString();
	FString TopGroupName;

	while (GroupNameAsString.Split(TEXT("."), &TopGroupName, &GroupNameAsString))
	{
		SubGroupNames.Add(TopGroupName);
	}
	SubGroupNames.Add(GroupNameAsString);
	
	for (int i = 1; i < SubGroupNames.Num(); ++i)
	{
		SubGroupNames[i] = FString::Printf(TEXT("%s.%s"), *SubGroupNames[i - 1], *SubGroupNames[i]);
	}
}

int32 FMassProcessorDependencySolver::CreateNodes(UMassProcessor& Processor)
{
	check(Processor.GetClass());
	FName ProcName = Processor.GetClass()->GetFName();

	if (const int32* NodeIndexPtr = NodeIndexMap.Find(ProcName))
	{
		// we can accept another instance of this processor class if the processor itself supports that.
		// Note that the first instance is added with the class while the subsequent instances are added with 
		// the instance name. This is done on purpose. The class name is used as dependency, so if at least the first 
		// processor is placed with the class name, like other processors, it can still be used to influence execution
		// order via ExecuteBefore and ExecuteAfter.
		if (Processor.ShouldAllowMultipleInstances())
		{
			ProcName = Processor.GetFName();
		}
		else
		{
			UE_LOG(LogMass, Warning, TEXT("%s Processor %s already registered. Duplicates are not supported.")
				, ANSI_TO_TCHAR(__FUNCTION__), *ProcName.ToString());
			return *NodeIndexPtr;
		}
	}

	const FMassProcessorExecutionOrder& ExecutionOrder = Processor.GetExecutionOrder();

	// first figure out the groups so that the group nodes come before the processor nodes, this is required for child
	// nodes to inherit group's dependencies like in scenarios where some processor required to ExecuteBefore a given group
	int32 ParentGroupNodeIndex = INDEX_NONE;
	if (ExecutionOrder.ExecuteInGroup.IsNone() == false)
	{
		TArray<FString> AllGroupNames;
		CreateSubGroupNames(ExecutionOrder.ExecuteInGroup, AllGroupNames);
	
		check(AllGroupNames.Num() > 0);

		for (const FString& GroupName : AllGroupNames)
		{
			const FName GroupFName(GroupName);
			int32* LocalGroupIndex = NodeIndexMap.Find(GroupFName);
			// group name hasn't been encountered yet - create it
			if (LocalGroupIndex == nullptr)
			{
				int32 NewGroupNodeIndex = AllNodes.Num();
				NodeIndexMap.Add(GroupFName, NewGroupNodeIndex);
				FNode& GroupNode = AllNodes.Add_GetRef({ GroupFName, nullptr, NewGroupNodeIndex });
				// just ignore depending on the dummy "root" node
				if (ParentGroupNodeIndex != INDEX_NONE)
				{
					GroupNode.OriginalDependencies.Add(ParentGroupNodeIndex);
					AllNodes[ParentGroupNodeIndex].SubNodeIndices.Add(NewGroupNodeIndex);
				}

				ParentGroupNodeIndex = NewGroupNodeIndex;
			}
			else
			{	
				ParentGroupNodeIndex = *LocalGroupIndex;
			}

		}
	}

	const int32 NodeIndex = AllNodes.Num();
	NodeIndexMap.Add(ProcName, NodeIndex);
	FNode& ProcessorNode = AllNodes.Add_GetRef({ ProcName, &Processor, NodeIndex });

	ProcessorNode.ExecuteAfter.Append(ExecutionOrder.ExecuteAfter);
	ProcessorNode.ExecuteBefore.Append(ExecutionOrder.ExecuteBefore);
	Processor.ExportRequirements(ProcessorNode.Requirements);
	ProcessorNode.Requirements.CountResourcesUsed();

	if (ParentGroupNodeIndex > 0)
	{
		AllNodes[ParentGroupNodeIndex].SubNodeIndices.Add(NodeIndex);
	}

	return NodeIndex;
}

void FMassProcessorDependencySolver::BuildDependencies()
{
	// at this point we have collected all the known processors and groups in AllNodes so we can transpose 
	// A.ExecuteBefore(B) type of dependencies into B.ExecuteAfter(A)
	for (int32 NodeIndex = 0; NodeIndex < AllNodes.Num(); ++NodeIndex)
	{
		for (int i = 0; i < AllNodes[NodeIndex].ExecuteBefore.Num(); ++i)
		{
			const FName BeforeDependencyName = AllNodes[NodeIndex].ExecuteBefore[i];
			int32 DependentNodeIndex = INDEX_NONE;
			int32* DependentNodeIndexPtr = NodeIndexMap.Find(BeforeDependencyName);
			if (DependentNodeIndexPtr == nullptr)
			{
				// missing dependency. Adding a "dummy" node representing those to still support ordering based on missing groups or processors 
				// For example, if Processor A and B declare dependency, respectively, "Before C" and "After C" we still 
				// expect A to come before B regardless of whether C exists or not.
				
				DependentNodeIndex = AllNodes.Num();
				NodeIndexMap.Add(BeforeDependencyName, DependentNodeIndex);
				AllNodes.Add({ BeforeDependencyName, nullptr, DependentNodeIndex });

				UE_LOG(LogMass, Log, TEXT("Unable to find dependency \"%s\" declared by %s. Creating a dummy dependency node.")
					, *BeforeDependencyName.ToString(), *AllNodes[NodeIndex].Name.ToString());
			}
			else
			{
				DependentNodeIndex = *DependentNodeIndexPtr;
			}

			check(AllNodes.IsValidIndex(DependentNodeIndex));
			AllNodes[DependentNodeIndex].ExecuteAfter.Add(AllNodes[NodeIndex].Name);
		}
		AllNodes[NodeIndex].ExecuteBefore.Reset();
	}

	// at this point all nodes contain:
	// - single "original dependency" pointing at its parent group
	// - ExecuteAfter populated with node names

	// Now, for every Name in ExecuteAfter we do the following:
	//	if Name represents a processor, add it as "original dependency"
	//	else, if Name represents a group:
	//		- append all group's child node names to ExecuteAfter
	// 
	for (int32 NodeIndex = 0; NodeIndex < AllNodes.Num(); ++NodeIndex)
	{
		for (int i = 0; i < AllNodes[NodeIndex].ExecuteAfter.Num(); ++i)
		{
			const FName AfterDependencyName = AllNodes[NodeIndex].ExecuteAfter[i];
			int32* PrerequisiteNodeIndexPtr = NodeIndexMap.Find(AfterDependencyName);
			int32 PrerequisiteNodeIndex = INDEX_NONE;

			if (PrerequisiteNodeIndexPtr == nullptr)
			{
				// missing dependency. Adding a "dummy" node representing those to still support ordering based on missing groups or processors 
				// For example, if Processor A and B declare dependency, respectively, "Before C" and "After C" we still 
				// expect A to come before B regardless of whether C exists or not.

				PrerequisiteNodeIndex = AllNodes.Num();
				NodeIndexMap.Add(AfterDependencyName, PrerequisiteNodeIndex);
				AllNodes.Add({ AfterDependencyName, nullptr, PrerequisiteNodeIndex });

				UE_LOG(LogMass, Log, TEXT("Unable to find dependency \"%s\" declared by %s. Creating a dummy dependency node.")
					, *AfterDependencyName.ToString(), *AllNodes[NodeIndex].Name.ToString());
			}
			else
			{
				PrerequisiteNodeIndex = *PrerequisiteNodeIndexPtr;
			}

			const FNode& PrerequisiteNode = AllNodes[PrerequisiteNodeIndex];

			if (PrerequisiteNode.IsGroup())
			{
				for (int32 SubNodeIndex : PrerequisiteNode.SubNodeIndices)
				{
					AllNodes[NodeIndex].ExecuteAfter.AddUnique(AllNodes[SubNodeIndex].Name);
				}
			}
			else
			{
				AllNodes[NodeIndex].OriginalDependencies.AddUnique(PrerequisiteNodeIndex);
			}
		}

		// if this node is a group push all the dependencies down on all the children
		// by design all child nodes come after group nodes so the child nodes' dependencies have not been processed yet
		if (AllNodes[NodeIndex].IsGroup() && AllNodes[NodeIndex].SubNodeIndices.Num())
		{
			for (int32 PrerequisiteNodeIndex : AllNodes[NodeIndex].OriginalDependencies)
			{
				checkSlow(PrerequisiteNodeIndex != NodeIndex);
				// in case of processor nodes we can store it directly
				if (AllNodes[PrerequisiteNodeIndex].IsGroup() == false)
				{
					for (int32 ChildNodeIndex : AllNodes[NodeIndex].SubNodeIndices)
					{
						AllNodes[ChildNodeIndex].OriginalDependencies.AddUnique(PrerequisiteNodeIndex);
					}
				}
				// special case - if dependency is a group and we haven't processed that group yet, we need to add it by name
				else if (PrerequisiteNodeIndex > NodeIndex)
				{
					const FName& PrerequisiteName = AllNodes[PrerequisiteNodeIndex].Name;
					for (int32 ChildNodeIndex : AllNodes[NodeIndex].SubNodeIndices)
					{
						AllNodes[ChildNodeIndex].ExecuteAfter.AddUnique(PrerequisiteName);
					}
				}
			}
		}
	}
}

void FMassProcessorDependencySolver::LogNode(const FNode& Node, int Indent)
{
	using UE::Mass::Private::NameViewToString;

	if (Node.IsGroup())
	{
		UE_LOG(LogMass, Log, TEXT("%*s%s before:%s after:%s"), Indent, TEXT(""), *Node.Name.ToString()
			, *NameViewToString(Node.ExecuteBefore)
			, *NameViewToString(Node.ExecuteAfter));

		for (const int32 NodeIndex : Node.SubNodeIndices)
		{
			LogNode(AllNodes[NodeIndex], Indent + 4);
		}
	}
	else
	{
		UE_LOG(LogMass, Log, TEXT("%*s%s before:%s after:%s"), Indent, TEXT(""), *Node.Name.ToString()
			, *NameViewToString(Node.Processor->GetExecutionOrder().ExecuteBefore)
			, *NameViewToString(Node.Processor->GetExecutionOrder().ExecuteAfter));
	}
}

#if 0 // disabled the graph building for now, leaving the old code here for reference
struct FDumpGraphDependencyUtils
{
	static void DumpGraphNode(FArchive& LogFile, const FProcessorDependencySolver::FNode& Node, int Indent, TSet<const FProcessorDependencySolver::FNode*>& AllNodes, const bool bRoot)
	{
		const FString NodeName = Node.Name.ToString();
		if (!Node.Processor)
		{
			const FString ClusterNodeName = NodeName.Replace(TEXT("."), TEXT("_"));
			const FString GraphNodeName = NodeName.Replace(TEXT("."), TEXT(" "));

			int32 Index = -1;
			Node.Name.ToString().FindLastChar(TEXT('.'), Index);
			const FString GroupName = NodeName.Mid(Index+1);

			LogFile.Logf(TEXT("%*ssubgraph cluster_%s"), Indent, TEXT(""), *ClusterNodeName);
			LogFile.Logf(TEXT("%*s{"), Indent, TEXT(""));
			LogFile.Logf(TEXT("%*slabel =\"%s\";"), Indent + 4, TEXT(""), *GroupName);
			LogFile.Logf(TEXT("%*s\"%s Start\"%s;"), Indent + 4, TEXT(""), *GraphNodeName, bRoot ? TEXT("") : TEXT("[shape=point style=invis]"));
			LogFile.Logf(TEXT("%*s\"%s End\"%s;"), Indent + 4, TEXT(""), *GraphNodeName, bRoot ? TEXT("") : TEXT("[shape=point style=invis]"));
			for (const FProcessorDependencySolver::FNode& SubNode : Node.SubNodes)
			{
				DumpGraphNode(LogFile, SubNode, Indent + 4, AllNodes, false);
			}
			LogFile.Logf(TEXT("%*s}"), Indent, TEXT(""));
		}
		else
		{
			LogFile.Logf(TEXT("%*s\"%s\""), Indent, TEXT(""), *NodeName);
			AllNodes.Add(&Node);
		}
	}

	static const FProcessorDependencySolver::FNode* FindDepNodeInParents(const TArray<const FProcessorDependencySolver::FNode*>& Parents, FName DependencyName)
	{
		for (int32 i = Parents.Num() - 1; i >= 0; --i)
		{
			const FProcessorDependencySolver::FNode* CurNode = Parents[i];
			int32 DependencyIndex = CurNode->FindNodeIndex(DependencyName);
			while(DependencyIndex != INDEX_NONE)
			{
				if(CurNode->SubNodes[DependencyIndex].Name == DependencyName)
				{
					return &CurNode->SubNodes[DependencyIndex];
				}
				else
				{
					// Dig down the chain
					CurNode = &CurNode->SubNodes[DependencyIndex];
					DependencyIndex = CurNode->FindNodeIndex(DependencyName);
				}
			}
		}
		return nullptr;
	};

	static bool DoAllSubNodeHasDependency(const FProcessorDependencySolver::FNode& Node, const FName DependencyName, bool bBeforeDep)
	{
		bool bDepExistInAllSibling = true;
		for (const FProcessorDependencySolver::FNode& SiblingSubNode : Node.SubNodes)
		{
			if (SiblingSubNode.Processor)
			{
				const TArray<FName>& ExecuteDep = bBeforeDep ? SiblingSubNode.Processor->GetExecutionOrder().ExecuteBefore : SiblingSubNode.Processor->GetExecutionOrder().ExecuteAfter;
				if (ExecuteDep.Find(DependencyName) == INDEX_NONE)
				{
					bDepExistInAllSibling = false;
					break;
				}
			}
			else if (!DoAllSubNodeHasDependency(SiblingSubNode, DependencyName, bBeforeDep))
			{
				bDepExistInAllSibling = false;
				break;
			}
		}
		return bDepExistInAllSibling;
	}

	static bool DoesDependencyExistIndirectly(const TArray<const FProcessorDependencySolver::FNode*>& Parents, const FProcessorDependencySolver::FNode& Node, const FName DepNameToFind, bool bBeforeDep)
	{
		check(Node.Processor);
		const TArray<FName>& ExecuteDep = bBeforeDep ? Node.Processor->GetExecutionOrder().ExecuteBefore : Node.Processor->GetExecutionOrder().ExecuteAfter;
		for (const FName& DependencyName : ExecuteDep)
		{
			if (DependencyName == DepNameToFind)
			{
				continue;
			}
			if (const FProcessorDependencySolver::FNode* DepNode = FindDepNodeInParents(Parents, DependencyName))
			{
				if(DepNode->Processor)
				{
					const TArray<FName>& ExecuteDepDep = bBeforeDep ? DepNode->Processor->GetExecutionOrder().ExecuteBefore : DepNode->Processor->GetExecutionOrder().ExecuteAfter;
					if(ExecuteDepDep.Find(DepNameToFind) != INDEX_NONE)
					{
						return true;
					}
				}
				else if(DoAllSubNodeHasDependency(*DepNode, DepNameToFind, bBeforeDep))
				{
					return true;
				}
			}
		}
		return false;
	}

	static void RemoveAllProcessorFromSet(const FProcessorDependencySolver::FNode& DepNode, TSet<const FProcessorDependencySolver::FNode*>& Set)
	{
		if (DepNode.Processor)
		{
			Set.Remove(&DepNode);
		}
		else
		{
			for (const FProcessorDependencySolver::FNode& SubNode : DepNode.SubNodes)
			{
				RemoveAllProcessorFromSet(SubNode, Set);
			}
		}
	}

	static bool AreAllProcessorInSet(const FProcessorDependencySolver::FNode& DepNode, TSet<const FProcessorDependencySolver::FNode*>& Set)
	{
		if (DepNode.Processor)
		{
			if (!Set.Contains(&DepNode))
			{
				return false;
			}
		}
		else
		{
			for (const FProcessorDependencySolver::FNode& SubNode : DepNode.SubNodes)
			{
				if (!AreAllProcessorInSet(SubNode, Set))
				{
					return false;
				}
			}
		}
		return true;
	}

	static void DumpGraphDependencies(FArchive& LogFile, const FProcessorDependencySolver::FNode& Node, TArray<const FProcessorDependencySolver::FNode*> Parents, TArray<const FProcessorDependencySolver::FNode*> CommonBeforeDep, TArray<const FProcessorDependencySolver::FNode*> CommonAfterDep, TSet<const FProcessorDependencySolver::FNode*>& DependsOnStart, TSet<const FProcessorDependencySolver::FNode*>& LinkToEnd)
	{
		const FString NodeName = Node.Name.ToString();
		const FProcessorDependencySolver::FNode* ParentNode = Parents.Num() > 0 ? Parents.Last() : nullptr;
		const FString ParentNodeName = ParentNode ? ParentNode->Name.ToString() : TEXT("");
		const FString ParentGraphNodeName = ParentNodeName.Replace(TEXT("."), TEXT(" "));
		const FString ParentClusterNodeName = ParentNodeName.Replace(TEXT("."), TEXT("_"));
		if (Node.Processor)
		{
			for (const FName& DependencyName : Node.Processor->GetExecutionOrder().ExecuteBefore)
			{
				if (const FProcessorDependencySolver::FNode* DepNode = FindDepNodeInParents(Parents, DependencyName))
				{
					LinkToEnd.Remove(&Node);
					RemoveAllProcessorFromSet(*DepNode, DependsOnStart);

					if (CommonBeforeDep.Find(DepNode) != INDEX_NONE)
					{
						continue;
					}
					if (DoesDependencyExistIndirectly(Parents, Node, DependencyName, true))
					{
						continue;
					}

					const FString DepNodeName = DepNode->Name.ToString();
					if (DepNode->Processor)
					{
						LogFile.Logf(TEXT("    \"%s\" -> \"%s\";"), *NodeName, *DepNodeName);
					}
					else
					{
						const FString DepClusterNodeName = DepNodeName.Replace(TEXT("."), TEXT("_"));
						const FString DepGraphNodeName = DepNodeName.Replace(TEXT("."), TEXT(" "));
						LogFile.Logf(TEXT("    \"%s\" -> \"%s Start\"[lhead=cluster_%s];"), *NodeName, *DepGraphNodeName, *DepClusterNodeName);
					}

				}
			}
			for (const FName& DependencyName : Node.Processor->GetExecutionOrder().ExecuteAfter)
			{
				if (const FProcessorDependencySolver::FNode* DepNode = FindDepNodeInParents(Parents, DependencyName))
				{
					DependsOnStart.Remove(&Node);
					RemoveAllProcessorFromSet(*DepNode, LinkToEnd);

					if (CommonAfterDep.Find(DepNode) != INDEX_NONE)
					{
						continue;
					}
					if (DoesDependencyExistIndirectly(Parents, Node, DependencyName, false))
					{
						continue;
					}

					const FString DepNodeName = DepNode->Name.ToString();
					if (DepNode->Processor)
					{
						LogFile.Logf(TEXT("    \"%s\" -> \"%s\";"), *DepNodeName, *NodeName);
					}
					else
					{
						const FString DepClusterNodeName = DepNodeName.Replace(TEXT("."), TEXT("_"));
						const FString DepGraphNodeName = DepNodeName.Replace(TEXT("."), TEXT(" "));
						LogFile.Logf(TEXT("    \"%s End\" -> \"%s\"[ltail=cluster_%s];"), *DepGraphNodeName, *NodeName, *DepClusterNodeName);
					}
				}
			}

			// Layouting
			LogFile.Logf(TEXT("    \"%s Start\" -> \"%s\" -> \"%s End\"[style=invis];"), *ParentGraphNodeName, *NodeName, *ParentGraphNodeName);
		}
		else if (ParentNode)
		{

			const FString ClusterNodeName = NodeName.Replace(TEXT("."), TEXT("_"));
			const FString GraphNodeName = NodeName.Replace(TEXT("."), TEXT(" "));

			// Find common dependency through out all sub nodes
 			TSet<FName> BeforeDepOuputed;
 			TSet<FName> AfterDepOutputed;
			for (const FProcessorDependencySolver::FNode& SubNode : Node.SubNodes)
			{
				if(SubNode.Processor)
				{
					for (const FName& DependencyName : SubNode.Processor->GetExecutionOrder().ExecuteBefore)
					{
						if (const FProcessorDependencySolver::FNode* DepNode = FindDepNodeInParents(Parents, DependencyName))
						{
							if (DoAllSubNodeHasDependency(Node, DependencyName, true /*bBeforeDep*/))
							{
								CommonBeforeDep.Add(DepNode);

								if (!DoesDependencyExistIndirectly(Parents, SubNode, DependencyName, true))
								{
									if (BeforeDepOuputed.Contains(DependencyName))
									{
										continue;
									}
									BeforeDepOuputed.Add(DependencyName);

									const FString DepNodeName = DepNode->Name.ToString();
									if (DepNode->Processor)
									{
										LogFile.Logf(TEXT("    \"%s End\" -> \"%s\"[ltail=cluster_%s];"), *GraphNodeName, *DepNodeName, *ClusterNodeName);
									}
									else
									{
										const FString DepClusterNodeName = DepNodeName.Replace(TEXT("."), TEXT("_"));
										const FString DepGraphNodeName = DepNodeName.Replace(TEXT("."), TEXT(" "));
										LogFile.Logf(TEXT("    \"%s End\" -> \"%s Start\"[ltail=cluster_%s, lhead=cluster_%s];"), *GraphNodeName, *DepGraphNodeName, *ClusterNodeName, *DepClusterNodeName);
									}
								}
							}
						}
					}

					for (const FName& DependencyName : SubNode.Processor->GetExecutionOrder().ExecuteAfter)
					{
						if (const FProcessorDependencySolver::FNode* DepNode = FindDepNodeInParents(Parents, DependencyName))
						{
							if (DoAllSubNodeHasDependency(Node, DependencyName, false /*bBeforeDep*/))
							{
								CommonAfterDep.Add(DepNode);

								if (!DoesDependencyExistIndirectly(Parents, SubNode, DependencyName, false))
								{
									if (AfterDepOutputed.Contains(DependencyName))
									{
										continue;
									}
									AfterDepOutputed.Add(DependencyName);

									const FString DepNodeName = DepNode->Name.ToString();
									if (DepNode->Processor)
									{
										LogFile.Logf(TEXT("    \"%s\" -> \"%s Start\"[lhead=cluster_%s];"), *DepNodeName, *GraphNodeName, *ClusterNodeName);
									}
									else
									{
										const FString DepClusterNodeName = DepNodeName.Replace(TEXT("."), TEXT("_"));
										const FString DepGraphNodeName = DepNodeName.Replace(TEXT("."), TEXT(" "));
										LogFile.Logf(TEXT("    \"%s End\" -> \"%s Start\"[ltail=cluster_%s, lhead=cluster_%s];"), *DepGraphNodeName,*GraphNodeName, *DepClusterNodeName, *ClusterNodeName);
									}
								}
							}
						}
					}
				}
			}

 			// Layouting
			LogFile.Logf(TEXT("    \"%s Start\" -> \"%s Start\" -> \"%s End\" -> \"%s End\"[style=invis];"), *ParentGraphNodeName, *GraphNodeName, *GraphNodeName, *ParentGraphNodeName);
		}

		Parents.Push(&Node);
		for (const FProcessorDependencySolver::FNode& SubNode : Node.SubNodes)
		{
			DumpGraphDependencies(LogFile, SubNode, Parents, CommonBeforeDep, CommonAfterDep, DependsOnStart, LinkToEnd);
		}
	}

	static void PromoteStartAndEndDependency(FArchive& LogFile, const FString& GraphName, const FProcessorDependencySolver::FNode& Node, TSet<const FProcessorDependencySolver::FNode*>& DependsOnStart, TSet<const FProcessorDependencySolver::FNode*>& LinkToEnd)
	{
		if (Node.Processor)
		{
			return;
		}

		bool bAllDependsOnStart = true;
		bool bAllLinkedToEnd = true;
		for (const FProcessorDependencySolver::FNode& SubNode : Node.SubNodes)
		{
			if (!AreAllProcessorInSet(SubNode, DependsOnStart))
			{
				bAllDependsOnStart = false;
			}

			if (!AreAllProcessorInSet(SubNode, LinkToEnd))
			{
				bAllLinkedToEnd = false;
			}

			if (!bAllDependsOnStart && !bAllLinkedToEnd)
			{
				break;
			}
		}

		const FString NodeName = Node.Name.ToString();
		const FString ClusterNodeName = NodeName.Replace(TEXT("."), TEXT("_"));
		const FString GraphNodeName = NodeName.Replace(TEXT("."), TEXT(" "));
		if (bAllDependsOnStart)
		{
			RemoveAllProcessorFromSet(Node, DependsOnStart);
			LogFile.Logf(TEXT("    \"%s Start\" -> \"%s Start\"[lhead=cluster_%s, weight=0];"), *GraphName, *GraphNodeName, *ClusterNodeName);
		}

		if (bAllLinkedToEnd)
		{
			RemoveAllProcessorFromSet(Node, LinkToEnd);
			LogFile.Logf(TEXT("    \"%s End\" -> \"%s End\"[ltail=cluster_%s, weight=0];"), *GraphNodeName, *GraphName, *ClusterNodeName);
		}

		if(!bAllDependsOnStart || !bAllLinkedToEnd)
		{
			for (const FProcessorDependencySolver::FNode& SubNode : Node.SubNodes)
			{
				PromoteStartAndEndDependency(LogFile, GraphName, SubNode, DependsOnStart, LinkToEnd);
			}
		}
	}

	static void DumpAllGraphDependencies(FArchive& LogFile, const FProcessorDependencySolver::FNode& GroupRootNode, const TSet<const FProcessorDependencySolver::FNode*>& AllNodes)
	{
		TSet<const FProcessorDependencySolver::FNode*> DependsOnStart = AllNodes;
		TSet<const FProcessorDependencySolver::FNode*> LinkToEnd = AllNodes;
		const FString GraphName = GroupRootNode.Name.ToString();
		FDumpGraphDependencyUtils::DumpGraphDependencies(LogFile, GroupRootNode, TArray<const FProcessorDependencySolver::FNode*>(), TArray<const FProcessorDependencySolver::FNode*>(), TArray<const FProcessorDependencySolver::FNode*>(), DependsOnStart, LinkToEnd);
		FDumpGraphDependencyUtils::PromoteStartAndEndDependency(LogFile, GraphName, GroupRootNode, DependsOnStart, LinkToEnd);
		for (const FProcessorDependencySolver::FNode* Node : DependsOnStart)
		{
			LogFile.Logf(TEXT("    \"%s Start\" -> \"%s\"[weight=0];"), *GraphName, *Node->Name.ToString());
		}
		for (const FProcessorDependencySolver::FNode* Node : LinkToEnd)
		{
			LogFile.Logf(TEXT("    \"%s\" -> \"%s End\"[weight=0];"), *Node->Name.ToString(), *GraphName);
		}
	}
};

void FProcessorDependencySolver::DumpGraph(FArchive& LogFile) const
{
	TSet<const FNode*> AllNodes;
	LogFile.Logf(TEXT("digraph MassProcessorGraph"));
	LogFile.Logf(TEXT("{"));
	LogFile.Logf(TEXT("    compound = true;"));
	LogFile.Logf(TEXT("    newrank = true;"));
	FDumpGraphDependencyUtils::DumpGraphNode(LogFile, GroupRootNode, 4/* Indent */, AllNodes, true/* bRoot */);
	FDumpGraphDependencyUtils::DumpAllGraphDependencies(LogFile, GroupRootNode, AllNodes);
	LogFile.Logf(TEXT("}"));
}

#endif // 0; disabled the graph building for now, leaving the old code here for reference

void FMassProcessorDependencySolver::Solve(TArray<FMassProcessorOrderInfo>& OutResult)
{
	using UE::Mass::Private::NameViewToString;

	if (AllNodes.Num() == 0)
	{
		return;
	}

	for (FNode& Node : AllNodes)
	{
		Node.TransientDependencies = Node.OriginalDependencies;
		Node.TotalWaitingNodes = 0;
	}

	TArray<int32> IndicesRemaining;
	IndicesRemaining.Reserve(AllNodes.Num());
	for (int32 i = 0; i < AllNodes.Num(); ++i)
	{
		// skip all the group nodes, all group dependencies have already been converted to individual processor dependencies
		if (AllNodes[i].IsGroup() == false)
		{
			IndicesRemaining.Add(i);
			AllNodes[i].IncreaseWaitingNodesCount(AllNodes);
		}
	}

	IndicesRemaining.Sort([this](const int32 IndexA, const int32 IndexB){
		return AllNodes[IndexA].TotalWaitingNodes > AllNodes[IndexB].TotalWaitingNodes;
	});

	// this is where we'll be tracking what's being accessed by whom
	FResourceUsage ResourceUsage(AllNodes);

	TArray<int32> SortedNodeIndices;
	SortedNodeIndices.Reserve(AllNodes.Num());

	while (IndicesRemaining.Num())
	{
		const bool bStepSuccessful = PerformSolverStep(ResourceUsage, IndicesRemaining, SortedNodeIndices);

		if (bStepSuccessful == false)
		{
			bAnyCyclesDetected = true;

			UE_LOG(LogMass, Error, TEXT("Detected processing dependency cycle:"));
			for (const int32 Index : IndicesRemaining)
			{
				UMassProcessor* Processor = AllNodes[Index].Processor;
				if (Processor)
				{
					UE_LOG(LogMass, Warning, TEXT("\t%s, group: %s, before: %s, after %s")
						, *Processor->GetName()
						, *Processor->GetExecutionOrder().ExecuteInGroup.ToString()
						, *NameViewToString(Processor->GetExecutionOrder().ExecuteBefore)
						, *NameViewToString(Processor->GetExecutionOrder().ExecuteAfter));
				}
				else
				{
					// group
					UE_LOG(LogMass, Warning, TEXT("\tGroup %s"), *AllNodes[Index].Name.ToString());
				}
			}
			UE_LOG(LogMass, Warning, TEXT("Cutting the chain at an arbitrary location."));

			// remove first dependency
			// note that if we're in a cycle handling scenario every node does have some dependencies left
			const int32 DependencyNodeIndex = AllNodes[IndicesRemaining[0]].TransientDependencies.Pop(/*bAllowShrinking=*/false);
			// we need to remove this dependency from original dependencies as well, otherwise we'll still have the cycle
			// in the data being produces as a result of the whole algorithm
			AllNodes[IndicesRemaining[0]].OriginalDependencies.Remove(DependencyNodeIndex);
		}
	}

	// now we have the desired order in SortedNodeIndices. We have to traverse it to add to OutResult
	for (int i = 0; i < SortedNodeIndices.Num(); ++i)
	{
		const int32 NodeIndex = SortedNodeIndices[i];

		TArray<FName> DependencyNames;
		for (const int32 DependencyIndex : AllNodes[NodeIndex].OriginalDependencies)
		{
			DependencyNames.AddUnique(AllNodes[DependencyIndex].Name);
		}

		// at this point we expect SortedNodeIndices to only point to regular processors (i.e. no groups)
		if (ensure(AllNodes[NodeIndex].Processor != nullptr))
		{
			OutResult.Add({ AllNodes[NodeIndex].Name, AllNodes[NodeIndex].Processor, FMassProcessorOrderInfo::EDependencyNodeType::Processor, DependencyNames, AllNodes[NodeIndex].SequencePositionIndex });
		}
	}
}

void FMassProcessorDependencySolver::ResolveDependencies(TArray<FMassProcessorOrderInfo>& OutResult, TSharedPtr<FMassEntityManager> EntityManager, FMassProcessorDependencySolver::FResult* InOutOptionalResult)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("Mass ResolveDependencies");

	if (Processors.Num() == 0)
	{
		return;
	}

	FScopedCategoryAndVerbosityOverride LogOverride(TEXT("LogMass"), ELogVerbosity::Log);

	if (InOutOptionalResult)
	{
		DependencyGraphFileName = InOutOptionalResult->DependencyGraphFileName;
	}

	bAnyCyclesDetected = false;

	UE_LOG(LogMass, Log, TEXT("Gathering dependencies data:"));

	AllNodes.Reset();
	NodeIndexMap.Reset();
	// as the very first node we add a "root" node that represents the "top level group" and also simplifies the rest
	// of the lookup code - if a processor declares it's in group None or depends on Node it we don't need to check that 
	// explicitly. 
	AllNodes.Add(FNode(FName(), nullptr, 0));
	NodeIndexMap.Add(FName(), 0);

	const bool bCreateVirtualArchetypes = (!EntityManager);
	if (bCreateVirtualArchetypes)
	{
		// create FMassEntityManager instance that we'll use to sort out processors' overlaps
		// the idea for this is that for every processor we have we create an archetype matching given processor's requirements. 
		// Once that's done we have a collection of "virtual" archetypes our processors expect. Then we ask every processor 
		// to cache the archetypes they'd accept, using processors' owned queries. The idea is that some of the nodes will 
		// end up with more than just the virtual archetype created for that specific node. The practice proved the idea correct. 
		EntityManager = MakeShareable(new FMassEntityManager());
	}

	// gather the processors information first
	for (UMassProcessor* Processor : Processors)
	{
		if (Processor == nullptr)
		{
			UE_LOG(LogMass, Warning, TEXT("%s nullptr found in Processors collection being processed"), ANSI_TO_TCHAR(__FUNCTION__));
			continue;
		}

		const int32 ProcessorNodeIndex = CreateNodes(*Processor);

		if (bCreateVirtualArchetypes)
		{
			// this line is a part of a nice trick we're doing here utilizing EntityManager's archetype creation based on 
			// what each processor expects, and EntityQuery's capability to cache archetypes matching its requirements (used below)
			EntityManager->CreateArchetype(AllNodes[ProcessorNodeIndex].Requirements.AsCompositionDescriptor());
		}
	}

	UE_LOG(LogMass, Verbose, TEXT("Pruning processors..."));

	int32 PrunedProcessorsCount = 0;
	for (FNode& Node : AllNodes)
	{
		if (Node.IsGroup() == false)
		{
			// for each processor-representing node we cache information on which archetypes among the once we've created 
			// above (see the EntityManager.CreateArchetype call in the previous loop) match this processor. 
			Node.Processor->GetArchetypesMatchingOwnedQueries(*EntityManager.Get(), Node.ValidArchetypes);

			// prune the archetype-less processors
			if (Node.ValidArchetypes.Num() == 0 && Node.Processor->ShouldAllowQueryBasedPruning(bGameRuntime))
			{
				CA_ASSUME(Node.Processor);
				UE_LOG(LogMass, Verbose, TEXT("\t%s"), *Node.Processor->GetName());

				if (InOutOptionalResult)
				{
					InOutOptionalResult->PrunedProcessorClasses.Add(Node.Processor->GetClass());
				}

				// clearing out the processor will result in the rest of the algorithm to treat it as a group - we still 
				// want to preserve the configured ExecuteBefore and ExecuteAfter dependencies
				Node.Processor = nullptr;
				++PrunedProcessorsCount;
			}
		}
	}

	UE_LOG(LogMass, Verbose, TEXT("Number of processors pruned: %d"), PrunedProcessorsCount);

	check(AllNodes.Num());
	LogNode(AllNodes[0]);

	BuildDependencies();

	// now none of the processor nodes depend on groups - we replaced these dependencies with depending directly 
	// on individual processors. However, we keep the group nodes around since we store the dependencies via index, so 
	// removing nodes would mess that up. Solve below ignores group nodes and OutResult will not have any groups once its done.

	Solve(OutResult);

	UE_LOG(LogMass, Verbose, TEXT("Dependency order:"));
	for (const FMassProcessorOrderInfo& Info : OutResult)
	{
		UE_LOG(LogMass, Verbose, TEXT("\t%s"), *Info.Name.ToString());
	}

	int32 MaxSequenceLength = 0;
	for (FNode& Node : AllNodes)
	{
		MaxSequenceLength = FMath::Max(MaxSequenceLength, Node.SequencePositionIndex);
	}

	UE_LOG(LogMass, Verbose, TEXT("Max sequence length: %d"), MaxSequenceLength);

	if (InOutOptionalResult)
	{
		InOutOptionalResult->MaxSequenceLength = MaxSequenceLength;
		InOutOptionalResult->ArchetypeDataVersion = EntityManager->GetArchetypeDataVersion();
	}
}

bool FMassProcessorDependencySolver::IsResultUpToDate(const FMassProcessorDependencySolver::FResult& InResult, TSharedPtr<FMassEntityManager> EntityManager)
{
	if (InResult.PrunedProcessorClasses.Num() == 0 || !EntityManager || InResult.ArchetypeDataVersion == EntityManager->GetArchetypeDataVersion())
	{
		return true;
	}

	// this is inefficient right now since we're using CDOs and need to check all archetypes every time.
	// Would be more efficient if we had a common place where all processors live, both active and inactive, so that we can utilize those. 
	for (const TSubclassOf<UMassProcessor>& ProcessorClass : InResult.PrunedProcessorClasses)
	{
		if (UMassProcessor* ProcessorCDO = ProcessorClass.GetDefaultObject())
		{
			if (ProcessorCDO->DoesAnyArchetypeMatchOwnedQueries(*EntityManager.Get()))
			{
				return false;
			}
		}
	}
	return true;
}

#undef LOCTEXT_NAMESPACE 
// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/ReferenceChainSearch.h"
#include "HAL/PlatformStackWalk.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "UObject/FastReferenceCollector.h"
#include "UObject/GCObject.h"
#include "HAL/ThreadHeartBeat.h"

DEFINE_LOG_CATEGORY_STATIC(LogReferenceChain, Log, All);

// Returns true if the object can't be collected by GC
static FORCEINLINE bool IsNonGCObject(FGCObjectInfo* Object, EReferenceChainSearchMode SearchMode)
{
	return Object->HasAnyInternalFlags(EInternalObjectFlags::GarbageCollectionKeepFlags | EInternalObjectFlags::RootSet) ||
		(GARBAGE_COLLECTION_KEEPFLAGS != RF_NoFlags && Object->HasAnyFlags(GARBAGE_COLLECTION_KEEPFLAGS) && !(SearchMode & EReferenceChainSearchMode::FullChain));
}

FReferenceChainSearch::FGraphNode* FReferenceChainSearch::FindOrAddNode(FGCObjectInfo* InObjectInfo)
{
	FGraphNode* ObjectNode = nullptr;
	FGraphNode** ExistingObjectNode = AllNodes.Find(InObjectInfo);
	if (ExistingObjectNode)
	{
		ObjectNode = *ExistingObjectNode;
		check(ObjectNode->ObjectInfo == InObjectInfo);
	}
	else
	{
		ObjectNode = new FGraphNode();
		ObjectNode->ObjectInfo = InObjectInfo;
		AllNodes.Add(InObjectInfo, ObjectNode);
	}
	return ObjectNode;
}

FReferenceChainSearch::FGraphNode* FReferenceChainSearch::FindOrAddNode(UObject* InObjectToFindNodeFor)
{
	return FindOrAddNode(FGCObjectInfo::FindOrAddInfoHelper(InObjectToFindNodeFor, ObjectToInfoMap));
}

void FReferenceChainSearch::LinkNodes(FGCObjectInfo* From, FGCObjectInfo* To, EReferenceType ReferenceType, FName PropertyName, TConstArrayView<uint64> StackFrames)
{
	FGraphNode* ToNode = FindOrAddNode(To);
	FGraphNode* FromNode = FindOrAddNode(From);
	FromNode->ReferencedObjects.Emplace(FNodeReferenceInfo(ToNode, ReferenceType, PropertyName, StackFrames));
	ToNode->ReferencedByObjects.Add(FromNode);
}

void FReferenceChainSearch::LinkNodes(UObject* From, UObject* To, EReferenceType ReferenceType, FName PropertyName, TConstArrayView<uint64> StackFrames)
{
	LinkNodes(FGCObjectInfo::FindOrAddInfoHelper(From, ObjectToInfoMap), FGCObjectInfo::FindOrAddInfoHelper(To, ObjectToInfoMap), ReferenceType, PropertyName, StackFrames);
}

int32 FReferenceChainSearch::BuildReferenceChainsRecursive(FGraphNode* TargetNode, TArray<FReferenceChain*>& ProducedChains, int32 ChainDepth, const int32 VisitCounter, EReferenceChainSearchMode SearchMode, FGraphNode* GCObjReferencerNode)
{
	int32 ProducedChainsCount = 0;

	// Always create a chain for the FGCObject referencer node even if we've found another path to it, because it handles many types of references
	if (TargetNode == GCObjReferencerNode)
	{
		// This is a root so we can construct a chain from this node up to the target node
		FReferenceChain* Chain = new FReferenceChain(ChainDepth);
		Chain->InsertNode(TargetNode);
		ProducedChains.Add(Chain);
		ProducedChainsCount = 1;
		return ProducedChainsCount;
	}

	if (TargetNode->Visited != VisitCounter)
	{
		TargetNode->Visited = VisitCounter;

		// Stop at root objects
		if (!IsNonGCObject(TargetNode->ObjectInfo, SearchMode))
		{			
			for (FGraphNode* ReferencedByNode : TargetNode->ReferencedByObjects)
			{
				// For each of the referencers of this node, duplicate the current chain and continue processing
				if (ReferencedByNode->Visited != VisitCounter || ReferencedByNode == GCObjReferencerNode)
				{
					int32 OldChainsCount = ProducedChains.Num();
					int32 NewChainsCount = BuildReferenceChainsRecursive(ReferencedByNode, ProducedChains, ChainDepth + 1, VisitCounter, SearchMode, GCObjReferencerNode);
					// Insert the current node to all chains produced recursively
					for (int32 NewChainIndex = OldChainsCount; NewChainIndex < (NewChainsCount + OldChainsCount); ++NewChainIndex)
					{
						FReferenceChain* Chain = ProducedChains[NewChainIndex];
						Chain->InsertNode(TargetNode);
					}
					ProducedChainsCount += NewChainsCount;
				}
			}			
		}
		else
		{
			// This is a root so we can construct a chain from this node up to the target node
			FReferenceChain* Chain = new FReferenceChain(ChainDepth);
			Chain->InsertNode(TargetNode);
			ProducedChains.Add(Chain);
			ProducedChainsCount = 1;
		}
	}

	return ProducedChainsCount;
}

void FReferenceChainSearch::RemoveChainsWithDuplicatedRoots(TArray<FReferenceChain*>& AllChains, FGraphNode* GCObjReferencerNode)
{
	// This is going to be rather slow but it depends on the number of chains which shouldn't be too bad (usually)
	for (int32 FirstChainIndex = 0; FirstChainIndex < AllChains.Num(); ++FirstChainIndex)
	{
		const FGraphNode* RootNode = AllChains[FirstChainIndex]->GetRootNode(GCObjReferencerNode);
		for (int32 SecondChainIndex = AllChains.Num() - 1; SecondChainIndex > FirstChainIndex; --SecondChainIndex)
		{
			if (AllChains[SecondChainIndex]->GetRootNode(GCObjReferencerNode) == RootNode)
			{
				delete AllChains[SecondChainIndex];
				AllChains.RemoveAt(SecondChainIndex);
			}
		}
	}
}

void FReferenceChainSearch::RemoveDuplicateGarbageChains(TArray<FReferenceChain*>& AllChains, FGraphNode* GCObjReferencerNode)
{
	typedef TPair<FGraphNode*, FGraphNode*> FGarbageChainKey; 

	TMap<FGarbageChainKey, FReferenceChain*> GarbageChains;
	TArray<FReferenceChain*> NoGarbageChains;

	for (int32 ChainIndex = 0; ChainIndex < AllChains.Num(); ++ChainIndex)
	{
		FReferenceChain* Chain = AllChains[ChainIndex];
		const int32 GarbageIndex = Chain->Nodes.FindLastByPredicate([](FGraphNode* Node){ return Node->ObjectInfo->IsGarbage(); });

		if (GarbageIndex == INDEX_NONE)
		{
			NoGarbageChains.Add(Chain);
			continue;
		}

		FGraphNode* Garbage = Chain->Nodes[GarbageIndex];
		FGraphNode* ChainReferencer = Chain->Nodes.Last();
		if (ChainReferencer == GCObjReferencerNode && Chain->Nodes.Num() > 2)
		{
			ChainReferencer = Chain->Nodes[Chain->Nodes.Num() - 2];
		}

		FGarbageChainKey Key(Garbage, ChainReferencer);
		if (FReferenceChain* Existing = GarbageChains.FindRef(Key))
		{
			int32 ExistingLen = Existing->Nodes.Num() - Existing->Nodes.Find(Garbage);
			int32 NewLen = Chain->Nodes.Num() - Chain->Nodes.Find(Garbage);
			if (NewLen < ExistingLen)
			{
				GarbageChains.FindOrAdd(Key, Chain);
			}
		}
		else
		{
			GarbageChains.Add(Key, Chain);
		}
	}
	AllChains.Reset();
	GarbageChains.GenerateValueArray(AllChains);
	AllChains.Append(NoGarbageChains);
}

typedef TTuple<FReferenceChainSearch::FGraphNode*, FReferenceChainSearch::FGraphNode*, FReferenceChainSearch::FGraphNode*> FChainDedupKey;

void FReferenceChainSearch::RemoveDuplicatedChains(TArray<FReferenceChain*>& AllChains, FGraphNode* GCObjectReferencerNode)
{
	// We consider chains identical if the direct referencer of the target node and the root node are identical
	TMap<FChainDedupKey, FReferenceChain*> UniqueChains;
	
	for (int32 ChainIndex = 0; ChainIndex < AllChains.Num(); ++ChainIndex)
	{
		FReferenceChain* Chain = AllChains[ChainIndex];
		FGraphNode* ChainRoot = Chain->Nodes[1];

		// If the chain referencers is the GUObjectReferencer then go one down to find a more unique object to use for deduplication
		FGraphNode* ChainReferencer = Chain->Nodes.Last();
		if (ChainReferencer == GCObjectReferencerNode && Chain->Nodes.Num() > 2)
		{
			ChainReferencer = Chain->Nodes[Chain->Nodes.Num()-2];
		}

		FChainDedupKey ChainRootAndReferencer(Chain->TargetNode, ChainRoot, ChainReferencer);

		FReferenceChain** ExistingChain = UniqueChains.Find(ChainRootAndReferencer);
		if (ExistingChain)
		{
			if ((*ExistingChain)->Nodes.Num() > Chain->Nodes.Num())
			{
				delete (*ExistingChain);
				UniqueChains[ChainRootAndReferencer] = Chain;
			}
			else
			{
				delete Chain;
			}
		}
		else
		{
			UniqueChains.Add(ChainRootAndReferencer, Chain);
		}
	}
	AllChains.Reset();
	UniqueChains.GenerateValueArray(AllChains);
}

void FReferenceChainSearch::BuildReferenceChains(TConstArrayView<FGraphNode*> TargetNodes, TArray<FReferenceChain*>& Chains, EReferenceChainSearchMode SearchMode, FGraphNode* GCObjReferencerNode)
{
	int32 VisitCounter = 0;
	TArray<FReferenceChain*> AllChains;
	for (FGraphNode* TargetNode : TargetNodes)
	{
		TArray<FReferenceChain*> ThisTargetChains;

		// Recursively construct reference chains
		for (FGraphNode* ReferencedByNode : TargetNode->ReferencedByObjects)
		{
			TargetNode->Visited = ++VisitCounter;
		
			AllChains.Reset();
			const int32 MinChainDepth = 2; // The chain will contain at least the TargetNode and the ReferencedByNode
			BuildReferenceChainsRecursive(ReferencedByNode, AllChains, MinChainDepth, VisitCounter, SearchMode, GCObjReferencerNode);

			for (FReferenceChain* Chain : AllChains)
			{
				Chain->TargetNode = TargetNode; // Store the node that created this chain in case we later truncate the chain
				Chain->InsertNode(TargetNode);
			}

			// Filter based on search mode	
			if (!!(SearchMode & EReferenceChainSearchMode::ExternalOnly))
			{
				for (int32 ChainIndex = AllChains.Num() - 1; ChainIndex >= 0; --ChainIndex)
				{
					FReferenceChain* Chain = AllChains[ChainIndex];	
					if (!Chain->IsExternal())
					{
						// Discard the chain
						delete Chain;
						AllChains.RemoveAtSwap(ChainIndex);
					}
				}
			}

			ThisTargetChains.Append(AllChains);
		}

		// Sort all chains based on the search criteria
		if (!(SearchMode & EReferenceChainSearchMode::Longest))
		{
			// Sort from the shortest to the longest chain
			ThisTargetChains.Sort([](const FReferenceChain& LHS, const FReferenceChain& RHS) { return LHS.Num() < RHS.Num(); });
		}
		else
		{
			// Sort from the longest to the shortest chain
			ThisTargetChains.Sort([](const FReferenceChain& LHS, const FReferenceChain& RHS) { return LHS.Num() > RHS.Num(); });
		}

		if (!!(SearchMode & (EReferenceChainSearchMode::ShortestToGarbage)))
		{
			RemoveDuplicateGarbageChains(ThisTargetChains, GCObjReferencerNode);
		}
		// Remove duplicated roots per target object
		else if (!!(SearchMode & (EReferenceChainSearchMode::Longest | EReferenceChainSearchMode::Shortest)))
		{
			RemoveChainsWithDuplicatedRoots(ThisTargetChains, GCObjReferencerNode);
		}

		Chains.Append(ThisTargetChains);
	}

	// If we didn't remove dupe-root chains, remove overall duplicated chains on the entire set of results
	if (!(SearchMode & (EReferenceChainSearchMode::Longest | EReferenceChainSearchMode::Shortest | EReferenceChainSearchMode::ShortestToGarbage)))
	{
		RemoveDuplicatedChains(Chains, GCObjReferencerNode);
	}

	// Finally, fill extended reference info for the remaining chains
	for (FReferenceChain* Chain : Chains)
	{
		Chain->FillReferenceInfo();
	}
}

void FReferenceChainSearch::BuildReferenceChainsForDirectReferences(TConstArrayView<FGraphNode*> TargetNodes, TArray<FReferenceChain*>& AllChains, EReferenceChainSearchMode SearchMode, FGraphNode* GCObjReferencerNode)
{
	for (FGraphNode* TargetNode : TargetNodes) 
	{
		for (FGraphNode* ReferencedByNode : TargetNode->ReferencedByObjects)
		{
			if (!(SearchMode & EReferenceChainSearchMode::ExternalOnly) || !ReferencedByNode->ObjectInfo->IsIn(TargetNode->ObjectInfo))
			{
				FReferenceChain* Chain = new FReferenceChain();
				Chain->TargetNode = TargetNode;
				Chain->AddNode(TargetNode);
				Chain->AddNode(ReferencedByNode);
				Chain->FillReferenceInfo();
				AllChains.Add(Chain);
			}
		}
	}
}

FString FReferenceChainSearch::GetObjectFlags(FGCObjectInfo* InObject)
{
	FString Flags;
	if (InObject->IsRooted())
	{
		Flags += TEXT("(root) ");
	}

	CA_SUPPRESS(6011)
	if (InObject->IsNative())
	{
		Flags += TEXT("(native) ");
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (InObject->HasAnyInternalFlags(EInternalObjectFlags::PendingKill))
	{
		Flags += TEXT("(PendingKill) ");
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	if (InObject->HasAnyInternalFlags(EInternalObjectFlags::Garbage))
	{
		Flags += TEXT("(Garbage) ");
	}

	if (InObject->HasAnyFlags(RF_Standalone))
	{
		Flags += TEXT("(standalone) ");
	}

	if (InObject->HasAnyInternalFlags(EInternalObjectFlags::Async))
	{
		Flags += TEXT("(async) ");
	}

	if (InObject->HasAnyInternalFlags(EInternalObjectFlags::AsyncLoading))
	{
		Flags += TEXT("(asyncloading) ");
	}

	if (InObject->IsDisregardForGC())
	{
		Flags += TEXT("(NeverGCed) ");
	}

	if (InObject->HasAnyInternalFlags(EInternalObjectFlags::ClusterRoot))
	{
		Flags += TEXT("(ClusterRoot) ");
	}
	if (InObject->GetOwnerIndex() > 0)
	{
		Flags += TEXT("(Clustered) ");
	}
	return Flags;
}

static void ConvertStackFramesToCallstack(const uint64* StackFrames, int32 NumStackFrames, int32 Indent, FOutputDevice& Out)
{
	// Convert the stack trace to text
	for (int32 Idx = 0; Idx < NumStackFrames; Idx++)
	{
		ANSICHAR Buffer[1024];
		Buffer[0] = '\0';
		FPlatformStackWalk::ProgramCounterToHumanReadableString(Idx, StackFrames[Idx], Buffer, sizeof(Buffer));

		if (FCStringAnsi::Strstr(Buffer, "TFastReferenceCollector") != nullptr)
		{
			break;
		}

		if (FCStringAnsi::Strstr(Buffer, "FWindowsPlatformStackWalk") == nullptr &&
			FCStringAnsi::Strstr(Buffer, "FDirectReferenceProcessor") == nullptr)
		{
			ANSICHAR* TrimmedBuffer = FCStringAnsi::Strstr(Buffer, "!");
			if (!TrimmedBuffer)
			{
				TrimmedBuffer = Buffer;
			}
			else
			{
				TrimmedBuffer++;
			}

			Out.Logf(ELogVerbosity::Log, TEXT("%s   ^ %s"), FCString::Spc(Indent), *FString(TrimmedBuffer));
		}
	}
}

void FReferenceChainSearch::DumpChain(FReferenceChainSearch::FReferenceChain* Chain, TFunctionRef<bool(FCallbackParams& Params)> ReferenceCallback, FOutputDevice& Out)
{
	if (Chain->Num())
	{
		bool bPostCallbackContinue = true;
		const int32 RootIndex = Chain->Num() - 1;
		const FNodeReferenceInfo* ReferenceInfo = Chain->GetReferenceInfo(RootIndex);
		FGCObjectInfo* ReferencerObject = Chain->GetNode(RootIndex)->ObjectInfo;
		{
			FCallbackParams Params;
			Params.Referencer = nullptr;
			Params.Object = ReferencerObject;
			Params.ReferenceInfo = nullptr;
			Params.Indent = FMath::Min<int32>(TCStringSpcHelper<TCHAR>::MAX_SPACES, Chain->Num() - RootIndex);
			Params.Out = &Out;

			Out.Logf(ELogVerbosity::Log, TEXT("%s%s %s"),
				FCString::Spc(Params.Indent),
				*GetObjectFlags(ReferencerObject),
				*ReferencerObject->GetFullName());

			bPostCallbackContinue = ReferenceCallback(Params);
		}

		// Roots are at the end so iterate from the last to the first node
		for (int32 NodeIndex = RootIndex - 1; NodeIndex >= 0 && bPostCallbackContinue; --NodeIndex)
		{
			FGCObjectInfo* Object = Chain->GetNode(NodeIndex)->ObjectInfo;

			FCallbackParams Params;
			Params.Referencer = ReferencerObject;
			Params.Object = Object;
			Params.ReferenceInfo = ReferenceInfo;
			Params.Indent = FMath::Min<int32>(TCStringSpcHelper<TCHAR>::MAX_SPACES, Chain->Num() - NodeIndex - 1);
			Params.Out = &Out;

			if (ReferenceInfo->Type == EReferenceType::Property)
			{
				FString ReferencingPropertyName;
				UClass* ReferencerClass = Cast<UClass>(ReferencerObject->GetClass()->TryResolveObject());
				TArray<FProperty*> ReferencingProperties;

				if (ReferencerClass && FGCStackSizeHelper::ConvertPathToProperties(ReferencerClass, ReferenceInfo->ReferencerName, ReferencingProperties))
				{
					FProperty* InnermostProperty = ReferencingProperties.Last();
					FProperty* OutermostProperty = ReferencingProperties[0];

					ReferencingPropertyName = FString::Printf(TEXT("%s %s%s::%s"),
						*InnermostProperty->GetCPPType(),
						OutermostProperty->GetOwnerClass()->GetPrefixCPP(),
						*OutermostProperty->GetOwnerClass()->GetName(),
						*ReferenceInfo->ReferencerName.ToString());
				}
				else
				{
					// Handle base UObject referencer info (it's only exposed to the GC token stream and not to the reflection system)
					static const FName ClassPropertyName(TEXT("Class"));
					static const FName OuterPropertyName(TEXT("Outer"));
					
					FString ClassName;
					if (ReferenceInfo->ReferencerName == ClassPropertyName || ReferenceInfo->ReferencerName == OuterPropertyName)
					{
						ClassName = TEXT("UObject");
					}
					else if (ReferencerClass)
					{
						// Use the native class name when possible
						ClassName = ReferencerClass->GetPrefixCPP();
						ClassName += ReferencerClass->GetName();
					}
					else
					{
						// Revert to the internal class name if not
						ClassName = ReferencerObject->GetClassName();
					}
					ReferencingPropertyName = FString::Printf(TEXT("UObject* %s::%s"), *ClassName, *ReferenceInfo->ReferencerName.ToString());
				}

				Out.Logf(ELogVerbosity::Log, TEXT("%s-> %s = %s %s"),
					FCString::Spc(Params.Indent),
					*ReferencingPropertyName,
					*GetObjectFlags(Object),
					*Object->GetFullName());
			}
			else if (ReferenceInfo->Type == EReferenceType::AddReferencedObjects)
			{
				FString UObjectOrGCObjectName;
				if (ReferenceInfo->ReferencerName.IsNone())
				{
					UClass* ReferencerClass = Cast<UClass>(ReferencerObject->GetClass()->TryResolveObject());
					if (ReferencerClass)
					{
						UObjectOrGCObjectName = ReferencerClass->GetPrefixCPP();
						UObjectOrGCObjectName += ReferencerClass->GetName();
					}
					else
					{
						UObjectOrGCObjectName += ReferencerObject->GetClassName();
					}
				}
				else
				{
					UObjectOrGCObjectName = ReferenceInfo->ReferencerName.ToString();
				}

				Out.Logf(ELogVerbosity::Log, TEXT("%s-> %s::AddReferencedObjects(%s %s)"),
					FCString::Spc(Params.Indent),
					*UObjectOrGCObjectName,
					*GetObjectFlags(Object),
					*Object->GetFullName());

				if (ReferenceInfo->StackFrames.Num())
				{
					ConvertStackFramesToCallstack(ReferenceInfo->StackFrames.GetData(), ReferenceInfo->StackFrames.Num(), Params.Indent, Out);
				}
			}
			else if (ReferenceInfo->Type == EReferenceType::OuterChain)
			{
				Out.Logf(ELogVerbosity::Log, TEXT("%s-> %s = %s %s"),
					FCString::Spc(Params.Indent),
					TEXT("Outer Chain"),
					*GetObjectFlags(Object),
					*Object->GetFullName());
			}
			else
			{
				Out.Logf(ELogVerbosity::Log, TEXT("%s-> %s = %s %s"),
					FCString::Spc(Params.Indent),
					TEXT("UNKNOWN"),
					*GetObjectFlags(Object),
					*Object->GetFullName());
			}

			bPostCallbackContinue = ReferenceCallback(Params);

			ReferencerObject = Object;
			ReferenceInfo = Chain->GetReferenceInfo(NodeIndex);			
		}
		Out.Logf(ELogVerbosity::Log, TEXT("  "));
	}
}

void FReferenceChainSearch::FReferenceChain::FillReferenceInfo()
{
	// The first entry is the object we were looking for references to so add an empty entry for it
	ReferenceInfos.Emplace(nullptr);

	// Iterate over all nodes and add reference info based on the next node (which is the object that referenced the current node)
	for (int32 NodeIndex = 1; NodeIndex < Nodes.Num(); ++NodeIndex)
	{
		FGraphNode* PreviousNode = Nodes[NodeIndex - 1];
		FGraphNode* CurrentNode = Nodes[NodeIndex];

		// Found the PreviousNode in the list of objects referenced by the CurrentNode
		const FNodeReferenceInfo* FoundInfo = nullptr;
		for (const FNodeReferenceInfo& Info : CurrentNode->ReferencedObjects)
		{
			if (Info.Object == PreviousNode)
			{
				FoundInfo = &Info;
				break;
			}
		}
		check(FoundInfo); // because there must have been a reference since we created this chain using it
		check(FoundInfo->Object == PreviousNode);
		ReferenceInfos.Add(FoundInfo);
	}
	check(ReferenceInfos.Num() == Nodes.Num());
}

bool FReferenceChainSearch::FReferenceChain::IsExternal() const
{
	if (Nodes.Num() > 1)
	{
		// Reference is external if the root (the last node) is not in the first node (target)
		return !Nodes.Last()->ObjectInfo->IsIn(Nodes[0]->ObjectInfo);
	}
	else
	{
		return false;
	}
}

/**
* Handles UObject references found by TFastReferenceCollector
*/
class FDirectReferenceProcessor : public FSimpleReferenceProcessorBase
{	
protected:
	TSet<FReferenceChainSearch::FObjectReferenceInfo> ReferencedObjects;
	TMap<UObject*, FGCObjectInfo*>& ObjectToInfoMap;

public:

	FDirectReferenceProcessor(TMap<UObject*, FGCObjectInfo*>& InObjectToInfoMap)
	: ObjectToInfoMap(InObjectToInfoMap)
	{
	}

	void Reset()
	{
		ReferencedObjects.Reset();
	}

	const TSet<FReferenceChainSearch::FObjectReferenceInfo>& GetReferencedObjects() const
	{
		return ReferencedObjects;
	}

	void HandleTokenStreamObjectReference(FGCArrayStruct& ObjectsToSerializeStruct, UObject* ReferencingObject, UObject*& Object, const int32 TokenIndex, const EGCTokenType TokenType, bool bAllowReferenceElimination)
	{		
		if (Object && Object != ReferencingObject) // Skip self-references just in case 
		{
			FGCObjectInfo* ObjectInfo = FGCObjectInfo::FindOrAddInfoHelper(Object, ObjectToInfoMap);

			FReferenceChainSearch::FObjectReferenceInfo RefInfo(ObjectInfo);
			if (!ReferencedObjects.Contains(RefInfo))
			{
				if (TokenIndex >= 0)
				{
					FTokenInfo TokenInfo = ReferencingObject->GetClass()->ReferenceTokenStream.GetTokenInfo(TokenIndex);
					RefInfo.ReferencerName = TokenInfo.Name;
					RefInfo.Type = FReferenceChainSearch::EReferenceType::Property;
				}
				else
				{
					RefInfo.Type = FReferenceChainSearch::EReferenceType::AddReferencedObjects;

					RefInfo.StackFrames.AddUninitialized(FReferenceChainSearch::FObjectReferenceInfo::MaxStackFrames);
					int32 NumStackFrames = FPlatformStackWalk::CaptureStackBackTrace(RefInfo.StackFrames.GetData(), RefInfo.StackFrames.Num());
					RefInfo.StackFrames.SetNum(NumStackFrames);

					if (FGCObject::GGCObjectReferencer && (!ReferencingObject || ReferencingObject == FGCObject::GGCObjectReferencer))
					{
						FString RefName;
						if (FGCObject::GGCObjectReferencer->GetReferencerName(Object, RefName, true))
						{
							// In case FGCObjects misbehave and give us long strings, truncate so we can make them an FName.
							if (RefName.Len() >= NAME_SIZE)
							{
								RefName.LeftInline(NAME_SIZE-1);
							}

							RefInfo.ReferencerName = FName(*RefName);
						}
						else if (ReferencingObject)
						{
							RefInfo.ReferencerName = *ReferencingObject->GetFullName();
						}
					}
					else if (ReferencingObject)
					{
						RefInfo.ReferencerName = *ReferencingObject->GetFullName();
					}
				}

				ReferencedObjects.Emplace(MoveTemp(RefInfo));
			}
		}
	}
};

class FMinimalReferenceProcessor : public FDirectReferenceProcessor
{	
	TSet<UObject*>& TargetObjects;
	bool bSearchInners = false;

public:
	FMinimalReferenceProcessor(
		TSet<UObject*>& InTargetObjects,
		TMap<UObject*, FGCObjectInfo*>& InObjectToInfoMap
	)
	: FDirectReferenceProcessor(InObjectToInfoMap)
	, TargetObjects(InTargetObjects)
	{
	}

	void SetSearchInners(bool InValue)
	{
		bSearchInners = InValue;
	}

	void HandleTokenStreamObjectReference(
		FGCArrayStruct& ObjectsToSerializeStruct,
		UObject* ReferencingObject,
		UObject*& Object,
		const int32 TokenIndex,
		const EGCTokenType TokenType,
		bool bAllowReferenceElimination
	)
	{		
		if (Object && Object != ReferencingObject) // Skip self-references just in case 
		{
			FGCObjectInfo* ObjectInfo = nullptr;
			if (bSearchInners)
			{
				// Walk the outer chain to see if this is the inner of an object we're looking for which would be referenced via this outer chain
				for (UObject* OuterObject = Object->GetOuter(); OuterObject; OuterObject = OuterObject->GetOuter())
				{
					if (TargetObjects.Contains(OuterObject))
					{
						// Create a node to reference this inner object, which we will later connect to the target object
						ObjectInfo = FGCObjectInfo::FindOrAddInfoHelper(Object, ObjectToInfoMap);
						break;
					}
				}
			}
			else if (TargetObjects.Contains(Object))
			{
				ObjectInfo = FGCObjectInfo::FindOrAddInfoHelper(Object, ObjectToInfoMap);
			}

			// We don't care about this reference
			if (!ObjectInfo) 
			{
				return;
			}

			FDirectReferenceProcessor::HandleTokenStreamObjectReference(ObjectsToSerializeStruct, ReferencingObject, Object, TokenIndex, TokenType, bAllowReferenceElimination);
		}
	}
};

template<typename ProcessorType>
class TReferenceSearchReferenceCollector : public TDefaultReferenceCollector<ProcessorType>
{
public:
	TReferenceSearchReferenceCollector (ProcessorType& InProcessor, FGCArrayStruct& InObjectArrayStruct)
		: TDefaultReferenceCollector<ProcessorType>(InProcessor, InObjectArrayStruct)
	{
	}

	virtual bool MarkWeakObjectReferenceForClearing(UObject** WeakReference) override
	{
		// To avoid false positives we need to implement this method just like GC does
		// as these references will be treated as weak and should not be reported
		return true;
	}
};

FReferenceChainSearch::FReferenceChainSearch(UObject* InObjectToFindReferencesTo, EReferenceChainSearchMode Mode /*= EReferenceChainSearchMode::PrintResults*/) 
: FReferenceChainSearch(TConstArrayView<UObject*>(&InObjectToFindReferencesTo, 1), Mode)
{
}

FReferenceChainSearch::FReferenceChainSearch(TConstArrayView<UObject*> InObjectsToFindReferencesTo, EReferenceChainSearchMode Mode /*= EReferenceChainSearchMode::PrintResults*/)
	: ObjectsToFindReferencesTo(InObjectsToFindReferencesTo)
	, SearchMode(Mode)
{
	FSlowHeartBeatScope DisableHangDetection; // This function can be very slow

	for( UObject* Obj : InObjectsToFindReferencesTo )
	{
		check(Obj);
		ObjectInfosToFindReferencesTo.Add(FGCObjectInfo::FindOrAddInfoHelper(Obj, ObjectToInfoMap));
	}

	// First pass is to find all direct references for each object
	if (!!(SearchMode & EReferenceChainSearchMode::Minimal))
	{
		FindMinimalDirectReferencesForObjects();
	}
	else
	{
		FindDirectReferencesForObjects();
	}

	// Second pass creates all reference chains
	PerformSearch();

	if (!!(Mode & (EReferenceChainSearchMode::PrintResults|EReferenceChainSearchMode::PrintAllResults)))
	{
		PrintResults(!!(Mode & EReferenceChainSearchMode::PrintAllResults));
	}
}

FReferenceChainSearch::FReferenceChainSearch(EReferenceChainSearchMode Mode)
	: SearchMode(Mode)
{
}

FReferenceChainSearch::~FReferenceChainSearch()
{
	Cleanup();
}

int64 FReferenceChainSearch::GetAllocatedSize() const
{
	int64 Size = 0;
	Size += ObjectsToFindReferencesTo.GetAllocatedSize();
	Size += ObjectInfosToFindReferencesTo.GetAllocatedSize();
	Size += ReferenceChains.GetAllocatedSize();
	for (const FReferenceChain* Chain : ReferenceChains)
	{
		Size += Chain->GetAllocatedSize();
	}
	Size += AllNodes.GetAllocatedSize();
	Size += AllNodes.Num() * sizeof(FGCObjectInfo); // GC object infos are heap allocated and owned by this structure 
	for (const TPair<FGCObjectInfo*, FGraphNode*> Pair : AllNodes)
	{
		// FGCObjectInfo makes no allocations
		Size += Pair.Value->GetAllocatedSize();
	}
	Size += ObjectToInfoMap.GetAllocatedSize();
	return Size;
}

void FReferenceChainSearch::PerformSearch()
{
	TArray<FGraphNode*> GraphNodesToFind;
	GraphNodesToFind.Reserve(ObjectsToFindReferencesTo.Num());
	for (UObject* Obj : ObjectsToFindReferencesTo)
	{
		FGraphNode* ObjectNodeToFindReferencesTo = FindOrAddNode(Obj);
		check(ObjectNodeToFindReferencesTo);
		GraphNodesToFind.Add(ObjectNodeToFindReferencesTo);
	}

	FGraphNode* GCObjectReferencerGraphNode = nullptr;
	if (FGCObject::GGCObjectReferencer)
	{
		FGCObjectInfo* ObjectInfo = FGCObjectInfo::FindOrAddInfoHelper(FGCObject::GGCObjectReferencer, ObjectToInfoMap);
		GCObjectReferencerGraphNode = AllNodes.FindRef(ObjectInfo);
	}

	// Now it's time to build the reference chain from all of the objects that reference the object to find references to
	if (!(SearchMode & EReferenceChainSearchMode::Direct))
	{		
		BuildReferenceChains(GraphNodesToFind, ReferenceChains, SearchMode, GCObjectReferencerGraphNode);
	}
	else
	{
		BuildReferenceChainsForDirectReferences(GraphNodesToFind, ReferenceChains, SearchMode, GCObjectReferencerGraphNode);
	}
}

#if ENABLE_GC_HISTORY
void FReferenceChainSearch::PerformSearchFromGCSnapshot(UObject* InObjectToFindReferencesTo, FGCSnapshot& InSnapshot)
{
	PerformSearchFromGCSnapshot(TConstArrayView<UObject*>(&InObjectToFindReferencesTo, 1), InSnapshot);
}

void FReferenceChainSearch::PerformSearchFromGCSnapshot(TConstArrayView<UObject*> InObjectsToFindReferencesTo, FGCSnapshot& InSnapshot)
{
	FSlowHeartBeatScope DisableHangDetection; // This function can be very slow

	Cleanup();

	// Temporarily move the generated object info structs. We don't want to copy everything to minimize memory usage and save a few ms
	ObjectToInfoMap = MoveTemp(InSnapshot.ObjectToInfoMap);

	ObjectsToFindReferencesTo = InObjectsToFindReferencesTo;
	ObjectInfosToFindReferencesTo.Reset();
	for (UObject* Obj : InObjectsToFindReferencesTo)
	{
		ObjectInfosToFindReferencesTo.Add(FGCObjectInfo::FindOrAddInfoHelper(Obj, ObjectToInfoMap));
	}
	FGCObjectInfo* GCObjectReferencerInfo = FGCObjectInfo::FindOrAddInfoHelper(FGCObject::GGCObjectReferencer, ObjectToInfoMap);

	// We can avoid copying object infos but we need to regenerate direct reference infos
	for (TPair<FGCObjectInfo*, TArray<FGCDirectReferenceInfo>*>& DirectReferencesInfo : InSnapshot.DirectReferences)
	{
		FGraphNode* ObjectNode = FindOrAddNode(DirectReferencesInfo.Key);
		for (FGCDirectReferenceInfo& ReferenceInfo : *DirectReferencesInfo.Value)
		{
			FGraphNode* ReferencedObjectNode = FindOrAddNode(ReferenceInfo.ReferencedObjectInfo);
			EReferenceType ReferenceType = EReferenceType::Unknown;
			if (GCObjectReferencerInfo == DirectReferencesInfo.Key || ReferenceInfo.ReferencerName == NAME_None)
			{
				ReferenceType = EReferenceType::AddReferencedObjects;
			}
			else
			{
				ReferenceType = EReferenceType::Property;
			}
			ObjectNode->ReferencedObjects.Emplace(FNodeReferenceInfo(ReferencedObjectNode, ReferenceType, ReferenceInfo.ReferencerName));
			ReferencedObjectNode->ReferencedByObjects.Add(ObjectNode);
		}
	}

	// Second pass creates all reference chains
	PerformSearch();

	if (!!(SearchMode & (EReferenceChainSearchMode::PrintResults | EReferenceChainSearchMode::PrintAllResults)))
	{
		PrintResults(!!(SearchMode & EReferenceChainSearchMode::PrintAllResults));
	}

	// Return the object info struct back to the snapshot
	InSnapshot.ObjectToInfoMap = MoveTemp(ObjectToInfoMap);
}
#endif // ENABLE_GC_HISTORY

template<typename ProcessorType>
struct TReferenceSearchHelper
{
	ProcessorType Processor;
	TFastReferenceCollector<
		ProcessorType, 
		TReferenceSearchReferenceCollector<ProcessorType>, 
		FGCArrayPool, 
		EFastReferenceCollectorOptions::AutogenerateTokenStream | EFastReferenceCollectorOptions::ProcessNoOpTokens
	> ReferenceCollector;
	FGCArrayStruct ArrayStruct;

	TReferenceSearchHelper(ProcessorType&& InProcessor)
	: Processor(MoveTemp(InProcessor))
	, ReferenceCollector(Processor, FGCArrayPool::Get())
	{
	}

	static bool DoNotFilterObjects(UObject* Object) { return true; }

	// HANDLE_REFERENCES is a function bool(const TSet<FReferenceChainSearch>&) function returning true to continue the search
	// FILTER_OBJECTS is a function bool(UObject*) that returns true if the object's outgoing references should be collected
	template<typename HANDLE_REFERENCES, typename FILTER_OBJECTS = decltype(DoNotFilterObjects)>
	void CollectAllReferences(bool bGCOnly, HANDLE_REFERENCES HandleReferences, FILTER_OBJECTS FilterObjects = &DoNotFilterObjects)
	{
		TArray<UObject*>& ObjectsToProcess = ArrayStruct.ObjectsToSerialize;
		for (FRawObjectIterator It; It; ++It)
		{
			FUObjectItem* ObjItem = *It;
			UObject* Object = static_cast<UObject*>(ObjItem->Object);

			// We can't ask the iterator for only GC objects because that would skip the GC Object referencer 
			if (bGCOnly && GUObjectArray.IsDisregardForGC(Object) && (Object != FGCObject::GGCObjectReferencer))
			{
				continue;
			}

			if (!FilterObjects(Object))
			{
				continue;
			}

			// Find direct references
			Processor.Reset();
			ObjectsToProcess.Reset();
			ObjectsToProcess.Add(Object);
			ReferenceCollector.CollectReferences(ArrayStruct);

			if (!HandleReferences(Object, Processor.GetReferencedObjects()))
			{
				break;
			}
		}
	}
};

void FReferenceChainSearch::FindDirectReferencesForObjects()
{
	TReferenceSearchHelper<FDirectReferenceProcessor> Search{FDirectReferenceProcessor(ObjectToInfoMap)};
	const bool bGCOnly = !!(SearchMode & EReferenceChainSearchMode::GCOnly);

	// First pass, create nodes for any objects which have outgoing references 
	Search.CollectAllReferences(bGCOnly, [&](UObject* SourceObject, const TSet<FObjectReferenceInfo>& References)
	{
		// Skip node creation if there are no outgoing references that we care about 
		if (References.Num() > 0)
		{
			FindOrAddNode(FGCObjectInfo::FindOrAddInfoHelper(SourceObject, ObjectToInfoMap));
		}
		return true;
	});

	Search.CollectAllReferences(bGCOnly, [&](UObject* SourceObject, const TSet<FObjectReferenceInfo>& References)
	{
		if (References.Num() > 0)
		{
			FGCObjectInfo* SourceObjectInfo = FGCObjectInfo::FindOrAddInfoHelper(SourceObject, ObjectToInfoMap);

			// Build direct reference tree filtering out leaf notes
			for (const FObjectReferenceInfo& ReferenceInfo : References)
			{
				FGCObjectInfo* ReferencedObject = ReferenceInfo.Object;
				if (AllNodes.Contains(ReferencedObject))
				{
					LinkNodes(SourceObjectInfo, ReferencedObject, ReferenceInfo.Type, ReferenceInfo.ReferencerName, ReferenceInfo.StackFrames);
				}
			}
		}
		return true;
	});
}

void FReferenceChainSearch::FindMinimalDirectReferencesForObjects()
{
	TSet<UObject*> TargetObjects;
	TReferenceSearchHelper<FMinimalReferenceProcessor> Search{FMinimalReferenceProcessor(TargetObjects, ObjectToInfoMap)};
	const bool bGCOnly = !!(SearchMode & EReferenceChainSearchMode::GCOnly);

	auto FilterObjects = [&TargetObjects](UObject* Object)
	{
		// Skip objects which are in any of what we're looking for so we only report reference external to that entire group
		for( UObject* OuterObject = Object; OuterObject; OuterObject = OuterObject->GetOuter())
		{
			if (TargetObjects.Contains(OuterObject))
			{
				return false;
			}
		}
		return true;
	};

	for (UObject* Object: ObjectsToFindReferencesTo)
	{
		TargetObjects.Add(Object);
	}
	
	// First pass, create nodes anything that directly references any of the target objects 
	Search.CollectAllReferences(bGCOnly, [&](UObject* SourceObject, const TSet<FObjectReferenceInfo>& References)
	{
		if (References.Num() > 0)
		{
			FGCObjectInfo* SourceObjectInfo = FGCObjectInfo::FindOrAddInfoHelper(SourceObject, ObjectToInfoMap);

			// Build direct reference tree filtering out leaf notes
			for (const FObjectReferenceInfo& ReferenceInfo : References)
			{
				FGCObjectInfo* ReferencedObject = ReferenceInfo.Object;
				LinkNodes(SourceObjectInfo, ReferencedObject, ReferenceInfo.Type, ReferenceInfo.ReferencerName, ReferenceInfo.StackFrames);
			}
		}
		return true;
	}, FilterObjects);

	// If we didn't find a reference to any of the target objects in our first pass, loosen requirements slightly
	TargetObjects.Reset();
	for (FGCObjectInfo* Info : ObjectInfosToFindReferencesTo)
	{
		if (FGraphNode* Node = AllNodes.FindRef(Info); !Node || Node->ReferencedByObjects.Num() == 0)
		{
			TargetObjects.Add(Info->TryResolveObject());
		}
	}

	// We failed to find direct references to some objects, try searching for direct references to one of their inners 
	if (TargetObjects.Num())
	{
		Search.Processor.SetSearchInners(true);
		Search.CollectAllReferences(bGCOnly, [&](UObject* SourceObject, const TSet<FObjectReferenceInfo>& References)
		{
			if (References.Num() > 0)
			{
				FGCObjectInfo* SourceObjectInfo = FGCObjectInfo::FindOrAddInfoHelper(SourceObject, ObjectToInfoMap);

				// Build direct reference tree
				for (const FObjectReferenceInfo& ReferenceInfo : References)
				{
					LinkNodes(SourceObjectInfo, ReferenceInfo.Object, ReferenceInfo.Type, ReferenceInfo.ReferencerName, ReferenceInfo.StackFrames);
					UObject* ReferencedObject = ReferenceInfo.Object->TryResolveObject();
					for (UObject* OuterObject = ReferencedObject; OuterObject; OuterObject = OuterObject->GetOuter())
					{
						if (TargetObjects.Contains(OuterObject))
						{
							LinkNodes(ReferencedObject, OuterObject, EReferenceType::OuterChain, NAME_None);
							TargetObjects.Remove(OuterObject);
						}
					}
				}

				if (TargetObjects.Num() == 0)
				{
					return false;
				}
			}
			return true;
		}, FilterObjects);
	}
}
int32 FReferenceChainSearch::PrintResults(bool bDumpAllChains /*= false*/, UObject* TargetObject /*= nullptr*/) const
{
	return PrintResults([](FCallbackParams& Params) { return true; }, bDumpAllChains, TargetObject);
}

int32 FReferenceChainSearch::PrintResults(TFunctionRef<bool(FCallbackParams& Params)> ReferenceCallback, bool bDumpAllChains /*= false*/, UObject* TargetObject /*= nullptr*/) const
{
	FSlowHeartBeatScope DisableHangDetection; // This function can be very slow

	const int32 MaxChainsToPrint = 100;
	int32 NumPrintedChains = 0;

	for (FReferenceChain* Chain : ReferenceChains)
	{
		if (TargetObject != nullptr && Chain->TargetNode->ObjectInfo->TryResolveObject() != TargetObject)
		{
			continue;
		}

		if (bDumpAllChains || NumPrintedChains < MaxChainsToPrint)
		{
			DumpChain(Chain, ReferenceCallback, *GLog);
			NumPrintedChains++;
		}
		else
		{
			UE_LOG(LogReferenceChain, Log, TEXT("Referenced by %d more reference chain(s)."), ReferenceChains.Num() - NumPrintedChains);
			break;
		}
	}

	if(NumPrintedChains == 0)
	{
		if (TargetObject)
		{
			FGCObjectInfo* ObjInfo = FGCObjectInfo::FindOrAddInfoHelper(TargetObject, *const_cast<TMap<UObject*, FGCObjectInfo*>*>(&ObjectToInfoMap));
			check(ObjInfo);
			UE_LOG(LogReferenceChain, Log, TEXT("%s%s is not currently reachable."),
				*GetObjectFlags(ObjInfo),
				*ObjInfo->GetFullName()
			);
		}
		else
		{
			UE_LOG(LogReferenceChain, Log, TEXT("No target objects are currently reachable."));
		}
	}

	return NumPrintedChains;
}

FString FReferenceChainSearch::GetRootPath(UObject* TargetObject /*= nullptr*/) const
{
	return GetRootPath([](FCallbackParams& Params) { return true; }, TargetObject);
}

FString FReferenceChainSearch::GetRootPath(TFunctionRef<bool(FCallbackParams& Params)> ReferenceCallback, UObject* TargetObject /*= nullptr*/) const
{
	FReferenceChain* Chain = nullptr;
	if (ReferenceChains.Num())
	{
		Chain = ReferenceChains[0];
		if( TargetObject )
		{
			int32 Index = ReferenceChains.IndexOfByPredicate([=](const FReferenceChain* Chain){return Chain->TargetNode->ObjectInfo->TryResolveObject() == TargetObject; });
			Chain = Index != INDEX_NONE ? ReferenceChains[Index] : nullptr;
		}
	}
	
	if (Chain)
	{
		FStringOutputDevice OutString;
		OutString.SetAutoEmitLineTerminator(true);
		DumpChain(Chain, ReferenceCallback, OutString);
		return MoveTemp(OutString);
	}
	else
	{
		if (TargetObject)
		{
			FGCObjectInfo ObjectInfo(TargetObject);
			return FString::Printf(TEXT("%s%s is not currently reachable."),
				*GetObjectFlags(&ObjectInfo),
				*ObjectInfo.GetFullName()
			);
		}
		else
		{
			return TEXT("No target objects are currently reachable.");
		}
	}
}

void FReferenceChainSearch::Cleanup()
{
	for (int32 ChainIndex = 0; ChainIndex < ReferenceChains.Num(); ++ChainIndex)
	{
		delete ReferenceChains[ChainIndex];
	}
	ReferenceChains.Empty();

	for (TPair<FGCObjectInfo*, FGraphNode*>& ObjectNodePair : AllNodes)
	{
		delete ObjectNodePair.Value;
	}
	AllNodes.Empty();

	for (TPair<UObject*, FGCObjectInfo*>& ObjectToInfoPair : ObjectToInfoMap)
	{
		delete ObjectToInfoPair.Value;
	}
	ObjectToInfoMap.Empty();
}

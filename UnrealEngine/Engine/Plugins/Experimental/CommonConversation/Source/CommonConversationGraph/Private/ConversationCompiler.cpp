// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConversationCompiler.h"
#include "ConversationGraph.h"
#include "ConversationGraphSchema.h"
#include "ConversationDatabase.h"

#include "ConversationGraphNode.h"

#include "ConversationSubNode.h"
#include "ConversationTaskNode.h"
#include "ConversationGraphNode_Task.h"

#include "ConversationEntryPointNode.h"
#include "ConversationGraphNode_EntryPoint.h"
#include "ConversationGraphNode_Knot.h"

#include "Kismet2/BlueprintEditorUtils.h"
#include "Stats/StatsMisc.h"
#include "UObject/UObjectHash.h"
#include "ConversationLinkNode.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/AssetManager.h"
#include "UObject/Package.h"
#include "Misc/MessageDialog.h"
#include "FileHelpers.h"

//////////////////////////////////////////////////////////////////////

#define LOCTEXT_NAMESPACE "FConversationCompiler"

enum class EConversationCompilerVersion
{
	Initial,
	AddedSupportForLazyloadMetadata,
	ChangedTheEntryTagsStructureToIncludeGuid,

	// -----<new versions can be added above this line>-------------------------------------------------
	VersionPlusOne,
	LatestVersion = VersionPlusOne - 1
};

//@TODO: CONVERSATION: Push this into the engine, and update UAssetToolsImpl::CreateUniqueAssetName, FBlueprintEditorUtils::FindUniqueKismetName, FNiagaraUtilities::GetUniqueName, etc... to use it
struct FUniqueNameGenerator
{
private:
	FString TrimmedBaseName;
	int32 IntSuffix = 0;
	int32 TrailingIntegerLength = 0;

public:
	FUniqueNameGenerator(const FString& InSeedName)
	{
		int32 CharIndex = InSeedName.Len() - 1;
		while (CharIndex >= 0 && InSeedName[CharIndex] >= TEXT('0') && InSeedName[CharIndex] <= TEXT('9'))
		{
			--CharIndex;
		}

		if ((InSeedName.Len() > 0) && (CharIndex == -1))
		{
			// This is the all numeric name, in this case we'd like to append _number, because just adding a number isn't great
			TrimmedBaseName = InSeedName + TEXT("_");
			IntSuffix = 2;
		}
		else if ((CharIndex >= 0) && (CharIndex < InSeedName.Len() - 1))
		{
			const FString TrailingInteger = InSeedName.RightChop(CharIndex + 1);
			TrailingIntegerLength = TrailingInteger.Len();
			TrimmedBaseName = InSeedName.Left(CharIndex + 1);
			IntSuffix = FCString::Atoi(*TrailingInteger);
		}
		else
		{
			TrimmedBaseName = InSeedName;
		}
	}

	// Generates the next name (does not test for uniqueness, that's up to the caller)
	FString GenerateName()
	{
		FString Result;
		if (IntSuffix < 1)
		{
			Result = TrimmedBaseName;
		}
		else
		{
			FString Suffix = FString::Printf(TEXT("%d"), IntSuffix);
			while (Suffix.Len() < TrailingIntegerLength)
			{
				Suffix = TEXT("0") + Suffix;
			}
			Result = FString::Printf(TEXT("%s%s"), *TrimmedBaseName, *Suffix);
		}

		IntSuffix++;

		return Result;
	}

	// Generates a name that will be unique within the specified outer
	FName GenerateUniqueNameWithinOuter(UObject* Outer)
	{
		while (true)
		{
			FName TestName(*GenerateName());
			if (FindObjectWithOuter(Outer, nullptr, TestName) == nullptr)
			{
				return TestName;
			}
		}
	}
};

//////////////////////////////////////////////////////////////////////
// FConversationCompiler


int32 FConversationCompiler::GetCompilerVersion()
{
	return (int32)EConversationCompilerVersion::LatestVersion;
}

UConversationGraph* FConversationCompiler::CreateNewGraph(UConversationDatabase* ConversationAsset, FName GraphName)
{
	UConversationGraph* NewGraph = CastChecked<UConversationGraph>(FBlueprintEditorUtils::CreateNewGraph(ConversationAsset, GraphName, UConversationGraph::StaticClass(), UConversationGraphSchema::StaticClass()));

	const UEdGraphSchema* Schema = NewGraph->GetSchema();
	Schema->CreateDefaultNodesForGraph(*NewGraph);

	NewGraph->OnCreated();

	return NewGraph;
}

UConversationGraph* FConversationCompiler::AddNewGraph(UConversationDatabase* ConversationAsset, const FString& DesiredName)
{
	// Find a unique default name for the duplicated asset
	FUniqueNameGenerator NameGenerator(DesiredName);
	const FName GraphName = NameGenerator.GenerateUniqueNameWithinOuter(ConversationAsset);

	check(ConversationAsset);
	UConversationGraph* NewGraph = CreateNewGraph(ConversationAsset, GraphName);
	ConversationAsset->SourceGraphs.Add(NewGraph);
	return NewGraph;
}

int32 FConversationCompiler::GetNumGraphs(UConversationDatabase* ConversationAsset)
{
	check(ConversationAsset);
	return ConversationAsset->SourceGraphs.Num();
}

UConversationGraph* FConversationCompiler::GetGraphFromBank(UConversationDatabase* ConversationAsset, int32 Index)
{
	check(ConversationAsset);
	return ConversationAsset->SourceGraphs.IsValidIndex(Index) ? CastChecked<UConversationGraph>(ConversationAsset->SourceGraphs[Index]) : nullptr;
}

void FConversationCompiler::RebuildBank(UConversationDatabase* ConversationAsset)
{
	SCOPE_LOG_TIME_IN_SECONDS(TEXT("FConversationCompiler::RebuildBank"), nullptr);

	check(ConversationAsset);

	ConversationAsset->CompilerVersion = GetCompilerVersion();

	// Merge all the graphs
	TArray<UConversationGraphNode*> AllGraphNodes;
	for (UEdGraph* Graph : ConversationAsset->SourceGraphs)
	{
		Graph->GetNodesOfClass<UConversationGraphNode>(/*inout*/ AllGraphNodes);
	}

	// Clear all error messages and add to the full nodes map (used for the editor only)
	ConversationAsset->FullNodeMap.Reset();
	for (UConversationGraphNode* EdNode : AllGraphNodes)
	{
		EdNode->ErrorMsg.Reset();

		if (EdNode->NodeInstance == nullptr)
		{
			EdNode->ErrorMsg = TEXT("Unknown Node");
			continue;
		}

		check(EdNode->GetRuntimeNode<UConversationNode>());
		check(EdNode->NodeGuid.IsValid());

		// Add to the editor full nodes map
		if (ConversationAsset->FullNodeMap.Contains(EdNode->NodeGuid))
		{
			EdNode->ErrorMsg = TEXT("Duplicate GUID");
		}
		else
		{
			ConversationAsset->FullNodeMap.Add(EdNode->NodeGuid, EdNode->GetRuntimeNode<UConversationNode>());
		}

		// Wire up subnodes
		if (UConversationGraphNode_Task* EdTaskNode = Cast<UConversationGraphNode_Task>(EdNode))
		{
			UConversationTaskNode* TaskNode = EdTaskNode->GetRuntimeNode<UConversationTaskNode>();
			TaskNode->SubNodes.Reset();
			for (UAIGraphNode* SubNode : EdTaskNode->SubNodes)
			{
				UConversationGraphNode* TypedSubNode = Cast<UConversationGraphNode>(SubNode);
				if (ensure(TypedSubNode))
				{
					UConversationSubNode* TaskSubNode = TypedSubNode->GetRuntimeNode<UConversationSubNode>();
					if (ensure(TaskSubNode))
					{
						TaskNode->SubNodes.Add(TaskSubNode);
					}
				}
			}
		}
		
		// Wire up links
		if (UConversationNodeWithLinks* RuntimeNodeWithLinks = Cast<UConversationNodeWithLinks>(EdNode->NodeInstance))
		{
			RuntimeNodeWithLinks->OutputConnections.Reset();

			UEdGraphPin* OutputPin = EdNode->GetOutputPin();
			check(OutputPin);

			ForeachConnectedOutgoingConversationNode(OutputPin, [RuntimeNodeWithLinks](UConversationGraphNode* RemoteNode) {
				RuntimeNodeWithLinks->OutputConnections.Add(RemoteNode->NodeGuid);
			});
		}
	}

	// Gather all entry points
	ConversationAsset->EntryTags.Reset();
	TMap<FGameplayTag, TArray<FGuid>> EntryMap;
	TArray<UConversationGraphNode_EntryPoint*> EntryGraphNodes;
	for (UConversationGraphNode* EdNode : AllGraphNodes)
	{
		if (UConversationGraphNode_EntryPoint* EdEntryNode = Cast<UConversationGraphNode_EntryPoint>(EdNode))
		{
			UConversationEntryPointNode* EntryNode = EdEntryNode->GetRuntimeNode<UConversationEntryPointNode>();
			if (ensure(EntryNode))
			{
				if (EntryNode->EntryTag.IsValid())
				{
					EntryMap.FindOrAdd(EntryNode->EntryTag).Add(EdEntryNode->NodeGuid);
					EntryGraphNodes.Add(EdEntryNode);
				}
				else
				{
					EdEntryNode->ErrorMsg = TEXT("No EntryTag set");
				}
			}
		}
	}

	// Add the resulting entry nodes to the entry list.
	for (const auto& KVP : EntryMap)
	{
		FConversationEntryList Entry;
		Entry.EntryTag = KVP.Key;
		Entry.DestinationList.Append(KVP.Value);

		ConversationAsset->EntryTags.Add(Entry);
	}

	// Determine reachability of the rest of the nodes
	TSet<UConversationGraphNode*> ReachableNodeSet;
	{
		TArray<UConversationGraphNode*> ReachableStack;
		ReachableStack.Append(EntryGraphNodes);

		while (ReachableStack.Num() > 0)
		{
			UConversationGraphNode* Candidate = ReachableStack.Pop();

			if (!ReachableNodeSet.Contains(Candidate) && !Candidate->HasErrors())
			{
				ReachableNodeSet.Add(Candidate);

				if (UEdGraphPin* OutputPin = Candidate->GetOutputPin())
				{
					ForeachConnectedOutgoingConversationNode(OutputPin, [&ReachableStack](UConversationGraphNode* RemoteNode) {
						ReachableStack.Add(RemoteNode);
					});
				}
			}
		}
	}

	// Add the reachable nodes to the map
	ConversationAsset->ReachableNodeMap.Reset();
	ConversationAsset->InternalNodeIds.Reset();
	ConversationAsset->ExitTags.Reset();
	for (UConversationGraphNode* EdNode : ReachableNodeSet)
	{
		UConversationNode* NodeInstance = CastChecked<UConversationNode>(EdNode->NodeInstance);
		NodeInstance->Compiled_NodeGUID = EdNode->NodeGuid;

		// (no need to check for uniqueness, we already did that for all nodes above)
		ConversationAsset->ReachableNodeMap.Add(EdNode->NodeGuid, NodeInstance);
		ConversationAsset->InternalNodeIds.Add(EdNode->NodeGuid);

		if (UConversationLinkNode* ExitTagNode = Cast<UConversationLinkNode>(NodeInstance))
		{
			ConversationAsset->ExitTags.AddLeafTag(ExitTagNode->GetRemoteEntryTag());
		}
	}

// 	TMap<FGameplayTag, UConversationNode*> EntryMap;
// 	TMap<FGuid, UConversationNode*> NodeMap;
// 	TArray<FCommonDialogueBankParticipant> Speakers;
// 	TMap<FGuid, UConversationNode*> FullNodeMap;


	//@TODO: CONVERSATION: Do stuff here for runtime use
	// See UBehaviorTreeComponent::RequestExecution


	//UPROPERTY(AssetRegistrySearchable)
	//TArray<FGuid> LinkedToNodeIds;

}

void FConversationCompiler::ForeachConnectedOutgoingConversationNode(UEdGraphPin* Pin, TFunctionRef<void(UConversationGraphNode*)> Predicate)
{
	for (UEdGraphPin* RemotePin : Pin->LinkedTo)
	{
		if (UConversationGraphNode_Knot* Knot = Cast<UConversationGraphNode_Knot>(RemotePin->GetOwningNode()))
		{
			ForeachConnectedOutgoingConversationNode(Knot->GetOutputPin(), Predicate);
		}
		else if (UConversationGraphNode* RemoteNode = Cast<UConversationGraphNode>(RemotePin->GetOwningNode()))
		{
			Predicate(RemoteNode);
		}
	}
}

void FConversationCompiler::ScanAndRecompileOutOfDateCompiledConversations()
{
	TArray<FAssetData> AllConversations;
	UAssetManager::Get().GetPrimaryAssetDataList(FPrimaryAssetType(UConversationDatabase::StaticClass()->GetFName()), AllConversations);

	TArray<UPackage*> OutOfDateConversationPackages;

	for (FAssetData& ConversationAsset : AllConversations)
	{
		const int32 ConversationCompilerVersion = ConversationAsset.GetTagValueRef<int32>(GET_MEMBER_NAME_CHECKED(UConversationDatabase, CompilerVersion));
		if (ConversationCompilerVersion < GetCompilerVersion())
		{
			if (UConversationDatabase* ConversationDB = Cast<UConversationDatabase>(ConversationAsset.GetAsset()))
			{
				FConversationCompiler::RebuildBank(ConversationDB);
				OutOfDateConversationPackages.Add(ConversationDB->GetOutermost());
			}
		}
	}

	if (OutOfDateConversationPackages.Num() > 0)
	{
		EAppReturnType::Type SaveConversations = FMessageDialog::Open(EAppMsgType::YesNo,
			FText::Format(LOCTEXT("ResaveConversations", "We found {0} conversations on an old version of the compiler that need to be resaved.\n\nSave?"), OutOfDateConversationPackages.Num())
		);

		if (SaveConversations == EAppReturnType::Yes)
		{
			FEditorFileUtils::PromptForCheckoutAndSave(OutOfDateConversationPackages, /*bCheckDirty*/false, /*bPromptToSave*/false);
		}
	}
}

#undef LOCTEXT_NAMESPACE
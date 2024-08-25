// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextGraph_EditorData.h"

#include "UncookedOnlyUtils.h"
#include "Curves/CurveFloat.h"
#include "Graph/AnimNextGraph.h"
#include "Graph/AnimNextGraphEntry.h"
#include "Graph/AnimNextGraph_EdGraphSchema.h"
#include "RigVMModel/RigVMFunctionLibrary.h"
#include "RigVMModel/RigVMNotifications.h"
#include "RigVMModel/Nodes/RigVMCollapseNode.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "UObject/LinkerLoad.h"

void UAnimNextGraph_EditorData::PostLoad()
{
	Super::PostLoad();

	if(GetLinkerCustomVersion(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::AnimNextMoveGraphsToEntries)
	{
		for(TObjectPtr<UAnimNextGraph_EdGraph> Graph : Graphs_DEPRECATED)
		{
			// Must preload entries so their data is populated or we cannot find the appropriate entries for graphs
			for(UAnimNextRigVMAssetEntry* Entry : Entries) 
			{
				Entry->GetLinker()->Preload(Entry);
			}
			
			URigVMGraph* FoundRigVMGraph = GetRigVMGraphForEditorObject(Graph);
			if(FoundRigVMGraph)
			{
				UAnimNextRigVMAssetEntry* FoundEntry = nullptr;
				for(UAnimNextRigVMAssetEntry* Entry : Entries)
				{
					if(UAnimNextGraphEntry* GraphEntry = Cast<UAnimNextGraphEntry>(Entry))
					{
						if(FoundRigVMGraph == static_cast<IAnimNextRigVMGraphInterface*>(GraphEntry)->GetRigVMGraph())
						{
							FoundEntry = Entry;
							break;
						}
					}
				}

				if(FoundEntry)
				{
					UAnimNextGraphEntry* GraphEntry = CastChecked<UAnimNextGraphEntry>(FoundEntry);
					GraphEntry->EdGraph = Graph;

					Graph->Rename(nullptr, FoundEntry, REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
					Graph->Initialize(this);
				}
			}
		}
	}
}

void UAnimNextGraph_EditorData::RecompileVM()
{
	UE::AnimNext::UncookedOnly::FUtils::GetAssetParameters(this, CachedExports);
	UE::AnimNext::UncookedOnly::FUtils::Compile(GetTypedOuter<UAnimNextGraph>());
}

void UAnimNextGraph_EditorData::HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject)
{
	switch(InNotifType)
	{
	case ERigVMGraphNotifType::PinAdded:
		{
			if (URigVMPin* Pin = Cast<URigVMPin>(InSubject))
			{
				if (Pin->IsDecoratorPin())
				{
					RequestAutoVMRecompilation();
				}
			}
			break;
		}
	}

	Super::HandleModifiedEvent(InNotifType, InGraph, InSubject);
}

TConstArrayView<TSubclassOf<UAnimNextRigVMAssetEntry>> UAnimNextGraph_EditorData::GetEntryClasses() const
{
	static const TSubclassOf<UAnimNextRigVMAssetEntry> Classes[] =
	{
		UAnimNextGraphEntry::StaticClass(),
	};
	
	return Classes;
}

void UAnimNextGraph_EditorData::CreateEdGraphForCollapseNode(URigVMCollapseNode* InNode)
{
	if (InNode->GetGraph()->IsA<URigVMFunctionLibrary>())
	{
		if (URigVMGraph* ContainedGraph = InNode->GetContainedGraph())
		{
			// create a sub graph
			UAnimNextGraph_EdGraph* RigFunctionGraph = NewObject<UAnimNextGraph_EdGraph>(this, *InNode->GetName(), RF_Transactional);
			RigFunctionGraph->Schema = UAnimNextGraph_EdGraphSchema::StaticClass();
			RigFunctionGraph->bAllowRenaming = true;
			RigFunctionGraph->bEditable = true;
			RigFunctionGraph->bAllowDeletion = true;
			RigFunctionGraph->ModelNodePath = ContainedGraph->GetNodePath();
			RigFunctionGraph->bIsFunctionDefinition = true;

			RigFunctionGraph->Initialize(this);

			RigVMClient.GetOrCreateController(ContainedGraph)->ResendAllNotifications();
		}
	}
}

UEdGraph* UAnimNextGraph_EditorData::CreateEdGraph(URigVMGraph* InRigVMGraph, bool bForce)
{
	check(InRigVMGraph);

	if(InRigVMGraph->IsA<URigVMFunctionLibrary>())
	{
		return nullptr;
	}

	UAnimNextGraphEntry* Entry = Cast<UAnimNextGraphEntry>(FindEntryForRigVMGraph(InRigVMGraph));
	if(Entry == nullptr)
	{
		// Not found, we could be adding a new entry, in which case the graph wont be assigned yet
		check(Entries.Num() > 0);
		check(Cast<IAnimNextRigVMGraphInterface>(Entries.Last()) != nullptr);
		check(Cast<IAnimNextRigVMGraphInterface>(Entries.Last())->GetRigVMGraph() == nullptr);
		Entry = Cast<UAnimNextGraphEntry>(FindEntryForRigVMGraph(nullptr));
	}

	if(Entry == nullptr)
	{
		return nullptr;
	}
	
	if(bForce)
	{
		RemoveEdGraph(InRigVMGraph);
	}

	FString GraphName = InRigVMGraph->GetName();
	check(!GraphName.IsEmpty());

	UAnimNextGraph_EdGraph* RigFunctionGraph = NewObject<UAnimNextGraph_EdGraph>(Entry, NAME_None, RF_Transactional);
	RigFunctionGraph->Schema = UAnimNextGraph_EdGraphSchema::StaticClass();
	RigFunctionGraph->bAllowDeletion = true;
	RigFunctionGraph->bIsFunctionDefinition = false;
	RigFunctionGraph->ModelNodePath = InRigVMGraph->GetNodePath();
	RigFunctionGraph->Initialize(this);

	Entry->EdGraph = RigFunctionGraph;
	if(Entry->Graph == nullptr)
	{
		Entry->Graph = InRigVMGraph;
	}
	else
	{
		check(Entry->Graph == InRigVMGraph);
	}

	return RigFunctionGraph;
}

bool UAnimNextGraph_EditorData::RemoveEdGraph(URigVMGraph* InModel)
{
	if(UAnimNextGraphEntry* Entry = Cast<UAnimNextGraphEntry>(FindEntryForRigVMGraph(InModel)))
	{
		RigVMClient.DestroyObject(Entry->EdGraph);
		Entry->EdGraph = nullptr;
		return true;
	}
	return false;
}

UAnimNextGraphEntry* UAnimNextGraphLibrary::AddGraph(UAnimNextGraph* InGraph, FName InName, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	return UE::AnimNext::UncookedOnly::FUtils::GetEditorData(InGraph)->AddGraph(InName, bSetupUndoRedo, bPrintPythonCommand);
}

UAnimNextGraphEntry* UAnimNextGraph_EditorData::AddGraph(FName InName, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(InName == NAME_None)
	{
		ReportError(TEXT("UAnimNextGraph_EditorData::AddGraph: Invalid graph name supplied."));
		return nullptr;
	}

	// Check for duplicate name
	FName NewGraphName = InName;
	auto DuplicateNamePredicate = [&NewGraphName](const UAnimNextRigVMAssetEntry* InEntry)
	{
		return InEntry->GetEntryName() == NewGraphName;
	};

	bool bAlreadyExists = Entries.ContainsByPredicate(DuplicateNamePredicate);
	int32 NameNumber = InName.GetNumber() + 1;
	while(bAlreadyExists)
	{
		NewGraphName = FName(InName, NameNumber++);
		bAlreadyExists =  Entries.ContainsByPredicate(DuplicateNamePredicate);
	}

	UAnimNextGraphEntry* NewEntry = CreateNewSubEntry<UAnimNextGraphEntry>(this);
	NewEntry->GraphName = NewGraphName;

	if(bSetupUndoRedo)
	{
		NewEntry->Modify();
		Modify();
	}
	
	Entries.Add(NewEntry);

	// Add new graph
	{
		TGuardValue<bool> EnablePythonPrint(bSuspendPythonMessagesForRigVMClient, !bPrintPythonCommand);
		TGuardValue<bool> DisableAutoCompile(bAutoRecompileVM, false);
		URigVMGraph* NewGraph = RigVMClient.AddModel(URigVMGraph::StaticClass()->GetFName(), bSetupUndoRedo);
		ensure(NewGraph);
		NewEntry->Graph = NewGraph;

		URigVMController* Controller = RigVMClient.GetController(NewGraph);
		UE::AnimNext::UncookedOnly::FUtils::SetupAnimGraph(NewEntry, Controller);
	}

	BroadcastModified();

	return NewEntry;
}
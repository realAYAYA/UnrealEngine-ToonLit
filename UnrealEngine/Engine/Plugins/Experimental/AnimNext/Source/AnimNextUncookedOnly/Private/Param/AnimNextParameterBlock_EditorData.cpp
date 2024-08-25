// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/AnimNextParameterBlock_EditorData.h"

#include "UncookedOnlyUtils.h"
#include "Curves/CurveFloat.h"
#include "Param/AnimNextParameterBlock.h"
#include "Param/AnimNextParameterBlockGraph.h"
#include "Param/AnimNextParameterBlockParameter.h"
#include "Param/AnimNextParameterBlock_EdGraph.h"
#include "Param/AnimNextParameterBlock_EdGraphSchema.h"
#include "RigVMModel/RigVMFunctionLibrary.h"
#include "RigVMModel/Nodes/RigVMCollapseNode.h"
#include "UObject/LinkerLoad.h"

const FName UAnimNextParameterBlock_EditorData::ExportsAssetRegistryTag = TEXT("Exports");

UAnimNextParameterBlockParameter* UAnimNextParameterBlock_EditorData::AddParameter(FName InName, FAnimNextParamType InType, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(InName == NAME_None)
	{
		ReportError(TEXT("UAnimNextParameterBlock_EditorData::AddParameter: Invalid parameter name supplied."));
		return nullptr;
	}

	// Check for duplicate parameter
	const bool bAlreadyExists = Entries.ContainsByPredicate([InName](const UAnimNextRigVMAssetEntry* InEntry)
	{
		if(const UAnimNextParameterBlockParameter* Parameter = Cast<UAnimNextParameterBlockParameter>(InEntry))
		{
			return Parameter->ParameterName == InName;
		}
		return false;
	});

	if(bAlreadyExists)
	{
		ReportError(TEXT("UAnimNextParameterBlock_EditorData::AddParameter: A parameter already exists for the supplied parameter name."));
		return nullptr;
	}

	UAnimNextParameterBlockParameter* NewEntry = CreateNewSubEntry<UAnimNextParameterBlockParameter>(this);
	NewEntry->ParameterName = InName;
	NewEntry->Type = InType;

	if(bSetupUndoRedo)
	{
		NewEntry->Modify();
		Modify();
	}
	
	Entries.Add(NewEntry);

	BroadcastModified();

	return NewEntry;
}


UAnimNextParameterBlockParameter* UAnimNextParameterBlockLibrary::AddParameter(UAnimNextParameterBlock* InBlock, FName InName, EPropertyBagPropertyType InValueType,
	EPropertyBagContainerType InContainerType, const UObject* InValueTypeObject, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	return UE::AnimNext::UncookedOnly::FUtils::GetEditorData(InBlock)->AddParameter(InName, FAnimNextParamType(InValueType, InContainerType, InValueTypeObject), bSetupUndoRedo, bPrintPythonCommand);
}

UAnimNextParameterBlockGraph* UAnimNextParameterBlockLibrary::AddGraph(UAnimNextParameterBlock* InBlock, FName InName, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	return UE::AnimNext::UncookedOnly::FUtils::GetEditorData(InBlock)->AddGraph(InName, bSetupUndoRedo, bPrintPythonCommand);
}

UAnimNextParameterBlockGraph* UAnimNextParameterBlock_EditorData::AddGraph(FName InName, bool bSetupUndoRedo, bool bPrintPythonCommand)
{
	if(InName == NAME_None)
	{
		ReportError(TEXT("UAnimNextParameterBlock_EditorData::AddGraph: Invalid graph name supplied."));
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

	UAnimNextParameterBlockGraph* NewEntry = CreateNewSubEntry<UAnimNextParameterBlockGraph>(this);
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
		UE::AnimNext::UncookedOnly::FUtils::SetupParameterGraph(Controller);
	}

	BroadcastModified();

	return NewEntry;
}

TConstArrayView<TSubclassOf<UAnimNextRigVMAssetEntry>> UAnimNextParameterBlock_EditorData::GetEntryClasses() const
{
	static const TSubclassOf<UAnimNextRigVMAssetEntry> Classes[] =
	{
		UAnimNextParameterBlockGraph::StaticClass(),
		UAnimNextParameterBlockParameter::StaticClass(),
	};
	
	return Classes;
}

void UAnimNextParameterBlock_EditorData::PostLoad()
{
	Super::PostLoad();

	if(GetLinkerCustomVersion(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::AnimNextMoveGraphsToEntries)
	{
		// Must preload entries so their data is populated or we cannot find the appropriate entries for graphs
		for(UAnimNextRigVMAssetEntry* Entry : Entries) 
		{
			Entry->GetLinker()->Preload(Entry);
		}
		
		for(TObjectPtr<UAnimNextParameterBlock_EdGraph> Graph : Graphs_DEPRECATED)
		{
			URigVMGraph* FoundRigVMGraph = GetRigVMGraphForEditorObject(Graph);
			if(FoundRigVMGraph)
			{
				UAnimNextRigVMAssetEntry* FoundEntry = nullptr;
				for(UAnimNextRigVMAssetEntry* Entry : Entries)
				{
					if(UAnimNextParameterBlockGraph* GraphEntry = Cast<UAnimNextParameterBlockGraph>(Entry))
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
					UAnimNextParameterBlockGraph* GraphEntry = CastChecked<UAnimNextParameterBlockGraph>(FoundEntry);
					GraphEntry->EdGraph = Graph;

					Graph->Rename(nullptr, FoundEntry, REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
					Graph->Initialize(this);
				}
			}
		}

		// We used to add a default model that is no longer needed
		URigVMGraph* DefaultModel = RigVMClient.GetDefaultModel();
		if(DefaultModel && DefaultModel->GetName() == TEXT("RigVMGraph"))
		{
			bool bFound = false;
			for(UAnimNextRigVMAssetEntry* Entry : Entries)
			{
				if(UAnimNextParameterBlockGraph* GraphEntry = Cast<UAnimNextParameterBlockGraph>(Entry))
				{
					if(DefaultModel == static_cast<IAnimNextRigVMGraphInterface*>(GraphEntry)->GetRigVMGraph())
					{
						bFound = true;
						break;
					}
				}
			}

			if(!bFound)
			{
				TGuardValue<bool> DisablePythonPrint(bSuspendPythonMessagesForRigVMClient, false);
				TGuardValue<bool> DisableAutoCompile(bAutoRecompileVM, false);
				RigVMClient.RemoveModel(DefaultModel->GetNodePath(), false);
			}
		}

		RecompileVM();
	}
}

void UAnimNextParameterBlock_EditorData::RecompileVM()
{
	UE::AnimNext::UncookedOnly::FUtils::GetAssetParameters(this, CachedExports);
	UE::AnimNext::UncookedOnly::FUtils::CompileVM(GetTypedOuter<UAnimNextParameterBlock>());
}

void UAnimNextParameterBlock_EditorData::CreateEdGraphForCollapseNode(URigVMCollapseNode* InNode)
{
	if (InNode->GetGraph()->IsA<URigVMFunctionLibrary>())
	{
		if (URigVMGraph* ContainedGraph = InNode->GetContainedGraph())
		{
			// create a sub graph
			UAnimNextParameterBlock_EdGraph* RigFunctionGraph = NewObject<UAnimNextParameterBlock_EdGraph>(this, *InNode->GetName(), RF_Transactional);
			RigFunctionGraph->Schema = UAnimNextParameterBlock_EdGraphSchema::StaticClass();
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

UEdGraph* UAnimNextParameterBlock_EditorData::CreateEdGraph(URigVMGraph* InRigVMGraph, bool bForce)
{
	check(InRigVMGraph);

	if(InRigVMGraph->IsA<URigVMFunctionLibrary>())
	{
		return nullptr;
	}

	UAnimNextParameterBlockGraph* Entry = Cast<UAnimNextParameterBlockGraph>(FindEntryForRigVMGraph(InRigVMGraph));
	if(Entry == nullptr)
	{
		// Not found, we could be adding a new entry, in which case the graph wont be assigned yet
		check(Entries.Num() > 0);
		check(Cast<IAnimNextRigVMGraphInterface>(Entries.Last()) != nullptr);
		check(Cast<IAnimNextRigVMGraphInterface>(Entries.Last())->GetRigVMGraph() == nullptr);
		Entry = Cast<UAnimNextParameterBlockGraph>(Entries.Last());
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

	UAnimNextParameterBlock_EdGraph* RigFunctionGraph = NewObject<UAnimNextParameterBlock_EdGraph>(Entry, NAME_None, RF_Transactional);
	RigFunctionGraph->Schema = UAnimNextParameterBlock_EdGraphSchema::StaticClass();
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

bool UAnimNextParameterBlock_EditorData::RemoveEdGraph(URigVMGraph* InModel)
{
	if(UAnimNextParameterBlockGraph* Entry = Cast<UAnimNextParameterBlockGraph>(FindEntryForRigVMGraph(InModel)))
	{
		RigVMClient.DestroyObject(Entry->EdGraph);
		Entry->EdGraph = nullptr;
		return true;
	}
	return false;
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackFilteredObject.h"

#include "EdGraphSchema_Niagara.h"
#include "NiagaraClipboard.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeOutput.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/NiagaraScriptViewModel.h"
#include "ViewModels/NiagaraScriptGraphViewModel.h"
#include "ViewModels/Stack/NiagaraStackFunctionInput.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "ViewModels/Stack/NiagaraStackFunctionInputCollection.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraStackFilteredObject)


#define LOCTEXT_NAMESPACE "UNiagaraStackFilteredObject"


UNiagaraStackFilteredObject::UNiagaraStackFilteredObject()
{
}

void UNiagaraStackFilteredObject::Initialize(FRequiredEntryData InRequiredEntryData, FString InOwningStackItemEditorDataKey)
{
	FString ObjectStackEditorDataKey = FString::Printf(TEXT("%s-FilteredView"), *InOwningStackItemEditorDataKey);
	Super::Initialize(InRequiredEntryData, InOwningStackItemEditorDataKey, ObjectStackEditorDataKey);

	VersionedEmitter = GetEmitterViewModel()->GetEmitter();
	FVersionedNiagaraEmitterData* EmitterData = VersionedEmitter.GetEmitterData();
	if (EmitterData && EmitterData->GetEditorData())
	{
		Cast<UNiagaraEmitterEditorData>(EmitterData->GetEditorData())->OnSummaryViewStateChanged().AddUObject(this, &UNiagaraStackFilteredObject::OnViewStateChanged);
	}
}

void UNiagaraStackFilteredObject::FinalizeInternal()
{
	if (UNiagaraEditorDataBase* EditorData = VersionedEmitter.GetEmitterData()->GetEditorData())
	{
		Cast<UNiagaraEmitterEditorData>(EditorData)->OnSummaryViewStateChanged().RemoveAll(this);
	}
	
	Super::FinalizeInternal();
}

FText UNiagaraStackFilteredObject::GetDisplayName() const
{
	return LOCTEXT("FilteredInputCollectionDisplayName", "Filtered Inputs");
}

bool UNiagaraStackFilteredObject::GetShouldShowInStack() const
{
	return true;
}

bool UNiagaraStackFilteredObject::GetIsEnabled() const
{
	return true;
}

struct FInputData
{
	const UEdGraphPin* Pin;
	FNiagaraTypeDefinition Type;
	int32 SortKey;
	FText Category;
	bool bIsStatic;
	bool bIsHidden;

	TArray<FInputData*> Children;
	bool bIsChild = false;
};


void UNiagaraStackFilteredObject::ProcessInputsForModule(TMap<FGuid, UNiagaraStackFunctionInputCollection*>& NewKnownInputCollections, TArray<UNiagaraStackEntry*>& NewChildren, UNiagaraNodeFunctionCall* InputFunctionCallNode)
{

}

void UNiagaraStackFilteredObject::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues)
{
	
	TSharedPtr<FNiagaraEmitterViewModel> ViewModel = GetEmitterViewModel();
	TSharedPtr<FNiagaraScriptViewModel> ScriptViewModelPinned = ViewModel->GetSharedScriptViewModel();
	checkf(ScriptViewModelPinned.IsValid(), TEXT("Can not refresh children when the script view model has been deleted."));

	AppendEmitterCategory(ScriptViewModelPinned, ENiagaraScriptUsage::EmitterSpawnScript, FGuid(), NewChildren, CurrentChildren, NewIssues);
 	AppendEmitterCategory(ScriptViewModelPinned, ENiagaraScriptUsage::EmitterUpdateScript, FGuid(), NewChildren, CurrentChildren, NewIssues);
	AppendEmitterCategory(ScriptViewModelPinned, ENiagaraScriptUsage::ParticleSpawnScript, FGuid(), NewChildren, CurrentChildren, NewIssues);
	AppendEmitterCategory(ScriptViewModelPinned, ENiagaraScriptUsage::ParticleUpdateScript, FGuid(), NewChildren, CurrentChildren, NewIssues);

	Super::RefreshChildrenInternal(CurrentChildren, NewChildren, NewIssues);
}

void UNiagaraStackFilteredObject::AppendEmitterCategory(TSharedPtr<FNiagaraScriptViewModel> ScriptViewModelPinned, ENiagaraScriptUsage ScriptUsage, FGuid ScriptUsageId, TArray<UNiagaraStackEntry*>& NewChildren, const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<FStackIssue>& NewIssues)
{
	UNiagaraGraph* Graph = ScriptViewModelPinned->GetGraphViewModel()->GetGraph();
	FText ErrorMessage;
	if (FNiagaraStackGraphUtilities::ValidateGraphForOutput(*Graph, ScriptUsage, ScriptUsageId, ErrorMessage) == true)
	{
		UNiagaraNodeOutput* MatchingOutputNode = Graph->FindEquivalentOutputNode(ScriptUsage, ScriptUsageId);

		TArray<UNiagaraNodeFunctionCall*> ModuleNodes;
		FNiagaraStackGraphUtilities::GetOrderedModuleNodes(*MatchingOutputNode, ModuleNodes);

		for (UNiagaraNodeFunctionCall* ModuleNode : ModuleNodes)
		{
			if (ModuleNode && (ModuleNode->HasValidScriptAndGraph() || ModuleNode->Signature.IsValid()))
			{
				RefreshChildrenForFunctionCall(ModuleNode, ModuleNode, CurrentChildren, NewChildren, NewIssues, true);
			}
		}
	}
}

void UNiagaraStackFilteredObject::PostRefreshChildrenInternal()
{
	Super::PostRefreshChildrenInternal();
}

void UNiagaraStackFilteredObject::OnViewStateChanged()
{
	if (!IsFinalized())
	{
		RefreshChildren();
	}
}


#undef LOCTEXT_NAMESPACE

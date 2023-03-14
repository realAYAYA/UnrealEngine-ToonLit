// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackSummaryViewInputCollection.h"
#include "EdGraphSchema_Niagara.h"
#include "NiagaraClipboard.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraSimulationStageBase.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/NiagaraScriptViewModel.h"
#include "ViewModels/NiagaraScriptGraphViewModel.h"
#include "ViewModels/Stack/NiagaraStackFunctionInput.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "ViewModels/Stack/NiagaraStackFunctionInputCollection.h"
#include "ViewModels/Stack/NiagaraStackSimulationStageGroup.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraStackSummaryViewInputCollection)


#define LOCTEXT_NAMESPACE "UNiagaraStackSummaryViewObject"


UNiagaraStackSummaryViewObject::UNiagaraStackSummaryViewObject()
{
}

void UNiagaraStackSummaryViewObject::Initialize(FRequiredEntryData InRequiredEntryData, FVersionedNiagaraEmitterWeakPtr InEmitter, FString InOwningStackItemEditorDataKey)
{
	checkf(Emitter.Emitter == nullptr, TEXT("Can only initialize once."));
	FString ObjectStackEditorDataKey = FString::Printf(TEXT("%s-FilteredView"), *InOwningStackItemEditorDataKey);
	Super::Initialize(InRequiredEntryData, InOwningStackItemEditorDataKey, ObjectStackEditorDataKey);

	Emitter = InEmitter;
	GetEmitterViewModel()->GetOrCreateEditorData().OnSummaryViewStateChanged().AddUObject(this, &UNiagaraStackSummaryViewObject::OnViewStateChanged);
}

void UNiagaraStackSummaryViewObject::FinalizeInternal()
{
	if (GetEmitterViewModel() && GetEmitterViewModel()->GetEmitter().GetEmitterData())
	{
		GetEmitterViewModel()->GetOrCreateEditorData().OnSummaryViewStateChanged().RemoveAll(this);
	}
	Super::FinalizeInternal();
}

FText UNiagaraStackSummaryViewObject::GetDisplayName() const
{
	return LOCTEXT("FilteredInputCollectionDisplayName", "Filtered Inputs");
}

bool UNiagaraStackSummaryViewObject::GetIsEnabled() const
{
	return true;
}


void UNiagaraStackSummaryViewObject::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues)
{	
	TSharedPtr<FNiagaraEmitterViewModel> ViewModel = GetEmitterViewModel();
	TSharedPtr<FNiagaraScriptViewModel> ScriptViewModelPinned = ViewModel->GetSharedScriptViewModel();
	checkf(ScriptViewModelPinned.IsValid(), TEXT("Can not refresh children when the script view model has been deleted."));

	FFunctionCallNodesState State;
	
	AppendEmitterCategory(State, ScriptViewModelPinned, ENiagaraScriptUsage::EmitterSpawnScript, FGuid(), NewIssues);
	AppendEmitterCategory(State, ScriptViewModelPinned, ENiagaraScriptUsage::EmitterUpdateScript, FGuid(), NewIssues);
	
	AppendEmitterCategory(State, ScriptViewModelPinned, ENiagaraScriptUsage::ParticleSpawnScript, FGuid(), NewIssues);
	AppendEmitterCategory(State, ScriptViewModelPinned, ENiagaraScriptUsage::ParticleUpdateScript, FGuid(), NewIssues);

	for (const FNiagaraEventScriptProperties& EventScriptProperties : ViewModel->GetEmitter().GetEmitterData()->GetEventHandlers())
	{
		AppendEmitterCategory(State, ScriptViewModelPinned, ENiagaraScriptUsage::ParticleEventScript, EventScriptProperties.Script->GetUsageId(), NewIssues);
	}
	
	for (UNiagaraSimulationStageBase* SimulationStage : GetEmitterViewModel()->GetEmitter().GetEmitterData()->GetSimulationStages())
	{
		AppendEmitterCategory(State, ScriptViewModelPinned, ENiagaraScriptUsage::ParticleSimulationStageScript, SimulationStage->Script->GetUsageId(), NewIssues);
	}	
	
	ApplyAllFunctionInputsToChildren(State, CurrentChildren, NewChildren, NewIssues, true);
	
	Super::RefreshChildrenInternal(CurrentChildren, NewChildren, NewIssues);

	if (NewChildren.Num() == 0)
	{
		FText EmptyAssignmentNodeMessageText = LOCTEXT("EmptySummaryNodeMessage", "No Parameters in Emitter Summary.\n\nTo add a parameter use the context menu for an available parameter in the stack. Once in the summary view you can use the drop down menu from the icon next to a parameter to set the DisplayName, SortIndex, and Category.");
		UNiagaraStackItemTextContent* EmtpySummaryMessage = FindCurrentChildOfTypeByPredicate<UNiagaraStackItemTextContent>(CurrentChildren,
			[&](UNiagaraStackItemTextContent* CurrentStackItemTextContent) { return CurrentStackItemTextContent->GetDisplayName().IdenticalTo(EmptyAssignmentNodeMessageText); });
		
		if (EmtpySummaryMessage == nullptr)
		{
			EmtpySummaryMessage = NewObject<UNiagaraStackItemTextContent>(this);
			EmtpySummaryMessage->Initialize(CreateDefaultChildRequiredData(), EmptyAssignmentNodeMessageText, GetStackEditorDataKey());
		}
		NewChildren.Add(EmtpySummaryMessage);	
	}
}

void UNiagaraStackSummaryViewObject::AppendEmitterCategory(FFunctionCallNodesState& State, TSharedPtr<FNiagaraScriptViewModel> ScriptViewModelPinned, ENiagaraScriptUsage ScriptUsage, FGuid ScriptUsageId, TArray<FStackIssue>& NewIssues)
{
	UNiagaraGraph* Graph = ScriptViewModelPinned->GetGraphViewModel()->GetGraph();
	FText ErrorMessage;
	if (FNiagaraStackGraphUtilities::ValidateGraphForOutput(*Graph, ScriptUsage, ScriptUsageId, ErrorMessage) == true)
	{
		UNiagaraNodeOutput* MatchingOutputNode = Graph->FindEquivalentOutputNode(ScriptUsage, ScriptUsageId);

		TArray<UNiagaraNodeFunctionCall*> ModuleNodes;
		FNiagaraStackGraphUtilities::GetOrderedModuleNodes(*MatchingOutputNode, ModuleNodes);

		bool bIsFirst = true;
		for (UNiagaraNodeFunctionCall* ModuleNode : ModuleNodes)
		{
			if (ModuleNode && (ModuleNode->HasValidScriptAndGraph() || ModuleNode->Signature.IsValid()))
			{
				AppendInputsForFunctionCall(State, ModuleNode, ModuleNode, NewIssues, true);
			}
		}
	}
}

void UNiagaraStackSummaryViewObject::PostRefreshChildrenInternal()
{
	Super::PostRefreshChildrenInternal();
}

void UNiagaraStackSummaryViewObject::GetSectionsInternal(TArray<FNiagaraStackSection>& OutStackSections) const
{
	if (GetEmitterViewModel().IsValid())
	{
		OutStackSections.Append(GetEmitterViewModel()->GetOrCreateEditorData().GetSummarySections());
	}
}

void UNiagaraStackSummaryViewObject::OnViewStateChanged()
{
	if (!IsFinalized())
	{
		RefreshChildren();
	}
}


#undef LOCTEXT_NAMESPACE

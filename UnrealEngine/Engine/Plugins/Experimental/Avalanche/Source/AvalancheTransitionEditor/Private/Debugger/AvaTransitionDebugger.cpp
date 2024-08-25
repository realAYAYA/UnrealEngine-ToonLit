// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_STATETREE_DEBUGGER

#include "AvaTransitionDebugger.h"
#include "AvaTransitionEditorLog.h"
#include "AvaTransitionTraceAnalyzer.h"
#include "AvaTransitionTree.h"
#include "Debugger/StateTreeDebugger.h"
#include "Extensions/IAvaTransitionDebuggableExtension.h"
#include "StateTreeExecutionTypes.h"
#include "ViewModels/AvaTransitionEditorViewModel.h"
#include "ViewModels/AvaTransitionViewModelSharedData.h"
#include "ViewModels/Registry/AvaTransitionViewModelRegistryCollection.h"

FAvaTransitionDebugger::FAvaTransitionDebugger()
{
}

FAvaTransitionDebugger::~FAvaTransitionDebugger()
{
	Stop();
}

void FAvaTransitionDebugger::Initialize(const TSharedRef<FAvaTransitionEditorViewModel>& InEditorViewModel)
{
	EditorViewModelWeak = InEditorViewModel;
}

void FAvaTransitionDebugger::Start()
{
	if (IsActive())
	{
		return;
	}

	TSharedPtr<FAvaTransitionEditorViewModel> EditorViewModel = EditorViewModelWeak.Pin();
	if (!EditorViewModel.IsValid())
	{
		return;
	}

	UAvaTransitionTree* TransitionTree = EditorViewModel->GetTransitionTree();
	if (!TransitionTree)
	{
		UE_LOG(LogAvaEditorTransition, Warning, TEXT("Transition Tree is invalid. Could not start debugger"));
		return;
	}

	Debugger = MakeShared<FStateTreeDebugger>();
	Debugger->SetAsset(TransitionTree);

	FAvaTransitionTraceAnalyzer::RegisterDebugger(this);
}

void FAvaTransitionDebugger::Stop()
{
	Debugger.Reset();
	TreeDebugInstances.Reset();

	FAvaTransitionTraceAnalyzer::UnregisterDebugger(this);
}

bool FAvaTransitionDebugger::IsActive() const
{
	return Debugger.IsValid();
}

void FAvaTransitionDebugger::OnTreeInstanceStarted(UStateTree& InStateTree, const FStateTreeInstanceDebugId& InInstanceDebugId, const FString& InInstanceName)
{
	TSharedPtr<FAvaTransitionEditorViewModel> EditorViewModel = EditorViewModelWeak.Pin();
	if (!EditorViewModel.IsValid())
	{
		return;
	}

	// Return early if the State Tree of this debugger doesn't match the one being pushed
	UStateTree* TransitionTree = EditorViewModel->GetTransitionTree();
	if (!TransitionTree || TransitionTree->LastCompiledEditorDataHash != InStateTree.LastCompiledEditorDataHash)
	{
		return;
	}

	// Ensure that a debug instance of the given id doesn't exist already
	int32 Index = TreeDebugInstances.IndexOfByKey(InInstanceDebugId);
	if (Index == INDEX_NONE)
	{
		TreeDebugInstances.Emplace(InInstanceDebugId, InInstanceName);
	}
}

void FAvaTransitionDebugger::OnTreeInstanceStopped(const FStateTreeInstanceDebugId& InInstanceDebugId)
{
	TSharedPtr<FAvaTransitionEditorViewModel> EditorViewModel = EditorViewModelWeak.Pin();
	if (!EditorViewModel.IsValid())
	{
		return;
	}

	int32 Index = TreeDebugInstances.IndexOfByKey(InInstanceDebugId);
	if (Index != INDEX_NONE)
	{
		TreeDebugInstances.RemoveAt(Index);
	}
}

void FAvaTransitionDebugger::OnNodeEntered(const FGuid& InNodeId, const FStateTreeInstanceDebugId& InInstanceDebugId)
{
	if (FAvaTransitionTreeDebugInstance* TreeDebugInstance = TreeDebugInstances.FindByKey(InInstanceDebugId))
	{
		TSharedPtr<IAvaTransitionDebuggableExtension> Debuggable = FindDebuggable(InNodeId);
		TreeDebugInstance->EnterDebuggable(Debuggable);
	}
}

void FAvaTransitionDebugger::OnNodeExited(const FGuid& InNodeId, const FStateTreeInstanceDebugId& InInstanceDebugId)
{
	if (FAvaTransitionTreeDebugInstance* TreeDebugInstance = TreeDebugInstances.FindByKey(InInstanceDebugId))
	{
		TSharedPtr<IAvaTransitionDebuggableExtension> Debuggable = FindDebuggable(InNodeId);
		TreeDebugInstance->ExitDebuggable(Debuggable);
	}
}

TSharedPtr<IAvaTransitionDebuggableExtension> FAvaTransitionDebugger::FindDebuggable(const FGuid& InNodeId) const
{
	if (TSharedPtr<FAvaTransitionEditorViewModel> EditorViewModel = EditorViewModelWeak.Pin())
	{
		TSharedRef<FAvaTransitionViewModelRegistryCollection> RegistryCollection = EditorViewModel->GetSharedData()->GetRegistryCollection();
		return UE::AvaCore::CastSharedPtr<IAvaTransitionDebuggableExtension>(RegistryCollection->FindViewModel(InNodeId));
	}
	return nullptr;
}

#endif // WITH_STATETREE_DEBUGGER

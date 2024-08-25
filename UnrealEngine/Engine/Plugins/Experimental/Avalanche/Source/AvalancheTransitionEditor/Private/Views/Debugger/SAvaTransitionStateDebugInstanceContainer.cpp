// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_STATETREE_DEBUGGER

#include "SAvaTransitionStateDebugInstanceContainer.h"
#include "AvaTransitionEditorStyle.h"
#include "Debugger/AvaTransitionDebugger.h"
#include "SAvaTransitionStateDebugInstance.h"
#include "ViewModels/AvaTransitionViewModelSharedData.h"
#include "ViewModels/State/AvaTransitionStateViewModel.h"
#include "Widgets/SBoxPanel.h"

void SAvaTransitionStateDebugInstanceContainer::Construct(const FArguments& InArgs, const TSharedRef<FAvaTransitionStateViewModel>& InStateViewModel)
{
	StateViewModelWeak = InStateViewModel;

	SetVisibility(TAttribute<EVisibility>::CreateSP(this, &SAvaTransitionStateDebugInstanceContainer::GetDebuggerVisibility));

	ChildSlot
	[
		SAssignNew(DebugInstanceContainer, SHorizontalBox)
	];

	Refresh();
}

void SAvaTransitionStateDebugInstanceContainer::Refresh()
{
	TSharedPtr<FAvaTransitionStateViewModel> StateViewModel = StateViewModelWeak.Pin();
	if (!StateViewModel.IsValid())
	{
		return;
	}

	DebugInstanceContainer->ClearChildren();

	for (const FAvaTransitionStateDebugInstance& DebugInstance : StateViewModel->GetDebugInstances())
	{
		DebugInstanceContainer->AddSlot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(SAvaTransitionStateDebugInstance, StateViewModel, DebugInstance)
			];
	}
}

EVisibility SAvaTransitionStateDebugInstanceContainer::GetDebuggerVisibility() const
{
	if (TSharedPtr<FAvaTransitionStateViewModel> StateViewModel = StateViewModelWeak.Pin())
	{
		TSharedRef<FAvaTransitionDebugger> Debugger = StateViewModel->GetSharedData()->GetDebugger();
		if (Debugger->IsActive())
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

#endif // WITH_STATETREE_DEBUGGER

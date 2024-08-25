// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTransitionViewModelSharedData.h"
#include "AvaTransitionSelection.h"
#include "Debugger/AvaTransitionDebugger.h"
#include "Registry/AvaTransitionViewModelRegistryCollection.h"

FAvaTransitionViewModelSharedData::FAvaTransitionViewModelSharedData()
	: RegistryCollection(MakeShared<FAvaTransitionViewModelRegistryCollection>())
	, Selection(MakeShared<FAvaTransitionSelection>())
#if WITH_STATETREE_DEBUGGER
	, Debugger(MakeShared<FAvaTransitionDebugger>())
#endif
{
}

void FAvaTransitionViewModelSharedData::Initialize(const TSharedRef<FAvaTransitionEditorViewModel>& InEditorViewModel)
{
	EditorViewModelWeak = InEditorViewModel;

#if WITH_STATETREE_DEBUGGER
	Debugger->Initialize(InEditorViewModel);
#endif
}

void FAvaTransitionViewModelSharedData::Refresh()
{
	RegistryCollection->Refresh();
}

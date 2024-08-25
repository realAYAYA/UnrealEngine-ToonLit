// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTransitionActions.h"
#include "ViewModels/AvaTransitionEditorViewModel.h"
#include "ViewModels/AvaTransitionViewModelSharedData.h"

FAvaTransitionActions::FAvaTransitionActions(FAvaTransitionEditorViewModel& InOwner)
	: Owner(InOwner)
{
}

bool FAvaTransitionActions::IsEditMode() const
{
	return !Owner.GetSharedData()->IsReadOnly();
}

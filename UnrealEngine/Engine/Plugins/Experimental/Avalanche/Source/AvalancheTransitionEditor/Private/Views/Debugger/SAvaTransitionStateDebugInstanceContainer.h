// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_STATETREE_DEBUGGER

#include "StateTreeExecutionTypes.h"
#include "Widgets/SCompoundWidget.h"

class FAvaTransitionStateDebugInstance;
class FAvaTransitionStateViewModel;
class SHorizontalBox;

/** Widget holding all the Debug Instance Widgets */
class SAvaTransitionStateDebugInstanceContainer : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAvaTransitionStateDebugInstanceContainer) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAvaTransitionStateViewModel>& InStateViewModel);

	void Refresh();

private:
	EVisibility GetDebuggerVisibility() const;

	TSharedPtr<SHorizontalBox> DebugInstanceContainer;

	TWeakPtr<FAvaTransitionStateViewModel> StateViewModelWeak;
};

#endif // WITH_STATETREE_DEBUGGER

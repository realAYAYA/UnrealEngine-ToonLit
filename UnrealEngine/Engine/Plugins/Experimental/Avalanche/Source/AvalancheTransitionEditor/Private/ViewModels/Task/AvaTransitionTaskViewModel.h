// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Extensions/IAvaTransitionWidgetExtension.h"
#include "ViewModels/AvaTransitionNodeViewModel.h"

struct FStateTreeTaskBase;
struct EVisibility;

/** View Model for a Task Node */
class FAvaTransitionTaskViewModel : public FAvaTransitionNodeViewModel, public IAvaTransitionWidgetExtension
{
public:
	UE_AVA_INHERITS(FAvaTransitionTaskViewModel, FAvaTransitionNodeViewModel, IAvaTransitionWidgetExtension)

	explicit FAvaTransitionTaskViewModel(const FStateTreeEditorNode& InEditorNode);

	FText GetTaskDescription() const;

	bool IsEnabled() const;

	EVisibility GetBreakpointVisibility() const;

	FText GetBreakpointTooltip() const;

	void UpdateTaskDescription();

	//~ Begin FAvaTransitionNodeViewModel
	virtual TArrayView<FStateTreeEditorNode> GetNodes(UStateTreeState& InState) const override;
	//~ End FAvaTransitionNodeViewModel

	//~ Begin IAvaTransitionWidgetExtension
	virtual TSharedRef<SWidget> CreateWidget() override;
	//~ End IAvaTransitionWidgetExtension

private:
	FText TaskDescription;
};

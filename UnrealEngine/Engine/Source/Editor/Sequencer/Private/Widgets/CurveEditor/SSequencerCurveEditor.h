// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class FSequencer;
class SCurveEditorPanel;

class SSequencerCurveEditor : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SSequencerCurveEditor)
	{}
	SLATE_END_ARGS()
public:
	void Construct(const FArguments& InArgs, TSharedRef<SCurveEditorPanel> InEditorPanel, TSharedPtr<FSequencer> InSequencer);

	TSharedRef<SWidget> MakeToolbar(TSharedRef<SCurveEditorPanel> InEditorPanel);

private:
	TWeakPtr<FSequencer> WeakSequencer;
};


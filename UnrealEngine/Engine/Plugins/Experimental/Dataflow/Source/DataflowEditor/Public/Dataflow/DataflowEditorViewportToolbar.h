// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "SCommonEditorViewportToolbarBase.h"

// ----------------------------------------------------------------------------------
class SDataflowEditorViewport;

/** Base toolbar for the dataflow. Should be extended to add more features */
class SDataflowViewportSelectionToolBar : public SCommonEditorViewportToolbarBase
{
public:
	SLATE_BEGIN_ARGS(SDataflowViewportSelectionToolBar) {}
	SLATE_END_ARGS()

	/** Constructs this widget with the given parameters */
	void Construct(const FArguments& InArgs, TSharedPtr<SDataflowEditorViewport> InDataflowViewport);
	
private:
	/** Reference to the parent viewport */
	TWeakPtr<SDataflowEditorViewport> EditorViewport;
};
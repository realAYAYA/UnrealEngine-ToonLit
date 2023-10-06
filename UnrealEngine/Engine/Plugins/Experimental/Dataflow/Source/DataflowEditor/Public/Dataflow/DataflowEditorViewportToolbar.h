// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "SEditorViewportToolBarMenu.h"

// ----------------------------------------------------------------------------------
class SDataflowEditorViewport;

class SDataflowViewportSelectionToolBar : public SViewportToolBar
{
public:
	SLATE_BEGIN_ARGS(SDataflowViewportSelectionToolBar) {}
		SLATE_ARGUMENT(TWeakPtr<SDataflowEditorViewport>, EditorViewport)
	SLATE_END_ARGS()

		/** Constructs this widget with the given parameters */
		void Construct(const FArguments& InArgs);

		TSharedRef<SWidget> MakeSelectionModeToolBar();


private:
	/** Reference to the parent viewport */
	TWeakPtr<SDataflowEditorViewport> EditorViewport;
};
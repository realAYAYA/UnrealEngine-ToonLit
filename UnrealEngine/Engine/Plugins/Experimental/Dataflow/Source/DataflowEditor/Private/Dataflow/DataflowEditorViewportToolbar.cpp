// Copyright Epic Games, Inc. All Rights Reserved.
#include "Dataflow/DataflowEditorViewportToolbar.h"
#include "Dataflow/DataflowEditorViewport.h"

void SDataflowViewportSelectionToolBar::Construct(const FArguments& InArgs, TSharedPtr<SDataflowEditorViewport> InDataflowViewport)
{
	EditorViewport = InDataflowViewport;
	
	SCommonEditorViewportToolbarBase::Construct(SCommonEditorViewportToolbarBase::FArguments(), InDataflowViewport);
}
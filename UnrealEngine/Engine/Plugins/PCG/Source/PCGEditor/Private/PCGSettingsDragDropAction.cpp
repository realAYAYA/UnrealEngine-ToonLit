// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGSettingsDragDropAction.h"

#include "PCGEditorGraph.h"
#include "PCGEditorGraphSchema.h"
#include "PCGEditorGraphSchemaActions.h"

FReply FPCGSettingsDragDropAction::DroppedOnPanel(const TSharedRef<class SWidget>& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& Graph)
{
	if (!Graph.GetSchema()->IsA<UPCGEditorGraphSchema>())
	{
		return FReply::Unhandled();
	}

	UPCGEditorGraph* EditorGraph = Cast<UPCGEditorGraph>(&Graph);
	if (!ensure(EditorGraph))
	{
		return FReply::Unhandled();
	}

	FPCGEditorGraphSchemaAction_NewSettingsElement::MakeSettingsNodesOrContextualMenu(Panel, ScreenPosition, &Graph, { SettingsObjectPath }, { GraphPosition }, /*bSelectNewNodes=*/true);

	return FReply::Handled();
}
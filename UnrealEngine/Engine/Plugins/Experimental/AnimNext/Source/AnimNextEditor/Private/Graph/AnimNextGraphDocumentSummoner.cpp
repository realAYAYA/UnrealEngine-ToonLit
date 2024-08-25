// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextGraphDocumentSummoner.h"
#include "Common/SActionMenu.h"
#include "Graph/AnimNextExecuteContext.h"
#include "Graph/AnimNextGraph_EdGraph.h"
#include "Workspace/AnimNextWorkspaceEditor.h"

namespace UE::AnimNext::Editor
{

FAnimNextGraphDocumentSummoner::FAnimNextGraphDocumentSummoner(FName InIdentifier, TSharedPtr<FWorkspaceEditor> InHostingApp)
	: FGraphDocumentSummoner(InIdentifier, InHostingApp)
{
}

FActionMenuContent FAnimNextGraphDocumentSummoner::OnCreateGraphActionMenu(UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed) const
{
	TSharedRef<SActionMenu> ActionMenu = SNew(SActionMenu)
		.AutoExpandActionMenu(bAutoExpand)
		.Graph(InGraph)
		.NewNodePosition(InNodePosition)
		.DraggedFromPins(InDraggedPins)
		.OnClosedCallback(InOnMenuClosed)
		.AllowedExecuteContexts( { FRigVMExecuteContext::StaticStruct(), FAnimNextExecuteContext::StaticStruct() });

	TSharedPtr<SWidget> FilterTextBox = StaticCastSharedRef<SWidget>(ActionMenu->GetFilterTextBox());
	return FActionMenuContent(StaticCastSharedRef<SWidget>(ActionMenu), FilterTextBox);
}

bool FAnimNextGraphDocumentSummoner::IsPayloadSupported(TSharedRef<FTabPayload> Payload) const
{
	UObject* Object = Payload->IsValid() ? FTabPayload_UObject::CastChecked<UObject>(Payload) : nullptr;
	return Object && Object->IsA<UAnimNextGraph_EdGraph>();
}

}

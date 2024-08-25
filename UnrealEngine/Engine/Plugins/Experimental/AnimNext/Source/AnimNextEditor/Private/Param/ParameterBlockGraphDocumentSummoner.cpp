// Copyright Epic Games, Inc. All Rights Reserved.

#include "ParameterBlockGraphDocumentSummoner.h"
#include "Common/SActionMenu.h"
#include "Graph/AnimNextGraph.h"
#include "Param/AnimNextParameterBlock_EdGraph.h"
#include "Param/AnimNextParameterExecuteContext.h"
#include "Workspace/AnimNextWorkspaceEditor.h"

namespace UE::AnimNext::Editor
{

FParameterBlockGraphDocumentSummoner::FParameterBlockGraphDocumentSummoner(FName InIdentifier, TSharedPtr<FWorkspaceEditor> InHostingApp)
	: FGraphDocumentSummoner(InIdentifier, InHostingApp)
{
}

FActionMenuContent FParameterBlockGraphDocumentSummoner::OnCreateGraphActionMenu(UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed) const
{
	TSharedRef<SActionMenu> ActionMenu = SNew(SActionMenu)
		.AutoExpandActionMenu(bAutoExpand)
		.Graph(InGraph)
		.NewNodePosition(InNodePosition)
		.DraggedFromPins(InDraggedPins)
		.OnClosedCallback(InOnMenuClosed)
		.AllowedExecuteContexts( { FRigVMExecuteContext::StaticStruct(), FAnimNextParameterExecuteContext::StaticStruct() });

	TSharedPtr<SWidget> FilterTextBox = StaticCastSharedRef<SWidget>(ActionMenu->GetFilterTextBox());
	return FActionMenuContent(StaticCastSharedRef<SWidget>(ActionMenu), FilterTextBox);
}

bool FParameterBlockGraphDocumentSummoner::IsPayloadSupported(TSharedRef<FTabPayload> Payload) const
{
	UObject* Object = Payload->IsValid() ? FTabPayload_UObject::CastChecked<UObject>(Payload) : nullptr;
	return Object && Object->IsA<UAnimNextParameterBlock_EdGraph>();
}

}

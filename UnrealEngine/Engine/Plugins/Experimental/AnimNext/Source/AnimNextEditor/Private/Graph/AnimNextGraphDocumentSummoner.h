// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Workspace/GraphDocumentSummoner.h"

namespace UE::AnimNext::Editor
{

class FAnimNextGraphDocumentSummoner : public FGraphDocumentSummoner
{
public:
	explicit FAnimNextGraphDocumentSummoner(FName InIdentifier, TSharedPtr<FWorkspaceEditor> InHostingApp);

private:
	// FGraphDocumentSummoner interface
	virtual FActionMenuContent OnCreateGraphActionMenu(UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed) const override;
	virtual bool IsPayloadSupported(TSharedRef<FTabPayload> Payload) const override;
};

}

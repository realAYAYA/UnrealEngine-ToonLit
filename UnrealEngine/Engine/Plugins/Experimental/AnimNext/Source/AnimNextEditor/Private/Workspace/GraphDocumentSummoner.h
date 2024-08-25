// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorkflowOrientedApp/WorkflowUObjectDocuments.h"
#include "EdGraph/EdGraph.h"
#include "GraphEditor.h"

class FWorkflowCentricApplication;
class SGraphEditor;

namespace UE::AnimNext::Editor
{
	class FWorkspaceEditor;
}

namespace UE::AnimNext::Editor
{

struct FGraphDocumentSummoner : public FDocumentTabFactoryForObjects<UEdGraph>
{
public:
	// Delegate called to save the state of a graph
	DECLARE_DELEGATE_ThreeParams(FOnSaveGraphState, UEdGraph*, FVector2D, float);

	FGraphDocumentSummoner(FName InIdentifier, TSharedPtr<FWorkspaceEditor> InHostingApp);

	FOnSaveGraphState& OnSaveGraphState() { return OnSaveGraphStateDelegate; }

private:
	// FWorkflowTabFactory interface
	virtual void OnTabActivated(TSharedPtr<SDockTab> Tab) const override;
	virtual void OnTabBackgrounded(TSharedPtr<SDockTab> Tab) const override;
	virtual void OnTabRefreshed(TSharedPtr<SDockTab> Tab) const override;
	virtual void SaveState(TSharedPtr<SDockTab> Tab, TSharedPtr<FTabPayload> Payload) const override;
	
	// FDocumentTabFactoryForObjects interface
	virtual TAttribute<FText> ConstructTabNameForObject(UEdGraph* DocumentID) const override;
	virtual const FSlateBrush* GetTabIconForObject(const FWorkflowTabSpawnInfo& Info, UEdGraph* DocumentID) const override;
	virtual TSharedRef<SWidget> CreateTabBodyForObject(const FWorkflowTabSpawnInfo& Info, UEdGraph* DocumentID) const override;
	virtual TSharedRef<FGenericTabHistory> CreateTabHistoryNode(TSharedPtr<FTabPayload> Payload) override;
	virtual bool IsPayloadSupported(TSharedRef<FTabPayload> Payload) const override;

	// Create the context 'action' menu used for graphs
	virtual FActionMenuContent OnCreateGraphActionMenu(UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed) const = 0;

	void OnGraphSelectionChanged(const TSet<UObject*>& NewSelection, TWeakObjectPtr<UEdGraph> InGraph) const;
	
	// The hosting app
	TWeakPtr<FWorkspaceEditor> HostingAppPtr;

	// Delegate called to save the state of a graph
	FOnSaveGraphState OnSaveGraphStateDelegate;

	// Command list for graphs
	TSharedPtr<FUICommandList> CommandList;
};

}

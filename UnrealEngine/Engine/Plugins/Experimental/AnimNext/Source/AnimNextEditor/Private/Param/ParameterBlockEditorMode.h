// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "WorkflowOrientedApp/ApplicationMode.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"
#include "WorkflowOrientedApp/WorkflowUObjectDocuments.h"

class FWorkflowCentricApplication;
class SGraphEditor;

namespace UE::AnimNext::Editor
{
	class FParameterBlockEditor;
}

namespace UE::AnimNext::Editor
{

DECLARE_DELEGATE_OneParam(FOnDetailsViewCreated, TSharedRef<IDetailsView>);

struct FParameterBlockEditorDocumentSummoner : public FDocumentTabFactoryForObjects<UEdGraph>
{
public:
	// Delegate called when a graph editor widget is created
	DECLARE_DELEGATE_RetVal_TwoParams(TSharedRef<SGraphEditor>, FOnCreateGraphEditorWidget, TSharedRef<FTabInfo>, UEdGraph*);

	// Delegate called when a graph editor is focused
	DECLARE_DELEGATE_OneParam(FOnGraphEditorFocused, TSharedRef<SGraphEditor>);

	// Delegate called when a graph editor is backgrounded
	DECLARE_DELEGATE_OneParam(FOnGraphEditorBackgrounded, TSharedRef<SGraphEditor>);

	// Delegate called to save the state of a graph
	DECLARE_DELEGATE_ThreeParams(FOnSaveGraphState, UEdGraph*, FVector2D, float);

	// Delegate called to save the state of a graph
	DECLARE_DELEGATE_RetVal_OneParam(const FSlateBrush*, FOnGetTabIcon, UEdGraph*);
	
	FParameterBlockEditorDocumentSummoner(TSharedPtr<FWorkflowCentricApplication> InHostingApp);

	FOnCreateGraphEditorWidget& OnCreateGraphEditorWidget() { return OnCreateGraphEditorWidgetDelegate; }
	FOnGraphEditorFocused& OnGraphEditorFocused() { return OnGraphEditorFocusedDelegate; }
	FOnGraphEditorBackgrounded& OnGraphEditorBackgrounded() { return OnGraphEditorBackgroundedDelegate; }
	FOnSaveGraphState& OnSaveGraphState() { return OnSaveGraphStateDelegate; }
	FOnGetTabIcon& OnGetTabIcon() { return OnGetTabIconDelegate; }

private:
	// FWorkflowTabFactory interface
	virtual void OnTabActivated(TSharedPtr<SDockTab> Tab) const override;
	virtual void OnTabBackgrounded(TSharedPtr<SDockTab> Tab) const override;
	virtual void OnTabRefreshed(TSharedPtr<SDockTab> Tab) const override;
	virtual void SaveState(TSharedPtr<SDockTab> Tab, TSharedPtr<FTabPayload> Payload) const override;
	
	// FDocumentTabFactoryForObjects interface
	virtual TAttribute<FText> ConstructTabNameForObject(UEdGraph* DocumentID) const override;
	virtual TSharedRef<SWidget> CreateTabBodyForObject(const FWorkflowTabSpawnInfo& Info, UEdGraph* DocumentID) const override;
	virtual const FSlateBrush* GetTabIconForObject(const FWorkflowTabSpawnInfo& Info, UEdGraph* DocumentID) const override;
	virtual TSharedRef<FGenericTabHistory> CreateTabHistoryNode(TSharedPtr<FTabPayload> Payload) override;

	// The hosting app
	TWeakPtr<FWorkflowCentricApplication> HostingAppPtr;

	// Delegate called when a graph editor widget is created
	FOnCreateGraphEditorWidget OnCreateGraphEditorWidgetDelegate;

	// Delegate called when a graph editor is focused
	FOnGraphEditorFocused OnGraphEditorFocusedDelegate;

	// Delegate called when a graph editor is backgrounded
	FOnGraphEditorBackgrounded OnGraphEditorBackgroundedDelegate;

	// Delegate called to save the state of a graph
	FOnSaveGraphState OnSaveGraphStateDelegate;

	// Delegate called to get a tab icon for a graph
	FOnGetTabIcon OnGetTabIconDelegate;
};

class FParameterBlockEditorMode : public FApplicationMode
{
public:
	FParameterBlockEditorMode(TSharedRef<FParameterBlockEditor> InHostingApp);

private:
	// FApplicationMode interface
	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) override;
	virtual void AddTabFactory(FCreateWorkflowTabFactory FactoryCreator) override;
	virtual void RemoveTabFactory(FName TabFactoryID) override;
	virtual void PreDeactivateMode() override;
	virtual void PostActivateMode() override;

	// The hosting app
	TWeakPtr<FParameterBlockEditor> ParameterBlockEditorPtr;

	// The tab factories we support
	FWorkflowAllowedTabSet TabFactories;
};

}


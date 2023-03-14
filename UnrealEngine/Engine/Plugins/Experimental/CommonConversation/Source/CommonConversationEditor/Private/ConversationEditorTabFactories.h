// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "EdGraph/EdGraph.h"
#include "Widgets/SWidget.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"
#include "GraphEditor.h"
#include "ConversationEditor.h"
#include "WorkflowOrientedApp/WorkflowUObjectDocuments.h"

class FConversationEditor;

struct FConversationDetailsSummoner : public FWorkflowTabFactory
{
public:
	FConversationDetailsSummoner(TSharedPtr<FConversationEditor> InConversationEditorPtr);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override;

protected:
	TWeakPtr<FConversationEditor> ConversationEditorPtr;
};

struct FConversationSearchSummoner : public FWorkflowTabFactory
{
public:
	FConversationSearchSummoner(TSharedPtr<FConversationEditor> InConversationEditorPtr);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override;

protected:
	TWeakPtr<FConversationEditor> ConversationEditorPtr;
};

struct FConversationTreeEditorSummoner : public FWorkflowTabFactory
{
public:
	FConversationTreeEditorSummoner(TSharedPtr<FConversationEditor> InConversationEditorPtr);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;
	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override;

protected:
	TWeakPtr<FConversationEditor> ConversationEditorPtr;
};

struct FConversationGraphEditorSummoner : public FDocumentTabFactoryForObjects<UEdGraph>
{
public:
	DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<SGraphEditor>, FOnCreateGraphEditorWidget, UEdGraph*);
public:
	FConversationGraphEditorSummoner(TSharedPtr<FConversationEditor> InConversationEditorPtr, FOnCreateGraphEditorWidget CreateGraphEditorWidgetCallback);

	virtual void OnTabActivated(TSharedPtr<SDockTab> Tab) const override;
	virtual void OnTabRefreshed(TSharedPtr<SDockTab> Tab) const override;

protected:
	virtual TAttribute<FText> ConstructTabNameForObject(UEdGraph* DocumentID) const override;
	virtual TSharedRef<SWidget> CreateTabBodyForObject(const FWorkflowTabSpawnInfo& Info, UEdGraph* DocumentID) const override;
	virtual const FSlateBrush* GetTabIconForObject(const FWorkflowTabSpawnInfo& Info, UEdGraph* DocumentID) const override;
	virtual void SaveState(TSharedPtr<SDockTab> Tab, TSharedPtr<FTabPayload> Payload) const override;

protected:
	TWeakPtr<FConversationEditor> ConversationEditorPtr;
	FOnCreateGraphEditorWidget OnCreateGraphEditorWidget;
};


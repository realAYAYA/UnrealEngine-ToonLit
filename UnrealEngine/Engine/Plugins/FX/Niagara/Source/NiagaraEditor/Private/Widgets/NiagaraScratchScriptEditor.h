// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraGraph.h"
#include "Widgets/SCompoundWidget.h"
#include "ViewModels/NiagaraSystemSelectionViewModel.h"
#include "ViewModels/NiagaraScratchPadScriptViewModel.h"
#include "ViewModels/NiagaraScratchPadViewModel.h"
#include "ViewModels/NiagaraScriptGraphViewModel.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"
#include "NiagaraScriptSource.h"
#include "Widgets/SNiagaraScriptGraph.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkflowOrientedApp/WorkflowUObjectDocuments.h"

class SNiagaraScratchPadScriptEditor : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SNiagaraScratchPadScriptEditor) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FNiagaraScratchPadScriptViewModel> InScriptViewModel);

	~SNiagaraScratchPadScriptEditor()
	{
		ClearHandles();
	}

	TSharedPtr<SGraphEditor> GetGraphEditor()
	{
		if (Graph)
			return Graph->GetGraphEditor();
		else
			return nullptr;
	}

	TSharedPtr<FNiagaraScratchPadScriptViewModel> GetViewModel()
	{
		return ScriptViewModel;
	}

	void SetViewModel(TSharedPtr<FNiagaraScratchPadScriptViewModel> InViewModel);


private:

	void ClearHandles()
	{
		if (ScriptViewModel)
		{
			ScriptViewModel->OnNodeIDFocusRequested().Remove(NodeIDHandle);
			ScriptViewModel->OnPinIDFocusRequested().Remove(PinIDHandle);
		}
	}

	FText GetNameText() const
	{
		return ScriptViewModel->GetDisplayName();
	}

	FText GetNameToolTipText() const
	{
		return ScriptViewModel->GetToolTip();
	}

	EVisibility GetUnappliedChangesVisibility() const
	{
		return ScriptViewModel->HasUnappliedChanges() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	FReply OnApplyButtonClicked()
	{
		ScriptViewModel->ApplyChanges();

		return FReply::Handled();
	}

	bool GetApplyButtonIsEnabled() const
	{
		return ScriptViewModel->HasUnappliedChanges();
	}

private:
	TSharedPtr<FNiagaraScratchPadScriptViewModel> ScriptViewModel;
	TSharedPtr<SNiagaraScriptGraph> Graph;

	FDelegateHandle NodeIDHandle;
	FDelegateHandle PinIDHandle;
};


/////////////////////////////////////////////////////
// FNiagaraGraphTabHistory

struct FNiagaraGraphTabHistory : public FGenericTabHistory
{
public:
	/**
	 * @param InFactory		The factory used to regenerate the content
	 * @param InPayload		The payload object used to regenerate the content
	 */
	FNiagaraGraphTabHistory(TSharedPtr<FDocumentTabFactory> InFactory, TSharedPtr<FTabPayload> InPayload)
		: FGenericTabHistory(InFactory, InPayload)
		, SavedLocation(FVector2D::ZeroVector)
		, SavedZoomAmount(INDEX_NONE)
	{

	}

	virtual void EvokeHistory(TSharedPtr<FTabInfo> InTabInfo, bool bPrevTabMatches) override;

	virtual void SaveHistory() override;

	virtual void RestoreHistory() override;

private:
	/** The graph editor represented by this history node. While this node is inactive, the graph editor is invalid */
	TWeakPtr< class SNiagaraScratchPadScriptEditor > GraphEditor;
	/** Saved location the graph editor was at when this history node was last visited */
	FVector2D SavedLocation;
	/** Saved zoom the graph editor was at when this history node was last visited */
	float SavedZoomAmount;
	/** Saved bookmark ID the graph editor was at when this history node was last visited */
	FGuid SavedBookmarkId;
};


struct FNiagaraGraphEditorSummoner : public FDocumentTabFactoryForObjects<UEdGraph>
{
public:
	DECLARE_DELEGATE_RetVal_TwoParams(TSharedRef<SNiagaraScratchPadScriptEditor>, FOnCreateGraphEditorWidget, TSharedRef<FTabInfo>, UEdGraph*);
public:
	FNiagaraGraphEditorSummoner(TSharedPtr<class FNiagaraSystemToolkit> InToolkit, FOnCreateGraphEditorWidget CreateGraphEditorWidgetCallback);

	virtual void OnTabActivated(TSharedPtr<SDockTab> Tab) const override;

	virtual void OnTabBackgrounded(TSharedPtr<SDockTab> Tab) const override;

	virtual void OnTabRefreshed(TSharedPtr<SDockTab> Tab) const override;


	virtual void SaveState(TSharedPtr<SDockTab> Tab, TSharedPtr<FTabPayload> Payload) const override;

	virtual TSharedRef<SDockTab> OnSpawnTab(const FSpawnTabArgs& SpawnArgs, TWeakPtr<FTabManager> WeakTabManager) const override;

protected:
	virtual TAttribute<FText> ConstructTabNameForObject(UEdGraph* DocumentID) const override;

	virtual TSharedRef<SWidget> CreateTabBodyForObject(const FWorkflowTabSpawnInfo& Info, UEdGraph* DocumentID) const override;

	virtual const FSlateBrush* GetTabIconForObject(const FWorkflowTabSpawnInfo& Info, UEdGraph* DocumentID) const override;

	virtual TSharedRef<FGenericTabHistory> CreateTabHistoryNode(TSharedPtr<FTabPayload> Payload) override;

protected:
	TWeakPtr<FNiagaraSystemToolkit> EditorPtr;
	FOnCreateGraphEditorWidget OnCreateGraphEditorWidget;
};

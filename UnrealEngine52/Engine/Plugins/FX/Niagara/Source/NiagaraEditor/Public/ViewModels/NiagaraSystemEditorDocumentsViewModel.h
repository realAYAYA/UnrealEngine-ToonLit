// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraCommon.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"
#include "WorkflowOrientedApp/WorkflowUObjectDocuments.h"
#include "Widgets/SNiagaraScriptGraph.h"
#include "NiagaraSystemEditorDocumentsViewModel.generated.h"

class FNiagaraSystemViewModel;
class FNiagaraScratchPadScriptViewModel;
class FNiagaraSystemGraphSelectionViewModel;
class FNiagaraObjectSelection;
class UNiagaraScript;
class UEdGraph;

UCLASS()
class NIAGARAEDITOR_API UNiagaraSystemEditorDocumentsViewModel : public UObject
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnRequestOpenChildScript, UEdGraph* /* Graph */);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnRequestCloseChildScript, UEdGraph* /* Graph */);

	DECLARE_MULTICAST_DELEGATE_OneParam(FScriptToolkitsActiveDocumentChanged, TSharedPtr<SDockTab>);


public:
	void Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel);
	void Finalize();

	void OpenChildScript(UEdGraph* Graph);
	void CloseChildScript(UEdGraph* Graph);

	TArray<class UNiagaraGraph*> GetEditableGraphsForActiveScriptDocument();
	TArray<class UNiagaraGraph*> GetAllGraphsForActiveScriptDocument();
	TArray<class UNiagaraGraph*> GetEditableGraphsForPrimaryDocument();
	TArray<class UNiagaraGraph*> GetAllGraphsForPrimaryDocument();
	void InitializePreTabManager(TSharedPtr<class FNiagaraSystemToolkit> InToolkit);
	void InitializePostTabManager(TSharedPtr<class FNiagaraSystemToolkit> InToolkit);

	void SetActiveDocumentTab(TSharedPtr<SDockTab> Tab);
	TWeakPtr<SDockTab> GetActiveDocumentTab() const { return ActiveDocumentTab; }
	FScriptToolkitsActiveDocumentChanged& OnActiveDocumentChanged() {
		return ActiveDocChangedDelegate;
	}

	bool IsPrimaryDocumentActive() const;
	TSharedPtr<class FNiagaraScratchPadScriptViewModel> GetActiveScratchPadViewModelIfSet();
	static TSharedPtr < class FNiagaraScratchPadScriptViewModel> GetScratchPadViewModelFromGraph(FNiagaraSystemViewModel* InSysViewModel, UEdGraph* InTargetGraph);

	void DrawAttentionToPrimaryDocument();
	void SetPrimaryDocumentID(const FName& TabId) { PrimaryDocumentTabId = TabId; }
	void SwapEditableScripts(TSharedPtr < class FNiagaraScratchPadScriptViewModel> OldScriptViewModel, TSharedPtr < class FNiagaraScratchPadScriptViewModel> NewScriptViewModel);

protected:
	TSharedPtr<SDockTab> OpenDocument(const UObject* DocumentID, FDocumentTracker::EOpenDocumentCause Cause);

	void NavigateTab(FDocumentTracker::EOpenDocumentCause InCause);

	void CloseDocumentTab(const UObject* DocumentID);

	// Finds any open tabs containing the specified document and adds them to the specified array; returns true if at least one is found
	bool FindOpenTabsContainingDocument(const UObject* DocumentID, /*inout*/ TArray< TSharedPtr<SDockTab> >& Results);

	FName PrimaryDocumentTabId;


private:
	TSharedRef<FNiagaraSystemViewModel> GetSystemViewModel();

	FScriptToolkitsActiveDocumentChanged ActiveDocChangedDelegate;

	TWeakPtr<FNiagaraSystemViewModel> SystemViewModelWeak;
	TWeakPtr< FNiagaraSystemGraphSelectionViewModel> SystemGraphSelectionVMWeak;

	TSharedRef<class SNiagaraScratchPadScriptEditor> CreateGraphEditorWidget(TSharedRef<FTabInfo> InTabInfo, UEdGraph* InGraph);

private:
	TSharedPtr<FDocumentTracker> DocumentManager;
	/** Factory that spawns graph editors; used to look up all tabs spawned by it. */
	TWeakPtr<FDocumentTabFactory> GraphEditorTabFactoryPtr;

	TSharedPtr<class FTabManager> TabManager;

	TWeakPtr<SDockTab> ActiveDocumentTab;

	TSharedPtr<FNiagaraScratchPadScriptViewModel> ActiveDocumentTabScriptViewModel;


};
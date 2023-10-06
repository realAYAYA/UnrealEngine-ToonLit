// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/WeakObjectPtr.h"
#include "EditorUndoClient.h"
#include "StateTreeViewModel.generated.h"

struct FStateTreeEditorBreakpoint;
struct FStateTreeDebugger;
struct FPropertyChangedEvent;

class UStateTreeEditorData;
class UStateTreeState;
class UStateTree;
class FMenuBuilder;
struct FStateTreePropertyPathBinding;

enum class FStateTreeViewModelInsert : uint8
{
	Before,
	After,
	Into,
};

/**
 * ModelView for editing StateTreeEditorData.
 */
class FStateTreeViewModel : public FEditorUndoClient, public TSharedFromThis<FStateTreeViewModel>
{
public:

	DECLARE_MULTICAST_DELEGATE(FOnAssetChanged);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnStatesChanged, const TSet<UStateTreeState*>& /*AffectedStates*/, const FPropertyChangedEvent& /*PropertyChangedEvent*/);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnStateAdded, UStateTreeState* /*ParentState*/, UStateTreeState* /*NewState*/);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnStatesRemoved, const TSet<UStateTreeState*>& /*AffectedParents*/);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnStatesMoved, const TSet<UStateTreeState*>& /*AffectedParents*/, const TSet<UStateTreeState*>& /*MovedStates*/);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSelectionChanged, const TArray<TWeakObjectPtr<UStateTreeState>>& /*SelectedStates*/);

	FStateTreeViewModel();
	virtual ~FStateTreeViewModel() override;

	void Init(UStateTreeEditorData* InTreeData);

	//~ FEditorUndoClient
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

	// Selection handling.
	void ClearSelection();
	void SetSelection(UStateTreeState* Selected);
	void SetSelection(const TArray<TWeakObjectPtr<UStateTreeState>>& InSelection);
	bool IsSelected(const UStateTreeState* State) const;
	bool IsChildOfSelection(const UStateTreeState* State) const;
	void GetSelectedStates(TArray<UStateTreeState*>& OutSelectedStates) const;
	void GetSelectedStates(TArray<TWeakObjectPtr<UStateTreeState>>& OutSelectedStates) const;
	bool HasSelection() const;

	// Returns associated state tree asset.
	const UStateTree* GetStateTree() const;
	
	// Returns array of subtrees to edit.
	TArray<TObjectPtr<UStateTreeState>>* GetSubTrees() const;
	int32 GetSubTreeCount() const;
	void GetSubTrees(TArray<TWeakObjectPtr<UStateTreeState>>& OutSubtrees) const;

	// Gets and sets StateTree view expansion state store in the asset.
	void SetPersistentExpandedStates(TSet<TWeakObjectPtr<UStateTreeState>>& InExpandedStates);
	void GetPersistentExpandedStates(TSet<TWeakObjectPtr<UStateTreeState>>& OutExpandedStates);

	// State manipulation commands
	void AddState(UStateTreeState* AfterState);
	void AddChildState(UStateTreeState* ParentState);
	void RenameState(UStateTreeState* State, FName NewName);
	void RemoveSelectedStates();
	void CopySelectedStates();
	bool CanPasteStatesFromClipboard() const;
	void PasteStatesFromClipboard(UStateTreeState* AfterState);
	void PasteStatesAsChildrenFromClipboard(UStateTreeState* ParentState);
	void DuplicateSelectedStates();
	void MoveSelectedStatesBefore(UStateTreeState* TargetState);
	void MoveSelectedStatesAfter(UStateTreeState* TargetState);
	void MoveSelectedStatesInto(UStateTreeState* TargetState);
	bool CanEnableStates() const;
	bool CanDisableStates() const;
	void SetSelectedStatesEnabled(bool bEnable);
	
	// Force to update the view externally.
	void NotifyAssetChangedExternally() const;
	void NotifyStatesChangedExternally(const TSet<UStateTreeState*>& ChangedStates, const FPropertyChangedEvent& PropertyChangedEvent) const;

	// Debugging
#if WITH_STATETREE_DEBUGGER
	TSharedRef<FStateTreeDebugger> GetDebugger() const { return Debugger; }
	void RefreshDebuggerBreakpoints();
#endif // WITH_STATETREE_DEBUGGER

	bool IsStateActiveInDebugger(const UStateTreeState& State) const;

	// Called when the whole asset is updated (i.e. undo/redo).
	FOnAssetChanged& GetOnAssetChanged() { return OnAssetChanged; }
	
	// Called when States are changed (i.e. change name or properties).
	FOnStatesChanged& GetOnStatesChanged() { return OnStatesChanged; }
	
	// Called each time a state is added.
	FOnStateAdded& GetOnStateAdded() { return OnStateAdded; }

	// Called each time a states are removed.
	FOnStatesRemoved& GetOnStatesRemoved() { return OnStatesRemoved; }

	// Called each time a state is removed.
	FOnStatesMoved& GetOnStatesMoved() { return OnStatesMoved; }

	// Called each time the selection changes.
	FOnSelectionChanged& GetOnSelectionChanged() { return OnSelectionChanged; }

protected:
	void GetExpandedStatesRecursive(UStateTreeState* State, TSet<TWeakObjectPtr<UStateTreeState>>& ExpandedStates);
	void MoveSelectedStates(UStateTreeState* TargetState, const FStateTreeViewModelInsert RelativeLocation);

	void PasteStatesAsChildrenFromText(const FString& TextToImport, UStateTreeState* ParentState, const int32 IndexToInsertAt);

	void HandleIdentifierChanged(const UStateTree& StateTree) const;
	
	void BindToDebuggerDelegates();
	
	TWeakObjectPtr<UStateTreeEditorData> TreeDataWeak;
	TSet<TWeakObjectPtr<UStateTreeState>> SelectedStates;

#if WITH_STATETREE_DEBUGGER
	void HandleBreakpointsChanged(const UStateTree& StateTree);
	void HandlePostCompile(const UStateTree& StateTree);

	TSharedRef<FStateTreeDebugger> Debugger;
	TArray<FGuid> ActiveStates;
#endif // WITH_STATETREE_DEBUGGER
	
	FOnAssetChanged OnAssetChanged;
	FOnStatesChanged OnStatesChanged;
	FOnStateAdded OnStateAdded;
	FOnStatesRemoved OnStatesRemoved;
	FOnStatesMoved OnStatesMoved;
	FOnSelectionChanged OnSelectionChanged;
};

/** Helper class to allow to copy bindings into clipboard. */
UCLASS(Hidden)
class STATETREEEDITORMODULE_API UStateTreeClipboardBindings : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TArray<FStateTreePropertyPathBinding> Bindings;
};
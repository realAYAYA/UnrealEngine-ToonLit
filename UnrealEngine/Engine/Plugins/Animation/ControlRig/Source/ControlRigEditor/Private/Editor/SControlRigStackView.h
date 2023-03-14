// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Graph/ControlRigGraph.h"
#include "ControlRig.h"
#include "ControlRigBlueprint.h"
#include "Widgets/Views/STreeView.h"
#include "RigVMCore/RigVM.h"

class FControlRigEditor;
class SControlRigStackView;
class FUICommandList;
class SSearchBox;

namespace ERigStackEntry
{
	enum Type
	{
		Operator,
		Info,
		Warning,
		Error,
	};
}

/** An item in the stack */
class FRigStackEntry : public TSharedFromThis<FRigStackEntry>
{
public:
	FRigStackEntry(int32 InEntryIndex, ERigStackEntry::Type InEntryType, int32 InInstructionIndex, ERigVMOpCode InOpCode, const FString& InLabel, const FRigVMASTProxy& InProxy);

	TSharedRef<ITableRow> MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable, TSharedRef<FRigStackEntry> InEntry, TSharedRef<FUICommandList> InCommandList, TSharedPtr<SControlRigStackView> InStackView, TWeakObjectPtr<UControlRigBlueprint> InBlueprint);

	int32 EntryIndex;
	ERigStackEntry::Type EntryType;
	int32 InstructionIndex;
	FString CallPath;
	FRigVMCallstack Callstack;
	ERigVMOpCode OpCode;
	FString Label;
	TArray<TSharedPtr<FRigStackEntry>> Children;
};

class SRigStackItem : public STableRow<TSharedPtr<FRigStackEntry>>
{
	SLATE_BEGIN_ARGS(SRigStackItem) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, TSharedRef<FRigStackEntry> InStackEntry, TSharedRef<FUICommandList> InCommandList, TWeakObjectPtr<UControlRigBlueprint> InBlueprint);

private:
	TWeakPtr<FRigStackEntry> WeakStackEntry;
	TWeakObjectPtr<UControlRigBlueprint> WeakBlueprint;
	TWeakPtr<FUICommandList> WeakCommandList;

	FText GetIndexText() const;
	FText GetLabelText() const;
	FSlateFontInfo GetLabelFont() const;
	FText GetVisitedCountText() const;
	FText GetDurationText() const;
};

class SControlRigStackView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SControlRigStackView) {}
	SLATE_END_ARGS()

	~SControlRigStackView();

	void Construct(const FArguments& InArgs, TSharedRef<FControlRigEditor> InControlRigEditor);

	/** Set Selection Changed */
	void OnSelectionChanged(TSharedPtr<FRigStackEntry> Selection, ESelectInfo::Type SelectInfo);

	TSharedPtr< SWidget > CreateContextMenu();

protected:

	/** Rebuild the tree view */
	void RefreshTreeView(URigVM* InVM);

private:

	/** Bind commands that this widget handles */
	void BindCommands();

	/** Make a row widget for the table */
	TSharedRef<ITableRow> MakeTableRowWidget(TSharedPtr<FRigStackEntry> InItem, const TSharedRef<STableViewBase>& OwnerTable, TWeakObjectPtr<UControlRigBlueprint> InBlueprint);

	/** Get children for the tree */
	void HandleGetChildrenForTree(TSharedPtr<FRigStackEntry> InItem, TArray<TSharedPtr<FRigStackEntry>>& OutChildren);

	/** Focus on the selected operator in the graph*/
	void HandleFocusOnSelectedGraphNode();

	void OnVMCompiled(UObject* InCompiledObject, URigVM* InCompiledVM);

	//* Focus on the instruction when the execution is halted */
	void HandleExecutionHalted(const int32 HaltedAtInstruction, UObject* InNode, const FName& InEntryName);

	/** Search box widget */
	TSharedPtr<SSearchBox> FilterBox;
	FText FilterText;
	void OnFilterTextChanged(const FText& SearchText);


	bool bSuspendModelNotifications;
	bool bSuspendControllerSelection;
	void HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);
	void HandleControlRigInitializedEvent(UControlRig* InControlRig, const EControlRigState InState, const FName& InEventName);
	void HandlePreviewControlRigUpdated(FControlRigEditor* InEditor);
	void HandleItemMouseDoubleClick(TSharedPtr<FRigStackEntry> InItem);

	/** Populate the execution stack with descriptive names for each instruction */
	void PopulateStackView(URigVM* InVM);

	TSharedPtr<STreeView<TSharedPtr<FRigStackEntry>>> TreeView;

	/** Command list we bind to */
	TSharedPtr<FUICommandList> CommandList;

	TWeakPtr<FControlRigEditor> ControlRigEditor;
	TWeakObjectPtr<UControlRigBlueprint> ControlRigBlueprint;
	TWeakObjectPtr<UControlRigGraph> Graph;

	TArray<TSharedPtr<FRigStackEntry>> Operators;

	int32 HaltedAtInstruction;

	FDelegateHandle OnModelModified;
	FDelegateHandle OnControlRigInitializedHandle;
	FDelegateHandle OnVMCompiledHandle;
	FDelegateHandle OnPreviewControlRigUpdatedHandle;
};

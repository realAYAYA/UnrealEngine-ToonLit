// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/RigVMEdGraph.h"
#include "RigVMHost.h"
#include "RigVMBlueprint.h"
#include "Widgets/Views/STreeView.h"
#include "RigVMCore/RigVM.h"
#include "RigVMBlueprint.h"

class FRigVMEditor;
class SRigVMExecutionStackView;
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
class RIGVMEDITOR_API FRigStackEntry : public TSharedFromThis<FRigStackEntry>
{
public:
	FRigStackEntry(int32 InEntryIndex, ERigStackEntry::Type InEntryType, int32 InInstructionIndex, ERigVMOpCode InOpCode, const FString& InLabel, const FRigVMASTProxy& InProxy);

	TSharedRef<ITableRow> MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable, TSharedRef<FRigStackEntry> InEntry, TSharedRef<FUICommandList> InCommandList, TSharedPtr<SRigVMExecutionStackView> InStackView, TWeakObjectPtr<URigVMBlueprint> InBlueprint);

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

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, TSharedRef<FRigStackEntry> InStackEntry, TSharedRef<FUICommandList> InCommandList, TWeakObjectPtr<URigVMBlueprint> InBlueprint);

private:
	TWeakPtr<FRigStackEntry> WeakStackEntry;
	TWeakObjectPtr<URigVMBlueprint> WeakBlueprint;
	TWeakPtr<FUICommandList> WeakCommandList;

	mutable double MicroSeconds;
	mutable TArray<double> MicroSecondsFrames;

	FText GetIndexText() const;
	FText GetLabelText() const;
	FSlateFontInfo GetLabelFont() const;
	FText GetTooltip() const;
	FText GetVisitedCountText() const;
	FText GetDurationText() const;
};

class RIGVMEDITOR_API SRigVMExecutionStackView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRigVMExecutionStackView) {}
	SLATE_END_ARGS()

	~SRigVMExecutionStackView();

	void Construct(const FArguments& InArgs, TSharedRef<FRigVMEditor> InRigVMEditor);

	/** Set Selection Changed */
	void OnSelectionChanged(TSharedPtr<FRigStackEntry> Selection, ESelectInfo::Type SelectInfo);

	TSharedPtr< SWidget > CreateContextMenu();

protected:

	/** Rebuild the tree view */
	void RefreshTreeView(URigVM* InVM, FRigVMExtendedExecuteContext* InVMContext);

private:

	/** Bind commands that this widget handles */
	void BindCommands();

	/** Make a row widget for the table */
	TSharedRef<ITableRow> MakeTableRowWidget(TSharedPtr<FRigStackEntry> InItem, const TSharedRef<STableViewBase>& OwnerTable, TWeakObjectPtr<URigVMBlueprint> InBlueprint);

	/** Get children for the tree */
	void HandleGetChildrenForTree(TSharedPtr<FRigStackEntry> InItem, TArray<TSharedPtr<FRigStackEntry>>& OutChildren);

	/** Focus on the selected operator in the graph*/
	void HandleFocusOnSelectedGraphNode();

	/** Offers a dialog to move to a specific instruction */
	void HandleGoToInstruction();

	/** Selects the target instructions for the current selection */
	void HandleSelectTargetInstructions();

	TArray<TSharedPtr<FRigStackEntry>> GetTargetItems(const TArray<TSharedPtr<FRigStackEntry>>& InItems) const;

	void UpdateTargetItemHighlighting();

	void OnVMCompiled(UObject* InCompiledObject, URigVM* InCompiledVM, FRigVMExtendedExecuteContext& InVMContext);

	//* Focus on the instruction when the execution is halted */
	void HandleExecutionHalted(const int32 HaltedAtInstruction, UObject* InNode, const FName& InEntryName);

	/** Search box widget */
	TSharedPtr<SSearchBox> FilterBox;
	FText FilterText;
	void OnFilterTextChanged(const FText& SearchText);


	bool bSuspendModelNotifications;
	bool bSuspendControllerSelection;
	void HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);
	void HandleHostInitializedEvent(URigVMHost* InHost, const FName& InEventName);
	void HandlePreviewHostUpdated(FRigVMEditor* InEditor);
	void HandleItemMouseDoubleClick(TSharedPtr<FRigStackEntry> InItem);

	/** Populate the execution stack with descriptive names for each instruction */
	void PopulateStackView(URigVM* InVM, FRigVMExtendedExecuteContext* InVMContext);

	TSharedPtr<STreeView<TSharedPtr<FRigStackEntry>>> TreeView;

	/** Command list we bind to */
	TSharedPtr<FUICommandList> CommandList;

	TWeakPtr<FRigVMEditor> RigVMEditor;
	TWeakObjectPtr<URigVMBlueprint> RigVMBlueprint;

	TArray<TSharedPtr<FRigStackEntry>> Operators;

	int32 HaltedAtInstruction;

	FDelegateHandle OnModelModified;
	FDelegateHandle OnHostInitializedHandle;
	FDelegateHandle OnVMCompiledHandle;
	FDelegateHandle OnPreviewHostUpdatedHandle;
};

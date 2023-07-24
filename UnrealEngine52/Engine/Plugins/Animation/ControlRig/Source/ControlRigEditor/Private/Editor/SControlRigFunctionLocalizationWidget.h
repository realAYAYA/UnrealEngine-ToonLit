// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ControlRigBlueprint.h"
#include "RigVMModel/Nodes/RigVMLibraryNode.h"
#include "Widgets/Views/SListView.h"
#include "Dialogs/Dialogs.h"

class SControlRigFunctionLocalizationWidget;

class SControlRigFunctionLocalizationItem : public TSharedFromThis<SControlRigFunctionLocalizationItem>
{
public:
	SControlRigFunctionLocalizationItem(const FRigVMGraphFunctionIdentifier& InFunction);

	FText DisplayText;
	FText ToolTipText;
	const FRigVMGraphFunctionIdentifier Function;
};

class SControlRigFunctionLocalizationTableRow : public STableRow<TSharedPtr<SControlRigFunctionLocalizationItem>>
{
	SLATE_BEGIN_ARGS(SControlRigFunctionLocalizationTableRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, SControlRigFunctionLocalizationWidget* InLocalizationWidget, TSharedRef<SControlRigFunctionLocalizationItem> InFunctionItem);
};

class SControlRigFunctionLocalizationWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SControlRigFunctionLocalizationWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const FRigVMGraphFunctionIdentifier& InFunctionToLocalize, UControlRigBlueprint* InTargetBlueprint);

	TSharedRef<ITableRow> GenerateFunctionListRow(TSharedPtr<SControlRigFunctionLocalizationItem> InItem, const TSharedRef<STableViewBase>& InOwningTable);
	ECheckBoxState IsFunctionEnabled(const FRigVMGraphFunctionIdentifier InFunction) const;
	void SetFunctionEnabled(ECheckBoxState NewState, const FRigVMGraphFunctionIdentifier InFunction);
	bool IsFunctionPublic(const FRigVMGraphFunctionIdentifier InFunction) const;

private:

	TArray<FRigVMGraphFunctionIdentifier> FunctionsToLocalize;
	TArray<TSharedPtr<SControlRigFunctionLocalizationItem>> FunctionItems;
	TMap<FRigVMGraphFunctionIdentifier, TSharedRef<SControlRigFunctionLocalizationTableRow>> TableRows;

	friend class SControlRigFunctionLocalizationTableRow;
	friend class SControlRigFunctionLocalizationDialog;
};

class SControlRigFunctionLocalizationDialog : public SWindow
{
public:
	
	SLATE_BEGIN_ARGS(SControlRigFunctionLocalizationDialog)
	{
	}

	SLATE_ARGUMENT(FRigVMGraphFunctionIdentifier, Function)
	SLATE_ARGUMENT(UControlRigBlueprint*, TargetBlueprint)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	EAppReturnType::Type ShowModal();
	TArray<FRigVMGraphFunctionIdentifier>& GetFunctionsToLocalize();

protected:

	TSharedPtr<SControlRigFunctionLocalizationWidget> FunctionsWidget;
	
	FReply OnButtonClick(EAppReturnType::Type ButtonID);
	bool IsOkButtonEnabled() const;
	EAppReturnType::Type UserResponse;
};
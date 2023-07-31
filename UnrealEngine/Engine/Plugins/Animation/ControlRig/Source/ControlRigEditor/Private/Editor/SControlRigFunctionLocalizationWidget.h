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
	SControlRigFunctionLocalizationItem(URigVMLibraryNode* InFunction);

	FText DisplayText;
	FText ToolTipText;
	URigVMLibraryNode* Function;
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

	void Construct(const FArguments& InArgs, URigVMLibraryNode* InFunctionToLocalize, UControlRigBlueprint* InTargetBlueprint);

	TSharedRef<ITableRow> GenerateFunctionListRow(TSharedPtr<SControlRigFunctionLocalizationItem> InItem, const TSharedRef<STableViewBase>& InOwningTable);
	ECheckBoxState IsFunctionEnabled(URigVMLibraryNode* InFunction) const;
	void SetFunctionEnabled(ECheckBoxState NewState, URigVMLibraryNode* InFunction);
	bool IsFunctionPublic(URigVMLibraryNode* InFunction) const;

private:

	TArray<URigVMLibraryNode*> FunctionsToLocalize;
	TArray<TSharedPtr<SControlRigFunctionLocalizationItem>> FunctionItems;
	TMap<URigVMLibraryNode*, TSharedRef<SControlRigFunctionLocalizationTableRow>> TableRows;

	friend class SControlRigFunctionLocalizationTableRow;
	friend class SControlRigFunctionLocalizationDialog;
};

class SControlRigFunctionLocalizationDialog : public SWindow
{
public:
	
	SLATE_BEGIN_ARGS(SControlRigFunctionLocalizationDialog)
	{
	}

	SLATE_ARGUMENT(URigVMLibraryNode*, Function)
	SLATE_ARGUMENT(UControlRigBlueprint*, TargetBlueprint)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	EAppReturnType::Type ShowModal();
	TArray<URigVMLibraryNode*>& GetFunctionsToLocalize();

protected:

	TSharedPtr<SControlRigFunctionLocalizationWidget> FunctionsWidget;
	
	FReply OnButtonClick(EAppReturnType::Type ButtonID);
	bool IsOkButtonEnabled() const;
	EAppReturnType::Type UserResponse;
};
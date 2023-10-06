// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMBlueprint.h"
#include "RigVMModel/Nodes/RigVMLibraryNode.h"
#include "Widgets/Views/SListView.h"
#include "Dialogs/Dialogs.h"

class SRigVMGraphFunctionLocalizationWidget;

class RIGVMEDITOR_API SRigVMGraphFunctionLocalizationItem : public TSharedFromThis<SRigVMGraphFunctionLocalizationItem>
{
public:
	SRigVMGraphFunctionLocalizationItem(const FRigVMGraphFunctionIdentifier& InFunction);

	FText DisplayText;
	FText ToolTipText;
	const FRigVMGraphFunctionIdentifier Function;
};

class RIGVMEDITOR_API SRigVMGraphFunctionLocalizationTableRow : public STableRow<TSharedPtr<SRigVMGraphFunctionLocalizationItem>>
{
	SLATE_BEGIN_ARGS(SRigVMGraphFunctionLocalizationTableRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, SRigVMGraphFunctionLocalizationWidget* InLocalizationWidget, TSharedRef<SRigVMGraphFunctionLocalizationItem> InFunctionItem);
};

class RIGVMEDITOR_API SRigVMGraphFunctionLocalizationWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRigVMGraphFunctionLocalizationWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const FRigVMGraphFunctionIdentifier& InFunctionToLocalize, URigVMBlueprint* InTargetBlueprint);

	TSharedRef<ITableRow> GenerateFunctionListRow(TSharedPtr<SRigVMGraphFunctionLocalizationItem> InItem, const TSharedRef<STableViewBase>& InOwningTable);
	ECheckBoxState IsFunctionEnabled(const FRigVMGraphFunctionIdentifier InFunction) const;
	void SetFunctionEnabled(ECheckBoxState NewState, const FRigVMGraphFunctionIdentifier InFunction);
	bool IsFunctionPublic(const FRigVMGraphFunctionIdentifier InFunction) const;

private:

	TArray<FRigVMGraphFunctionIdentifier> FunctionsToLocalize;
	TArray<TSharedPtr<SRigVMGraphFunctionLocalizationItem>> FunctionItems;
	TMap<FRigVMGraphFunctionIdentifier, TSharedRef<SRigVMGraphFunctionLocalizationTableRow>> TableRows;

	friend class SRigVMGraphFunctionLocalizationTableRow;
	friend class SRigVMGraphFunctionLocalizationDialog;
};

class RIGVMEDITOR_API SRigVMGraphFunctionLocalizationDialog : public SWindow
{
public:
	
	SLATE_BEGIN_ARGS(SRigVMGraphFunctionLocalizationDialog)
	{
	}

	SLATE_ARGUMENT(FRigVMGraphFunctionIdentifier, Function)
	SLATE_ARGUMENT(URigVMBlueprint*, TargetBlueprint)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	EAppReturnType::Type ShowModal();
	TArray<FRigVMGraphFunctionIdentifier>& GetFunctionsToLocalize();

protected:

	TSharedPtr<SRigVMGraphFunctionLocalizationWidget> FunctionsWidget;
	
	FReply OnButtonClick(EAppReturnType::Type ButtonID);
	bool IsOkButtonEnabled() const;
	EAppReturnType::Type UserResponse;
};
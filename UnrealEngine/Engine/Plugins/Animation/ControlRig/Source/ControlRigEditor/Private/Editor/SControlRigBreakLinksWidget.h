// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ControlRigBlueprint.h"
#include "Dialogs/Dialogs.h"
#include "IAssetTypeActions.h"

DECLARE_DELEGATE_OneParam(FControlRigOnFocusOnLinkRequestedDelegate, URigVMLink*);

class SControlRigBreakLinksWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SControlRigBreakLinksWidget) {}
	SLATE_ARGUMENT(FControlRigOnFocusOnLinkRequestedDelegate, OnFocusOnLink)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TArray<URigVMLink*> InLinks);

private:

	/** Handler for when the user double clicks, presses enter, or presses space on an asset */
	void OnLinkDoubleClicked(const URigVMLink* InLink, EAssetTypeActivationMethod::Type ActivationMethod);

	TSharedRef<ITableRow> GenerateItemRow(URigVMLink* Item, const TSharedRef<STableViewBase>& OwnerTable);
 
	TArray<URigVMLink*> Links;
	FControlRigOnFocusOnLinkRequestedDelegate OnFocusOnLink;

	void HandleItemMouseDoubleClick(URigVMLink* InItem);

	friend class SControlRigBreakLinksDialog;
};

class SControlRigBreakLinksDialog : public SWindow
{
public:
	
	SLATE_BEGIN_ARGS(SControlRigBreakLinksDialog)
	{
	}

	SLATE_ARGUMENT(TArray<URigVMLink*>, Links)
	SLATE_ARGUMENT(FControlRigOnFocusOnLinkRequestedDelegate, OnFocusOnLink)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	EAppReturnType::Type ShowModal();

protected:

	TSharedPtr<SControlRigBreakLinksWidget> BreakLinksWidget;
	
	FReply OnButtonClick(EAppReturnType::Type ButtonID);
	bool IsOkButtonEnabled() const;
	EAppReturnType::Type UserResponse;
};
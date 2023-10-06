// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMBlueprint.h"
#include "Dialogs/Dialogs.h"
#include "IAssetTypeActions.h"

class ITableRow;
class STableViewBase;

DECLARE_DELEGATE_OneParam(FRigVMOnFocusOnLinkRequestedDelegate, URigVMLink*);

class RIGVMEDITOR_API SRigVMGraphBreakLinksWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRigVMGraphBreakLinksWidget) {}
	SLATE_ARGUMENT(FRigVMOnFocusOnLinkRequestedDelegate, OnFocusOnLink)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TArray<URigVMLink*> InLinks);

private:

	/** Handler for when the user double clicks, presses enter, or presses space on an asset */
	void OnLinkDoubleClicked(const URigVMLink* InLink, EAssetTypeActivationMethod::Type ActivationMethod);

	TSharedRef<ITableRow> GenerateItemRow(URigVMLink* Item, const TSharedRef<STableViewBase>& OwnerTable);
 
	TArray<URigVMLink*> Links;
	FRigVMOnFocusOnLinkRequestedDelegate OnFocusOnLink;

	void HandleItemMouseDoubleClick(URigVMLink* InItem);

	friend class SRigVMGraphBreakLinksDialog;
};

class RIGVMEDITOR_API SRigVMGraphBreakLinksDialog : public SWindow
{
public:
	
	SLATE_BEGIN_ARGS(SRigVMGraphBreakLinksDialog)
	{
	}

	SLATE_ARGUMENT(TArray<URigVMLink*>, Links)
	SLATE_ARGUMENT(FRigVMOnFocusOnLinkRequestedDelegate, OnFocusOnLink)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	EAppReturnType::Type ShowModal();

protected:

	TSharedPtr<SRigVMGraphBreakLinksWidget> BreakLinksWidget;
	
	FReply OnButtonClick(EAppReturnType::Type ButtonID);
	bool IsOkButtonEnabled() const;
	EAppReturnType::Type UserResponse;
};
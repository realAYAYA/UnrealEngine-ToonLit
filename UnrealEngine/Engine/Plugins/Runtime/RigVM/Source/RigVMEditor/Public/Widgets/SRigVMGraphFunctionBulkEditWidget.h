// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMBlueprint.h"
#include "RigVMModel/Nodes/RigVMLibraryNode.h"
#include "Dialogs/Dialogs.h"
#include "IAssetTypeActions.h"

class SRigVMGraphFunctionBulkEditWidget;

class RIGVMEDITOR_API SRigVMGraphFunctionBulkEditWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRigVMGraphFunctionBulkEditWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, URigVMBlueprint* InBlueprint, URigVMController* InController, URigVMLibraryNode* InFunction, ERigVMControllerBulkEditType InEditType);

private:

	/** Handler for when an asset context menu has been requested. */
	TSharedPtr<SWidget> OnGetAssetContextMenu( const TArray<FAssetData>& SelectedAssets );

	/** Handler for when the user double clicks, presses enter, or presses space on an asset */
	void OnAssetsActivated(const TArray<FAssetData>& ActivatedAssets, EAssetTypeActivationMethod::Type ActivationMethod);

	/** Show a progress bar and load all of the assets */
	void LoadAffectedAssets();

	URigVMBlueprint* Blueprint;
	URigVMController* Controller;
	URigVMLibraryNode* Function;
	ERigVMControllerBulkEditType EditType;

	TSharedRef<SWidget> MakeAssetViewForReferencedAssets();

	friend class SRigVMGraphFunctionBulkEditDialog;
};

class RIGVMEDITOR_API SRigVMGraphFunctionBulkEditDialog : public SWindow
{
public:
	
	SLATE_BEGIN_ARGS(SRigVMGraphFunctionBulkEditDialog)
	{
	}

	SLATE_ARGUMENT(URigVMBlueprint*, Blueprint)
	SLATE_ARGUMENT(URigVMController*, Controller)
	SLATE_ARGUMENT(URigVMLibraryNode*, Function)
	SLATE_ARGUMENT(ERigVMControllerBulkEditType, EditType)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	EAppReturnType::Type ShowModal();

protected:

	TSharedPtr<SRigVMGraphFunctionBulkEditWidget> BulkEditWidget;
	
	FReply OnButtonClick(EAppReturnType::Type ButtonID);
	bool IsOkButtonEnabled() const;
	EAppReturnType::Type UserResponse;
};
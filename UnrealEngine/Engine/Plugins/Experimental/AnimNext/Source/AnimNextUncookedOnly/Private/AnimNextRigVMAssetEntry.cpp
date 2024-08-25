// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextRigVMAssetEntry.h"
#include "AnimNextRigVMAssetEditorData.h"

void UAnimNextRigVMAssetEntry::Initialize(UAnimNextRigVMAssetEditorData* InEditorData)
{
	InEditorData->RigVMGraphModifiedEvent.RemoveAll(this);
	InEditorData->RigVMGraphModifiedEvent.AddUObject(this, &UAnimNextRigVMAssetEntry::HandleRigVMGraphModifiedEvent);
}

bool UAnimNextRigVMAssetEntry::IsAsset() const
{
	// Entries are considered assets to allow using the asset logic for save dialogs, etc.
	// Also, they return true even if pending kill, in order to show up as deleted in these dialogs.
	return IsPackageExternal() && !GetPackage()->HasAnyFlags(RF_Transient) && !HasAnyFlags(RF_Transient | RF_ClassDefaultObject);
}

void UAnimNextRigVMAssetEntry::BroadcastModified()
{
	if(UAnimNextRigVMAssetEditorData* EditorData = Cast<UAnimNextRigVMAssetEditorData>(GetOuter()))
	{
		EditorData->BroadcastModified();
	}
}
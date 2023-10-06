// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/AnimNextParameterBlockEntry.h"
#include "Param/AnimNextParameterBlock_EditorData.h"

void UAnimNextParameterBlockEntry::Initialize(UAnimNextParameterBlock_EditorData* InEditorData)
{
	InEditorData->RigVMGraphModifiedEvent.RemoveAll(this);
	InEditorData->RigVMGraphModifiedEvent.AddUObject(this, &UAnimNextParameterBlockEntry::HandleRigVMGraphModifiedEvent);
}

void UAnimNextParameterBlockEntry::HandleRigVMGraphModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject)
{
	
}

bool UAnimNextParameterBlockEntry::IsAsset() const
{
	// Entries are considered assets to allow using the asset logic for save dialogs, etc.
	// Also, they return true even if pending kill, in order to show up as deleted in these dialogs.
	return IsPackageExternal() && !GetPackage()->HasAnyFlags(RF_Transient) && !HasAnyFlags(RF_Transient | RF_ClassDefaultObject);
}

void UAnimNextParameterBlockEntry::BroadcastModified()
{
	if(UAnimNextParameterBlock_EditorData* EditorData = Cast<UAnimNextParameterBlock_EditorData>(GetOuter()))
	{
		EditorData->BroadcastModified();
	}
}
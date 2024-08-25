// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextRigVMAssetEntry.generated.h"

class UAnimNextRigVMAssetEditorData;
enum class ERigVMGraphNotifType : uint8;
class URigVMGraph;

namespace UE::AnimNext::Editor
{
	class SRigVMAssetViewRow;
	struct FRigVMAssetViewEntry;
}

/** Base class that defines an entry in a parameter block, e.g. a parameter binding */
UCLASS(MinimalAPI, BlueprintType, Abstract)
class UAnimNextRigVMAssetEntry : public UObject
{
	GENERATED_BODY()
public:
	// Binds delegates to owning editor data
	virtual void Initialize(UAnimNextRigVMAssetEditorData* InEditorData);

	// Allow entries the opportunity to handle RigVM modification events
	virtual void HandleRigVMGraphModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject) {}

	// Get this entry's name
	virtual FName GetEntryName() const PURE_VIRTUAL(UAnimNextRigVMAssetEntry::GetEntryName, return NAME_None;)

	// Set this entry's name
	virtual void SetEntryName(FName InName, bool bSetupUndoRedo = true) {}

	// Get the name to be displayed in the UI for this entry
	virtual FText GetDisplayName() const { return FText::FromName(GetEntryName()); }

	// Get the tooltip to be displayed for the name in the UI for this entry
	virtual FText GetDisplayNameTooltip() const { return FText::FromName(GetEntryName()); }

	// UObject interface
	virtual bool IsAsset() const override;

protected:
	void BroadcastModified();
};
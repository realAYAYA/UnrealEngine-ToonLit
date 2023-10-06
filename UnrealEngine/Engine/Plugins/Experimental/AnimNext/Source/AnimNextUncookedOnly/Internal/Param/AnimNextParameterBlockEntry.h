// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextParameterBlockEntry.generated.h"

class UAnimNextParameterBlock_EditorData;
enum class ERigVMGraphNotifType : uint8;
class URigVMGraph;

namespace UE::AnimNext::Editor
{
class SParameterBlockViewRow;
class FParameterBlockEditor;
struct FParameterBlockViewEntry;
}

/** Base class that defines an entry in a parameter block, e.g. a parameter binding */
UCLASS(MinimalAPI, BlueprintType, Abstract)
class UAnimNextParameterBlockEntry : public UObject
{
	GENERATED_BODY()

	friend class UE::AnimNext::Editor::SParameterBlockViewRow;
	friend class UE::AnimNext::Editor::FParameterBlockEditor;
	friend struct UE::AnimNext::Editor::FParameterBlockViewEntry;

	void Initialize(UAnimNextParameterBlock_EditorData* InEditorData);

	void HandleRigVMGraphModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);
	
	// Get the name to be displayed in the UI for this entry
	virtual FText GetDisplayName() const PURE_VIRTUAL(GetDisplayName, return FText::GetEmpty();)

	// Get the tooltip to be displayed for the name in the UI for this entry
	virtual FText GetDisplayNameTooltip() const PURE_VIRTUAL(GetDisplayNameTooltip, return FText::GetEmpty();)

	// Get any external objects/assets that this entry can edit
	virtual void GetEditedObjects(TArray<UObject*>& OutObjects) const {}

	// UObject interface
	virtual bool IsAsset() const override;

protected:
	void BroadcastModified();
};
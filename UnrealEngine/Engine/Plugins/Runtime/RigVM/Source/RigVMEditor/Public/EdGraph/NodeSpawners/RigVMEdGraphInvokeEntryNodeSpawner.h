// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/NodeSpawners/RigVMEdGraphNodeSpawner.h"
#include "RigVMModel/RigVMGraph.h"
#include "RigVMEdGraphInvokeEntryNodeSpawner.generated.h"

class URigVMEdGraphNode;
class URigVMBlueprint;

UCLASS(Transient)
class RIGVMEDITOR_API URigVMEdGraphInvokeEntryNodeSpawner : public URigVMEdGraphNodeSpawner
{
	GENERATED_BODY()

public:

	static URigVMEdGraphInvokeEntryNodeSpawner* CreateForEntry(URigVMBlueprint* InBlueprint, const FName& InEntryName, const FText& InMenuDesc, const FText& InCategory, const FText& InTooltip);

	// UBlueprintNodeSpawner interface
	virtual void Prime() override;
	virtual FBlueprintNodeSignature GetSpawnerSignature() const override;
	virtual FBlueprintActionUiSpec GetUiSpec(FBlueprintActionContext const& Context, FBindingSet const& Bindings) const override;
	virtual UEdGraphNode* Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const override;
	virtual bool IsTemplateNodeFilteredOut(FBlueprintActionFilter const& Filter) const override;
	// End UBlueprintNodeSpawner interface

private:

	/** The pin type we will spawn */
	TWeakObjectPtr<URigVMBlueprint> Blueprint;
	TWeakObjectPtr<URigVMGraph> GraphOwner;
	FName EntryName;

	friend class UEngineTestControlRig;
};

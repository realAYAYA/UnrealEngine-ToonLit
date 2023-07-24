// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActionFilter.h"
#include "BlueprintNodeSignature.h"
#include "BlueprintFieldNodeSpawner.h"
#include "RigVMModel/RigVMGraph.h"
#include "ControlRigInvokeEntryNodeSpawner.generated.h"

class UControlRigGraphNode;
class UControlRigBlueprint;

UCLASS(Transient)
class CONTROLRIGEDITOR_API UControlRigInvokeEntryNodeSpawner : public UBlueprintNodeSpawner
{
	GENERATED_BODY()

public:

	static UControlRigInvokeEntryNodeSpawner* CreateForEntry(UControlRigBlueprint* InBlueprint, const FName& InEntryName, const FText& InMenuDesc, const FText& InCategory, const FText& InTooltip);

	// UBlueprintNodeSpawner interface
	virtual void Prime() override;
	virtual FBlueprintNodeSignature GetSpawnerSignature() const override;
	virtual FBlueprintActionUiSpec GetUiSpec(FBlueprintActionContext const& Context, FBindingSet const& Bindings) const override;
	virtual UEdGraphNode* Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const override;
	// End UBlueprintNodeSpawner interface

private:

	/** The pin type we will spawn */
	TWeakObjectPtr<UControlRigBlueprint> Blueprint;
	TWeakObjectPtr<URigVMGraph> GraphOwner;
	FName EntryName;

	friend class UEngineTestControlRig;
};

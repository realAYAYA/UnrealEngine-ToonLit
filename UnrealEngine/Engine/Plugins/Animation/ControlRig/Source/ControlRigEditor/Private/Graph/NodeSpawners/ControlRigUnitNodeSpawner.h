// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "UObject/UnrealType.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "BlueprintActionFilter.h"
#include "BlueprintNodeSignature.h"
#include "BlueprintFieldNodeSpawner.h"
#include "ControlRigBlueprint.h"
#include "Graph/ControlRigGraph.h"
#include "Graph/ControlRigGraphNode.h"
#include "RigVMModel/Nodes/RigVMUnitNode.h"
#include "ControlRigUnitNodeSpawner.generated.h"

class UControlRigGraphNode;

UCLASS(Transient)
class CONTROLRIGEDITOR_API UControlRigUnitNodeSpawner : public UBlueprintNodeSpawner
{
	GENERATED_BODY()

public:
	/**
	 * Creates a new UControlRigVariableNodeSpawner, charged with spawning 
	 * a new member-variable node
	 * 
	 * @return A newly allocated instance of this class.
	 */
	static UControlRigUnitNodeSpawner* CreateFromStruct(UScriptStruct* InStruct, const FText& InMenuDesc, const FText& InCategory, const FText& InTooltip);

	// UBlueprintNodeSpawner interface
	virtual void Prime() override;
	virtual FBlueprintNodeSignature GetSpawnerSignature() const override;
	virtual FBlueprintActionUiSpec GetUiSpec(FBlueprintActionContext const& Context, FBindingSet const& Bindings) const override;
	virtual UEdGraphNode* Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const override;
	static void HookupMutableNode(URigVMNode* InModelNode, UControlRigBlueprint* InRigBlueprint);
	// End UBlueprintNodeSpawner interface

private:
	/** The unit type we will spawn */
	UPROPERTY(Transient)
	TObjectPtr<UScriptStruct> StructTemplate;

	static UControlRigGraphNode* SpawnNode(UEdGraph* ParentGraph, UBlueprint* Blueprint, UScriptStruct* StructTemplate, FVector2D const Location);

	friend class UEngineTestControlRig;
	friend class FControlRigEditor;
};

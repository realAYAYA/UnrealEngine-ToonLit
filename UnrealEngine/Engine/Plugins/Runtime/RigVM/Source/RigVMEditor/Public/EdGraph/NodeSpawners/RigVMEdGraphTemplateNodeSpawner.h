// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "UObject/UnrealType.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "EdGraph/NodeSpawners/RigVMEdGraphNodeSpawner.h"
#include "EdGraph/RigVMEdGraph.h"
#include "EdGraph/RigVMEdGraphNode.h"
#include "RigVMModel/Nodes/RigVMTemplateNode.h"
#include "RigVMEdGraphTemplateNodeSpawner.generated.h"

class URigVMEdGraphNode;

UCLASS(Transient)
class RIGVMEDITOR_API URigVMEdGraphTemplateNodeSpawner : public URigVMEdGraphNodeSpawner
{
	GENERATED_BODY()

public:
	/**
	 * Creates a new URigVMEdGraphVariableNodeSpawner, charged with spawning 
	 * a new member-variable node
	 * 
	 * @return A newly allocated instance of this class.
	 */
	static URigVMEdGraphTemplateNodeSpawner* CreateFromNotation(const FName& InNotation, const FText& InMenuDesc, const FText& InCategory, const FText& InTooltip);

	// UBlueprintNodeSpawner interface
	virtual void Prime() override;
	virtual FBlueprintNodeSignature GetSpawnerSignature() const override;
	bool IsTemplateNodeFilteredOut(FBlueprintActionFilter const& Filter) const override;
	virtual FBlueprintActionUiSpec GetUiSpec(FBlueprintActionContext const& Context, FBindingSet const& Bindings) const override;
	virtual UEdGraphNode* Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const override;
	// End UBlueprintNodeSpawner interface

private:
	/** The unit type we will spawn */
	UPROPERTY(Transient)
	FName TemplateNotation;

	const FRigVMTemplate* Template = nullptr;

	static URigVMEdGraphNode* SpawnNode(UEdGraph* ParentGraph, UBlueprint* Blueprint, const FRigVMTemplate* Template, FVector2D const Location);

	friend class UEngineTestControlRig;
	friend class FRigVMEditor;
};

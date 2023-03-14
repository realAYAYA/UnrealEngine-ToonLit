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
#include "ControlRigFunctionRefNodeSpawner.generated.h"

class UControlRigGraphNode;

UCLASS(Transient)
class CONTROLRIGEDITOR_API UControlRigFunctionRefNodeSpawner : public UBlueprintNodeSpawner
{
	GENERATED_BODY()

public:

	/**
	 * Creates a new UControlRigVariableNodeSpawner, charged with spawning 
	 * a new member-variable node
	 * 
	 * @return A newly allocated instance of this class.
	 */
	static UControlRigFunctionRefNodeSpawner* CreateFromFunction(URigVMLibraryNode* InFunction);

	/**
	 * Creates a new UControlRigVariableNodeSpawner, charged with spawning 
	 * a new member-variable node
	 * 
	 * @return A newly allocated instance of this class.
	 */
	static UControlRigFunctionRefNodeSpawner* CreateFromAssetData(const FAssetData& InAssetData, const FControlRigPublicFunctionData& InPublicFunction);

	// UBlueprintNodeSpawner interface
	virtual void Prime() override;
	virtual FBlueprintNodeSignature GetSpawnerSignature() const override;
	virtual FBlueprintActionUiSpec GetUiSpec(FBlueprintActionContext const& Context, FBindingSet const& Bindings) const override;
	virtual UEdGraphNode* Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const override;
	virtual bool IsTemplateNodeFilteredOut(FBlueprintActionFilter const& Filter) const override;
	// End UBlueprintNodeSpawner interface

private:

	/** The unit type we will spawn [optional] */
	UPROPERTY(Transient)
	mutable TWeakObjectPtr<URigVMLibraryNode> ReferencedFunctionPtr;

	/** The asset object path we'll spawn from [optional] */
	UPROPERTY(Transient)
	FName ReferencedAssetObjectPath;

	/** The public function definition we will spawn from [optional] */
	UPROPERTY(Transient)
	FControlRigPublicFunctionData ReferencedPublicFunctionData;

	/** Marked as true for local function definitions */
	UPROPERTY(Transient)
	bool bIsLocalFunction;

	static UControlRigGraphNode* SpawnNode(UEdGraph* ParentGraph, UBlueprint* Blueprint, URigVMLibraryNode* InFunction, FVector2D const Location);

	friend class UEngineTestControlRig;
	friend class FControlRigEditor;
};

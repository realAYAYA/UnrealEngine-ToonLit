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
#include "RigVMBlueprint.h"
#include "EdGraph/RigVMEdGraph.h"
#include "EdGraph/RigVMEdGraphNode.h"
#include "RigVMModel/Nodes/RigVMUnitNode.h"
#include "RigVMEdGraphFunctionRefNodeSpawner.generated.h"

class URigVMEdGraphNode;

UCLASS(Transient)
class RIGVMEDITOR_API URigVMEdGraphFunctionRefNodeSpawner : public URigVMEdGraphNodeSpawner
{
	GENERATED_BODY()

public:

	/**
	 * Creates a new URigVMEdGraphFunctionRefNodeSpawner, charged with spawning a function reference
	 * 
	 * @return A newly allocated instance of this class.
	 */
	static URigVMEdGraphFunctionRefNodeSpawner* CreateFromFunction(URigVMLibraryNode* InFunction);

	/**
	 * Creates a new URigVMEdGraphFunctionRefNodeSpawner, charged with spawning a function reference
	 * 
	 * @return A newly allocated instance of this class.
	 */
	static URigVMEdGraphFunctionRefNodeSpawner* CreateFromAssetData(const FAssetData& InAssetData, const FRigVMGraphFunctionHeader& InPublicFunction);
	static URigVMEdGraphFunctionRefNodeSpawner* CreateFromAssetData(const FAssetData& InAssetData, const FRigVMOldPublicFunctionData& InPublicFunction);

	// UBlueprintNodeSpawner interface
	virtual void Prime() override;
	virtual FBlueprintNodeSignature GetSpawnerSignature() const override;
	virtual FBlueprintActionUiSpec GetUiSpec(FBlueprintActionContext const& Context, FBindingSet const& Bindings) const override;
	virtual UEdGraphNode* Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const override;
	virtual bool IsTemplateNodeFilteredOut(FBlueprintActionFilter const& Filter) const override;
	// End UBlueprintNodeSpawner interface

private:

	/** The public function definition we will spawn from [optional] */
	UPROPERTY(Transient)
	mutable FRigVMGraphFunctionHeader ReferencedPublicFunctionHeader;

	/** Marked as true for local function definitions */
	UPROPERTY(Transient)
	bool bIsLocalFunction;

	UPROPERTY()
	FSoftObjectPath AssetPath;

	static URigVMEdGraphNode* SpawnNode(UEdGraph* ParentGraph, UBlueprint* Blueprint, FRigVMGraphFunctionHeader& InFunction, FVector2D const Location);

	friend class UEngineTestControlRig;
	friend class FRigVMEditor;
};

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
#include "RigVMEdGraphEnumNodeSpawner.generated.h"

class URigVMEdGraphNode;

UCLASS(Transient)
class RIGVMEDITOR_API URigVMEdGraphEnumNodeSpawner : public URigVMEdGraphNodeSpawner
{
	GENERATED_BODY()

public:

	/**
	 * Creates a new URigVMEdGraphEnumNodeSpawner
	 * 
	 * @return A newly allocated instance of this class.
	 */
	static URigVMEdGraphEnumNodeSpawner* CreateForEnum(UEnum* InEnum, const FText& InMenuDesc, const FText& InCategory, const FText& InTooltip);

	// UBlueprintNodeSpawner interface
	virtual FBlueprintNodeSignature GetSpawnerSignature() const override;
	virtual FBlueprintActionUiSpec GetUiSpec(FBlueprintActionContext const& Context, FBindingSet const& Bindings) const override;
	virtual UEdGraphNode* Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const override;
	// End UBlueprintNodeSpawner interface

private:

	UEnum* Enum;
	friend class UEngineTestControlRig;
};

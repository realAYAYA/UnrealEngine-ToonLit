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
#include "RigVMCore/RigVMByteCode.h"
#include "ControlRigArrayNodeSpawner.generated.h"

class UControlRigGraphNode;

UCLASS(Transient)
class CONTROLRIGEDITOR_API UControlRigArrayNodeSpawner : public UBlueprintNodeSpawner
{
	GENERATED_BODY()

public:

	/**
	 * Creates a new UControlRigArrayNodeSpawner, charged with spawning 
	 * a new member-variable node
	 * 
	 * @return A newly allocated instance of this class.
	 */
	static UControlRigArrayNodeSpawner* CreateGeneric(ERigVMOpCode InOpCode, const FText& InMenuDesc, const FText& InCategory, const FText& InTooltip);

	// UBlueprintNodeSpawner interface
	virtual FBlueprintNodeSignature GetSpawnerSignature() const override;
	virtual FBlueprintActionUiSpec GetUiSpec(FBlueprintActionContext const& Context, FBindingSet const& Bindings) const override;
	virtual UEdGraphNode* Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const override;
	virtual bool IsTemplateNodeFilteredOut(FBlueprintActionFilter const& Filter) const override;
	// End UBlueprintNodeSpawner interface

private:

	ERigVMOpCode OpCode;
	friend class UEngineTestControlRig;
};

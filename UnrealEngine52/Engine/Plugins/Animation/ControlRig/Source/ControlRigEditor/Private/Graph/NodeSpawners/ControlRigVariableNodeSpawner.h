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
#include "RigVMCore/RigVMExternalVariable.h"
#include "RigVMModel/RigVMVariableDescription.h"
#include "ControlRigVariableNodeSpawner.generated.h"

class UControlRigGraphNode;
class UControlRigBlueprint;

UCLASS(Transient)
class CONTROLRIGEDITOR_API UControlRigVariableNodeSpawner : public UBlueprintNodeSpawner
{
	GENERATED_BODY()

public:

	/**
	 * Creates a new UControlRigVariableNodeSpawner, charged with spawning 
	 * a new member-variable node
	 * 
	 * @return A newly allocated instance of this class.
	 */
	static UControlRigVariableNodeSpawner* CreateFromExternalVariable(UControlRigBlueprint* InBlueprint, const FRigVMExternalVariable& InExternalVariable, bool bInIsGetter, const FText& InMenuDesc, const FText& InCategory, const FText& InTooltip);
	static UControlRigVariableNodeSpawner* CreateFromLocalVariable(UControlRigBlueprint* InBlueprint, URigVMGraph* InGraphOwner, const FRigVMGraphVariableDescription& InLocalVariable, bool bInIsGetter, const FText& InMenuDesc, const FText& InCategory, const FText& InTooltip);

	// UBlueprintNodeSpawner interface
	virtual void Prime() override;
	virtual FBlueprintNodeSignature GetSpawnerSignature() const override;
	virtual FBlueprintActionUiSpec GetUiSpec(FBlueprintActionContext const& Context, FBindingSet const& Bindings) const override;
	virtual UEdGraphNode* Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const override;
	virtual bool IsTemplateNodeFilteredOut(FBlueprintActionFilter const& Filter) const override;
	// End UBlueprintNodeSpawner interface

private:

	/** The pin type we will spawn */
	TWeakObjectPtr<UControlRigBlueprint> Blueprint;
	TWeakObjectPtr<URigVMGraph> GraphOwner;
	FRigVMExternalVariable ExternalVariable;
	bool bIsGetter;
	bool bIsLocalVariable;	

	friend class UEngineTestControlRig;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/BlueprintExtension.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Blueprint.h"
#include "Containers/Array.h"
#include "Containers/Set.h"
#include "ModifierCompilationBlueprintExtension.generated.h"

class UK2Node_EnhancedInputAction;
class UVCamModifier;

/**
 * Validates modifier Blueprint when compiled.
 * 
 * Actions:
 * - Add a warning to every input action node that is not found in the MappingContext in the class defaults.
 */
UCLASS()
class VCAMCOREEDITOR_API UModifierCompilationBlueprintExtension : public UBlueprintExtension
{
	GENERATED_BODY()
public:

	//~ Begin UBlueprintExtension Interface
	virtual void HandleGenerateFunctionGraphs(FKismetCompilerContext* CompilerContext) override;
	//~ End UBlueprintExtension Interface

	static bool RequiresRecompileToDetectIssues(UBlueprint& Blueprint);

private:
	
	static void ValidateEnhancedInputAction(FKismetCompilerContext& CompilerContext, UVCamModifier& OldModifierCDO, UK2Node_EnhancedInputAction& EnhancedInputActionNode);
	static bool DoesInputMappingContainReferencedAction(UVCamModifier& OldModifierCDO, UK2Node_EnhancedInputAction& EnhancedInputActionNode);
};


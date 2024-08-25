// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModifierCompilationBlueprintExtension.h"

#include "Modifier/VCamModifier.h"

#include "EdGraph/EdGraphNode.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "K2Node_EnhancedInputAction.h"
#include "Kismet2/CompilerResultsLog.h"
#include "KismetCompiler.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/UObjectToken.h"

#define LOCTEXT_NAMESPACE "UModifierCompilationBlueprintExtension"

namespace UE::VCamCoreEditor::Private
{
	enum class EBreakBehaviour
	{
		Break,
		Continue
	};
	
	/**
	 * @param Blueprint Blueprint of which to iterate all nodes
	 * @param Callback has this signature EBreakBehaviour(UEdGraphNode& Node)
	 */
	template<typename TCallback>
	static void ForEachNode(UBlueprint& Blueprint, TCallback Callback)
	{
		TArray<UEdGraph*> Graphs;
		Blueprint.GetAllGraphs(Graphs);
		TSet<UEdGraphNode*> AlreadyVisited;
		for (UEdGraph* Graph : Graphs)
		{
			for (UEdGraphNode* GraphNode : Graph->Nodes)
			{
				if (!GraphNode || AlreadyVisited.Contains(GraphNode))
				{
					continue;
				}

				AlreadyVisited.Add(GraphNode);
				if (Callback(*GraphNode) == EBreakBehaviour::Break)
				{
					return;
				}
			}
		}
	}
}

void UModifierCompilationBlueprintExtension::HandleGenerateFunctionGraphs(FKismetCompilerContext* CompilerContext)
{
	UVCamModifier* OldModifierCDO = CompilerContext && CompilerContext->OldClass
		? Cast<UVCamModifier>(CompilerContext->OldClass->GetDefaultObject())
		: nullptr;
	if (!OldModifierCDO)
	{
		return;
	}

	// Blueprints are compiled during the loading (editor) screen. The FLinker loads the InputMappingContext reference but will only serialize it AFTER the initial compilation making our checks obsolete.
	// However, FCompilationExtensionManager starts a new compilation once the InputMappingContext's been loaded.
	const bool bShouldEarlyOut = OldModifierCDO->InputMappingContext && OldModifierCDO->InputMappingContext->HasAnyFlags(RF_NeedLoad);
	if (bShouldEarlyOut)
	{
		return;
	}
	
	using namespace UE::VCamCoreEditor::Private;
	ForEachNode(*CompilerContext->Blueprint, [this, CompilerContext, OldModifierCDO](UEdGraphNode& GraphNode)
	{
		if (UK2Node_EnhancedInputAction* EnhancedInputActionNode = Cast<UK2Node_EnhancedInputAction>(&GraphNode))
		{
			ValidateEnhancedInputAction(*CompilerContext, *OldModifierCDO, *EnhancedInputActionNode);
		}

		return EBreakBehaviour::Continue;
	});
}

bool UModifierCompilationBlueprintExtension::RequiresRecompileToDetectIssues(UBlueprint& Blueprint)
{
	UVCamModifier* CDO = Blueprint.GeneratedClass ? Cast<UVCamModifier>(Blueprint.GeneratedClass->GetDefaultObject()) : nullptr;
	if (!CDO)
	{
		return true;
	}

	bool bFoundAnyIssue = false;
	using namespace UE::VCamCoreEditor::Private;
	ForEachNode(Blueprint, [CDO, &bFoundAnyIssue](UEdGraphNode& GraphNode)
	{
		UK2Node_EnhancedInputAction* EnhancedInputActionNode = Cast<UK2Node_EnhancedInputAction>(&GraphNode);
		bFoundAnyIssue = EnhancedInputActionNode && !DoesInputMappingContainReferencedAction(*CDO, *EnhancedInputActionNode);
		return bFoundAnyIssue
			? EBreakBehaviour::Break
			: EBreakBehaviour::Continue;
	});
	return bFoundAnyIssue;
}

void UModifierCompilationBlueprintExtension::ValidateEnhancedInputAction(FKismetCompilerContext& CompilerContext, UVCamModifier& OldModifierCDO, UK2Node_EnhancedInputAction& EnhancedInputActionNode)
{
	if (!DoesInputMappingContainReferencedAction(OldModifierCDO, EnhancedInputActionNode))
	{
		const FName PropertyName = GET_MEMBER_NAME_CHECKED(UVCamModifier, InputMappingContext);
		CompilerContext.MessageLog.Warning(TEXT("@@: InputAction "), &EnhancedInputActionNode)
			->AddToken(FUObjectToken::Create(EnhancedInputActionNode.InputAction))
			->AddToken(FTextToken::Create(LOCTEXT("EnhancedInput.MiddleOfMessage", " is not part of ")))
			->AddToken(FUObjectToken::Create(OldModifierCDO.InputMappingContext))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT("EnhancedInput.EndOfMessageFmt", " referenced by this modifier's {0} property"), FText::FromName(PropertyName))));
		
		EnhancedInputActionNode.bHasCompilerMessage = true;
		EnhancedInputActionNode.ErrorMsg = FString::Printf(TEXT("InputAction %s is not part of %s referenced this modifier's %s property."),
			EnhancedInputActionNode.InputAction ? *EnhancedInputActionNode.InputAction->GetName() : TEXT("None"),
			OldModifierCDO.InputMappingContext ? *OldModifierCDO.InputMappingContext->GetName() : TEXT("None"),
			*PropertyName.ToString()
			);
		EnhancedInputActionNode.ErrorType = EMessageSeverity::Warning;
	}
	else if (const bool bWasWarningGeneratedByUs = EnhancedInputActionNode.bHasCompilerMessage && EnhancedInputActionNode.ErrorMsg.Contains(TEXT("InputMappingContext")))
	{
		EnhancedInputActionNode.bHasCompilerMessage = false;
		EnhancedInputActionNode.ErrorMsg.Reset();
	}
}

bool UModifierCompilationBlueprintExtension::DoesInputMappingContainReferencedAction(UVCamModifier& OldModifierCDO,UK2Node_EnhancedInputAction& EnhancedInputActionNode)
{
	return OldModifierCDO.InputMappingContext && OldModifierCDO.InputMappingContext->GetMappings().ContainsByPredicate([SearchedAction = EnhancedInputActionNode.InputAction.Get()](const FEnhancedActionKeyMapping& Item)
		{
			return Item.Action == SearchedAction;
		});
}

#undef LOCTEXT_NAMESPACE

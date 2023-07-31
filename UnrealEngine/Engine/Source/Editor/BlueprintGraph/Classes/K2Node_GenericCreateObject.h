// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "K2Node_ConstructObjectFromClass.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "K2Node_GenericCreateObject.generated.h"

class FKismetCompilerContext;
class UEdGraph;
class UK2Node_CallFunction;
class UObject;

UCLASS()
class BLUEPRINTGRAPH_API UK2Node_GenericCreateObject : public UK2Node_ConstructObjectFromClass
{
	GENERATED_BODY()

	//~ Begin UEdGraphNode Interface.
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual void EarlyValidation(class FCompilerResultsLog& MessageLog) const override;
	virtual bool IsCompatibleWithGraph(const UEdGraph* TargetGraph) const override;
	//~ End UEdGraphNode Interface.

	virtual bool UseWorldContext() const override { return false; }
	virtual bool UseOuter() const override { return true; }

	/**
	 * attaches a self node to the self pin of 'this' if the CallCreateNode function has DefaultToSelf in it's metadata
	 *
	 * @param	CompilerContext		the context to expand in - likely passed from ExpandNode
	 * @param	SourceGraph			the graph to expand in - likely passed from ExpandNode
	 * @param	CallCreateNode		the CallFunction node that 'this' is imitating
	 *
	 * @return	true on success.
	 */
	bool ExpandDefaultToSelfPin(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UK2Node_CallFunction* CallCreateNode);
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "K2Node_DataChannelBase.h"
#include "K2Node_WriteDataChannel.generated.h"


UCLASS(MinimalAPI)
class UK2Node_WriteDataChannel : public UK2Node_DataChannelBase
{
	GENERATED_BODY()

public:
	UK2Node_WriteDataChannel();

	// //~ Begin UEdGraphNode Interface.
	virtual void AllocateDefaultPins() override;
	// //~ End UEdGraphNode Interface.

	//~ Begin K2Node Interface
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& InActionRegistrar) const override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	//~ End K2Node Interface

private:
	UFunction* GetWriteFunctionForType(const FNiagaraTypeDefinition& TypeDef);
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagsK2Node_MultiCompareBase.h"
#include "GameplayTagsK2Node_MultiCompareGameplayTagContainer.generated.h"

namespace ENodeTitleType { enum Type : int; }

class FBlueprintActionDatabaseRegistrar;
class UEdGraph;

UCLASS()
class UGameplayTagsK2Node_MultiCompareGameplayTagContainer : public UGameplayTagsK2Node_MultiCompareBase
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	// End of UEdGraphNode interface

	// UK2Node interface
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;		
	// End of UK2Node interface

private:

	virtual void AddPinToSwitchNode() override;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif

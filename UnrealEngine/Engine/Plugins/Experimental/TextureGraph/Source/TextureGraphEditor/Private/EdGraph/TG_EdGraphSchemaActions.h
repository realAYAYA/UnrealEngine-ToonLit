// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraphSchema.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPath.h"

#include "TG_EdGraphSchemaActions.generated.h"

class UEdGraph;
class UEdGraphPin;
class UTG_Expression;

USTRUCT()
struct FTG_EdGraphSchemaAction_NewNode : public FEdGraphSchemaAction
{
	GENERATED_BODY()

	// Inherit the base class's constructors
	using FEdGraphSchemaAction::FEdGraphSchemaAction;

public:
	UPROPERTY()
	TSubclassOf<UTG_Expression> TG_ExpressionClass;

	//~ Begin FEdGraphSchemaAction Interface
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	//~ End FEdGraphSchemaAction Interface

	static UEdGraphNode* CreateExpressionNode(UEdGraph* ParentGraph, const UClass* ExpressionClass, UEdGraphPin* FromPin,  const FVector2D Location, bool bSelectNewNode = true);
	static UEdGraphNode* CreateExpressionNode(UEdGraph* ParentGraph, UTG_Expression* Expression, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true);
};

/** Action to add a 'comment' node to the graph */
USTRUCT()
struct FTG_EdGraphSchemaAction_NewComment : public FEdGraphSchemaAction
{
	GENERATED_BODY()

	// Inherit the base class's constructors
	using FEdGraphSchemaAction::FEdGraphSchemaAction;

	// Simple type info
	static FName StaticGetTypeId()
	{
		static FName Type("FTG_EdGraphSchemaAction_NewComment");
		return Type;
	}

	// FEdGraphSchemaAction interface
	virtual FName GetTypeId() const override { return StaticGetTypeId(); }
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	// End of FEdGraphSchemaAction interface
};


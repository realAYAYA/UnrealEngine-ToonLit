// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphSchema.h"
#include "Styling/AppStyle.h"
#include "GraphEditorSchemaActions.generated.h"

struct FSlateBrush;

USTRUCT()
struct FAnimNextGraphSchemaAction : public FEdGraphSchemaAction
{
	GENERATED_BODY()

	FAnimNextGraphSchemaAction() = default;
	
	FAnimNextGraphSchemaAction(FText InNodeCategory, FText InMenuDesc, FText InToolTip, FText InKeywords = FText::GetEmpty())
		: FEdGraphSchemaAction(MoveTemp(InNodeCategory), MoveTemp(InMenuDesc), MoveTemp(InToolTip), 0, MoveTemp(InKeywords))
	{
	}

	virtual const FSlateBrush* GetIconBrush() const
	{
		return FAppStyle::Get().GetBrush("NoBrush");
	}

	virtual const FLinearColor& GetIconColor() const
	{
		static const FLinearColor DefaultColor;
		return DefaultColor;
	}
};

USTRUCT()
struct FAnimNextGraphSchemaAction_RigUnit : public FAnimNextGraphSchemaAction
{
	GENERATED_BODY()

	FAnimNextGraphSchemaAction_RigUnit() = default;
	
	FAnimNextGraphSchemaAction_RigUnit(UScriptStruct* InStructTemplate, FText InNodeCategory, FText InMenuDesc, FText InToolTip, FText InKeywords = FText::GetEmpty())
		: FAnimNextGraphSchemaAction(InNodeCategory, InMenuDesc, InToolTip, InKeywords)
		, StructTemplate(InStructTemplate)
	{}

	// FEdGraphSchemaAction Interface
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2D Location, bool bSelectNewNode = true) override;
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) { return nullptr; }

private:
	// The script struct for our rig unit
	UScriptStruct* StructTemplate = nullptr;
};


USTRUCT()
struct FAnimNextGraphSchemaAction_DispatchFactory : public FAnimNextGraphSchemaAction
{
	GENERATED_BODY()

	FAnimNextGraphSchemaAction_DispatchFactory() = default;

	FAnimNextGraphSchemaAction_DispatchFactory(FName InNotation, FText InNodeCategory, FText InMenuDesc, FText InToolTip, FText InKeywords = FText::GetEmpty())
		: FAnimNextGraphSchemaAction(InNodeCategory, InMenuDesc, InToolTip, InKeywords)
		, Notation(InNotation)
	{}

	// FEdGraphSchemaAction Interface
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2D Location, bool bSelectNewNode = true) override;
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) { return nullptr; }

private:
	// Notation for dispatch factory
	FName Notation;
};

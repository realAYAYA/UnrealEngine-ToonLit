// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphSchema.h"
#include "Styling/AppStyle.h"
#include "AnimNextInterfaceGraphEditorSchemaActions.generated.h"

struct FSlateBrush;

USTRUCT()
struct FAnimNextInterfaceGraphSchemaAction : public FEdGraphSchemaAction
{
	GENERATED_BODY()

	FAnimNextInterfaceGraphSchemaAction() = default;
	
	FAnimNextInterfaceGraphSchemaAction(FText InNodeCategory, FText InMenuDesc, FText InToolTip, FText InKeywords = FText::GetEmpty())
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
struct FAnimNextInterfaceGraphSchemaAction_RigUnit : public FAnimNextInterfaceGraphSchemaAction
{
	GENERATED_BODY()

	FAnimNextInterfaceGraphSchemaAction_RigUnit() = default;
	
	FAnimNextInterfaceGraphSchemaAction_RigUnit(UScriptStruct* InStructTemplate, FText InNodeCategory, FText InMenuDesc, FText InToolTip, FText InKeywords = FText::GetEmpty())
		: FAnimNextInterfaceGraphSchemaAction(InNodeCategory, InMenuDesc, InToolTip, InKeywords)
		, StructTemplate(InStructTemplate)
	{}

	// FEdGraphSchemaAction Interface
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2D Location, bool bSelectNewNode = true) override;
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) { return nullptr; }

private:
	// The script struct for our rig unit
	UScriptStruct* StructTemplate = nullptr;
};

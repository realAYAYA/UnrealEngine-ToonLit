// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphSchema.h"
#include "Styling/AppStyle.h"
#include "DataInterfaceGraphEditorSchemaActions.generated.h"

struct FSlateBrush;

USTRUCT()
struct FDataInterfaceGraphSchemaAction : public FEdGraphSchemaAction
{
	GENERATED_BODY()

	FDataInterfaceGraphSchemaAction() = default;
	
	FDataInterfaceGraphSchemaAction(FText InNodeCategory, FText InMenuDesc, FText InToolTip, FText InKeywords = FText::GetEmpty())
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
struct FDataInterfaceGraphSchemaAction_RigUnit : public FDataInterfaceGraphSchemaAction
{
	GENERATED_BODY()

	FDataInterfaceGraphSchemaAction_RigUnit() = default;
	
	FDataInterfaceGraphSchemaAction_RigUnit(UScriptStruct* InStructTemplate, FText InNodeCategory, FText InMenuDesc, FText InToolTip, FText InKeywords = FText::GetEmpty())
		: FDataInterfaceGraphSchemaAction(InNodeCategory, InMenuDesc, InToolTip, InKeywords)
		, StructTemplate(InStructTemplate)
	{}

	// FEdGraphSchemaAction Interface
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2D Location, bool bSelectNewNode = true) override;
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) { return nullptr; }

private:
	// The script struct for our rig unit
	UScriptStruct* StructTemplate = nullptr;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphSchema.h"
#include "Editor/RigVMEditorStyle.h"
#include "Styling/AppStyle.h"
#include "GraphEditorSchemaActions.generated.h"

struct FSlateBrush;

USTRUCT()
struct FAnimNextSchemaAction : public FEdGraphSchemaAction
{
	GENERATED_BODY()

	FAnimNextSchemaAction() = default;
	
	FAnimNextSchemaAction(FText InNodeCategory, FText InMenuDesc, FText InToolTip, FText InKeywords = FText::GetEmpty())
		: FEdGraphSchemaAction(MoveTemp(InNodeCategory), MoveTemp(InMenuDesc), MoveTemp(InToolTip), 0, MoveTemp(InKeywords))
	{
	}

	virtual const FSlateBrush* GetIconBrush() const
	{
		return FRigVMEditorStyle::Get().GetBrush("RigVM.Unit");
	}

	virtual const FLinearColor& GetIconColor() const
	{
		return FLinearColor::White;
	}
};

USTRUCT()
struct FAnimNextSchemaAction_RigUnit : public FAnimNextSchemaAction
{
	GENERATED_BODY()

	FAnimNextSchemaAction_RigUnit() = default;
	
	FAnimNextSchemaAction_RigUnit(UScriptStruct* InStructTemplate, FText InNodeCategory, FText InMenuDesc, FText InToolTip, FText InKeywords = FText::GetEmpty())
		: FAnimNextSchemaAction(InNodeCategory, InMenuDesc, InToolTip, InKeywords)
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
struct FAnimNextSchemaAction_DispatchFactory : public FAnimNextSchemaAction
{
	GENERATED_BODY()

	FAnimNextSchemaAction_DispatchFactory() = default;

	FAnimNextSchemaAction_DispatchFactory(FName InNotation, FText InNodeCategory, FText InMenuDesc, FText InToolTip, FText InKeywords = FText::GetEmpty())
		: FAnimNextSchemaAction(InNodeCategory, InMenuDesc, InToolTip, InKeywords)
		, Notation(InNotation)
	{}

	virtual const FSlateBrush* GetIconBrush() const
	{
		return FRigVMEditorStyle::Get().GetBrush("RigVM.Template");
	}
	
	// FEdGraphSchemaAction Interface
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2D Location, bool bSelectNewNode = true) override;
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) { return nullptr; }

private:
	// Notation for dispatch factory
	FName Notation;
};

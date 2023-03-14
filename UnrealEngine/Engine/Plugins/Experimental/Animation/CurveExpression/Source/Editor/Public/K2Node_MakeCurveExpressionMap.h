// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "K2Node.h"

#include "K2Node_MakeCurveExpressionMap.generated.h"

USTRUCT()
struct FCurveExpressionList
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Expressions")
	FString AssignmentExpressions;
};

UCLASS()
class CURVEEXPRESSIONEDITOR_API UK2Node_MakeCurveExpressionMap :
	public UK2Node
{
	GENERATED_BODY()

public:
	UK2Node_MakeCurveExpressionMap();
	
	UEdGraphPin* GetOutputPin() const;
	TMap<FName, FString> GetExpressionMap() const;
	
	// UEdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual void ValidateNodeDuringCompilation(FCompilerResultsLog& MessageLog) const override;
	// End of UEdGraphNode interface

	// UK2Node interface
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& InActionRegistrar) const override;
	virtual FNodeHandlingFunctor* CreateNodeHandler(FKismetCompilerContext& CompilerContext) const override;
	virtual FText GetMenuCategory() const override;
	virtual bool ShouldShowNodeProperties() const override { return true; }
	virtual bool IsNodePure() const override { return true; }
	// End of UK2Node interface

	UPROPERTY(EditAnywhere, Category="Expressions")
	FCurveExpressionList Expressions;

private:
	static const FName OutputPinName;
};

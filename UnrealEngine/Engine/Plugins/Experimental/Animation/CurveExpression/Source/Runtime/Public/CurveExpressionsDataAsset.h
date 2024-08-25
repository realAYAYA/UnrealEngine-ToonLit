// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ExpressionEvaluator.h"

#include "Engine/DataAsset.h"

#include "CurveExpressionsDataAsset.generated.h"


struct FCurveExpressionAssignment
{
	int32 LineIndex = INDEX_NONE;
	FName TargetName;
	
	FStringView Expression;
};


struct FCurveExpressionParsedAssignment
{
	int32 LineIndex = INDEX_NONE;
	FName TargetName;
		
	TVariant<CurveExpression::Evaluator::FExpressionObject, CurveExpression::Evaluator::FParseError> Result; 
};


USTRUCT(BlueprintType)
struct FCurveExpressionList
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Curve Expressions")
	FString AssignmentExpressions;

	TArray<FCurveExpressionAssignment> GetAssignments() const;
	
	/** Return compiled expression results */
	TArray<FCurveExpressionParsedAssignment> GetParsedAssignments() const;
};

struct FExpressionData
{
	TArray<FName> NamedConstants;
	TMap<FName, CurveExpression::Evaluator::FExpressionObject> ExpressionMap;
};


UCLASS(BlueprintType)
class CURVEEXPRESSION_API UCurveExpressionsDataAsset :
	public UDataAsset
{
	GENERATED_BODY()
public:

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category="Expressions")
	FCurveExpressionList Expressions;
#endif

	TSharedPtr<const FExpressionData> GetCompiledExpressionData() const { return ExpressionData; }

	// UObject overrides
	void Serialize(FArchive& Ar) override;
#if WITH_EDITOR
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	
private:
#if WITH_EDITOR
	void CompileExpressions();
#endif
	
	UPROPERTY()
	TArray<FName> NamedConstants_DEPRECATED;	
	
	TSharedPtr<FExpressionData> ExpressionData;
};

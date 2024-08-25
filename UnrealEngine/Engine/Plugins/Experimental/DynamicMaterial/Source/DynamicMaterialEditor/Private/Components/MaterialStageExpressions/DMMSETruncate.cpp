// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSETruncate.h"
#include "Materials/MaterialExpressionTruncate.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionTruncate"

UDMMaterialStageExpressionTruncate::UDMMaterialStageExpressionTruncate()
	: UDMMaterialStageExpressionMathBase(
		LOCTEXT("Truncate", "Truncate"),
		UDMMaterialStageExpression::FindClass(TEXT("MaterialExpressionTruncate"))
	)
{
	SetupInputs(1);
}

#undef LOCTEXT_NAMESPACE

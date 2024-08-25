// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSERound.h"
#include "Materials/MaterialExpressionRound.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionRound"

UDMMaterialStageExpressionRound::UDMMaterialStageExpressionRound()
	: UDMMaterialStageExpressionMathBase(
		LOCTEXT("Round", "Round"),
		UDMMaterialStageExpression::FindClass(TEXT("MaterialExpressionRound"))
	)
{
	SetupInputs(1);

	OutputConnectors[0].Name = LOCTEXT("Whole Number", "Whole Number");
}

#undef LOCTEXT_NAMESPACE

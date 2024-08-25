// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSEArctangentFast.h"
#include "Materials/MaterialExpressionArctangentFast.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionArctangentFast"

UDMMaterialStageExpressionArctangentFast::UDMMaterialStageExpressionArctangentFast()
	: UDMMaterialStageExpressionMathBase(
		LOCTEXT("ArctangentFast", "Arctangent (Fast)"),
		UDMMaterialStageExpression::FindClass(TEXT("MaterialExpressionArctangentFast"))
	)
{
	SetupInputs(1);

	bSingleChannelOnly = true;

	InputConnectors[0].Name = LOCTEXT("O/A", "O/A");
	OutputConnectors[0].Name = LOCTEXT("Angle", "Angle");
}

#undef LOCTEXT_NAMESPACE

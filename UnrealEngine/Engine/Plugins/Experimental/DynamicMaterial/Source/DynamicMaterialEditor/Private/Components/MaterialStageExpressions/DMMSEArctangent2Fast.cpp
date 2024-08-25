// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSEArctangent2Fast.h"
#include "Materials/MaterialExpressionArctangent2Fast.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionArctangent2Fast"

UDMMaterialStageExpressionArctangent2Fast::UDMMaterialStageExpressionArctangent2Fast()
	: UDMMaterialStageExpressionMathBase(
		LOCTEXT("Arctangent2Fast", "Arctangent2 (Fast)"),
		UDMMaterialStageExpression::FindClass(TEXT("MaterialExpressionArctangent2Fast"))
	)
{
	SetupInputs(2);

	InputConnectors[0].Name = LOCTEXT("O", "O");
	InputConnectors[1].Name = LOCTEXT("H", "H");
	OutputConnectors[0].Name = LOCTEXT("Angle", "Angle");
}

#undef LOCTEXT_NAMESPACE

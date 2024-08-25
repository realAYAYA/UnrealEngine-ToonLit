// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSEArcCosineFast.h"

#include "Materials/MaterialExpressionArccosineFast.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionArccosineFast"

UDMMaterialStageExpressionArccosineFast::UDMMaterialStageExpressionArccosineFast()
	: UDMMaterialStageExpressionMathBase(
		LOCTEXT("ArccosineFast", "Arccosine (Fast)"),
		UDMMaterialStageExpression::FindClass(TEXT("MaterialExpressionArccosineFast"))
	)
{
	SetupInputs(1);

	bSingleChannelOnly = true;

	InputConnectors[0].Name = LOCTEXT("A/H", "A/H");
	OutputConnectors[0].Name = LOCTEXT("Angle", "Angle");
}

#undef LOCTEXT_NAMESPACE

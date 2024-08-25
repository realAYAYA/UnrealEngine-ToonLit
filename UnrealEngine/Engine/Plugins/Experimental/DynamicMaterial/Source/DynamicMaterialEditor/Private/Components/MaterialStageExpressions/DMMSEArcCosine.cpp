// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSEArcCosine.h"

#include "Materials/MaterialExpressionArccosine.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionArccosine"

UDMMaterialStageExpressionArccosine::UDMMaterialStageExpressionArccosine()
	: UDMMaterialStageExpressionMathBase(
		LOCTEXT("Arccosine", "Arccosine"),
		UMaterialExpressionArccosine::StaticClass()
	)
{
	SetupInputs(1);

	bSingleChannelOnly = true;

	InputConnectors[0].Name = LOCTEXT("A/H", "A/H");
	OutputConnectors[0].Name = LOCTEXT("Angle", "Angle");
}

#undef LOCTEXT_NAMESPACE

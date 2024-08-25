// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSEArcSine.h"

#include "Materials/MaterialExpressionArcsine.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionArcsine"

UDMMaterialStageExpressionArcsine::UDMMaterialStageExpressionArcsine()
	: UDMMaterialStageExpressionMathBase(
		LOCTEXT("Arcsine", "Arcsine"),
		UMaterialExpressionArcsine::StaticClass()
	)
{
	SetupInputs(1);

	bSingleChannelOnly = true;

	InputConnectors[0].Name = LOCTEXT("O/H", "O/H");
	OutputConnectors[0].Name = LOCTEXT("Angle", "Angle");
}

#undef LOCTEXT_NAMESPACE

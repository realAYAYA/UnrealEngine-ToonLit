// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSEArctangent2.h"
#include "Materials/MaterialExpressionArctangent2.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionArctangent2"

UDMMaterialStageExpressionArctangent2::UDMMaterialStageExpressionArctangent2()
	: UDMMaterialStageExpressionMathBase(
		LOCTEXT("Arctangent2", "Arctangent2"),
		UMaterialExpressionArctangent2::StaticClass()
	)
{
	SetupInputs(2);

	bSingleChannelOnly = true;

	InputConnectors[0].Name = LOCTEXT("O", "O");
	InputConnectors[1].Name = LOCTEXT("H", "H");
	OutputConnectors[0].Name = LOCTEXT("Angle", "Angle");
}

#undef LOCTEXT_NAMESPACE

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSECosine.h"
#include "Materials/MaterialExpressionCosine.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionCosine"

UDMMaterialStageExpressionCosine::UDMMaterialStageExpressionCosine()
	: UDMMaterialStageExpressionMathBase(
		LOCTEXT("Cosine", "Cosine"),
		UMaterialExpressionCosine::StaticClass()
	)
{
	SetupInputs(1);

	InputConnectors[0].Name = LOCTEXT("Angle", "Angle");
	OutputConnectors[0].Name = LOCTEXT("A/H", "A/H");
}

#undef LOCTEXT_NAMESPACE

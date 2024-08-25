// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSETangent.h"
#include "Materials/MaterialExpressionTangent.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionTangent"

UDMMaterialStageExpressionTangent::UDMMaterialStageExpressionTangent()
	: UDMMaterialStageExpressionMathBase(
		LOCTEXT("Tangent", "Tangent"),
		UMaterialExpressionTangent::StaticClass()
	)
{
	SetupInputs(1);

	bSingleChannelOnly = true;

	InputConnectors[0].Name = LOCTEXT("Angle", "Angle");
	OutputConnectors[0].Name = LOCTEXT("A/O", "A/O");
}

#undef LOCTEXT_NAMESPACE

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSEStep.h"
#include "Materials/MaterialExpressionStep.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionStep"

UDMMaterialStageExpressionStep::UDMMaterialStageExpressionStep()
	: UDMMaterialStageExpressionMathBase(
		LOCTEXT("Step", "Step"),
		UMaterialExpressionStep::StaticClass()
	)
{
	SetupInputs(2);

	bAllowSingleFloatMatch = false;

	InputConnectors[0].Name = LOCTEXT("Y", "Y");
	InputConnectors[1].Name = LOCTEXT("X", "X");
}

#undef LOCTEXT_NAMESPACE

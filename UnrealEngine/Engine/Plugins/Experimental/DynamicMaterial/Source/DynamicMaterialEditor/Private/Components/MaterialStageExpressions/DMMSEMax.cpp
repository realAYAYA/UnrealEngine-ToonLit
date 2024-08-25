// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSEMax.h"
#include "Materials/MaterialExpressionMax.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionMax"

UDMMaterialStageExpressionMax::UDMMaterialStageExpressionMax()
	: UDMMaterialStageExpressionMathBase(
		LOCTEXT("Max", "Max"),
		UMaterialExpressionMax::StaticClass()
	)
{
	SetupInputs(2);

	bAllowSingleFloatMatch = false;
}

#undef LOCTEXT_NAMESPACE

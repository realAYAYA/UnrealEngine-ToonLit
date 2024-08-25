// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSELogarithm10.h"
#include "Materials/MaterialExpressionLogarithm10.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionLogarithm10"

UDMMaterialStageExpressionLogarithm10::UDMMaterialStageExpressionLogarithm10()
	: UDMMaterialStageExpressionMathBase(
		LOCTEXT("Logarithm10", "Log (Base 10)"),
		UMaterialExpressionLogarithm10::StaticClass()
	)
{
	SetupInputs(1);
}

#undef LOCTEXT_NAMESPACE

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSESquareRoot.h"
#include "Materials/MaterialExpressionSquareRoot.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionSquareRoot"

UDMMaterialStageExpressionSquareRoot::UDMMaterialStageExpressionSquareRoot()
	: UDMMaterialStageExpressionMathBase(
		LOCTEXT("SquareRoot", "Square Root"),
		UMaterialExpressionSquareRoot::StaticClass()
	)
{
	SetupInputs(1);
}

#undef LOCTEXT_NAMESPACE

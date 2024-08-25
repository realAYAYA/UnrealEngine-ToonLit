// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSEAbs.h"
#include "Materials/MaterialExpressionAbs.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionAbs"

UDMMaterialStageExpressionAbs::UDMMaterialStageExpressionAbs()
	: UDMMaterialStageExpressionMathBase(
		LOCTEXT("Abs", "Abs"),
		UMaterialExpressionAbs::StaticClass()
	)
{
	SetupInputs(1);
}

#undef LOCTEXT_NAMESPACE

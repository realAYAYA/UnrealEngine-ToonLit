// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSECeil.h"
#include "Materials/MaterialExpressionCeil.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionCeil"

UDMMaterialStageExpressionCeil::UDMMaterialStageExpressionCeil()
	: UDMMaterialStageExpressionMathBase(
		LOCTEXT("Ceil", "Ceil"),
		UMaterialExpressionCeil::StaticClass()
	)
{
	SetupInputs(1);
}

#undef LOCTEXT_NAMESPACE

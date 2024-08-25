// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSECrossProduct.h"
#include "Materials/MaterialExpressionCrossProduct.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionCrossProduct"

UDMMaterialStageExpressionCrossProduct::UDMMaterialStageExpressionCrossProduct()
	: UDMMaterialStageExpressionMathBase(
		LOCTEXT("CrossProduct", "Cross Product"),
		UMaterialExpressionCrossProduct::StaticClass()
	)
{
	SetupInputs(2);

	bAllowSingleFloatMatch = false;
}

#undef LOCTEXT_NAMESPACE

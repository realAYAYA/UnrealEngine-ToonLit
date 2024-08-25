// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSEDotProduct.h"
#include "Materials/MaterialExpressionDotProduct.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionDotProduct"

UDMMaterialStageExpressionDotProduct::UDMMaterialStageExpressionDotProduct()
	: UDMMaterialStageExpressionMathBase(
		LOCTEXT("DotProduct", "Dot Product"),
		UMaterialExpressionDotProduct::StaticClass()
	)
{
	SetupInputs(2);
}

#undef LOCTEXT_NAMESPACE

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSEOneMinus.h"
#include "Materials/MaterialExpressionOneMinus.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionOneMinus"

UDMMaterialStageExpressionOneMinus::UDMMaterialStageExpressionOneMinus()
	: UDMMaterialStageExpressionMathBase(
		LOCTEXT("OneMinus", "One Minus"),
		UMaterialExpressionOneMinus::StaticClass()
	)
{
	SetupInputs(1);
}

#undef LOCTEXT_NAMESPACE

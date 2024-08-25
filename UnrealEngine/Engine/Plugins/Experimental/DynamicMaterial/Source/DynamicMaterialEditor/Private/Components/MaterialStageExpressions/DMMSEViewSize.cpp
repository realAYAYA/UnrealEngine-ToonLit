// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSEViewSize.h"
#include "Materials/MaterialExpressionViewSize.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionViewSize"

UDMMaterialStageExpressionViewSize::UDMMaterialStageExpressionViewSize()
	: UDMMaterialStageExpression(
		LOCTEXT("ViewSize", "ViewSize"),
		UDMMaterialStageExpression::FindClass(TEXT("MaterialExpressionViewSize"))
	)
{
	Menus.Add(EDMExpressionMenu::Other);

	OutputConnectors.Add({0, LOCTEXT("Size", "Size"), EDMValueType::VT_Float2});
}

#undef LOCTEXT_NAMESPACE

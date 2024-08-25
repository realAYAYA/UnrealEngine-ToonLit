// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSETime.h"
#include "Materials/MaterialExpressionTime.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionTime"

UDMMaterialStageExpressionTime::UDMMaterialStageExpressionTime()
	: UDMMaterialStageExpression(
		LOCTEXT("Time", "Time"),
		UMaterialExpressionTime::StaticClass()
	)
{
	Menus.Add(EDMExpressionMenu::Time);

	OutputConnectors.Add({0, LOCTEXT("Time", "Time"), EDMValueType::VT_Float1});
}

#undef LOCTEXT_NAMESPACE

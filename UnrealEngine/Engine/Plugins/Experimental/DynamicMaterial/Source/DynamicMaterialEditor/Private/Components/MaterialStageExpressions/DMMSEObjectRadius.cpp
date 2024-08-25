// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSEObjectRadius.h"
#include "Materials/MaterialExpressionObjectRadius.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionObjectRadius"

UDMMaterialStageExpressionObjectRadius::UDMMaterialStageExpressionObjectRadius()
	: UDMMaterialStageExpression(
		LOCTEXT("ObjectRadius", "Object Radius"),
		UDMMaterialStageExpression::FindClass(TEXT("MaterialExpressionObjectRadius"))
	)
{
	Menus.Add(EDMExpressionMenu::Object);

	OutputConnectors.Add({0, LOCTEXT("Radius", "Radius"), EDMValueType::VT_Float1});
}

#undef LOCTEXT_NAMESPACE

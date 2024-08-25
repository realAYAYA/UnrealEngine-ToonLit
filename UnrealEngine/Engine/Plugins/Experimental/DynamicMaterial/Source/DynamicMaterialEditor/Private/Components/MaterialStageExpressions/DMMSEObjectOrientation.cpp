// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSEObjectOrientation.h"
#include "Materials/MaterialExpressionObjectOrientation.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionObjectOrientation"

UDMMaterialStageExpressionObjectOrientation::UDMMaterialStageExpressionObjectOrientation()
	: UDMMaterialStageExpression(
		LOCTEXT("ObjectOrientation", "Object Orientation"),
		UMaterialExpressionObjectOrientation::StaticClass()
	)
{
	Menus.Add(EDMExpressionMenu::Object);

	OutputConnectors.Add({0, LOCTEXT("Orientation", "Orientation"), EDMValueType::VT_Float3_RPY});
}

#undef LOCTEXT_NAMESPACE

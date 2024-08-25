// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSEReflectionVectorWS.h"
#include "Materials/MaterialExpressionReflectionVectorWS.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionReflectionVectorWS"

UDMMaterialStageExpressionReflectionVectorWS::UDMMaterialStageExpressionReflectionVectorWS()
	: UDMMaterialStageExpression(
		LOCTEXT("ReflectionVectorWS", "Reflection Vector (WS)"),
		UMaterialExpressionReflectionVectorWS::StaticClass()
	)
{
	Menus.Add(EDMExpressionMenu::Geometry);
	Menus.Add(EDMExpressionMenu::WorldSpace);

	OutputConnectors.Add({0, LOCTEXT("Vector", "Vector"), EDMValueType::VT_Float3_XYZ});
}

#undef LOCTEXT_NAMESPACE

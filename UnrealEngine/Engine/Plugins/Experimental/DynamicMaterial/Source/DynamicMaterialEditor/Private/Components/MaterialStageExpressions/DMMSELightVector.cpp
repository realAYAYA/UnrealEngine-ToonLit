// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSELightVector.h"
#include "Materials/MaterialExpressionLightVector.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionLightVector"

UDMMaterialStageExpressionLightVector::UDMMaterialStageExpressionLightVector()
	: UDMMaterialStageExpression(
		LOCTEXT("LightVector", "Light Vector"),
		UDMMaterialStageExpression::FindClass(TEXT("MaterialExpressionLightVector"))
	)
{
	Menus.Add(EDMExpressionMenu::WorldSpace);

	OutputConnectors.Add({0, LOCTEXT("Vector", "Vector"), EDMValueType::VT_Float3_XYZ});
}

#undef LOCTEXT_NAMESPACE

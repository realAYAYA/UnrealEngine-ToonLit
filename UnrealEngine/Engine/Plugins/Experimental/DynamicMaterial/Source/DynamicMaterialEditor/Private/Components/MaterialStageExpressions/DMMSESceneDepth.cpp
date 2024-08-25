// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSESceneDepth.h"
#include "Materials/MaterialExpressionSceneDepth.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionSceneDepth"

UDMMaterialStageExpressionSceneDepth::UDMMaterialStageExpressionSceneDepth()
	: UDMMaterialStageExpression(
		LOCTEXT("SceneDepth", "Scene Depth"),
		UDMMaterialStageExpression::FindClass(TEXT("MaterialExpressionSceneDepth"))
	)
{
	InputConnectors.Add({0, LOCTEXT("UV", "UV"), EDMValueType::VT_Float2});

	Menus.Add(EDMExpressionMenu::Geometry);
	Menus.Add(EDMExpressionMenu::Other);

	OutputConnectors.Add({0, LOCTEXT("Depth", "Depth"), EDMValueType::VT_Float1});
}

#undef LOCTEXT_NAMESPACE

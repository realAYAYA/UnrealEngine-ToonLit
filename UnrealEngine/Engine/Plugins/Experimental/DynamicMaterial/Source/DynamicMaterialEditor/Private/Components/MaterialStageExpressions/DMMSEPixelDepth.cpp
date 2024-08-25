// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSEPixelDepth.h"
#include "Materials/MaterialExpressionPixelDepth.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionPixelDepth"

UDMMaterialStageExpressionPixelDepth::UDMMaterialStageExpressionPixelDepth()
	: UDMMaterialStageExpression(
		LOCTEXT("PixelDepth", "PixelDepth"),
		UMaterialExpressionPixelDepth::StaticClass()
	)
{
	Menus.Add(EDMExpressionMenu::Other);

	OutputConnectors.Add({0, LOCTEXT("Depth", "Depth"), EDMValueType::VT_Float1});
}

#undef LOCTEXT_NAMESPACE

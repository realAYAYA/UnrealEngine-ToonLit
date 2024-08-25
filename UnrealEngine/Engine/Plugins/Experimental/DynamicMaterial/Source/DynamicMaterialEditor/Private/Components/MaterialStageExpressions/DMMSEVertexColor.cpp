// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSEVertexColor.h"
#include "Materials/MaterialExpressionVertexColor.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionVertexColor"

UDMMaterialStageExpressionVertexColor::UDMMaterialStageExpressionVertexColor()
	: UDMMaterialStageExpression(
		LOCTEXT("VertexColor", "Vertex Color"),
		UMaterialExpressionVertexColor::StaticClass()
	)
{
	Menus.Add(EDMExpressionMenu::Geometry);

	OutputConnectors.Add({0, LOCTEXT("Color", "Color"), EDMValueType::VT_Float4_RGBA});
}

#undef LOCTEXT_NAMESPACE

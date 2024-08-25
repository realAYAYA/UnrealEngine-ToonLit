// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSEVertexTangentWS.h"
#include "Materials/MaterialExpressionVertexTangentWS.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionVertexTangentWS"

UDMMaterialStageExpressionVertexTangentWS::UDMMaterialStageExpressionVertexTangentWS()
	: UDMMaterialStageExpression(
		LOCTEXT("VertexTangentWS", "Vertex Tangent (WS)"),
		UMaterialExpressionVertexTangentWS::StaticClass()
	)
{
	Menus.Add(EDMExpressionMenu::Geometry);
	Menus.Add(EDMExpressionMenu::WorldSpace);

	OutputConnectors.Add({0, LOCTEXT("Tangent", "Tangent"), EDMValueType::VT_Float3_XYZ});
}

#undef LOCTEXT_NAMESPACE

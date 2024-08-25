// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSEVertexNormalWS.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionVertexNormalWS"

UDMMaterialStageExpressionVertexNormalWS::UDMMaterialStageExpressionVertexNormalWS()
	: UDMMaterialStageExpression(
		LOCTEXT("VertexNormalWS", "Vertex Normal (WS)"),
		UMaterialExpressionVertexNormalWS::StaticClass()
	)
{
	Menus.Add(EDMExpressionMenu::Geometry);
	Menus.Add(EDMExpressionMenu::WorldSpace);

	OutputConnectors.Add({0, LOCTEXT("Normal", "Normal"), EDMValueType::VT_Float3_XYZ});
}

#undef LOCTEXT_NAMESPACE

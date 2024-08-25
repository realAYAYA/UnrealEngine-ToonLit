// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSECameraPositionWS.h"
#include "Materials/MaterialExpressionCameraPositionWS.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionCameraPositionWS"

UDMMaterialStageExpressionCameraPositionWS::UDMMaterialStageExpressionCameraPositionWS()
	: UDMMaterialStageExpression(
		LOCTEXT("CameraPositionWS", "Camera Position (WS)"),
		UMaterialExpressionCameraPositionWS::StaticClass()
	)
{
	Menus.Add(EDMExpressionMenu::Camera);
	Menus.Add(EDMExpressionMenu::WorldSpace);

	OutputConnectors.Add({0, LOCTEXT("Position", "Position"), EDMValueType::VT_Float3_XYZ});
}

#undef LOCTEXT_NAMESPACE

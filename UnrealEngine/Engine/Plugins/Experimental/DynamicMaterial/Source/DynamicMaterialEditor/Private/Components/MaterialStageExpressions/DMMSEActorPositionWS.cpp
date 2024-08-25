// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSEActorPositionWS.h"
#include "Materials/MaterialExpressionActorPositionWS.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionActorPositionWS"

UDMMaterialStageExpressionActorPositionWS::UDMMaterialStageExpressionActorPositionWS()
	: UDMMaterialStageExpression(
		LOCTEXT("ActorPositionWS", "Actor Position (WS)"),
		UDMMaterialStageExpression::FindClass(TEXT("MaterialExpressionActorPositionWS"))
	)
{
	Menus.Add(EDMExpressionMenu::Object);
	Menus.Add(EDMExpressionMenu::WorldSpace);

	OutputConnectors.Add({0, LOCTEXT("Position", "Position"), EDMValueType::VT_Float3_XYZ});
}

#undef LOCTEXT_NAMESPACE

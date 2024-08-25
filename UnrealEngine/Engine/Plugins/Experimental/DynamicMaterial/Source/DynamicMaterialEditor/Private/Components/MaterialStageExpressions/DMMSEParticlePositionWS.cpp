// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSEParticlePositionWS.h"
#include "Materials/MaterialExpressionParticlePositionWS.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionParticlePositionWS"

UDMMaterialStageExpressionParticlePositionWS::UDMMaterialStageExpressionParticlePositionWS()
	: UDMMaterialStageExpression(
		LOCTEXT("ParticlePositionWS", "Particle Position (WS)"),
		UDMMaterialStageExpression::FindClass(TEXT("MaterialExpressionParticlePositionWS"))
	)
{
	Menus.Add(EDMExpressionMenu::Particle);
	Menus.Add(EDMExpressionMenu::WorldSpace);

	OutputConnectors.Add({0, LOCTEXT("Position", "Position"), EDMValueType::VT_Float3_XYZ});
}

#undef LOCTEXT_NAMESPACE

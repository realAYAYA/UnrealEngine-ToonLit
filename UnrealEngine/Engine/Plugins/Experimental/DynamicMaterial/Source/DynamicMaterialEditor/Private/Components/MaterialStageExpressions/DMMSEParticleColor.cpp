// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSEParticleColor.h"
#include "Materials/MaterialExpressionParticleColor.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionParticleColor"

UDMMaterialStageExpressionParticleColor::UDMMaterialStageExpressionParticleColor()
	: UDMMaterialStageExpression(
		LOCTEXT("ParticleColor", "Particle Color"),
		UDMMaterialStageExpression::FindClass(TEXT("MaterialExpressionParticleColor"))
	)
{
	Menus.Add(EDMExpressionMenu::Particle);

	OutputConnectors.Add({0, LOCTEXT("Color", "Color"), EDMValueType::VT_Float4_RGBA});
}

#undef LOCTEXT_NAMESPACE

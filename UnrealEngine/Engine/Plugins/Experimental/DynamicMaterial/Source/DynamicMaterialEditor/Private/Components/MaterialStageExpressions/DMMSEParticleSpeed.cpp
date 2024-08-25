// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSEParticleSpeed.h"
#include "Materials/MaterialExpressionParticleSpeed.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionParticleSpeed"

UDMMaterialStageExpressionParticleSpeed::UDMMaterialStageExpressionParticleSpeed()
	: UDMMaterialStageExpression(
		LOCTEXT("ParticleSpeed", "Particle Speed"),
		UDMMaterialStageExpression::FindClass(TEXT("MaterialExpressionParticleSpeed"))
	)
{
	Menus.Add(EDMExpressionMenu::Particle);
	Menus.Add(EDMExpressionMenu::Object);

	OutputConnectors.Add({0, LOCTEXT("Speed", "Speed"), EDMValueType::VT_Float1});
}

#undef LOCTEXT_NAMESPACE

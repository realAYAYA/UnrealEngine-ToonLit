// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSEParticleRadius.h"
#include "Materials/MaterialExpressionParticleRadius.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionParticleRadius"

UDMMaterialStageExpressionParticleRadius::UDMMaterialStageExpressionParticleRadius()
	: UDMMaterialStageExpression(
		LOCTEXT("ParticleRadius", "Particle Radius"),
		UDMMaterialStageExpression::FindClass(TEXT("MaterialExpressionParticleRadius"))
	)
{
	Menus.Add(EDMExpressionMenu::Geometry);
	Menus.Add(EDMExpressionMenu::Particle);

	OutputConnectors.Add({0, LOCTEXT("Radius", "Radius"), EDMValueType::VT_Float1});
}

#undef LOCTEXT_NAMESPACE

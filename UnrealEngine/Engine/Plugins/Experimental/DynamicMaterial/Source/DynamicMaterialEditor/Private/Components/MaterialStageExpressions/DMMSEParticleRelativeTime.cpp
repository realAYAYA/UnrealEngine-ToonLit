// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSEParticleRelativeTime.h"
#include "Materials/MaterialExpressionParticleRelativeTime.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionParticleRelativeTime"

UDMMaterialStageExpressionParticleRelativeTime::UDMMaterialStageExpressionParticleRelativeTime()
	: UDMMaterialStageExpression(
		LOCTEXT("ParticleRelativeTime", "Particle Relative Time"),
		UDMMaterialStageExpression::FindClass(TEXT("MaterialExpressionParticleRelativeTime"))
	)
{
	Menus.Add(EDMExpressionMenu::Particle);
	Menus.Add(EDMExpressionMenu::Time);

	OutputConnectors.Add({0, LOCTEXT("RelativeTime", "Relative Time"), EDMValueType::VT_Float1});
}

#undef LOCTEXT_NAMESPACE

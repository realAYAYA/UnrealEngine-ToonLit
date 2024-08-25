// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSEParticleMotionBlurFade.h"
#include "Materials/MaterialExpressionParticleMotionBlurFade.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionParticleMotionBlurFade"

UDMMaterialStageExpressionParticleMotionBlurFade::UDMMaterialStageExpressionParticleMotionBlurFade()
	: UDMMaterialStageExpression(
		LOCTEXT("ParticleMotionBlurFade", "Particle Motion Blur Fade"),
		UDMMaterialStageExpression::FindClass(TEXT("MaterialExpressionParticleMotionBlurFade"))
	)
{
	Menus.Add(EDMExpressionMenu::Particle);

	OutputConnectors.Add({0, LOCTEXT("Motion Blur Fade", "Motion Blur Fade"), EDMValueType::VT_Float1});
}

#undef LOCTEXT_NAMESPACE

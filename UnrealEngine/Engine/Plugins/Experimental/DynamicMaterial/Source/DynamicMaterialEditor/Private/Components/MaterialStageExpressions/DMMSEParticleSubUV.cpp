// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSEParticleSubUV.h"
#include "Materials/MaterialExpressionParticleSubUV.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionParticleSubUV"

UDMMaterialStageExpressionParticleSubUV::UDMMaterialStageExpressionParticleSubUV()
	: UDMMaterialStageExpressionTextureSampleBase(
		LOCTEXT("ParticleSubUV", "Particle Sub UV"),
		UMaterialExpressionParticleSubUV::StaticClass()
	)
{
	Menus.Add(EDMExpressionMenu::Particle);
	Menus.Add(EDMExpressionMenu::Texture);
}

#undef LOCTEXT_NAMESPACE

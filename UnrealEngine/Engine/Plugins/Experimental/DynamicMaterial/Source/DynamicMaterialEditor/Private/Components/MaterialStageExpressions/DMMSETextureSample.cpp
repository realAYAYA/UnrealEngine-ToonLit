// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSETextureSample.h"
#include "Materials/MaterialExpressionTextureSample.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionTextureSample"

UDMMaterialStageExpressionTextureSample::UDMMaterialStageExpressionTextureSample()
	: UDMMaterialStageExpressionTextureSampleBase(
		LOCTEXT("Texture", "Texture"),
		UMaterialExpressionTextureSample::StaticClass()
	)
{
	Menus.Add(EDMExpressionMenu::Texture);
}

#undef LOCTEXT_NAMESPACE

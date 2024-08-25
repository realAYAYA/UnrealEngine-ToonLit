// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSEAdd.h"
#include "Materials/MaterialExpressionAdd.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionAdd"

UDMMaterialStageExpressionAdd::UDMMaterialStageExpressionAdd()
	: UDMMaterialStageExpressionMathBase(
		LOCTEXT("Add", "Add"),
		UMaterialExpressionAdd::StaticClass()
	)
{
	SetupInputs(2);
}

#undef LOCTEXT_NAMESPACE
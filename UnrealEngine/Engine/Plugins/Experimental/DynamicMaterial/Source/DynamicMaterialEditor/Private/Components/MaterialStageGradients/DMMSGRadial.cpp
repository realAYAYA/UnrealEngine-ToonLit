// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageGradients/DMMSGRadial.h"
#include "DMMaterialFunctionLibrary.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Model/DMMaterialBuildState.h"
#include "Model/DMMaterialBuildUtils.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageGradientRadial"

UDMMaterialStageGradientRadial::UDMMaterialStageGradientRadial()
	: UDMMaterialStageGradient(LOCTEXT("GradientRadial", "Radial Gradient"))
{
}

void UDMMaterialStageGradientRadial::GenerateExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState) const
{
	if (!IsComponentValid() || !IsComponentAdded())
	{
		return;
	}

	if (InBuildState->HasStageSource(this))
	{
		return;
	}

	UMaterialExpressionMaterialFunctionCall* Radial = FDMMaterialFunctionLibrary::Get().GetRadialGradientExponential(InBuildState->GetDynamicMaterial(), UE_DM_NodeComment_Default);
	UMaterialExpression* MakeFloat = FDMMaterialFunctionLibrary::Get().GetMakeFloat3(InBuildState->GetDynamicMaterial(), UE_DM_NodeComment_Default);

	Radial->ConnectExpression(MakeFloat->GetInput(0), 0);
	Radial->ConnectExpression(MakeFloat->GetInput(1), 0);
	Radial->ConnectExpression(MakeFloat->GetInput(2), 0);

	InBuildState->AddStageSourceExpressions(this, {Radial, MakeFloat});
}

#undef LOCTEXT_NAMESPACE

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageGradients/DMMSGLinear.h"
#include "DMDefs.h"
#include "DMMaterialFunctionLibrary.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionFmod.h"
#include "Materials/MaterialExpressionIf.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Model/DMMaterialBuildState.h"
#include "Model/DMMaterialBuildUtils.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageGradientLinear"

UDMMaterialStageGradientLinear::UDMMaterialStageGradientLinear()
	: UDMMaterialStageGradient(LOCTEXT("GradientLinear", "Linear Gradient"))
{
	EditableProperties.Add(GET_MEMBER_NAME_CHECKED(UDMMaterialStageGradientLinear, Tiling));
}

void UDMMaterialStageGradientLinear::GenerateExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState) const
{
	if (!IsComponentValid() || !IsComponentAdded())
	{
		return;
	}

	if (InBuildState->HasStageSource(this))
	{
		return;
	}

	UMaterialExpressionComponentMask* Mask = InBuildState->GetBuildUtils().CreateExpression<UMaterialExpressionComponentMask>(UE_DM_NodeComment_Default);

	Mask->R = 1;
	Mask->G = 0;
	Mask->B = 0;
	Mask->A = 0;

	TArray<UMaterialExpression*> OutputExpressions;
	OutputExpressions.Add(Mask);

	UMaterialExpression* Output = nullptr;

	switch (Tiling)
	{
		case ELinearGradientTileType::NoTile:
			Output = Mask;
			break;

		case ELinearGradientTileType::Tile:
		{
			UMaterialExpressionFmod* Fmod = InBuildState->GetBuildUtils().CreateExpression<UMaterialExpressionFmod>(UE_DM_NodeComment_Default);
			UMaterialExpressionConstant* Divisor = InBuildState->GetBuildUtils().CreateExpression<UMaterialExpressionConstant>(UE_DM_NodeComment_Default);

			Mask->ConnectExpression(&Fmod->A, 0);

			Divisor->R = 1.f;
			Divisor->ConnectExpression(&Fmod->B, 0);

			Output = Fmod;
			OutputExpressions.Append({Divisor, Fmod});
			break;
		}

		case ELinearGradientTileType::TileAndMirror:
		{
			UMaterialExpressionFmod* Fmod = InBuildState->GetBuildUtils().CreateExpression<UMaterialExpressionFmod>(UE_DM_NodeComment_Default);
			UMaterialExpressionConstant* Divisor = InBuildState->GetBuildUtils().CreateExpression<UMaterialExpressionConstant>(UE_DM_NodeComment_Default);
			UMaterialExpressionSubtract* Subtract = InBuildState->GetBuildUtils().CreateExpression<UMaterialExpressionSubtract>(UE_DM_NodeComment_Default);
			UMaterialExpressionIf* IfExpr = InBuildState->GetBuildUtils().CreateExpression<UMaterialExpressionIf>(UE_DM_NodeComment_Default);

			Mask->ConnectExpression(&Fmod->A, 0);

			Divisor->R = 2.f;
			Divisor->ConnectExpression(&Fmod->B, 0);

			Fmod->ConnectExpression(&Subtract->B, 0);
			Fmod->ConnectExpression(&IfExpr->A, 0);
			Fmod->ConnectExpression(&IfExpr->AEqualsB, 0);
			Fmod->ConnectExpression(&IfExpr->ALessThanB, 0);

			Subtract->ConstA = 2.f;
			Subtract->ConnectExpression(&IfExpr->AGreaterThanB, 0);

			IfExpr->ConstB = 1.f;
			IfExpr->EqualsThreshold = 0.f;

			Output = IfExpr;
			OutputExpressions.Append({Divisor, Fmod, Subtract, IfExpr});
			break;
		}

		default:
			checkNoEntry();
			break;
	}

	UMaterialExpression* MakeFloat = FDMMaterialFunctionLibrary::Get().GetMakeFloat3(InBuildState->GetDynamicMaterial(), UE_DM_NodeComment_Default);
	Output->ConnectExpression(MakeFloat->GetInput(0), 0);
	Output->ConnectExpression(MakeFloat->GetInput(1), 0);
	Output->ConnectExpression(MakeFloat->GetInput(2), 0);

	OutputExpressions.Add(MakeFloat);	
	InBuildState->AddStageSourceExpressions(this, OutputExpressions);
}

void UDMMaterialStageGradientLinear::SetTilingType(ELinearGradientTileType InType)
{
	if (Tiling == InType)
	{
		return;
	}

	Tiling = InType;

	Update(EDMUpdateType::Structure);
}

#undef LOCTEXT_NAMESPACE

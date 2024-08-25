// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DMMaterialStageBlendFunction.h"
#include "DMDefs.h"
#include "DMMaterialFunctionLibrary.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Model/DMMaterialBuildState.h"
#include "Model/DMMaterialBuildUtils.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageBlendFunction"

UDMMaterialStageBlendFunction::UDMMaterialStageBlendFunction()
	: UDMMaterialStageBlendFunction(LOCTEXT("BlendFunction", "Blend Function"), nullptr)
{
}

UDMMaterialStageBlendFunction::UDMMaterialStageBlendFunction(const FText& InName, UMaterialFunctionInterface* InMaterialFunction)
	: UDMMaterialStageBlend(InName)
	, MaterialFunction(InMaterialFunction)
{
}

UDMMaterialStageBlendFunction::UDMMaterialStageBlendFunction(const FText& InName, const FName& InFunctionName, const FString& InFunctionPath)
	: UDMMaterialStageBlend(InName)
	, MaterialFunction(FDMMaterialFunctionLibrary::Get().GetFunction(InFunctionName, InFunctionPath))
{
	check(MaterialFunction);
}

void UDMMaterialStageBlendFunction::GenerateExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState) const
{
	if (!IsComponentValid() || !IsComponentAdded())
	{
		return;
	}

	if (InBuildState->HasStageSource(this))
	{
		return;
	}

	if (!MaterialFunction)
	{
		return;
	}

	UMaterialExpressionMaterialFunctionCall* FunctionCall = InBuildState->GetBuildUtils().CreateExpression<UMaterialExpressionMaterialFunctionCall>(UE_DM_NodeComment_Default);;
	FunctionCall->SetMaterialFunction(MaterialFunction);
	FunctionCall->UpdateFromFunctionResource();

	InBuildState->AddStageSourceExpressions(this, {FunctionCall});
}

void UDMMaterialStageBlendFunction::ConnectOutputToInput(const TSharedRef<FDMMaterialBuildState>& InBuildState, int32 InInputIndex, UMaterialExpression* InSourceExpression,
	int32 InSourceOutputIndex, int32 InSourceOutputChannel)
{
	check(InSourceExpression);
	check(InSourceExpression->GetOutputs().IsValidIndex(InSourceOutputIndex));
	check(InInputIndex >= 0 && InInputIndex <= 2);

	const TArray<UMaterialExpression*>& StageSourceExpressions = InBuildState->GetStageSourceExpressions(this);
	check(!StageSourceExpressions.IsEmpty());

	if (!MaterialFunction)
	{
		return;
	}

	TArray<FFunctionExpressionInput> Inputs;
	TArray<FFunctionExpressionOutput> Outputs;
	MaterialFunction->GetInputsAndOutputs(Inputs, Outputs);

	int32 InputAlphaIndex = INDEX_NONE;
	int32 InputAIndex = INDEX_NONE;
	int32 InputBIndex = INDEX_NONE;

	// Could go by name here, but that is more bug prone.
	for (int32 InputIdx = 0; InputIdx < Inputs.Num(); ++InputIdx)
	{
		if (Inputs[InputIdx].ExpressionInput->InputType == FunctionInput_Vector3)
		{
			if (InputAIndex == INDEX_NONE)
			{
				InputAIndex = InputIdx;
			}
			else if (InputBIndex == INDEX_NONE)
			{
				InputBIndex = InputIdx;
			}
		}
		else if (Inputs[InputIdx].ExpressionInput->InputType == FunctionInput_Scalar)
		{
			if (InputAlphaIndex == INDEX_NONE)
			{
				InputAlphaIndex = InputIdx;
			}
		}
	}

	switch (InInputIndex)
	{
		case InputA:
			ConnectOutputToInput_Internal(InBuildState, StageSourceExpressions[0] /* FunctionCall */, InputAIndex, InSourceExpression, InSourceOutputIndex, InSourceOutputChannel);
			break;

		case InputB:
			ConnectOutputToInput_Internal(InBuildState, StageSourceExpressions[0] /* FunctionCall */, InputBIndex, InSourceExpression, InSourceOutputIndex, InSourceOutputChannel);
			break;

		case InputAlpha:
			ConnectOutputToInput_Internal(InBuildState, StageSourceExpressions[0] /* FunctionCall */, InputAlphaIndex, InSourceExpression, InSourceOutputIndex, InSourceOutputChannel);
			break;

		default:
			checkNoEntry();
			break;
	}
}

#undef LOCTEXT_NAMESPACE

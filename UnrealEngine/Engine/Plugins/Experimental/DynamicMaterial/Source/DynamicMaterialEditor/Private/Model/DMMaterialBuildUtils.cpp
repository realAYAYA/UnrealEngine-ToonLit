// Copyright Epic Games, Inc. All Rights Reserved.

#include "Model/DMMaterialBuildUtils.h"
#include "Model/DMMaterialBuildState.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureObject.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpression.h"
#include "Materials/Material.h"
#include "MaterialEditingLibrary.h"
#include "DMPrivate.h"
#include "Components/DMMaterialStageInput.h"

FDMMaterialBuildUtils::FDMMaterialBuildUtils(FDMMaterialBuildState& InBuildState)
	: BuildState(InBuildState)
{
}

UMaterialExpression* FDMMaterialBuildUtils::CreateDefaultExpression() const
{
	UMaterialExpressionConstant* Constant = CreateExpression<UMaterialExpressionConstant>(UE_DM_NodeComment_Default, BuildState.GetDynamicMaterial());
	Constant->R = 0.f;

	return Constant;
}

UMaterialExpression* FDMMaterialBuildUtils::CreateExpression(TSubclassOf<UMaterialExpression> InExpressionClass, const FString& InComment, 
	UObject* InAsset /*= nullptr*/) const
{
	check(BuildState.GetDynamicMaterial());
	check(InExpressionClass.Get());
	check(InExpressionClass.Get() != UMaterialExpression::StaticClass());

	UMaterialExpression* NewExpression = UMaterialEditingLibrary::CreateMaterialExpressionEx(
		BuildState.GetDynamicMaterial(), 
		/* In Material Function */ nullptr,
		InExpressionClass.Get(),
		InAsset, 
		/* PosX */ 0, 
		/* PosY */ 0,
		/* Mark Dirty */ BuildState.ShouldDirtyAssets()
	);

	NewExpression->Desc = InComment;

	BuildState.GetDynamicMaterial()->GetEditorOnlyData()->ExpressionCollection.AddExpression(NewExpression);

	return NewExpression;
}

UMaterialExpression* FDMMaterialBuildUtils::CreateExpressionParameter(TSubclassOf<UMaterialExpression> InExpressionClass, 
	FName InParameterName, const FString& InComment, UObject* InAsset /*= nullptr*/) const
{
	check(BuildState.GetDynamicMaterial());
	check(InExpressionClass.Get());
	check(InExpressionClass.Get() != UMaterialExpression::StaticClass());

	UMaterialExpression* NewExpression = UMaterialEditingLibrary::CreateMaterialExpressionEx(
		BuildState.GetDynamicMaterial(),
		/* In Material Function */ nullptr,
		InExpressionClass.Get(),
		InAsset,
		/* PosX */ 0,
		/* PosY */ 0,
		/* Mark Dirty */ true
	);

	NewExpression->Desc = InComment;

	NewExpression->SetParameterName(InParameterName);
	BuildState.GetDynamicMaterial()->GetEditorOnlyData()->ExpressionCollection.AddExpression(NewExpression);

	TArray<UMaterialExpression*>& ExpressionList = BuildState.GetDynamicMaterial()->EditorParameters.FindOrAdd(InParameterName);
	ExpressionList.Add(NewExpression);

	return NewExpression;
}

TArray<UMaterialExpression*> FDMMaterialBuildUtils::CreateExpressionInputs(const TArray<FDMMaterialStageConnection>& InInputConnectionMap,
	int32 InStageSourceInputIdx, const TArray<UDMMaterialStageInput*>& InStageInputs, int32& OutOutputIndex, int32& OutOutputChannel) const
{
	if (InStageInputs.IsEmpty())
	{
		return CreateExpressionInput(nullptr);
	}

	// TODO Combine inputs into a single output
	if (InStageInputs.Num() == 1)
	{
		TArray<UMaterialExpression*> InputExpressions = CreateExpressionInput(InStageInputs[0]);
		check(InputExpressions.IsEmpty() == false);

		static constexpr int32 InputIdx = FDMMaterialStageConnectorChannel::FIRST_STAGE_INPUT;
		const FDMMaterialStageConnectorChannel* InputChannel = nullptr;

		for (const FDMMaterialStageConnectorChannel& Channel : InInputConnectionMap[InStageSourceInputIdx].Channels)
		{
			if (Channel.SourceIndex == InputIdx)
			{
				InputChannel = &Channel;
				break;
			}
		}

		if (!InputChannel)
		{
			OutOutputIndex = 0;
			OutOutputChannel = FDMMaterialStageConnectorChannel::WHOLE_CHANNEL;
		}
		else
		{
			const TArray<FDMMaterialStageConnector>& OutputConnectors = InStageInputs[0]->GetOutputConnectors();
			check(OutputConnectors.IsValidIndex(InputChannel->OutputIndex));

			OutOutputIndex = OutputConnectors[InputChannel->OutputIndex].Index;
			OutOutputChannel = InputChannel->OutputChannel;
		}

		return InputExpressions;
	}

	TArray<UMaterialExpression*> Expressions;
	TArray<TArray<UMaterialExpression*>> PerInputExpressions;

	// There are a 4 channels in an RGBA input.
	for (UDMMaterialStageInput* StageInput : InStageInputs)
	{
		TArray<UMaterialExpression*> InputExpressions = CreateExpressionInput(StageInput);
		PerInputExpressions.Add(InputExpressions);
		Expressions.Append(InputExpressions);
	}

	struct FDMMaskOutput
	{
		UMaterialExpression* Mask;
		int32 OutputIndex;
	};

	TArray<FDMMaskOutput> Masks;

	for (int32 StageInputIdx = 0; StageInputIdx < PerInputExpressions.Num(); ++StageInputIdx)
	{
		const int32 InputIdx = FDMMaterialStageConnectorChannel::FIRST_STAGE_INPUT + StageInputIdx;
		const FDMMaterialStageConnectorChannel* InputChannel = nullptr;

		for (const FDMMaterialStageConnectorChannel& Channel : InInputConnectionMap[InStageSourceInputIdx].Channels)
		{
			if (Channel.SourceIndex == InputIdx)
			{
				InputChannel = &Channel;
				break;
			}
		}

		UMaterialExpression* LastExpression = PerInputExpressions[StageInputIdx].Last();

		if (!InputChannel
			|| (InputChannel->OutputIndex == 0
				&& InputChannel->OutputChannel == FDMMaterialStageConnectorChannel::WHOLE_CHANNEL))
		{
			Masks.Add({LastExpression, 0});
			continue;
		}

		const int32 InnateOutputIndex = InStageInputs[InStageSourceInputIdx]->GetInnateMaskOutput(
			InInputConnectionMap[InStageSourceInputIdx].Channels[StageInputIdx].OutputIndex,
			InInputConnectionMap[InStageSourceInputIdx].Channels[StageInputIdx].OutputChannel
		);

		if (InnateOutputIndex != INDEX_NONE)
		{
			Masks.Add({LastExpression, InnateOutputIndex});
		}
		else
		{
			const TArray<FDMMaterialStageConnector>& OutputConnectors = InStageInputs[InStageSourceInputIdx]->GetOutputConnectors();
			check(OutputConnectors.IsValidIndex(InputChannel->OutputIndex));

			const int32 NodeOutputIndex = OutputConnectors[InputChannel->OutputIndex].Index;

			UMaterialExpressionComponentMask* Mask = CreateExpressionBitMask(
				LastExpression,
				NodeOutputIndex,
				InInputConnectionMap[InStageSourceInputIdx].Channels[StageInputIdx].OutputChannel
			);

			Masks.Add({Mask, 0});
			Expressions.Add(Mask);
		}
	}

	TArray<UMaterialExpressionAppendVector*> Appends;

	for (int32 StageInputIdx = 1; StageInputIdx < InStageInputs.Num(); ++StageInputIdx)
	{
		UMaterialExpression* PreviousExpression = nullptr;
		int32 PreviousOutputIndex = INDEX_NONE;

		if (StageInputIdx == 1)
		{
			PreviousExpression = Masks[0].Mask;
			PreviousOutputIndex = Masks[0].OutputIndex;
		}
		else
		{
			PreviousExpression = Appends[StageInputIdx - 1];
			PreviousOutputIndex = 0;
		}

		UMaterialExpressionAppendVector* Append = CreateExpressionAppend(
			PreviousExpression,
			PreviousOutputIndex,
			Masks[StageInputIdx].Mask,
			Masks[StageInputIdx].OutputIndex
		);

		Appends.Add(Append);
	}

	Expressions.Append(Appends);
	
	OutOutputIndex = 0;
	OutOutputChannel = FDMMaterialStageConnectorChannel::WHOLE_CHANNEL;

	return Expressions;
}

TArray<UMaterialExpression*> FDMMaterialBuildUtils::CreateExpressionInput(UDMMaterialStageInput* InInput) const
{
	if (!InInput)
	{
		UMaterialExpressionConstant4Vector* Constant = Cast<UMaterialExpressionConstant4Vector>(
			CreateExpression<UMaterialExpressionConstant4Vector>(UE_DM_NodeComment_Default)
		);

		Constant->Constant = FLinearColor::Black;
		Constant->Constant.A = 0.f;
		BuildState.AddOtherExpressions({Constant});
		return {Constant};
	}

	InInput->GenerateExpressions(BuildState.AsShared());
	return BuildState.GetStageSourceExpressions(InInput);
}

UMaterialExpressionComponentMask* FDMMaterialBuildUtils::CreateExpressionBitMask(UMaterialExpression* InExpression, 
	int32 InOutputIndex, int32 InOutputChannels) const
{
	UMaterialExpressionComponentMask* Mask = Cast<UMaterialExpressionComponentMask>(
		CreateExpression<UMaterialExpressionComponentMask>(UE_DM_NodeComment_Default)
	);

	Mask->Input.Expression = InExpression;
	Mask->Input.OutputIndex = InOutputIndex;

	if (InOutputChannels == FDMMaterialStageConnectorChannel::WHOLE_CHANNEL)
	{
		const uint32 OutputType = InExpression->GetOutputType(InOutputIndex);

		Mask->R = (OutputType == MCT_Float);
		Mask->G = (OutputType == MCT_Float2);
		Mask->B = (OutputType == MCT_Float3);
		Mask->A = (OutputType == MCT_Float4);

		return Mask;
	}

	Mask->R = !!(InOutputChannels & FDMMaterialStageConnectorChannel::FIRST_CHANNEL);
	Mask->G = !!(InOutputChannels & FDMMaterialStageConnectorChannel::SECOND_CHANNEL);
	Mask->B = !!(InOutputChannels & FDMMaterialStageConnectorChannel::THIRD_CHANNEL);
	Mask->A = !!(InOutputChannels & FDMMaterialStageConnectorChannel::FOURTH_CHANNEL);

	BuildState.AddOtherExpressions({Mask});

	return Mask;
}

UMaterialExpressionAppendVector* FDMMaterialBuildUtils::CreateExpressionAppend(UMaterialExpression* InExpressionA, 
	int32 InOutputIndexA, UMaterialExpression* InExpressionB, int32 InOutputIndexB) const
{
	UMaterialExpressionAppendVector* Append = Cast<UMaterialExpressionAppendVector>(
		CreateExpression<UMaterialExpressionAppendVector>(UE_DM_NodeComment_Default)
	);

	Append->A.Expression = InExpressionA;
	Append->A.OutputIndex = InOutputIndexA;

	Append->B.Expression = InExpressionB;
	Append->B.OutputIndex = InOutputIndexB;

	return Append;
}

void FDMMaterialBuildUtils::UpdatePreviewMaterial(UMaterialExpression* InLastExpression, int32 OutputIdx, int32 InOutputChannel, 
	int32 InSize) const
{
	FColorMaterialInput& EmissiveColor = BuildState.GetDynamicMaterial()->GetEditorOnlyData()->EmissiveColor;

	EmissiveColor.Expression = nullptr;
	EmissiveColor.OutputIndex = 0;
	EmissiveColor.SetMask(0, 0, 0, 0, 0);

	if (!InLastExpression)
	{
		return;
	}

	if (UMaterialExpressionTextureObject* TextureObject = Cast<UMaterialExpressionTextureObject>(InLastExpression))
	{
		UMaterialExpressionTextureSample* NewSampler = CreateExpression<UMaterialExpressionTextureSample>(UE_DM_NodeComment_Default);
		NewSampler->Desc = "Auto sampler";
		BuildState.AddOtherExpressions({NewSampler});

		TextureObject->ConnectExpression(NewSampler->GetInput(1), 0);

		EmissiveColor.Expression = NewSampler;
		EmissiveColor.OutputIndex = 0;
	}
	else if (UMaterialExpressionTextureObjectParameter* TextureObjectParam = Cast<UMaterialExpressionTextureObjectParameter>(InLastExpression))
	{
		UMaterialExpressionTextureSample* NewSampler = CreateExpression<UMaterialExpressionTextureSample>(UE_DM_NodeComment_Default);
		NewSampler->Desc = "Auto sampler";
		BuildState.AddOtherExpressions({NewSampler});

		TextureObjectParam->ConnectExpression(NewSampler->GetInput(1), 0);

		EmissiveColor.Expression = NewSampler;
		EmissiveColor.OutputIndex = 0;
	}
	// Single material property, connect it up to emissive and output it.
	else
	{
		EmissiveColor.Expression = InLastExpression;

		const TArray<FExpressionOutput>& Outputs = InLastExpression->GetOutputs();

		if (Outputs.IsValidIndex(OutputIdx))
		{
			EmissiveColor.OutputIndex = OutputIdx;
			UE::DynamicMaterialEditor::Private::SetMask(EmissiveColor, Outputs[OutputIdx], InOutputChannel);
		}
	}
}

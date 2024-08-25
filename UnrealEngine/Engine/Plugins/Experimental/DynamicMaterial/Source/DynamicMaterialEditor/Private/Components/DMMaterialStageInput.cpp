// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DMMaterialStageInput.h"
#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialStage.h"
#include "Components/DMMaterialStageThroughput.h"
#include "DMComponentPath.h"
#include "Components/DMMaterialLayer.h"
#include "Model/DMMaterialBuildState.h"
#include "Model/DMMaterialBuildUtils.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"

const FString UDMMaterialStageInput::StageInputPrefixStr = FString(TEXT("DMMaterialStageInput"));

void UDMMaterialStageInput::Update(EDMUpdateType InUpdateType)
{
	if (!IsComponentValid())
	{
		return;
	}

	if (HasComponentBeenRemoved())
	{
		return;
	}

	if (InUpdateType == EDMUpdateType::Structure)
	{
		MarkComponentDirty();
	}
	
	UDMMaterialStage* Stage = GetStage();
	check(Stage);

	if (UDMMaterialStageThroughput* Throughput = Cast<UDMMaterialStageThroughput>(Stage->GetSource()))
	{
		Throughput->Update(InUpdateType);
		Stage->InputUpdated(this, InUpdateType);
	}
	else
	{
		Stage->Update(InUpdateType);
	}

	Super::Update(InUpdateType);
}

void UDMMaterialStageInput::UpdatePreviewMaterial(UMaterial* InPreviewMaterial /*= nullptr*/)
{
	if (!IsComponentValid())
	{
		return;
	}

	if (!InPreviewMaterial)
	{
		if (!PreviewMaterial)
		{
			CreatePreviewMaterial();
		}

		InPreviewMaterial = PreviewMaterial;

		if (!PreviewMaterial)
		{
			return;
		}
	}

	UDMMaterialStage* Stage = GetStage();
	check(Stage);

	UDMMaterialLayerObject* Layer = Stage->GetLayer();
	check(Layer);

	UDMMaterialSlot* Slot = Layer->GetSlot();
	check(Slot);

	UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = Slot->GetMaterialModelEditorOnlyData();
	check(ModelEditorOnlyData);

	TSharedRef<FDMMaterialBuildState> BuildState = ModelEditorOnlyData->CreateBuildState(InPreviewMaterial);
	BuildState->SetPreviewMaterial();

	GenerateExpressions(BuildState);
	UMaterialExpression* StageSourceExpression = BuildState->GetLastStageSourceExpression(this);

	const FDMMaterialStageConnectorChannel* InputChannel = Stage->FindInputChannel(this);

	if (InputChannel
		&& (InputChannel->OutputIndex != 0
			|| InputChannel->OutputChannel != FDMMaterialStageConnectorChannel::WHOLE_CHANNEL))
	{
		check(OutputConnectors.IsValidIndex(InputChannel->OutputIndex));
		int32 NodeOutputIndex = OutputConnectors[InputChannel->OutputIndex].Index;

		BuildState->GetBuildUtils().UpdatePreviewMaterial(StageSourceExpression, NodeOutputIndex, InputChannel->OutputChannel, 32);
	}
	else
	{
		BuildState->GetBuildUtils().UpdatePreviewMaterial(StageSourceExpression, 0, FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 32);
	}
}

FString UDMMaterialStageInput::GetComponentPathComponent() const
{
	if (UDMMaterialStage* Stage = GetStage())
	{
		const TArray<UDMMaterialStageInput*>& Inputs = Stage->GetInputs();
		int32 Index = INDEX_NONE;

		for (int32 InputIdx = 0; InputIdx < Inputs.Num(); ++InputIdx)
		{
			if (Inputs[InputIdx] == this)
			{
				Index = InputIdx;
				break;
			}
		}

		return FString::Printf(
			TEXT("%s%hc%i%hc"),
			*UDMMaterialStage::InputsPathToken,
			FDMComponentPath::ParameterOpen,
			Index,
			FDMComponentPath::ParameterClose
		);
	}

	return Super::GetComponentPathComponent();
}

void UDMMaterialStageInput::GetComponentPathInternal(TArray<FString>& OutChildComponentPathComponents) const
{
	// Strip off the type index
	if (OutChildComponentPathComponents.IsEmpty() == false)
	{
		int32 IndexOfUnderscore = INDEX_NONE;

		if (OutChildComponentPathComponents.Last().FindChar('_', IndexOfUnderscore))
		{
			OutChildComponentPathComponents.Last().LeftInline(IndexOfUnderscore);
		}
	}

	Super::GetComponentPathInternal(OutChildComponentPathComponents);
}

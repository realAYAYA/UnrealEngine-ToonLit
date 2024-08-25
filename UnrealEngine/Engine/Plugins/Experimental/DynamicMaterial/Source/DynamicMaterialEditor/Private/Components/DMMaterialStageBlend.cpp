// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "Components/DMMaterialStageBlend.h"
#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialStage.h"
#include "Components/DMMaterialStageInput.h"
#include "Components/MaterialStageExpressions/DMMSETextureSample.h"
#include "Components/MaterialStageInputs/DMMSIExpression.h"
#include "Components/MaterialStageInputs/DMMSITextureUV.h"
#include "Components/MaterialStageInputs/DMMSIValue.h"
#include "Components/MaterialValues/DMMaterialValueFloat1.h"
#include "DMPrivate.h"
#include "Helpers/DMInputNodeBuilder.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Model/DMMaterialBuildState.h"
#include "Model/DMMaterialBuildUtils.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
 
#define LOCTEXT_NAMESPACE "DMMaterialProperty"
 
TArray<TStrongObjectPtr<UClass>> UDMMaterialStageBlend::Blends = {};
 
UDMMaterialStageBlend::UDMMaterialStageBlend()
	: UDMMaterialStageBlend(FText::GetEmpty())
{
}
 
UDMMaterialStageBlend::UDMMaterialStageBlend(const FText& InName)
	: UDMMaterialStageThroughput(InName)
{
	bInputRequired = true;
	bAllowNestedInputs = true;
 
	InputConnectors.Add({InputAlpha, LOCTEXT("Opacity", "Opacity"), EDMValueType::VT_Float1});
	InputConnectors.Add({InputA, LOCTEXT("PreviousStage", "Previous Stage"), EDMValueType::VT_Float3_RGB});
	InputConnectors.Add({InputB, LOCTEXT("Base", "Base"), EDMValueType::VT_Float3_RGB});
 
	OutputConnectors.Add({0, LOCTEXT("Blend", "Blend"), EDMValueType::VT_Float3_RGB});
}
 
UDMMaterialStage* UDMMaterialStageBlend::CreateStage(TSubclassOf<UDMMaterialStageBlend> InMaterialStageBlendClass, UDMMaterialLayerObject* InLayer)
{
	check(InMaterialStageBlendClass);
 
	GetAvailableBlends();
	check(Blends.Contains(TStrongObjectPtr<UClass>(InMaterialStageBlendClass.Get())));
 
	const FDMUpdateGuard Guard;
 
	UDMMaterialStage* NewStage = UDMMaterialStage::CreateMaterialStage(InLayer);

	UDMMaterialStageBlend* SourceBlend = NewObject<UDMMaterialStageBlend>(
		NewStage, 
		InMaterialStageBlendClass.Get(), 
		NAME_None, 
		RF_Transactional
	);

	check(SourceBlend);
 
	NewStage->SetSource(SourceBlend);
 
	return NewStage;
}
 
const TArray<TStrongObjectPtr<UClass>>& UDMMaterialStageBlend::GetAvailableBlends()
{
	if (Blends.IsEmpty())
	{
		GenerateBlendList();
	}
 
	return Blends;
}
 
void UDMMaterialStageBlend::GenerateBlendList()
{
	Blends.Empty();
 
	const TArray<TStrongObjectPtr<UClass>>& SourceList = UDMMaterialStageSource::GetAvailableSourceClasses();
 
	for (const TStrongObjectPtr<UClass>& SourceClass : SourceList)
	{
		UDMMaterialStageBlend* StageBlendCDO = Cast<UDMMaterialStageBlend>(SourceClass->GetDefaultObject(true));
 
		if (!StageBlendCDO)
		{
			continue;
		}
 
		Blends.Add(SourceClass);
	}
}
 
bool UDMMaterialStageBlend::CanInputAcceptType(int32 InputIndex, EDMValueType ValueType) const
{
	check(InputConnectors.IsValidIndex(InputIndex));
 
	if (InputIndex == InputAlpha)
	{
		return InputConnectors[InputIndex].IsCompatibleWith(ValueType);
	}
 
	if (!UDMValueDefinitionLibrary::GetValueDefinition(ValueType).IsFloatType())
	{
		return false;
	}
 
	return InputConnectors[InputIndex].IsCompatibleWith(ValueType);
}
 
void UDMMaterialStageBlend::GetMaskAlphaBlendNode(const TSharedRef<FDMMaterialBuildState>& InBuildState, UMaterialExpression*& OutExpression, 
	int32& OutOutputIndex, int32& OutOutputChannel) const
{
	FDMMaterialStageConnectorChannel Channel;
	TArray<UMaterialExpression*> Expressions;
 
	OutOutputIndex = ResolveInput(InBuildState, 0, Channel, Expressions);
	OutOutputChannel = Channel.OutputChannel;
	OutExpression = Expressions.Last();
}

bool UDMMaterialStageBlend::UpdateStagePreviewMaterial(UDMMaterialStage* InStage, UMaterial* InPreviewMaterial, 
	UMaterialExpression*& OutMaterialExpression, int32& OutputIndex)
{
	check(InStage);
	check(InPreviewMaterial);

	UDMMaterialLayerObject* Layer = InStage->GetLayer();
	check(Layer);

	UDMMaterialSlot* Slot = Layer->GetSlot();
	check(Slot);

	UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = Slot->GetMaterialModelEditorOnlyData();
	check(ModelEditorOnlyData);

	const TArray<FDMMaterialStageConnection>& InputConnectionMap = InStage->GetInputConnectionMap();

	if (!InputConnectionMap.IsValidIndex(InputB)
		|| InputConnectionMap[InputB].Channels.Num() != 1)
	{
		return false;
	}

	TSharedRef<FDMMaterialBuildState> BuildState = ModelEditorOnlyData->CreateBuildState(InPreviewMaterial);
	BuildState->SetPreviewMaterial();

	UDMMaterialStageSource* PreviewSource = GetInputB();

	PreviewSource->GenerateExpressions(BuildState);
	const TArray<UMaterialExpression*>& SourceExpressions = BuildState->GetStageSourceExpressions(PreviewSource);

	if (SourceExpressions.IsEmpty())
	{
		return false;
	}

	UMaterialExpression* LastExpression = SourceExpressions.Last();
	OutputIndex = InputConnectionMap[InputB].Channels[0].OutputIndex;

	{
		const UDMMaterialStageSource* Source = nullptr;

		for (const TPair<const UDMMaterialStageSource*, TArray<UMaterialExpression*>>& Pair : BuildState->GetStageSourceMap())
		{
			if (Pair.Value.IsEmpty())
			{
				continue;
			}

			if (Pair.Value.Last() == LastExpression)
			{
				Source = Pair.Key;
				break;
			}
		}

		if (Source && Source->GetOutputConnectors().IsValidIndex(OutputIndex))
		{
			OutputIndex = Source->GetOutputConnectors()[OutputIndex].Index;
		}
	}

	if (InputConnectionMap[InputB].Channels[0].OutputChannel != FDMMaterialStageConnectorChannel::WHOLE_CHANNEL)
	{
		LastExpression = BuildState->GetBuildUtils().CreateExpressionBitMask(
			LastExpression,
			OutputIndex,
			InputConnectionMap[InputB].Channels[0].OutputChannel
		);

		OutputIndex = 0;
	}

	OutMaterialExpression = LastExpression;

	return true;
}

void UDMMaterialStageBlend::AddDefaultInput(int32 InInputIndex) const
{
	if (!IsComponentValid())
	{
		return;
	}

	UDMMaterialStage* Stage = GetStage();
	check(Stage);
 
	switch (InInputIndex)
	{
		case InputAlpha:
		{
			UDMMaterialStageInputValue* InputValue = UDMMaterialStageInputValue::ChangeStageInput_NewLocalValue(
				Stage, 
				InInputIndex, 
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL,
				EDMValueType::VT_Float1, 
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
			);

			check(InputValue);
 
			UDMMaterialValueFloat1* Float1Value = Cast<UDMMaterialValueFloat1>(InputValue->GetValue());
			check(Float1Value);
 
			Float1Value->SetDefaultValue(1.f);
			Float1Value->ApplyDefaultValue();
			Float1Value->SetValueRange(FFloatInterval(0, 1));
			break;
		}
 
		case InputA:
		{
			UDMMaterialLayerObject* Layer = Stage->GetLayer();
			check(Layer);
 
			EDMMaterialPropertyType StageProperty = Layer->GetMaterialProperty();
			check(StageProperty != EDMMaterialPropertyType::None);
 
			const UDMMaterialLayerObject* PreviousLayer = Layer->GetPreviousLayer(StageProperty, EDMMaterialLayerStage::Base);
 
			if (PreviousLayer)
			{
				Stage->ChangeInput_PreviousStage(
					InInputIndex, 
					FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
					StageProperty,
					0, 
					FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
				);
				break;
			}
			
			Stage->ChangeInput_PreviousStage(
				InInputIndex, 
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
				EDMMaterialPropertyType::EmissiveColor,
				0, 
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
			);
			break;
		}
 
		case InputB:
			if (CanInputAcceptType(InInputIndex, EDMValueType::VT_Float3_RGB))
			{
				UDMMaterialStageInputExpression::ChangeStageInput_Expression(
					Stage,
					UDMMaterialStageExpressionTextureSample::StaticClass(), 
					InInputIndex, 
					FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
					0,
					FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
				);
			}
			break;
 
		default:
			checkNoEntry();
			break;
	}
}

bool UDMMaterialStageBlend::CanChangeInput(int32 InputIndex) const
{
	return (InputIndex == InputB);
}
 
bool UDMMaterialStageBlend::CanChangeInputType(int32 InputIndex) const
{
	return false;
}

bool UDMMaterialStageBlend::IsInputVisible(int32 InputIndex) const
{
	if (InputIndex == InputA)
	{
		return false;
	}

	return Super::IsInputVisible(InputIndex);
}

int32 UDMMaterialStageBlend::ResolveInput(const TSharedRef<FDMMaterialBuildState>& InBuildState, int32 InputIndex, FDMMaterialStageConnectorChannel& OutChannel, TArray<UMaterialExpression*>& OutExpressions) const
{
	int32 NodeOutputIndex = Super::ResolveInput(InBuildState, InputIndex, OutChannel, OutExpressions);

	if (InputIndex == InputB)
	{
		if (UDMMaterialStage* Stage = GetStage())
		{
			if (UDMMaterialLayerObject* Layer = Stage->GetLayer())
			{
				Layer->ApplyEffects(
					InBuildState, 
					Stage, 
					OutExpressions, 
					OutChannel.OutputChannel, 
					NodeOutputIndex
				);
			}
		}
	}

	return NodeOutputIndex;
}

FText UDMMaterialStageBlend::GetStageDescription() const
{
	if (UDMMaterialStageInput* StageInputB = GetInputB())
	{
		return StageInputB->GetComponentDescription();
	}

	return Super::GetStageDescription();
}

FDMExpressionInput UDMMaterialStageBlend::GetLayerMaskLinkTextureUVInputExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState) const
{
	FDMExpressionInput ExpressionInput = {};
 
	if (!SupportsLayerMaskTextureUVLink())
	{
		return ExpressionInput;
	}
 
	UDMMaterialStage* Stage = GetStage();
	check(Stage);
 
	const TArray<FDMMaterialStageConnection>& InputConnections = Stage->GetInputConnectionMap();
	const TArray<UDMMaterialStageInput*>& StageInputs = Stage->GetInputs();
 
	FDMMaterialStageConnectorChannel Channel;
 
	for (int32 InputIdx = InputA; InputIdx <= InputB; ++InputIdx)
	{
		if (!InputConnections.IsValidIndex(InputIdx)
			|| InputConnections[InputIdx].Channels.Num() != 1
			|| InputConnections[InputIdx].Channels[0].SourceIndex < FDMMaterialStageConnectorChannel::FIRST_STAGE_INPUT)
		{
			continue;
		}
 
		int32 StageInputIdx = InputConnections[InputIdx].Channels[0].SourceIndex - FDMMaterialStageConnectorChannel::FIRST_STAGE_INPUT;
 
		ExpressionInput.OutputIndex = ResolveLayerMaskTextureUVLinkInputImpl(
			InBuildState, 
			StageInputs[StageInputIdx], 
			Channel, 
			ExpressionInput.OutputExpressions
		);
		
		ExpressionInput.OutputChannel = Channel.OutputChannel;
		
		if (ExpressionInput.IsValid())
		{
			break;
		}
		else
		{
			ExpressionInput = {};
		}
	}
 
	return ExpressionInput;
}
 
void UDMMaterialStageBlend::UpdatePreviewMaterial(UMaterial* InPreviewMaterial)
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
 
		if (!InPreviewMaterial)
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

	UDynamicMaterialModel* MaterialModel = ModelEditorOnlyData->GetMaterialModel();
	check(MaterialModel);
 
	const TArray<FDMMaterialStageConnection>& InputConnectionMap = Stage->GetInputConnectionMap();
	const TArray<UDMMaterialStageInput*>& StageInputs = Stage->GetInputs();
	TArray<UE::DynamicMaterialEditor::Private::FDMInputInputs> Inputs; // There can be multiple inputs per input
	bool bHasStageInput = false;
 
	for (int32 InputIdx = 0; InputIdx < InputConnectors.Num(); ++InputIdx)
	{
		TArray<UDMMaterialStageInput*> ChannelInputs;
 
		if (!InputConnectionMap.IsValidIndex(InputIdx))
		{
			continue;
		}
 
		ChannelInputs.SetNum(InputConnectionMap[InputIdx].Channels.Num());
		bool bNonStageInput = false;
 
		for (int32 ChannelIdx = 0; ChannelIdx < InputConnectionMap[InputIdx].Channels.Num(); ++ChannelIdx)
		{
			if (InputConnectionMap[InputIdx].Channels[ChannelIdx].SourceIndex == FDMMaterialStageConnectorChannel::PREVIOUS_STAGE)
			{
				bHasStageInput = true;
				ChannelInputs[ChannelIdx] = nullptr;
				continue;
			}
 
			if (InputConnectionMap[InputIdx].Channels[ChannelIdx].SourceIndex >= FDMMaterialStageConnectorChannel::FIRST_STAGE_INPUT)
			{
				int32 StageInputIdx = InputConnectionMap[InputIdx].Channels[ChannelIdx].SourceIndex - FDMMaterialStageConnectorChannel::FIRST_STAGE_INPUT;
				ChannelInputs[ChannelIdx] = StageInputs[StageInputIdx];
				bNonStageInput = true;
				continue;
			}
		}
 
		if (ChannelInputs.IsEmpty())
		{
			continue;
		}
 
		if (bNonStageInput)
		{
			Inputs.Add({InputIdx, ChannelInputs});
		}
	}
 
	TSharedRef<FDMMaterialBuildState> BuildState = ModelEditorOnlyData->CreateBuildState(InPreviewMaterial);
	BuildState->SetPreviewMaterial();
 
	if (!bHasStageInput || Inputs.IsEmpty())
	{
		Stage->GenerateExpressions(BuildState);
		UMaterialExpression* StageExpression = BuildState->GetLastStageExpression(Stage);
 
		BuildState->GetBuildUtils().UpdatePreviewMaterial(
			StageExpression, 
			0, 
			FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
			32
		);
	}
	else
	{
		UE::DynamicMaterialEditor::Private::BuildExpressionInputs(
			BuildState, 
			InputConnectionMap, 
			Inputs
		);
	}
}

UDMMaterialValueFloat1* UDMMaterialStageBlend::GetInputAlpha() const
{
	if (UDMMaterialStage* Stage = GetStage())
	{
		const TArray<FDMMaterialStageConnection>& InputMap = Stage->GetInputConnectionMap();

		if (InputMap.IsValidIndex(InputAlpha)
			&& InputMap[InputAlpha].Channels.IsValidIndex(0))
		{
			TArray<UDMMaterialStageInput*> Inputs = Stage->GetInputs();
			int32 StageInputIdx = InputMap[InputAlpha].Channels[0].SourceIndex - FDMMaterialStageConnectorChannel::FIRST_STAGE_INPUT;

			if (Stage->GetInputs().IsValidIndex(StageInputIdx))
			{
				if (UDMMaterialStageInputValue* InputValue = Cast<UDMMaterialStageInputValue>(Stage->GetInputs()[StageInputIdx]))
				{
					return Cast<UDMMaterialValueFloat1>(InputValue->GetValue());
				}
			}
		}
	}

	return nullptr;
}

UDMMaterialStageInput* UDMMaterialStageBlend::GetInputB() const
{
	if (UDMMaterialStage* Stage = GetStage())
	{
		const TArray<FDMMaterialStageConnection>& InputMap = Stage->GetInputConnectionMap();

		if (InputMap.IsValidIndex(InputB)
			&& InputMap[InputB].Channels.IsValidIndex(0))
		{
			TArray<UDMMaterialStageInput*> Inputs = Stage->GetInputs();
			int32 StageInputIdx = InputMap[InputB].Channels[0].SourceIndex - FDMMaterialStageConnectorChannel::FIRST_STAGE_INPUT;

			if (Stage->GetInputs().IsValidIndex(StageInputIdx))
			{
				return Stage->GetInputs()[StageInputIdx];
			}
		}
	}

	return nullptr;
}
 
#undef LOCTEXT_NAMESPACE

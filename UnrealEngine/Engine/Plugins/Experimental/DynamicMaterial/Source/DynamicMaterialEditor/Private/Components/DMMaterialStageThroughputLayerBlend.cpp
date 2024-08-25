// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "Components/DMMaterialStageThroughputLayerBlend.h"
#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialStage.h"
#include "Components/DMMaterialSubStage.h"
#include "Components/MaterialStageExpressions/DMMSETextureSample.h"
#include "Components/MaterialStageInputs/DMMSIExpression.h"
#include "Components/MaterialStageInputs/DMMSIValue.h"
#include "Components/MaterialValues/DMMaterialValueTexture.h"
#include "DMDefs.h"
#include "DMMaterialFunctionLibrary.h"
#include "DMPrivate.h"
#include "DynamicMaterialEditorSettings.h"
#include "Helpers/DMInputNodeBuilder.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionSceneTexture.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Model/DMMaterialBuildState.h"
#include "Model/DMMaterialBuildUtils.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
 
#define LOCTEXT_NAMESPACE "DMMaterialStageThroughputLayerBlend"

namespace UE::DynamicMaterialEditor::Private
{
	UMaterialFunctionInterface* GetAlphaBlend()
	{
		static UMaterialFunctionInterface* AlphaBlend = FDMMaterialFunctionLibrary::Get().GetFunction(
			"DM_AlphaBlend",
			TEXT("/Script/Engine.MaterialFunction'/DynamicMaterial/MaterialFunctions/MF_DM_AlphaBlend.MF_DM_AlphaBlend'")
		);

		return AlphaBlend;
	}
}
 
UDMMaterialStage* UDMMaterialStageThroughputLayerBlend::CreateStage(UDMMaterialLayerObject* InLayer)
{
		const FDMUpdateGuard Guard;
 
	UDMMaterialStage* NewStage = UDMMaterialStage::CreateMaterialStage(InLayer);
 
	UDMMaterialStageThroughputLayerBlend* NewLayerBlend = NewObject<UDMMaterialStageThroughputLayerBlend>(NewStage, NAME_None, RF_Transactional);
	check(NewLayerBlend);
 
	NewStage->SetSource(NewLayerBlend);
	NewStage->SetCanChangeSource(false);
 
	return NewStage;
}
 
UDMMaterialStageThroughputLayerBlend::UDMMaterialStageThroughputLayerBlend()
	: UDMMaterialStageThroughput(
		LOCTEXT("LayerBlend", "Layer Blend")
	)
{
	InputConnectors.Add({InputPreviousLayer, LOCTEXT("PreviousLayer", "Previous Layer"), EDMValueType::VT_Float_Any});
	InputConnectors.Add({InputBaseStage, LOCTEXT("BaseStage", "Base Stage"), EDMValueType::VT_Float_Any});
	InputConnectors.Add({InputMaskSource, LOCTEXT("MaskSource", "Mask Source"), EDMValueType::VT_Float1});
 
	OutputConnectors.Add({0, LOCTEXT("Blend", "Blend"), EDMValueType::VT_Float_Any});
 
	bInputRequired = true;
	bAllowNestedInputs = true;
	bPremultiplyAlpha = true;
	MaskChannelOverride = EAvaColorChannel::None;

	EditableProperties.Add(GET_MEMBER_NAME_CHECKED(UDMMaterialStageThroughputLayerBlend, MaskChannelOverride));
	EditableProperties.Add(GET_MEMBER_NAME_CHECKED(UDMMaterialStageThroughputLayerBlend, bPremultiplyAlpha));
 
	bBlockUpdate = false;
	bIsAlphaOnlyBlend = false;
}
 
void UDMMaterialStageThroughputLayerBlend::OnComponentAdded()
{
	Super::OnComponentAdded();
 
	if (!IsComponentValid())
	{
		return;
	}

	InitBlendStage();
 
	UpdateAlphaOnlyMaskStatus();
 
	if (bIsAlphaOnlyBlend)
	{
		UpdateAlphaOnlyMasks(EDMUpdateType::Structure);
	}
}
 
void UDMMaterialStageThroughputLayerBlend::AddDefaultInput(int32 InInputIndex) const
{
	if (!IsComponentValid())
	{
		return;
	}

	UDMMaterialStage* Stage = GetStage();
	check(Stage);
 
	const UDMMaterialLayerObject* Layer = Stage->GetLayer();
	check(Layer);
 
	switch (InInputIndex)
	{
		case InputPreviousLayer:
		case InputBaseStage:
			// Resolved internally
			break;
 
		case InputMaskSource:
		{
			UDMMaterialStageInputExpression* InputExpression = UDMMaterialStageInputExpression::ChangeStageInput_Expression(
				Stage,
				UDMMaterialStageExpressionTextureSample::StaticClass(),
				InInputIndex,
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
				0, // RGB pin
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL // Alpha channel
			);

			UDMMaterialSubStage* SubStage = InputExpression->GetSubStage();
			check(SubStage);

			UDMMaterialStageInputValue* InputValue = UDMMaterialStageInputValue::ChangeStageInput_NewLocalValue(
				SubStage,
				0, 
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
				EDMValueType::VT_Texture, 
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
			);

			check(InputValue);

			UDMMaterialValueTexture* InputTexture = Cast<UDMMaterialValueTexture>(InputValue->GetValue());
			check(InputTexture);

			InputTexture->SetDefaultValue(UDynamicMaterialEditorSettings::Get()->DefaultOpaqueTexture.LoadSynchronous());
			InputTexture->ApplyDefaultValue();
			break;
		}
 
		default:
			checkNoEntry();
			break;
	}
}
 
bool UDMMaterialStageThroughputLayerBlend::CanChangeInput(int32 InputIndex) const
{
	return (InputIndex == InputMaskSource);
}
 
bool UDMMaterialStageThroughputLayerBlend::IsInputVisible(int32 InputIndex) const
{
	if (!Super::IsInputVisible(InputIndex))
	{
		return false;
	}
 
	return (InputIndex == InputMaskSource);
}
 
void UDMMaterialStageThroughputLayerBlend::Update(EDMUpdateType InUpdateType)
{
	if (bBlockUpdate)
	{
		return;
	}

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
		UDMMaterialStage* Stage = GetStage();
		check(Stage);
 
		InputConnectors[1].Type = EDMValueType::VT_Float_Any;
		OutputConnectors[0].Type = EDMValueType::VT_Float_Any;
 
		if (const UDMMaterialLayerObject* Layer = Stage->GetLayer())
		{
			if (const UDMMaterialStageSource* LayerSource = Layer->GetStage(EDMMaterialLayerStage::Base)->GetSource())
			{
				const TArray<FDMMaterialStageConnector>& LayerSourceOutputConnectors = LayerSource->GetOutputConnectors();
				check(LayerSourceOutputConnectors.IsValidIndex(0));

				InputConnectors[InputBaseStage].Type = LayerSourceOutputConnectors[0].Type;
				OutputConnectors[0].Type = LayerSourceOutputConnectors[0].Type;
			}
		}
	}
 
	UpdateLinkedInputStage(InUpdateType);
 
	UpdateAlphaOnlyMasks(InUpdateType);

	Super::Update(InUpdateType);

	PullMaskChannelOverride();
}
 
int32 UDMMaterialStageThroughputLayerBlend::ResolveInput(const TSharedRef<FDMMaterialBuildState>& InBuildState, int32 InputIndex, FDMMaterialStageConnectorChannel& OutChannel, TArray<UMaterialExpression*>& OutExpressions) const
{
	switch (InputIndex)
	{
		case InputPreviousLayer:
		{
			const UDMMaterialStage* Stage = GetStage();
			check(Stage);

			const UDMMaterialLayerObject* Layer = Stage->GetLayer();
			check(Layer);

			EDMMaterialPropertyType StageProperty = Layer->GetMaterialProperty();
			check(StageProperty != EDMMaterialPropertyType::None);

			if (const UDMMaterialLayerObject* PreviousLayer = Layer->GetPreviousLayer(StageProperty, EDMMaterialLayerStage::Base))
			{
				PreviousLayer->GenerateExpressions(InBuildState);
				OutExpressions.Append(InBuildState->GetLayerExpressions(PreviousLayer));
				return 0;
			}

			UMaterialExpressionConstant3Vector* Constant3Vector = InBuildState->GetBuildUtils().CreateExpression<UMaterialExpressionConstant3Vector>(UE_DM_NodeComment_Default);
			check(Constant3Vector);

			Constant3Vector->Constant = FLinearColor::Black;

			OutExpressions.Add(Constant3Vector);

			return 0;
		}
 
		case InputBaseStage:
		{
			const UDMMaterialStage* Stage = GetStage();
			check(Stage);
 
			const UDMMaterialLayerObject* Layer = Stage->GetLayer();
			check(Layer);

			UDMMaterialStage* BaseStage = Layer->GetStage(EDMMaterialLayerStage::Base, /* Enabled Only */ true);
			check(BaseStage);
 
			BaseStage->GenerateExpressions(InBuildState);
			OutExpressions.Append(InBuildState->GetStageExpressions(BaseStage));
 
			return OutChannel.OutputIndex;
		}
 
		case InputMaskSource:
			return ResolveMaskInput(InBuildState, InputIndex, OutChannel, OutExpressions);			
 
		default:
			checkNoEntry();
			return OutChannel.OutputIndex;
	}
}
 
void UDMMaterialStageThroughputLayerBlend::GenerateExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState) const
{
	if (!IsComponentValid() || !IsComponentAdded())
	{
		return;
	}

	if (InBuildState->HasStageSource(this))
	{
		return;
	}

	if (InBuildState->IsPreviewMaterial())
	{
		GeneratePreviewExpressions(InBuildState);
	}
	else
	{
		GenerateMainExpressions(InBuildState);
	}
}

void UDMMaterialStageThroughputLayerBlend::PostLoad()
{
	Super::PostLoad();

	InitBlendStage();

	PullMaskChannelOverride();
}

void UDMMaterialStageThroughputLayerBlend::PostEditImport()
{
	Super::PostEditImport();

	InitBlendStage();
}

void UDMMaterialStageThroughputLayerBlend::InitBlendStage()
{
	UDMMaterialStage* Stage = GetStage();
	check(Stage);

	Stage->GetOnUpdate().AddUObject(this, &UDMMaterialStageThroughputLayerBlend::OnStageUpdated);
}

void UDMMaterialStageThroughputLayerBlend::GenerateMainExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState) const
{
	using namespace UE::DynamicMaterialEditor::Private;

	check(GetAlphaBlend());

	UMaterialExpressionMaterialFunctionCall* FunctionCall = InBuildState->GetBuildUtils().CreateExpression<UMaterialExpressionMaterialFunctionCall>(UE_DM_NodeComment_Default);;
	FunctionCall->SetMaterialFunction(GetAlphaBlend());
	FunctionCall->UpdateFromFunctionResource();

	TArray<UMaterialExpression*> Expressions = {FunctionCall};

	bool bIsOpacitySlot = false;
	UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = nullptr;
	UDMMaterialSlot* RGBSlot = nullptr;
	EDMMaterialPropertyType RGBProperty = EDMMaterialPropertyType::None;

	if (UDMMaterialStage* Stage = GetStage())
	{
		if (const UDMMaterialLayerObject* Layer = Stage->GetLayer())
		{
			switch (Layer->GetMaterialProperty())
			{
				case EDMMaterialPropertyType::Opacity:
				case EDMMaterialPropertyType::OpacityMask:
					bIsOpacitySlot = true;
					break;

				default:
					bIsOpacitySlot = false;
					break;
			}

			if (bIsOpacitySlot)
			{
				if (UDMMaterialSlot* Slot = Layer->GetSlot())
				{
					ModelEditorOnlyData = Slot->GetMaterialModelEditorOnlyData();

					if (ModelEditorOnlyData)
					{
						switch (ModelEditorOnlyData->GetShadingModel())
						{
							case EDMMaterialShadingModel::DefaultLit:
								RGBProperty = EDMMaterialPropertyType::BaseColor;
								RGBSlot = ModelEditorOnlyData->GetSlotForMaterialProperty(RGBProperty);
								break;

							case EDMMaterialShadingModel::Unlit:
								RGBProperty = EDMMaterialPropertyType::EmissiveColor;
								RGBSlot = ModelEditorOnlyData->GetSlotForMaterialProperty(RGBProperty);
								break;
						}
					}
				}
			}
		}
	}

	if (bIsOpacitySlot && ModelEditorOnlyData && RGBSlot && RGBProperty != EDMMaterialPropertyType::None)
	{
		UMaterialExpression* LastExpression = nullptr;
		int32 OutputIndex;
		int32 OutputChannel;
		ModelEditorOnlyData->GenerateOpacityExpressions(InBuildState, RGBSlot, RGBProperty, LastExpression, OutputIndex, OutputChannel);

		if (LastExpression)
		{
			Expressions.Add(LastExpression);

			UMaterialExpressionMultiply* Multiply = InBuildState->GetBuildUtils().CreateExpression<UMaterialExpressionMultiply>(UE_DM_NodeComment_Default);
			Multiply->A.Expression = LastExpression;
			Multiply->A.OutputIndex = OutputIndex;
			SetMask(Multiply->A, LastExpression->GetOutputs()[OutputIndex], OutputChannel);

			Multiply->B.Expression = FunctionCall;
			Multiply->B.OutputIndex = 0;
			SetMask(Multiply->B, FunctionCall->GetOutputs()[0], FDMMaterialStageConnectorChannel::WHOLE_CHANNEL);

			Expressions.Add(Multiply);
		}
	}

	InBuildState->AddStageSourceExpressions(this, Expressions);
}

void UDMMaterialStageThroughputLayerBlend::PostEditorDuplicate(UDynamicMaterialModel* InMaterialModel, UDMMaterialComponent* InParent)
{
	Super::PostEditorDuplicate(InMaterialModel, InParent);

	InitBlendStage();
}

bool UDMMaterialStageThroughputLayerBlend::IsPropertyVisible(FName InProperty) const
{
	static const FName MaskChannelName = GET_MEMBER_NAME_CHECKED(UDMMaterialStageThroughputLayerBlend, MaskChannelOverride);

	if (InProperty == MaskChannelName)
	{
		return CanUseMaskChannelOverride();
	}
		
	return Super::IsPropertyVisible(InProperty);
}

void UDMMaterialStageThroughputLayerBlend::GeneratePreviewExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState) const
{
	UDMMaterialStage* Stage = GetStage();
	check(Stage);

	const TArray<UDMMaterialStageInput*>& StageInputs = Stage->GetInputs();
	const TArray<FDMMaterialStageConnection>& StageConnections = Stage->GetInputConnectionMap();

	if (StageConnections.IsValidIndex(InputMaskSource) && StageConnections[InputMaskSource].Channels.IsEmpty() == false)
	{
		int32 StageInputIdx = StageConnections[InputMaskSource].Channels[0].SourceIndex - FDMMaterialStageConnectorChannel::FIRST_STAGE_INPUT;

		if (StageInputs.IsValidIndex(StageInputIdx))
		{
			StageInputs[StageInputIdx]->GenerateExpressions(InBuildState);
			TArray<UMaterialExpression*> MaskInputExpressions = InBuildState->GetStageSourceExpressions(StageInputs[StageInputIdx]);
			check(!MaskInputExpressions.IsEmpty());

			if (UDMMaterialLayerObject* Layer = Stage->GetLayer())
			{
				int32 Channel = FDMMaterialStageConnectorChannel::WHOLE_CHANNEL;
				int32 OutputIndex = 0;
				Layer->ApplyEffects(InBuildState, Stage, MaskInputExpressions, Channel, OutputIndex);
			}

			InBuildState->AddStageSourceExpressions(this, MaskInputExpressions);
		}
	}
}

void UDMMaterialStageThroughputLayerBlend::UpdateLinkedInputStage(EDMUpdateType InUpdateType)
{
	UDMMaterialStage* MaskStage = GetTypedParent<UDMMaterialStage>(/* bInAllowSubclasses */ true);

	if (!MaskStage)
	{
		return;
	}

	if (UDMMaterialSubStage* SubStage = Cast<UDMMaterialSubStage>(MaskStage))
	{
		MaskStage = SubStage->GetParentMostStage();
	}

	const UDMMaterialLayerObject* Layer = MaskStage->GetLayer();

	if (!Layer)
	{
		return;
	}

	if (Layer->GetStageType(MaskStage) != EDMMaterialLayerStage::Mask)
	{
		return;
	}

	UDMMaterialStage* BaseStage = Layer->GetStage(EDMMaterialLayerStage::Base);

	if (!BaseStage)
	{
		return;
	}

	bool bInputInCommon = false;

	for (UDMMaterialStageInput* MaskInput : MaskStage->GetInputs())
	{
		for (UDMMaterialStageInput* BaseInput : BaseStage->GetInputs())
		{
			if (MaskInput == BaseInput)
			{
				bInputInCommon = true;
				break;
			}

			if (UDMMaterialStageInputValue* MaskInputValue = Cast<UDMMaterialStageInputValue>(MaskInput))
			{
				if (UDMMaterialStageInputValue* BaseInputValue = Cast<UDMMaterialStageInputValue>(BaseInput))
				{
					if (MaskInputValue->GetValue() && MaskInputValue->GetValue() == BaseInputValue->GetValue())
					{
						bInputInCommon = true;
						break;
					}
				}
			}
		}

		if (bInputInCommon)
		{
			break;
		}
	}

	if (bInputInCommon)
	{
		bBlockUpdate = true;
		BaseStage->Update(InUpdateType);
		bBlockUpdate = false;
	}
}

FText UDMMaterialStageThroughputLayerBlend::GetStageDescription() const
{
	if (UDMMaterialStage* Stage = GetStage())
	{
		const TArray<FDMMaterialStageConnection>& InputMap = Stage->GetInputConnectionMap();

		if (InputMap.IsValidIndex(UDMMaterialStageThroughputLayerBlend::InputMaskSource)
			&& InputMap[UDMMaterialStageThroughputLayerBlend::InputMaskSource].Channels.IsValidIndex(0))
		{
			TArray<UDMMaterialStageInput*> Inputs = Stage->GetInputs();
			const int32 StageInputIdx = InputMap[UDMMaterialStageThroughputLayerBlend::InputMaskSource].Channels[0].SourceIndex - FDMMaterialStageConnectorChannel::FIRST_STAGE_INPUT;

			if (Inputs.IsValidIndex(StageInputIdx))
			{
				return Inputs[StageInputIdx]->GetComponentDescription();
			}
		}
	}

	return Super::GetComponentDescription();
}

bool UDMMaterialStageThroughputLayerBlend::UpdateStagePreviewMaterial(UDMMaterialStage* InStage, UMaterial* InPreviewMaterial, 
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

	if (!InputConnectionMap.IsValidIndex(InputMaskSource)
		|| InputConnectionMap[InputMaskSource].Channels.Num() != 1)
	{
		return false;
	}

	TSharedRef<FDMMaterialBuildState> BuildState = ModelEditorOnlyData->CreateBuildState(InPreviewMaterial);
	BuildState->SetPreviewMaterial();

	UDMMaterialStageSource* PreviewSource = GetInputMask();

	PreviewSource->GenerateExpressions(BuildState);
	TArray<UMaterialExpression*> SourceExpressions = BuildState->GetStageSourceExpressions(PreviewSource);

	if (SourceExpressions.IsEmpty())
	{
		return false;
	}

	UMaterialExpression* ComponentMask = SourceExpressions.Last();
	OutputIndex = InputConnectionMap[InputMaskSource].Channels[0].OutputIndex;
	int32 Channel;

	{
		const UDMMaterialStageSource* Source = nullptr;

		for (const TPair<const UDMMaterialStageSource*, TArray<UMaterialExpression*>>& Pair : BuildState->GetStageSourceMap())
		{
			if (Pair.Value.IsEmpty())
			{
				continue;
			}

			if (Pair.Value.Last() == ComponentMask)
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

	// Pick the first selected channel - this is always a single channel.
	if (InputConnectionMap[InputMaskSource].Channels[0].OutputChannel & FDMMaterialStageConnectorChannel::FIRST_CHANNEL)
	{
		Channel = FDMMaterialStageConnectorChannel::FIRST_CHANNEL;
	}
	else if (InputConnectionMap[InputMaskSource].Channels[0].OutputChannel & FDMMaterialStageConnectorChannel::SECOND_CHANNEL)
	{
		Channel = FDMMaterialStageConnectorChannel::SECOND_CHANNEL;
	}
	else if (InputConnectionMap[InputMaskSource].Channels[0].OutputChannel & FDMMaterialStageConnectorChannel::THIRD_CHANNEL)
	{
		Channel = FDMMaterialStageConnectorChannel::THIRD_CHANNEL;
	}
	else if (InputConnectionMap[InputMaskSource].Channels[0].OutputChannel & FDMMaterialStageConnectorChannel::FOURTH_CHANNEL)
	{
		Channel = FDMMaterialStageConnectorChannel::FOURTH_CHANNEL;
	}
	else
	{
		Channel = FDMMaterialStageConnectorChannel::FIRST_CHANNEL;
	}

	ComponentMask = BuildState->GetBuildUtils().CreateExpressionBitMask(
		ComponentMask,
		OutputIndex,
		Channel
	);

	SourceExpressions.Add(ComponentMask);

	OutputIndex = 0;
	Channel = FDMMaterialStageConnectorChannel::WHOLE_CHANNEL;

	Layer->ApplyEffects(BuildState, InStage, SourceExpressions, Channel, OutputIndex);

	OutMaterialExpression = SourceExpressions.Last();

	return true;
}

void UDMMaterialStageThroughputLayerBlend::NotifyPostChange(const FPropertyChangedEvent& InPropertyChangedEvent, FEditPropertyChain* InPropertyThatChanged)
{
	if (!IsComponentValid())
	{
		return;
	}

	static const FName MaskChannelName = GET_MEMBER_NAME_CHECKED(UDMMaterialStageThroughputLayerBlend, MaskChannelOverride);

	if (InPropertyChangedEvent.GetPropertyName() == MaskChannelName)
	{
		PushMaskChannelOverride();
	}
	else
	{
		PullMaskChannelOverride();
	}

	Super::NotifyPostChange(InPropertyChangedEvent, InPropertyThatChanged);
}

void UDMMaterialStageThroughputLayerBlend::ConnectOutputToInput(const TSharedRef<FDMMaterialBuildState>& InBuildState, int32 InInputIndex, 
	UMaterialExpression* InSourceExpression, int32 InSourceOutputIndex, int32 InSourceOutputChannel)
{
	check(InSourceExpression);
	check(InSourceExpression->GetOutputs().IsValidIndex(InSourceOutputIndex));
	check(InInputIndex >= InputPreviousLayer && InInputIndex <= InputMaskSource);
 
	const TArray<UMaterialExpression*>& StageSourceExpressions = InBuildState->GetStageSourceExpressions(this);
	check(!StageSourceExpressions.IsEmpty());

	using namespace UE::DynamicMaterialEditor::Private;

	check(GetAlphaBlend());

	TArray<FFunctionExpressionInput> Inputs;
	TArray<FFunctionExpressionOutput> Outputs;
	GetAlphaBlend()->GetInputsAndOutputs(Inputs, Outputs);

	int32 InputPreviousIndex = INDEX_NONE;
	int32 InputLayerIndex = INDEX_NONE;
	int32 InputMaskIndex = INDEX_NONE;

	// Could go by name here, but this is more bug prone.
	for (int32 InputIdx = 0; InputIdx < Inputs.Num(); ++InputIdx)
	{
		if (Inputs[InputIdx].ExpressionInput->InputType == FunctionInput_Vector3)
		{
			if (InputPreviousIndex == INDEX_NONE)
			{
				InputPreviousIndex = InputIdx;
			}
			else if (InputLayerIndex == INDEX_NONE)
			{
				InputLayerIndex = InputIdx;
			}
		}
		else if (Inputs[InputIdx].ExpressionInput->InputType == FunctionInput_Scalar)
		{
			if (InputMaskIndex == INDEX_NONE)
			{
				InputMaskIndex = InputIdx;
			}
		}
	}
 
	switch (InInputIndex)
	{
		case InputPreviousLayer:
			ConnectOutputToInput_Internal(
				InBuildState, 
				StageSourceExpressions[0] /* FunctionCall */, 
				InputPreviousIndex, 
				InSourceExpression, 
				InSourceOutputIndex, 
				InSourceOutputChannel
			);
			break;
 
		case InputBaseStage:
			ConnectOutputToInput_Internal(
				InBuildState, 
				StageSourceExpressions[0] /* FunctionCall */, 
				InputLayerIndex, 
				InSourceExpression, 
				InSourceOutputIndex, 
				InSourceOutputChannel
			);
			break;
 
		case InputMaskSource:
			ConnectOutputToInput_Internal(
				InBuildState, 
				StageSourceExpressions[0] /* FunctionCall */, 
				InputMaskIndex, 
				InSourceExpression, 
				InSourceOutputIndex, 
				InSourceOutputChannel
			);
			break;
 
		default:
			checkNoEntry();
			break;
	}
}
 
void UDMMaterialStageThroughputLayerBlend::GetMaskOutput(const TSharedRef<FDMMaterialBuildState>& InBuildState, UMaterialExpression*& OutExpression,
	int32& OutOutputIndex, int32& OutOutputChannel) const
{
	FDMMaterialStageConnectorChannel Channel;
	TArray<UMaterialExpression*> Expressions;
 
	OutOutputIndex = ResolveInput(InBuildState, 2, Channel, Expressions);
	OutOutputChannel = Channel.OutputChannel;
	OutExpression = Expressions.Last();
}
 
void UDMMaterialStageThroughputLayerBlend::SetPremultiplyAlpha(bool bInValue)
{
	if (!IsComponentValid())
	{
		return;
	}

	if (bPremultiplyAlpha == bInValue)
	{
		return;
	}
 
	bPremultiplyAlpha = bInValue;
 
	Update(EDMUpdateType::Structure);
}
 
int32 UDMMaterialStageThroughputLayerBlend::ResolveMaskInput(const TSharedRef<FDMMaterialBuildState>& InBuildState, int32 InputIndex, 
	FDMMaterialStageConnectorChannel& OutChannel, TArray<UMaterialExpression*>& OutExpressions) const
{
	// Our baseline input. Could be a texture node or color value. Anything inputy.
	OutChannel.OutputIndex = Super::ResolveInput(InBuildState, InputIndex, OutChannel, OutExpressions);

	UDMMaterialStage* ThisStage = GetStage();
	check(ThisStage);
 
	const UDMMaterialLayerObject* ThisLayer = ThisStage->GetLayer();
	check(ThisLayer);
	check(ThisLayer->GetStage(EDMMaterialLayerStage::Mask));

	ThisLayer->ApplyEffects(
		InBuildState, 
		ThisStage, 
		OutExpressions, 
		OutChannel.OutputChannel, 
		OutChannel.OutputIndex
	);
		
	while (const UDMMaterialLayerObject* NextLayer = ThisLayer->GetNextLayer(ThisLayer->GetMaterialProperty(), EDMMaterialLayerStage::Mask))
	{
		UDMMaterialStage* NextMaskStage = NextLayer->GetStage(EDMMaterialLayerStage::Mask, /* Enabled Only */ true);

		// If the next layer has a base enabled, we stop scanning.
		// If the next layer's mask is disabled, we stop scanning.
		if (!NextMaskStage || NextLayer->GetStage(EDMMaterialLayerStage::Base, /* Enabled Only */ true))
		{
			break;
		}
 
		// The next stage is one which has its base disabled and its mask enabled, so we multiply (not max) the outputs.
		UDMMaterialStageThroughputLayerBlend* NextMaskBlend = Cast<UDMMaterialStageThroughputLayerBlend>(NextMaskStage->GetSource());
 
		if (!NextMaskBlend)
		{
			break;
		}
		
		// Resolve the input of the next blend layer mask channel
		FDMMaterialStageConnectorChannel NextOutChannel;
		TArray<UMaterialExpression*> NextOutExpressions;

		NextMaskBlend->ResolveInput(
			InBuildState, 
			InputIndex, 
			NextOutChannel, 
			NextOutExpressions
		);
 
		if (NextOutExpressions.IsEmpty())
		{
			break;
		}

		NextLayer->ApplyEffects(
			InBuildState, 
			NextMaskStage, 
			NextOutExpressions, 
			NextOutChannel.OutputChannel, 
			NextOutChannel.OutputIndex
		);

		UMaterialExpressionMultiply* AlphaMultiply = InBuildState->GetBuildUtils().CreateExpression<UMaterialExpressionMultiply>(UE_DM_NodeComment_Default);

		ConnectOutputToInput_Internal(
			InBuildState, 
			AlphaMultiply, 
			0, 
			OutExpressions.Last(), 
			OutChannel.OutputIndex, 
			OutChannel.OutputChannel
		);

		ConnectOutputToInput_Internal(
			InBuildState, 
			AlphaMultiply, 
			1, 
			NextOutExpressions.Last(), 
			NextOutChannel.OutputIndex, 
			NextOutChannel.OutputChannel
		);
 
		OutExpressions.Append(NextOutExpressions);
		OutExpressions.Add(AlphaMultiply);
 
		OutChannel.OutputIndex = 0;
		OutChannel.OutputChannel = FDMMaterialStageConnectorChannel::WHOLE_CHANNEL;

		ThisLayer = NextLayer;
	}

	return OutChannel.OutputIndex;
}
 
void UDMMaterialStageThroughputLayerBlend::UpdatePreviewMaterial(UMaterial* InPreviewMaterial /*= nullptr*/)
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

	const TArray<FDMMaterialStageConnection>& InputConnectionMap = Stage->GetInputConnectionMap();
	const TArray<UDMMaterialStageInput*>& StageInputs = Stage->GetInputs();
 
	TArray<UDMMaterialStageInput*> ChannelInputs;
 
	if (!InputConnectionMap.IsValidIndex(2))
	{
		return;
	}
 
	ChannelInputs.SetNum(InputConnectionMap[2].Channels.Num());
 
	for (int32 ChannelIdx = 0; ChannelIdx < InputConnectionMap[2].Channels.Num(); ++ChannelIdx)
	{
		if (InputConnectionMap[2].Channels[ChannelIdx].SourceIndex == FDMMaterialStageConnectorChannel::PREVIOUS_STAGE)
		{
			ChannelInputs[ChannelIdx] = nullptr;
			continue;
		}
 
		if (InputConnectionMap[2].Channels[ChannelIdx].SourceIndex >= FDMMaterialStageConnectorChannel::FIRST_STAGE_INPUT)
		{
			int32 StageInputIdx = InputConnectionMap[2].Channels[ChannelIdx].SourceIndex - FDMMaterialStageConnectorChannel::FIRST_STAGE_INPUT;
			ChannelInputs[ChannelIdx] = StageInputs[StageInputIdx];
			continue;
		}
	}
 
	if (ChannelInputs.IsEmpty())
	{
		return;
	}
 
	TSharedRef<FDMMaterialBuildState> BuildState = ModelEditorOnlyData->CreateBuildState(InPreviewMaterial);
	BuildState->SetPreviewMaterial();

	if (ChannelInputs.IsEmpty())
	{
		Stage->GenerateExpressions(BuildState);
		UMaterialExpression* StageExpression = BuildState->GetLastStageExpression(Stage);
 
		BuildState->GetBuildUtils().UpdatePreviewMaterial(StageExpression, 0, FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 32);
	}
	else
	{
		UE::DynamicMaterialEditor::Private::BuildExpressionInputs(BuildState, InputConnectionMap, {{2, ChannelInputs}});
	}
}
 
void UDMMaterialStageThroughputLayerBlend::UpdateAlphaOnlyMaskStatus()
{
	if (!IsComponentValid())
	{
		return;
	}

	UDMMaterialStage* Stage = GetStage();
	
	if (!Stage)
	{
		return;
	}
 
	const UDMMaterialLayerObject* Layer = Stage->GetLayer();
	
	if (!Layer)
	{
		return;
	}
 
	bIsAlphaOnlyBlend = (Layer->IsStageEnabled(EDMMaterialLayerStage::Base) == false && Layer->IsStageEnabled(EDMMaterialLayerStage::Mask) == true);
}
 
void UDMMaterialStageThroughputLayerBlend::OnStageUpdated(UDMMaterialComponent* InComponent, EDMUpdateType InUpdateType)
{
	if (bBlockUpdate)
	{
		return;
	}

	if (!IsComponentValid())
	{
		return;
	}

	if (InUpdateType == EDMUpdateType::Structure)
	{
		const bool bIsAlphaOnlyBlendNow = bIsAlphaOnlyBlend;
		UpdateAlphaOnlyMaskStatus();
 
		if (bIsAlphaOnlyBlendNow || bIsAlphaOnlyBlend)
		{
			UpdateAlphaOnlyMasks(InUpdateType);
		}
	}
}
 
void UDMMaterialStageThroughputLayerBlend::UpdateAlphaOnlyMasks(EDMUpdateType InUpdateType)
{
	if (!IsComponentValid())
	{
		return;
	}

	UDMMaterialStage* Stage = GetStage();
	
	if (!Stage)
	{
		return;
	}
 
	const UDMMaterialLayerObject* CurrentLayer = Stage->GetLayer();
	
	if (!CurrentLayer)
	{
		return;
	}

	UDMMaterialStage* MaskStage = CurrentLayer->GetStage(EDMMaterialLayerStage::Mask, /* Enabled Only */ true);
 
	if (CurrentLayer->GetStage(EDMMaterialLayerStage::Base, /* Enabled Only */ true) || !MaskStage)
	{
		return;
	}
 
	const UDMMaterialLayerObject* PreviousLayer = nullptr;
 
	while (true)
	{
		PreviousLayer = CurrentLayer->GetPreviousLayer(CurrentLayer->GetMaterialProperty(), EDMMaterialLayerStage::None);
 
		if (!PreviousLayer)
		{
			break;
		}

		MaskStage = PreviousLayer->GetStage(EDMMaterialLayerStage::Mask, /* Enabled Only */ true);

		// We're looking for a chain of alpha stages. If we find no alpha stage and an invalid
		// base stage, we're got an error.
		if (!PreviousLayer->GetStage(EDMMaterialLayerStage::Base) || !MaskStage)
		{
			break;
		}
 
		CurrentLayer = PreviousLayer;
 
		if (PreviousLayer->IsStageEnabled(EDMMaterialLayerStage::Base))
		{
			break;
		}
	}
 
	if (PreviousLayer)
	{
		// Calling this will inevitably recall this method when the chain updates.
		// Block this update.
		bBlockUpdate = true;
		PreviousLayer->GetStage(EDMMaterialLayerStage::Mask)->Update(InUpdateType);
		bBlockUpdate = false;
	}
}

UDMMaterialStageInput* UDMMaterialStageThroughputLayerBlend::GetInputMask() const
{
	if (UDMMaterialStage* Stage = GetStage())
	{
		const TArray<FDMMaterialStageConnection>& InputMap = Stage->GetInputConnectionMap();

		if (InputMap.IsValidIndex(InputMaskSource)
			&& InputMap[InputMaskSource].Channels.IsValidIndex(0))
		{
			TArray<UDMMaterialStageInput*> Inputs = Stage->GetInputs();
			int32 StageInputIdx = InputMap[InputMaskSource].Channels[0].SourceIndex - FDMMaterialStageConnectorChannel::FIRST_STAGE_INPUT;

			if (Stage->GetInputs().IsValidIndex(StageInputIdx))
			{
				return Stage->GetInputs()[StageInputIdx];
			}
		}
	}

	return nullptr;
}

EAvaColorChannel UDMMaterialStageThroughputLayerBlend::GetMaskChannelOverride() const
{
	if (CanUseMaskChannelOverride())
	{
		PullMaskChannelOverride();
		return MaskChannelOverride;
	}

	return EAvaColorChannel::None;
}

void UDMMaterialStageThroughputLayerBlend::SetMaskChannelOverride(EAvaColorChannel InMaskChannel)
{
	if (!CanUseMaskChannelOverride())
	{
		return;
	}

	if (GetMaskChannelOverride() == InMaskChannel)
	{
		return;
	}

	MaskChannelOverride = InMaskChannel;
	PushMaskChannelOverride();

	Update(EDMUpdateType::Structure);
}

bool UDMMaterialStageThroughputLayerBlend::CanUseMaskChannelOverride() const
{
	return GetDefaultMaskChannelOverrideOutputIndex() != INDEX_NONE;
}

int32 UDMMaterialStageThroughputLayerBlend::GetDefaultMaskChannelOverrideOutputIndex() const
{
	UDMMaterialStageInput* MaskInput = GetInputMask();

	if (!MaskInput)
	{
		return INDEX_NONE;
	}

	const TArray<FDMMaterialStageConnector>& MaskInputOutputConnectors = MaskInput->GetOutputConnectors();

	for (int32 Index = 0; Index < MaskInputOutputConnectors.Num(); ++Index)
	{
		if (UDMValueDefinitionLibrary::GetValueDefinition(MaskInputOutputConnectors[Index].Type).GetFloatCount() > 1)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

bool UDMMaterialStageThroughputLayerBlend::IsValidMaskChannelOverrideOutputIndex(int32 InIndex) const
{
	UDMMaterialStageInput* MaskInput = GetInputMask();

	if (!MaskInput)
	{
		return false;
	}

	const TArray<FDMMaterialStageConnector>& MaskInputOutputConnectors = MaskInput->GetOutputConnectors();

	if (!MaskInputOutputConnectors.IsValidIndex(InIndex))
	{
		return false;
	}

	return UDMValueDefinitionLibrary::GetValueDefinition(MaskInputOutputConnectors[InIndex].Type).GetFloatCount() > 1;
}

void UDMMaterialStageThroughputLayerBlend::PullMaskChannelOverride() const
{
	MaskChannelOverride = EAvaColorChannel::None;

	if (!CanUseMaskChannelOverride())
	{
		return;
	}

	UDMMaterialStage* Stage = GetStage();

	if (!Stage)
	{
		return;
	}

	const TArray<FDMMaterialStageConnection>& InputMap = Stage->GetInputConnectionMap();

	if (!InputMap.IsValidIndex(UDMMaterialStageThroughputLayerBlend::InputMaskSource)
		|| !InputMap[UDMMaterialStageThroughputLayerBlend::InputMaskSource].Channels.IsValidIndex(0))
	{
		return;
	}

	TArray<UDMMaterialStageInput*> Inputs = Stage->GetInputs();
	const FDMMaterialStageConnectorChannel& MaskConnectorChannel = InputMap[UDMMaterialStageThroughputLayerBlend::InputMaskSource].Channels[0];

	switch (MaskConnectorChannel.OutputChannel)
	{
		case FDMMaterialStageConnectorChannel::FIRST_CHANNEL:
			MaskChannelOverride = EAvaColorChannel::Red;
			break;

		case FDMMaterialStageConnectorChannel::SECOND_CHANNEL:
			MaskChannelOverride = EAvaColorChannel::Green;
			break;

		case FDMMaterialStageConnectorChannel::THIRD_CHANNEL:
			MaskChannelOverride = EAvaColorChannel::Blue;
			break;

		case FDMMaterialStageConnectorChannel::FOURTH_CHANNEL:
			MaskChannelOverride = EAvaColorChannel::Alpha;
			break;

		default:
			// Do nothing
			break;
	}
}

void UDMMaterialStageThroughputLayerBlend::PushMaskChannelOverride()
{
	UDMMaterialStage* Stage = GetStage();

	if (!Stage)
	{
		return;
	}

	if (!CanUseMaskChannelOverride())
	{
		return;
	}

	const TArray<FDMMaterialStageConnection>& InputMap = Stage->GetInputConnectionMap();

	if (!InputMap.IsValidIndex(UDMMaterialStageThroughputLayerBlend::InputMaskSource)
		|| !InputMap[UDMMaterialStageThroughputLayerBlend::InputMaskSource].Channels.IsValidIndex(0))
	{
		return;
	}

	TArray<UDMMaterialStageInput*> Inputs = Stage->GetInputs();
	const FDMMaterialStageConnectorChannel& MaskConnectorChannel = InputMap[UDMMaterialStageThroughputLayerBlend::InputMaskSource].Channels[0];
	const int32 MaskInputIdx = MaskConnectorChannel.SourceIndex - FDMMaterialStageConnectorChannel::FIRST_STAGE_INPUT;

	if (!Inputs.IsValidIndex(MaskInputIdx))
	{
		return;
	}

	const int32 OutputIndex = IsValidMaskChannelOverrideOutputIndex(MaskConnectorChannel.OutputIndex)
		? MaskConnectorChannel.OutputIndex
		: GetDefaultMaskChannelOverrideOutputIndex();

	int32 OutputChannel;

	switch (MaskChannelOverride)
	{
		case EAvaColorChannel::Red:
			OutputChannel = FDMMaterialStageConnectorChannel::FIRST_CHANNEL;
			break;

		case EAvaColorChannel::Green:
			OutputChannel = FDMMaterialStageConnectorChannel::SECOND_CHANNEL;
			break;

		case EAvaColorChannel::Blue:
			OutputChannel = FDMMaterialStageConnectorChannel::THIRD_CHANNEL;
			break;

		case EAvaColorChannel::Alpha:
			OutputChannel = FDMMaterialStageConnectorChannel::FOURTH_CHANNEL;
			break;

		default:
			OutputChannel = FDMMaterialStageConnectorChannel::WHOLE_CHANNEL;
			break;
	}

	Stage->UpdateInputMap(InputMaskSource, MaskConnectorChannel.SourceIndex, FDMMaterialStageConnectorChannel::WHOLE_CHANNEL,
		OutputIndex, OutputChannel, MaskConnectorChannel.MaterialProperty);
}

#undef LOCTEXT_NAMESPACE

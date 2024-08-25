// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageInputs/DMMSITextureUV.h"
#include "Components/DMMaterialEffect.h"
#include "Components/DMMaterialEffectStack.h"
#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialParameter.h"
#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialStage.h"
#include "Components/DMMaterialStageExpression.h"
#include "Components/DMTextureUV.h"
#include "DMComponentPath.h"
#include "DMDefs.h"
#include "DMMaterialFunctionLibrary.h"
#include "DynamicMaterialEditorModule.h"
#include "DynamicMaterialModule.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Model/DMMaterialBuildState.h"
#include "Model/DMMaterialBuildUtils.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageInputTextureUV"

const FString UDMMaterialStageInputTextureUV::TextureUVPathToken = FString(TEXT("TextureUV"));

UDMMaterialStage* UDMMaterialStageInputTextureUV::CreateStage(UDynamicMaterialModel* InMaterialModel, UDMMaterialLayerObject* InLayer)
{
	const FDMUpdateGuard Guard;

	UDMMaterialStage* NewStage = UDMMaterialStage::CreateMaterialStage(InLayer);

	UDMMaterialStageInputTextureUV* InputTextureUV = NewObject<UDMMaterialStageInputTextureUV>(NewStage, NAME_None, RF_Transactional);
	check(InputTextureUV);

	InputTextureUV->Init(InMaterialModel);

	NewStage->SetSource(InputTextureUV);

	return NewStage;
}

UDMMaterialStageInputTextureUV* UDMMaterialStageInputTextureUV::ChangeStageSource_UV(UDMMaterialStage* InStage, bool bInDoUpdate)
{
	check(InStage);

	if (!InStage->CanChangeSource())
	{
		return nullptr;
	}

	UDMMaterialLayerObject* Layer = InStage->GetLayer();
	check(Layer);

	UDMMaterialSlot* Slot = Layer->GetSlot();
	check(Slot);

	UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = Slot->GetMaterialModelEditorOnlyData();
	check(ModelEditorOnlyData);

	UDynamicMaterialModel* MaterialModel = ModelEditorOnlyData->GetMaterialModel();
	check(MaterialModel);

	UDMMaterialStageInputTextureUV* InputTextureUV = InStage->ChangeSource<UDMMaterialStageInputTextureUV>(
		[MaterialModel](UDMMaterialStage* InStage, UDMMaterialStageSource* InNewSource)
		{
			const FDMUpdateGuard Guard;
			CastChecked<UDMMaterialStageInputTextureUV>(InNewSource)->Init(MaterialModel);;
		});

	return InputTextureUV;
}

UDMMaterialStageInputTextureUV* UDMMaterialStageInputTextureUV::ChangeStageInput_UV(UDMMaterialStage* InStage, int32 InInputIdx,
	int32 InInputChannel, int32 InOutputChannel)
{
	check(InStage);

	UDMMaterialStageSource* Source = InStage->GetSource();
	check(Source);

	UDMMaterialLayerObject* Layer = InStage->GetLayer();
	check(Layer);

	UDMMaterialSlot* Slot = Layer->GetSlot();
	check(Slot);

	UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = Slot->GetMaterialModelEditorOnlyData();
	check(ModelEditorOnlyData);

	UDynamicMaterialModel* MaterialModel = ModelEditorOnlyData->GetMaterialModel();
	check(MaterialModel);

	UDMMaterialStageInputTextureUV* NewInputTextureUV = InStage->ChangeInput<UDMMaterialStageInputTextureUV>(
		InInputIdx, InInputChannel, 0, InOutputChannel, 
		[MaterialModel](UDMMaterialStage* InStage, UDMMaterialStageInput* InNewInput)
		{
			const FDMUpdateGuard Guard;
			CastChecked<UDMMaterialStageInputTextureUV>(InNewInput)->Init(MaterialModel);
		}
	);

	return NewInputTextureUV;
}

FText UDMMaterialStageInputTextureUV::GetComponentDescription() const
{
	return LOCTEXT("TexUV", "Tex UV");
}

FText UDMMaterialStageInputTextureUV::GetChannelDescription(const FDMMaterialStageConnectorChannel& Channel)
{
	return LOCTEXT("TextureUV", "Texture UV");
}

void UDMMaterialStageInputTextureUV::Init(UDynamicMaterialModel* InMaterialModel)
{
	check(InMaterialModel);

	TextureUV = UDMTextureUV::CreateTextureUV(InMaterialModel);
	InitTextureUV();
}

void UDMMaterialStageInputTextureUV::GenerateExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState) const
{
	if (!IsComponentValid() || !IsComponentAdded())
	{
		return;
	}

	if (InBuildState->HasStageSource(this))
	{
		return;
	}

	TArray<UMaterialExpression*> Expressions = CreateTextureUVExpressions(InBuildState, TextureUV);

	AddEffects(InBuildState, Expressions);

	InBuildState->AddStageSourceExpressions(this, Expressions);
}

bool UDMMaterialStageInputTextureUV::Modify(bool bInAlwaysMarkDirty)
{
	const bool bSaved = Super::Modify(bInAlwaysMarkDirty);

	if (TextureUV)
	{
		TextureUV->Modify(bInAlwaysMarkDirty);
	}

	return bSaved;
}

void UDMMaterialStageInputTextureUV::PostLoad()
{
	const bool bComponentValid = IsComponentValid();

	if (bComponentValid)
	{
		if (FDynamicMaterialModule::AreUObjectsSafe() && !TextureUV)
		{
			if (UDMMaterialStage* Stage = GetStage())
			{
				if (UDMMaterialLayerObject* Layer = Stage->GetLayer())
				{
					if (UDMMaterialSlot* Slot = Layer->GetSlot())
					{
						if (UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = Slot->GetMaterialModelEditorOnlyData())
						{
							if (UDynamicMaterialModel* MaterialModel = ModelEditorOnlyData->GetMaterialModel())
							{
								Init(MaterialModel);

								if (IsComponentAdded())
								{
									TextureUV->SetComponentState(EDMComponentLifetimeState::Added);
								}
							}
						}
					}
				}
			}
		}
	}

	Super::PostLoad();

	if (bComponentValid)
	{
		InitTextureUV();
	}
}

void UDMMaterialStageInputTextureUV::PostEditImport()
{
	Super::PostEditImport();

	if (!IsComponentValid())
	{
		return;
	}

	InitTextureUV();
}

void UDMMaterialStageInputTextureUV::PostEditorDuplicate(UDynamicMaterialModel* InMaterialModel,
	UDMMaterialComponent* InParent)
{
	Super::PostEditorDuplicate(InMaterialModel, InParent);

	if (TextureUV)
	{
		if (GUndo)
		{
			TextureUV->Modify();
		}

		TextureUV->PostEditorDuplicate(InMaterialModel, this);
	}

	InitTextureUV();
}

UDMMaterialStageInputTextureUV::UDMMaterialStageInputTextureUV()
{
	EditableProperties.Add(GET_MEMBER_NAME_CHECKED(UDMMaterialStageInputTextureUV, TextureUV));

	UpdateOutputConnectors();
}

void UDMMaterialStageInputTextureUV::UpdateOutputConnectors()
{
	if (!IsComponentValid())
	{
		return;
	}

	OutputConnectors.Empty();
	OutputConnectors.Add({0, LOCTEXT("UV", "UV"), EDMValueType::VT_Float2});
}

void UDMMaterialStageInputTextureUV::OnComponentAdded()
{
	if (!IsComponentValid())
	{
		return;
	}

	Super::OnComponentAdded();

	if (TextureUV)
	{
		if (GUndo)
		{
			TextureUV->Modify();
		}

		TextureUV->SetComponentState(EDMComponentLifetimeState::Added);
	}
}

void UDMMaterialStageInputTextureUV::OnComponentRemoved()
{
	Super::OnComponentRemoved();

	if (TextureUV)
	{
		if (GUndo)
		{
			TextureUV->Modify();
		}

		TextureUV->SetComponentState(EDMComponentLifetimeState::Removed);
	}
}

UDMMaterialComponent* UDMMaterialStageInputTextureUV::GetSubComponentByPath(FDMComponentPath& InPath,
	const FDMComponentPathSegment& InPathSegment) const
{
	if (InPathSegment.GetToken() == TextureUVPathToken)
	{
		return TextureUV;
	}

	return Super::GetSubComponentByPath(InPath, InPathSegment);
}

void UDMMaterialStageInputTextureUV::OnTextureUVUpdated(UDMMaterialComponent* InComponent, EDMUpdateType InUpdateType)
{
	if (!IsComponentValid())
	{
		return;
	}

	if (InComponent == TextureUV && InUpdateType == EDMUpdateType::Structure)
	{
		Update(InUpdateType);
	}
}

UMaterialExpressionScalarParameter* UDMMaterialStageInputTextureUV::CreateScalarParameter(const TSharedRef<FDMMaterialBuildState>& InBuildState, FName InParamName, float InValue)
{
	UMaterialExpressionScalarParameter* NewExpression = InBuildState->GetBuildUtils().CreateExpressionParameter<UMaterialExpressionScalarParameter>(InParamName, UE_DM_NodeComment_Default);
	check(NewExpression);

	NewExpression->DefaultValue = InValue;

	return NewExpression;
}

TArray<UMaterialExpression*> UDMMaterialStageInputTextureUV::CreateTextureUVExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState,
	UDMTextureUV* InTextureUV)
{
	if (InBuildState->IsIgnoringUVs())
	{
		UMaterialExpression* UVSourceExpression = InBuildState->GetBuildUtils().CreateExpression<UMaterialExpressionTextureCoordinate>(UE_DM_NodeComment_Default);

		return {UVSourceExpression};
	}

	check(IsValid(InTextureUV));

	static const FString MaterialFunc_Name_TextureUV_Mirror_None = "MaterialFunction'/DynamicMaterial/MaterialFunctions/MF_DM_TextureUV.MF_DM_TextureUV'";
	static const FString MaterialFunc_Name_TextureUV_Mirror_X    = "MaterialFunction'/DynamicMaterial/MaterialFunctions/MF_DM_TextureUV_Mirror_X.MF_DM_TextureUV_Mirror_X'";
	static const FString MaterialFunc_Name_TextureUV_Mirror_Y    = "MaterialFunction'/DynamicMaterial/MaterialFunctions/MF_DM_TextureUV_Mirror_Y.MF_DM_TextureUV_Mirror_Y'";
	static const FString MaterialFunc_Name_TextureUV_Mirror_XY   = "MaterialFunction'/DynamicMaterial/MaterialFunctions/MF_DM_TextureUV_Mirror_XY.MF_DM_TextureUV_Mirror_XY'";

	using namespace UE::DynamicMaterial;

	UMaterialExpressionMaterialFunctionCall* TextureUVFunc = nullptr;

	if (InTextureUV->GetMirrorOnX() == false && InTextureUV->GetMirrorOnY() == false)
	{
		TextureUVFunc = FDMMaterialFunctionLibrary::Get().MakeExpression(
			InBuildState->GetDynamicMaterial(),
			"MF_DM_TextureUVFunc",
			MaterialFunc_Name_TextureUV_Mirror_None,
			UE_DM_NodeComment_Default
		);
	}
	// GetMirrorOnX() == true
	else if (InTextureUV->GetMirrorOnY() == false)
	{
		TextureUVFunc = FDMMaterialFunctionLibrary::Get().MakeExpression(
			InBuildState->GetDynamicMaterial(),
			"MF_DM_TextureUV_Mirror_X",
			MaterialFunc_Name_TextureUV_Mirror_X,
			UE_DM_NodeComment_Default
		);
	}
	// GetMirrorOnY() == true
	else if (InTextureUV->GetMirrorOnX() == false)
	{
		TextureUVFunc = FDMMaterialFunctionLibrary::Get().MakeExpression(
			InBuildState->GetDynamicMaterial(),
			"MF_DM_TextureUV_Mirror_Y",
			MaterialFunc_Name_TextureUV_Mirror_Y,
			UE_DM_NodeComment_Default
		);
	}
	// GetMirrorOnX() == true && // GetMirrorOnY() == true
	else
	{
		TextureUVFunc = FDMMaterialFunctionLibrary::Get().MakeExpression(
			InBuildState->GetDynamicMaterial(),
			"MF_DM_TextureUV_Mirror_XY",
			MaterialFunc_Name_TextureUV_Mirror_XY,
			UE_DM_NodeComment_Default
		);
	}

	TMap<FName, int32> NameToInputIndex;
	TArrayView<FExpressionInput*> FuncInputs = TextureUVFunc->GetInputsView();

	for (int32 InputIdx = 0; InputIdx < FuncInputs.Num(); ++InputIdx)
	{
		NameToInputIndex.Emplace(FuncInputs[InputIdx]->InputName, InputIdx);
	}

	// Output nodes
	TArray<UMaterialExpression*> Nodes;
	InTextureUV->MaterialNodesCreated.Empty();

	// UV Source
	static const FName UVInputName = TEXT("UV");
	check(NameToInputIndex.Contains(UVInputName));
	TSubclassOf<UMaterialExpression> UVSourceClass = nullptr;

	switch (InTextureUV->GetUVSource())
	{
		case EDMUVSource::Texture:
			UVSourceClass = UMaterialExpressionTextureCoordinate::StaticClass();
			break;

		case EDMUVSource::ScreenPosition:
			UVSourceClass = UDMMaterialStageExpression::FindClass(TEXT("MaterialExpressionScreenPosition"));
			break;

		case EDMUVSource::WorldPosition:
			UVSourceClass = UMaterialExpressionWorldPosition::StaticClass();
			break;

		default:
			checkNoEntry();
			break;
	}

	check(UVSourceClass.Get());
	UMaterialExpression* UVSourceNode = InBuildState->GetBuildUtils().CreateExpression(UVSourceClass.Get(), UE_DM_NodeComment_Default);

	Nodes.Add(UVSourceNode);

	if (InTextureUV->GetUVSource() == EDMUVSource::WorldPosition)
	{
		UMaterialExpression* UVSourceNodeMask = InBuildState->GetBuildUtils().CreateExpressionBitMask(UVSourceNode, 0,
			FDMMaterialStageConnectorChannel::SECOND_CHANNEL + FDMMaterialStageConnectorChannel::THIRD_CHANNEL); // Y and Z

		Nodes.Add(UVSourceNodeMask);
	}

	Nodes.Last()->ConnectExpression(TextureUVFunc->GetInput(NameToInputIndex[UVInputName]), 0);

	auto CreateParameterExpression = [InTextureUV, &InBuildState, &NameToInputIndex, TextureUVFunc, &Nodes]
		(int32 ParamGroupId, int32 ParamId, FName InputName, float DefaultValue)
		{
			check(InTextureUV->MaterialParameters.Contains(ParamId));
			check(NameToInputIndex.Contains(InputName));

			UMaterialExpressionScalarParameter* ParamNode = CreateScalarParameter(InBuildState, InTextureUV->MaterialParameters[ParamId]->GetParameterName());
			InTextureUV->MaterialNodesCreated.FindOrAdd(ParamGroupId, true);

			ParamNode->DefaultValue = DefaultValue;
			ParamNode->ConnectExpression(TextureUVFunc->GetInput(NameToInputIndex[InputName]), 0);

			Nodes.Add(ParamNode);
		};

	CreateParameterExpression(ParamID::Offset,   ParamID::OffsetX,  TEXT("OffsetX"),  InTextureUV->GetOffset().X);
	CreateParameterExpression(ParamID::Offset,   ParamID::OffsetY,  TEXT("OffsetY"),  InTextureUV->GetOffset().Y);
	CreateParameterExpression(ParamID::Pivot,    ParamID::PivotX,   TEXT("PivotX"),   InTextureUV->GetPivot().X);
	CreateParameterExpression(ParamID::Pivot,    ParamID::PivotY,   TEXT("PivotY"),   InTextureUV->GetPivot().Y);
	CreateParameterExpression(ParamID::Rotation, ParamID::Rotation, TEXT("Rotation"), InTextureUV->GetRotation());
	CreateParameterExpression(ParamID::Scale,    ParamID::ScaleX,   TEXT("ScaleX"),   InTextureUV->GetScale().X);
	CreateParameterExpression(ParamID::Scale,    ParamID::ScaleY,   TEXT("ScaleY"),   InTextureUV->GetScale().Y);

	// Output
	Nodes.Add(TextureUVFunc);

	return Nodes;
}

void UDMMaterialStageInputTextureUV::InitTextureUV()
{
	if (TextureUV)
	{
		if (GUndo)
		{
			TextureUV->Modify();
		}

		TextureUV->SetParentComponent(this);
		TextureUV->GetOnUpdate().AddUObject(this, &UDMMaterialStageInputTextureUV::OnTextureUVUpdated);
	}
}

void UDMMaterialStageInputTextureUV::AddEffects(const TSharedRef<FDMMaterialBuildState>& InBuildState, TArray<UMaterialExpression*>& InOutExpressions) const
{
	UDMMaterialStage* Stage = GetStage();

	if (!Stage)
	{
		return;
	}

	UDMMaterialLayerObject* Layer = Stage->GetLayer();

	if (!Layer)
	{
		return;
	}

	UDMMaterialEffectStack* EffectStack = Layer->GetEffectStack();

	if (!EffectStack)
	{
		return;
	}

	int32 Channel = FDMMaterialStageConnectorChannel::WHOLE_CHANNEL;
	int32 OutputIndex = 0;
	EffectStack->ApplyEffects(InBuildState, EDMMaterialEffectTarget::TextureUV, InOutExpressions, Channel, OutputIndex);
}

#undef LOCTEXT_NAMESPACE

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DMMaterialStageFunction.h"

#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialStage.h"
#include "Components/DMMaterialValue.h"
#include "Components/MaterialStageInputs/DMMSIValue.h"
#include "Components/MaterialValues/DMMaterialValueFloat1.h"
#include "Components/MaterialValues/DMMaterialValueFloat2.h"
#include "Components/MaterialValues/DMMaterialValueFloat3XYZ.h"
#include "Components/MaterialValues/DMMaterialValueFloat4.h"
#include "DMPrivate.h"
#include "DynamicMaterialEditorModule.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Model/DMMaterialBuildState.h"
#include "Model/DMMaterialBuildUtils.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageFunction"

TSoftObjectPtr<UMaterialFunctionInterface> UDMMaterialStageFunction::NoOp = TSoftObjectPtr<UMaterialFunctionInterface>(FSoftObjectPath(TEXT(
	"/Script/Engine.MaterialFunction'/DynamicMaterial/MaterialFunctions/MF_DM_NoOp.MF_DM_NoOp'"
)));

UDMMaterialStage* UDMMaterialStageFunction::CreateStage(UDMMaterialLayerObject* InLayer)
{
	const FDMUpdateGuard Guard;

	UDMMaterialStage* NewStage = UDMMaterialStage::CreateMaterialStage(InLayer);

	UDMMaterialStageFunction* SourceFunction = NewObject<UDMMaterialStageFunction>(
		NewStage, 
		StaticClass(), 
		NAME_None, 
		RF_Transactional
	);

	check(SourceFunction);

	NewStage->SetSource(SourceFunction);

	return NewStage;
}

UDMMaterialStageFunction* UDMMaterialStageFunction::ChangeStageSource_Function(UDMMaterialStage* InStage, UMaterialFunctionInterface* InMaterialFunction)
{
	check(InStage);

	if (!InStage->CanChangeSource())
	{
		return nullptr;
	}

	check(InMaterialFunction);

	UDMMaterialStageFunction* NewFunction = InStage->ChangeSource<UDMMaterialStageFunction>(
		[InMaterialFunction](UDMMaterialStage* InStage, UDMMaterialStageSource* InNewSource)
		{
			const FDMUpdateGuard Guard;
			CastChecked<UDMMaterialStageFunction>(InNewSource)->SetMaterialFunction(InMaterialFunction);
		});

	return NewFunction;
}

UMaterialFunctionInterface* UDMMaterialStageFunction::GetNoOpFunction()
{
	return NoOp.LoadSynchronous();
}

void UDMMaterialStageFunction::SetMaterialFunction(UMaterialFunctionInterface* InMaterialFunction)
{
	if (MaterialFunction == InMaterialFunction)
	{
		return;
	}

	MaterialFunction = InMaterialFunction;

	OnMaterialFunctionChanged();
}

UDMMaterialValue* UDMMaterialStageFunction::GetInputValue(int32 InIndex) const
{
	TArray<UDMMaterialValue*> Values = GetInputValues();

	if (Values.IsValidIndex(InIndex))
	{
		return Values[InIndex];
	}

	return nullptr;
}

TArray<UDMMaterialValue*> UDMMaterialStageFunction::GetInputValues() const
{
	UDMMaterialStage* Stage = GetStage();

	if (!Stage)
	{
		return {};
	}

	TArray<UDMMaterialValue*> Values;

	for (UDMMaterialStageInput* Input : Stage->GetInputs())
	{
		if (UDMMaterialStageInputValue* InputValue = Cast<UDMMaterialStageInputValue>(Input))
		{
			if (UDMMaterialValue* Value = InputValue->GetValue())
			{
				Values.Add(Value);
			}
		}
	}

	return Values;
}

void UDMMaterialStageFunction::AddDefaultInput(int32 InInputIndex) const
{
	check(InInputIndex == 0);

	UDMMaterialStage* Stage = GetStage();
	check(Stage);

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
	}
	else
	{
		Stage->ChangeInput_PreviousStage(
			InInputIndex, 
			FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
			EDMMaterialPropertyType::EmissiveColor,
			0, 
			FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
		);
	}
}

bool UDMMaterialStageFunction::CanChangeInput(int32 InputIndex) const
{
	return true;
}

bool UDMMaterialStageFunction::CanChangeInputType(int32 InputIndex) const
{
	return false;
}

bool UDMMaterialStageFunction::IsInputVisible(int32 InputIndex) const
{
	return true;
}

void UDMMaterialStageFunction::GenerateExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState) const
{
	if (!IsComponentValid() || !IsComponentAdded())
	{
		return;
	}

	if (InBuildState->HasStageSource(this))
	{
		return;
	}

	UMaterialFunctionInterface* ActualMaterialFunction = MaterialFunction;

	if (!IsValid(ActualMaterialFunction))
	{
		ActualMaterialFunction = NoOp.LoadSynchronous();
	}

	if (!ActualMaterialFunction)
	{
		return;
	}

	UMaterialExpressionMaterialFunctionCall* FunctionCall = InBuildState->GetBuildUtils().CreateExpression<UMaterialExpressionMaterialFunctionCall>(UE_DM_NodeComment_Default);
	FunctionCall->SetMaterialFunction(ActualMaterialFunction);
	FunctionCall->UpdateFromFunctionResource();

	InBuildState->AddStageSourceExpressions(this, {FunctionCall});
}

FText UDMMaterialStageFunction::GetComponentDescription() const
{
	if (UMaterialFunctionInterface* MaterialFunctionInterface = MaterialFunction.Get())
	{
		const FString Caption = MaterialFunctionInterface->GetUserExposedCaption();

		if (!Caption.IsEmpty())
		{
			return FText::FromString(Caption);
		}
	}

	return Super::GetComponentDescription();
}

void UDMMaterialStageFunction::PreEditChange(FEditPropertyChain& PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	MaterialFunction_PreEdit = MaterialFunction;
}

void UDMMaterialStageFunction::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (MaterialFunction != MaterialFunction_PreEdit)
	{
		OnMaterialFunctionChanged();
	}
}

void UDMMaterialStageFunction::PostLoad()
{
	Super::PostLoad();

	if (NeedsFunctionInit())
	{
		InitFunction();
	}
}

UDMMaterialStageFunction::UDMMaterialStageFunction()
	: UDMMaterialStageThroughput(LOCTEXT("MaterialFunction", "Material Function"))
{
	MaterialFunction = nullptr;

	bInputRequired = false;
	bAllowNestedInputs = true;

	InputConnectors.Add({InputPreviousStage, LOCTEXT("PreviousStage", "Previous Stage"), EDMValueType::VT_Float3_RGB});

	OutputConnectors.Add({0, LOCTEXT("Output", "Output"), EDMValueType::VT_Float3_RGB});

	EditableProperties.Add(GET_MEMBER_NAME_CHECKED(UDMMaterialStageFunction, MaterialFunction));
}

void UDMMaterialStageFunction::OnMaterialFunctionChanged()
{
	DeinitFunction();
	InitFunction();

	if (FDMUpdateGuard::CanUpdate())
	{
		Update(EDMUpdateType::Structure);
	}
}

bool UDMMaterialStageFunction::NeedsFunctionInit() const
{
	UMaterialFunctionInterface* MaterialFunctionInterface = MaterialFunction.Get();

	TArray<UDMMaterialValue*> InputValues = GetInputValues();

	if (!IsValid(MaterialFunctionInterface))
	{
		// If we have no function, but we do have inputs, they need to be refreshed (removed).
		return !InputValues.IsEmpty();
	}

	TArray<FFunctionExpressionInput> Inputs;
	TArray<FFunctionExpressionOutput> Outputs;
	MaterialFunctionInterface->GetInputsAndOutputs(Inputs, Outputs);

	if (Outputs.IsEmpty())
	{
		UE::DynamicMaterialEditor::Private::LogError(TEXT("Function must have at least one output."));
		return false;
	}

	if (!Inputs[0].ExpressionInput || !Outputs[0].ExpressionOutput
		|| Inputs[0].ExpressionInput->InputType != Outputs[0].ExpressionOutput->GetOutputType(0))
	{
		UE::DynamicMaterialEditor::Private::LogError(TEXT("Function's first input must match its first output."));
		return false;
	}

	if (Inputs.Num() != InputValues.Num())
	{
		return true;
	}

	for (int32 InputIndex = 0; InputIndex < Inputs.Num(); ++InputIndex)
	{
		UMaterialExpressionFunctionInput* FunctionInput = Inputs[InputIndex].ExpressionInput.Get();

		if (!IsValid(FunctionInput))
		{
			UE::DynamicMaterialEditor::Private::LogError(TEXT("Function has missing input object."));
			return false;
		}

		// First input must be a scalar or vector3.
		if (InputIndex == 0)
		{
			switch (FunctionInput->InputType)
			{
				case EFunctionInputType::FunctionInput_Scalar:
				case EFunctionInputType::FunctionInput_Vector3:
					break;

				default:
					UE::DynamicMaterialEditor::Private::LogError(TEXT("Function has invalid first input - must be a scalar or vector3."));
					return false;
			}

			continue;
		}

		EDMValueType ValueType = EDMValueType::VT_None;

		switch (FunctionInput->InputType)
		{
			case EFunctionInputType::FunctionInput_Scalar:
				ValueType = EDMValueType::VT_Float1;
				break;

			case EFunctionInputType::FunctionInput_Vector2:
				ValueType = EDMValueType::VT_Float2;
				break;

			case EFunctionInputType::FunctionInput_Vector3:
				ValueType = EDMValueType::VT_Float3_XYZ;
				break;

			case EFunctionInputType::FunctionInput_Vector4:
				ValueType = EDMValueType::VT_Float4_RGBA;
				break;

			case EFunctionInputType::FunctionInput_Texture2D:
			case EFunctionInputType::FunctionInput_TextureCube:
			case EFunctionInputType::FunctionInput_VolumeTexture:
				ValueType = EDMValueType::VT_Texture;
				break;

			default:
				UE::DynamicMaterialEditor::Private::LogError(TEXT("Function has invalid input type - must be a scalar, vector or texture."));
				return false;
		}

		if (ValueType != InputValues[InputIndex]->GetType())
		{
			return true;
		}
	}

	return false;
}

void UDMMaterialStageFunction::InitFunction()
{
	if (!IsValid(MaterialFunction.Get()))
	{
		return;
	}

	UDMMaterialStage* Stage = GetStage();

	if (!Stage)
	{
		MaterialFunction = nullptr;
		return;
	}

	UDMMaterialSlot* Slot = GetTypedParent<UDMMaterialSlot>(/* bAllowSubclasses */ true);

	if (!Slot)
	{
		MaterialFunction = nullptr;
		return;
	}

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = Slot->GetMaterialModelEditorOnlyData();

	if (!EditorOnlyData)
	{
		MaterialFunction = nullptr;
		return;
	}

	UDynamicMaterialModel* MaterialModel = EditorOnlyData->GetMaterialModel();

	if (!MaterialModel)
	{
		MaterialFunction = nullptr;
		return;
	}

	TArray<FFunctionExpressionInput> Inputs;
	TArray<FFunctionExpressionOutput> Outputs;
	MaterialFunction->GetInputsAndOutputs(Inputs, Outputs);

	if (Inputs.IsEmpty())
	{
		UE::DynamicMaterialEditor::Private::LogError(TEXT("Function must have at least one input."));
		MaterialFunction = nullptr;
		return;
	}

	if (Outputs.IsEmpty())
	{
		UE::DynamicMaterialEditor::Private::LogError(TEXT("Function must have at least one output."));
		MaterialFunction = nullptr;
		return;
	}

	UDMMaterialLayerObject* Layer = Stage->GetLayer();
	check(Layer);

	const EDMMaterialPropertyType StageProperty = Layer->GetMaterialProperty();

	{
		FDMMaterialStageConnector PreviousStageConnector = InputConnectors[0];
		InputConnectors.SetNumZeroed(Inputs.Num());
		InputConnectors[0] = PreviousStageConnector;
	}

	for (int32 InputIndex = 0; InputIndex < Inputs.Num(); ++InputIndex)
	{
		UMaterialExpressionFunctionInput* FunctionInput = Inputs[InputIndex].ExpressionInput.Get();

		if (!IsValid(FunctionInput))
		{
			UE::DynamicMaterialEditor::Private::LogError(TEXT("Function has missing input object."));
			InputConnectors.SetNum(1);
			MaterialFunction = nullptr;
			return;
		}

		// First input must be a scalar or vector3.
		if (InputIndex == InputPreviousStage)
		{
			switch (FunctionInput->InputType)
			{
				case EFunctionInputType::FunctionInput_Scalar:
				case EFunctionInputType::FunctionInput_Vector3:
					break;

				default:
					UE::DynamicMaterialEditor::Private::LogError(TEXT("Function has invalid first input - must be a scalar or vector."));
					InputConnectors.SetNum(1);
					MaterialFunction = nullptr;
					return;
			}

			Stage->ChangeInput_PreviousStage(
				InputPreviousStage, 
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
				StageProperty,
				0, 
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
			);

			continue;
		}

		EDMValueType ValueType = EDMValueType::VT_None;

		switch (FunctionInput->InputType)
		{
			case EFunctionInputType::FunctionInput_Scalar:
				ValueType = EDMValueType::VT_Float1;
				break;

			case EFunctionInputType::FunctionInput_Vector2:
				ValueType = EDMValueType::VT_Float2;
				break;

			case EFunctionInputType::FunctionInput_Vector3:
				ValueType = EDMValueType::VT_Float3_XYZ;
				break;

			case EFunctionInputType::FunctionInput_Vector4:
				ValueType = EDMValueType::VT_Float4_RGBA;
				break;

			case EFunctionInputType::FunctionInput_Texture2D:
			case EFunctionInputType::FunctionInput_TextureCube:
			case EFunctionInputType::FunctionInput_VolumeTexture:
				ValueType = EDMValueType::VT_Texture;
				break;

			default:
				UE::DynamicMaterialEditor::Private::LogError(TEXT("Function has invalid input type - must be a scalar, vector or texture."));
				InputConnectors.SetNum(1);
				MaterialFunction = nullptr;
				return;
		}

		InputConnectors[InputIndex].Index = InputIndex;
		InputConnectors[InputIndex].Type = ValueType;

		if (Inputs[InputIndex].ExpressionInput->InputName == NAME_None)
		{
			static const FText InputNameFormat = LOCTEXT("InputFormat", "Input {0}");
			InputConnectors[InputIndex].Name = FText::Format(InputNameFormat, FText::AsNumber(InputIndex + 1));
		}
		else
		{
			InputConnectors[InputIndex].Name = FText::FromName(Inputs[InputIndex].ExpressionInput->InputName);
		}

		UDMMaterialStageInputValue* InputValue = UDMMaterialStageInputValue::ChangeStageInput_NewLocalValue(
			Stage,
			InputIndex, 
			FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
			ValueType,
			FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
		);
		
		check(InputValue);

		UDMMaterialValue* Value = InputValue->GetValue();
		check(Value);

		if (FunctionInput->bUsePreviewValueAsDefault)
		{
			switch (FunctionInput->InputType)
			{
				case EFunctionInputType::FunctionInput_Scalar:
					if (UDMMaterialValueFloat1* Float1Value = Cast<UDMMaterialValueFloat1>(Value))
					{
						Float1Value->SetDefaultValue(FunctionInput->PreviewValue.X);
						Float1Value->ApplyDefaultValue();
					}
					break;

				case EFunctionInputType::FunctionInput_Vector2:
					if (UDMMaterialValueFloat2* Float2Value = Cast<UDMMaterialValueFloat2>(Value))
					{
						Float2Value->SetDefaultValue({FunctionInput->PreviewValue.X, FunctionInput->PreviewValue.Y});
						Float2Value->ApplyDefaultValue();
					}
					break;

				case EFunctionInputType::FunctionInput_Vector3:
					if (UDMMaterialValueFloat3XYZ* Float3Value = Cast<UDMMaterialValueFloat3XYZ>(Value))
					{
						Float3Value->SetDefaultValue({FunctionInput->PreviewValue.X, FunctionInput->PreviewValue.Y, FunctionInput->PreviewValue.Z});
						Float3Value->ApplyDefaultValue();
					}
					break;

				case EFunctionInputType::FunctionInput_Vector4:
					if (UDMMaterialValueFloat4* Float4Value = Cast<UDMMaterialValueFloat4>(Value))
					{
						Float4Value->SetDefaultValue(FunctionInput->PreviewValue);
						Float4Value->ApplyDefaultValue();
					}
					break;

				default:
					// Not possible
					break;
			}
		}
	}
}

void UDMMaterialStageFunction::DeinitFunction()
{
	InputConnectors.SetNum(1);

	if (UDMMaterialStage* Stage = GetStage())
	{
		if (GUndo)
		{
			Stage->Modify();
		}

		Stage->RemoveAllInputs();
	}
}

void UDMMaterialStageFunction::OnComponentAdded()
{
	if (!IsComponentValid())
	{
		return;
	}

	if (NeedsFunctionInit())
	{
		InitFunction();
	}

	Super::OnComponentAdded();
}

void UDMMaterialStageFunction::OnComponentRemoved()
{
	DeinitFunction();

	Super::OnComponentRemoved();
}

#undef LOCTEXT_NAMESPACE

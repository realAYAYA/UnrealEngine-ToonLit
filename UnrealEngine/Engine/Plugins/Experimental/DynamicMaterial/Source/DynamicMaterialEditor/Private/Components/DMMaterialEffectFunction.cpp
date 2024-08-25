// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DMMaterialEffectFunction.h"
#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialValue.h"
#include "Components/MaterialValues/DMMaterialValueFloat1.h"
#include "Components/MaterialValues/DMMaterialValueFloat2.h"
#include "Components/MaterialValues/DMMaterialValueFloat3XYZ.h"
#include "Components/MaterialValues/DMMaterialValueFloat4.h"
#include "DMComponentPath.h"
#include "DMPrivate.h"
#include "DynamicMaterialEditorModule.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Model/DMMaterialBuildState.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"

#define LOCTEXT_NAMESPACE "DMMaterialEffectFunction"

const FString UDMMaterialEffectFunction::InputsPathToken = TEXT("Inputs");

UDMMaterialEffectFunction::UDMMaterialEffectFunction()
{
}

UMaterialFunctionInterface* UDMMaterialEffectFunction::GetMaterialFunction() const
{
	return MaterialFunctionPtr;
}

bool UDMMaterialEffectFunction::SetMaterialFunction(UMaterialFunctionInterface* InFunction)
{
	if (MaterialFunctionPtr == InFunction)
	{
		return false;
	}

	MaterialFunctionPtr = InFunction;

	OnMaterialFunctionChanged();

	Update(EDMUpdateType::Structure);

	return true;
}

UDMMaterialValue* UDMMaterialEffectFunction::GetInputValue(int32 InIndex) const
{
	if (InputValues.IsValidIndex(InIndex))
	{
		return InputValues[InIndex];
	}

	return nullptr;
}

TArray<UDMMaterialValue*> UDMMaterialEffectFunction::BP_GetInputValues() const
{
	TArray<UDMMaterialValue*> Values;
	Values.Reserve(InputValues.Num());

	for (const TObjectPtr<UDMMaterialValue>& Value : InputValues)
	{
		Values.Add(Value);
	}

	return Values;
}

const TArray<TObjectPtr<UDMMaterialValue>>& UDMMaterialEffectFunction::GetInputValues() const
{
	return InputValues;
}

FText UDMMaterialEffectFunction::GetEffectName() const
{
	if (UMaterialFunctionInterface* MaterialFunction = MaterialFunctionPtr.Get())
	{
		const FString Caption = MaterialFunction->GetUserExposedCaption();

		if (!Caption.IsEmpty())
		{
			return FText::FromString(Caption);
		}
	}

	static const FText Name = LOCTEXT("EffectFunction", "Effect Function");
	return Name;
}

FText UDMMaterialEffectFunction::GetEffectDescription() const
{
	if (UMaterialFunctionInterface* MaterialFunction = MaterialFunctionPtr.Get())
	{
		const FString& Description = MaterialFunction->GetDescription();

		if (!Description.IsEmpty())
		{
			return FText::FromString(Description);
		}
	}

	return FText::GetEmpty();
}

void UDMMaterialEffectFunction::ApplyTo(const TSharedRef<FDMMaterialBuildState>& InBuildState, TArray<UMaterialExpression*>& InOutStageExpressions,
	int32& InOutLastExpressionOutputChannel, int32& InOutLastExpressionOutputIndex) const
{
	if (!IsComponentValid() || !IsComponentAdded())
	{
		return;
	}

	if (InOutStageExpressions.IsEmpty())
	{
		return;
	}

	UMaterialFunctionInterface* MaterialFunction = MaterialFunctionPtr;

	if (!IsValid(MaterialFunction))
	{
		return;
	}

	UMaterialExpressionMaterialFunctionCall* FunctionCall = InBuildState->GetBuildUtils().CreateExpression<UMaterialExpressionMaterialFunctionCall>(UE_DM_NodeComment_Default);
	FunctionCall->SetMaterialFunction(MaterialFunction);
	FunctionCall->UpdateFromFunctionResource();

	if (FunctionCall->FunctionInputs.Num() != InputValues.Num())
	{
		return;
	}

	UMaterialExpression* LastStageExpression = InOutStageExpressions.Last();

	TArray<UMaterialExpression*> LastInputExpressions;
	LastInputExpressions.Reserve(InputValues.Num() + 1);

	for (const TObjectPtr<UDMMaterialValue>& InputValue : InputValues)
	{
		// Certain inputs (such as index 0) are intentionally nullptr just to align input values with function inputs.
		if (!InputValue)
		{
			LastInputExpressions.Add(nullptr);
			continue;
		}

		InputValue->GenerateExpression(InBuildState);

		if (InBuildState->HasValue(InputValue))
		{
			const TArray<UMaterialExpression*>& ValueExpressions = InBuildState->GetValueExpressions(InputValue);

			if (!ValueExpressions.IsEmpty())
			{
				LastInputExpressions.Add(ValueExpressions.Last());
				InOutStageExpressions.Append(ValueExpressions);
				continue;
			}
		}

		LastInputExpressions.Add(nullptr);
	}

	LastInputExpressions[0] = LastStageExpression;

	for (int32 InputIndex = 0; InputIndex < InputValues.Num(); ++InputIndex)
	{
		if (LastInputExpressions[InputIndex])
		{
			if (InputIndex == 0)
			{
				if (InOutLastExpressionOutputChannel != FDMMaterialStageConnectorChannel::WHOLE_CHANNEL)
				{
					LastInputExpressions[0] = InBuildState->GetBuildUtils().CreateExpressionBitMask(
						LastInputExpressions[0], 
						InOutLastExpressionOutputIndex,
						InOutLastExpressionOutputChannel
					);

					InOutStageExpressions.Add(LastInputExpressions[0]);
				}

				FunctionCall->FunctionInputs[InputIndex].Input.Connect(
					InOutLastExpressionOutputIndex, 
					LastInputExpressions[InputIndex]
				);
			}
			else
			{
				FunctionCall->FunctionInputs[InputIndex].Input.Connect(0, LastInputExpressions[InputIndex]);
			}
		}
		else
		{
			FunctionCall->FunctionInputs[InputIndex].Input.Expression = nullptr;
			FunctionCall->FunctionInputs[InputIndex].Input.OutputIndex = 0;
		}
	}

	InOutStageExpressions.Add(FunctionCall);

	// Output index from an effect function is always the first output.
	InOutLastExpressionOutputIndex = 0;
	InOutLastExpressionOutputChannel = FDMMaterialStageConnectorChannel::WHOLE_CHANNEL;
}

FText UDMMaterialEffectFunction::GetComponentDescription() const
{
	return GetEffectName();
}

void UDMMaterialEffectFunction::PostEditorDuplicate(UDynamicMaterialModel* InMaterialModel, UDMMaterialComponent* InParent)
{
	Super::PostEditorDuplicate(InMaterialModel, InParent);

	for (const TObjectPtr<UDMMaterialValue>& Value : InputValues)
	{
		if (Value)
		{
			Value->PostEditorDuplicate(InMaterialModel, this);
		}
	}
}

bool UDMMaterialEffectFunction::Modify(bool bInAlwaysMarkDirty)
{
	const bool bSaved = Super::Modify(bInAlwaysMarkDirty);

	for (const TObjectPtr<UDMMaterialValue>& Value : InputValues)
	{
		if (Value)
		{
			Value->Modify(bInAlwaysMarkDirty);
		}
	}

	return bSaved;
}

void UDMMaterialEffectFunction::PostLoad()
{
	Super::PostLoad();

	if (NeedsFunctionInit())
	{
		InitFunction();
	}
}

void UDMMaterialEffectFunction::OnMaterialFunctionChanged()
{
	DeinitFunction();
	InitFunction();
}

void UDMMaterialEffectFunction::DeinitFunction()
{
	for (const TObjectPtr<UDMMaterialValue>& Value : InputValues)
	{
		if (Value)
		{
			Value->SetComponentState(EDMComponentLifetimeState::Removed);
		}
	}

	InputValues.Empty();
}

bool UDMMaterialEffectFunction::NeedsFunctionInit() const
{
	if (!IsValid(MaterialFunctionPtr))
	{
		// If we have no function, but we do have inputs, they need to be refreshed (removed).
		return !InputValues.IsEmpty();
	}

	TArray<FFunctionExpressionInput> Inputs;
	TArray<FFunctionExpressionOutput> Outputs;
	MaterialFunctionPtr->GetInputsAndOutputs(Inputs, Outputs);

	if (Outputs.IsEmpty())
	{
		UE::DynamicMaterialEditor::Private::LogError(TEXT("Effect Function must have at least one output."));
		return false;
	}

	if (!Inputs[0].ExpressionInput || !Outputs[0].ExpressionOutput
		|| Inputs[0].ExpressionInput->InputType != Outputs[0].ExpressionOutput->GetOutputType(0))
	{
		UE::DynamicMaterialEditor::Private::LogError(TEXT("Effect Function's first input must match its first output."));
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
			UE::DynamicMaterialEditor::Private::LogError(TEXT("Effect Function has missing input object."));
			return false;
		}

		// First input must be a scalar or vector.
		if (InputIndex == 0)
		{
			switch (FunctionInput->InputType)
			{
				case EFunctionInputType::FunctionInput_Scalar:
				case EFunctionInputType::FunctionInput_Vector2:
				case EFunctionInputType::FunctionInput_Vector3:
				case EFunctionInputType::FunctionInput_Vector4:
					break;

				default:
					UE::DynamicMaterialEditor::Private::LogError(TEXT("Effect Function has invalid first input -  must be a scalar or vector."));
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
				UE::DynamicMaterialEditor::Private::LogError(TEXT("Effect Function has invalid input type -  must be a scalar, vector or texture."));
				return false;
		}

		if (ValueType != InputValues[InputIndex]->GetType())
		{
			return true;
		}
	}

	return false;
}

void UDMMaterialEffectFunction::InitFunction()
{
	if (!IsValid(MaterialFunctionPtr))
	{
		return;
	}

	UDMMaterialSlot* Slot = GetTypedParent<UDMMaterialSlot>(/* bAllowSubclasses */ true);

	if (!Slot)
	{
		MaterialFunctionPtr = nullptr;
		return;
	}

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = Slot->GetMaterialModelEditorOnlyData();

	if (!EditorOnlyData)
	{
		MaterialFunctionPtr = nullptr;
		return;
	}

	UDynamicMaterialModel* MaterialModel = EditorOnlyData->GetMaterialModel();

	if (!MaterialModel)
	{
		MaterialFunctionPtr = nullptr;
		return;
	}

	TArray<FFunctionExpressionInput> Inputs;
	TArray<FFunctionExpressionOutput> Outputs;
	MaterialFunctionPtr->GetInputsAndOutputs(Inputs, Outputs);

	if (Inputs.IsEmpty())
	{
		UE::DynamicMaterialEditor::Private::LogError(TEXT("Effect Function must have at least one input."));
		MaterialFunctionPtr = nullptr;
		return;
	}

	if (Outputs.IsEmpty())
	{
		UE::DynamicMaterialEditor::Private::LogError(TEXT("Effect Function must have at least one output."));
		MaterialFunctionPtr = nullptr;
		return;
	}

	InputValues.Reserve(Inputs.Num());

	const bool bSetValueAdded = IsComponentAdded();

	for (int32 InputIndex = 0; InputIndex < Inputs.Num(); ++InputIndex)
	{
		UMaterialExpressionFunctionInput* FunctionInput = Inputs[InputIndex].ExpressionInput.Get();

		if (!IsValid(FunctionInput))
		{
			UE::DynamicMaterialEditor::Private::LogError(TEXT("Effect Function has missing input object."));
			MaterialFunctionPtr = nullptr;
			return;
		}

		// First input must be a scalar or vector.
		if (InputIndex == 0)
		{
			switch (FunctionInput->InputType)
			{
				case EFunctionInputType::FunctionInput_Scalar:
					EffectTarget = EDMMaterialEffectTarget::MaskStage;
					break;

				case EFunctionInputType::FunctionInput_Vector2:
					EffectTarget = EDMMaterialEffectTarget::TextureUV;
					break;

				case EFunctionInputType::FunctionInput_Vector3:
					EffectTarget = EDMMaterialEffectTarget::BaseStage;
					break;

				case EFunctionInputType::FunctionInput_Vector4:
					EffectTarget = EDMMaterialEffectTarget::Slot;
					break;

				default:
					UE::DynamicMaterialEditor::Private::LogError(TEXT("Effect Function has invalid first input -  must be a scalar or vector."));
					MaterialFunctionPtr = nullptr;
					return;
			}

			InputValues.Add(nullptr);
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
				UE::DynamicMaterialEditor::Private::LogError(TEXT("Effect Function has invalid input type -  must be a scalar, vector or texture."));
				MaterialFunctionPtr = nullptr;
				return;
		}

		UDMMaterialValue* Value = UDMMaterialValue::CreateMaterialValue(MaterialModel, TEXT(""), ValueType, /* bInLocal */ true);
		InputValues.Add(Value);

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

		if (bSetValueAdded)
		{
			Value->SetComponentState(EDMComponentLifetimeState::Added);
		}
	}
}

UDMMaterialComponent* UDMMaterialEffectFunction::GetSubComponentByPath(FDMComponentPath& InPath, const FDMComponentPathSegment& InPathSegment) const
{
	if (InPathSegment.GetToken() == InputsPathToken)
	{
		int32 InputIndex;

		if (InPathSegment.GetParameter(InputIndex))
		{
			if (InputValues.IsValidIndex(InputIndex))
			{
				return InputValues[InputIndex]->GetComponentByPath(InPath);
			}
		}
	}

	return Super::GetSubComponentByPath(InPath, InPathSegment);
}

void UDMMaterialEffectFunction::OnComponentAdded()
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

void UDMMaterialEffectFunction::OnComponentRemoved()
{
	DeinitFunction();

	Super::OnComponentRemoved();
}

#undef LOCTEXT_NAMESPACE

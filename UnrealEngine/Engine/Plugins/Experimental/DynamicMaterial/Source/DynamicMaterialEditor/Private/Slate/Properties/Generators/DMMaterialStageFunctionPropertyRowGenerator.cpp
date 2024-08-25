// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/Properties/Generators/DMMaterialStageFunctionPropertyRowGenerator.h"
#include "Components/DMMaterialComponent.h"
#include "Components/DMMaterialStageFunction.h"
#include "Components/DMMaterialValue.h"
#include "Components/MaterialStageInputs/DMMSIFunction.h"
#include "DynamicMaterialEditorModule.h"
#include "IDetailPropertyRow.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Slate/SDMComponentEdit.h"
#include "Slate/SDMEditor.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageFunctionPropertyRowGenerator"

const TSharedRef<FDMMaterialStageFunctionPropertyRowGenerator>& FDMMaterialStageFunctionPropertyRowGenerator::Get()
{
	static TSharedRef<FDMMaterialStageFunctionPropertyRowGenerator> Generator = MakeShared<FDMMaterialStageFunctionPropertyRowGenerator>();
	return Generator;
}

void FDMMaterialStageFunctionPropertyRowGenerator::AddComponentProperties(const TSharedRef<SDMComponentEdit>& InComponentEditWidget, UDMMaterialComponent* InComponent,
	TArray<FDMPropertyHandle>& InOutPropertyRows, TSet<UDMMaterialComponent*>& InOutProcessedObjects)
{
	if (!IsValid(InComponent))
	{
		return;
	}

	if (InOutProcessedObjects.Contains(InComponent))
	{
		return;
	}

	UDMMaterialStageInputFunction* StageInputFunction = Cast<UDMMaterialStageInputFunction>(InComponent);

	if (!StageInputFunction)
	{
		return;
	}

	UDMMaterialStageFunction* StageFunction = StageInputFunction->GetMaterialStageFunction();

	if (!StageFunction)
	{
		return;
	}

	InOutProcessedObjects.Add(InComponent);

	UMaterialFunctionInterface* MaterialFunction = StageFunction->GetMaterialFunction();

	if (!IsValid(MaterialFunction))
	{
		return;
	}

	TArray<FFunctionExpressionInput> Inputs;
	TArray<FFunctionExpressionOutput> Outputs;
	MaterialFunction->GetInputsAndOutputs(Inputs, Outputs);

	TArray<UDMMaterialValue*> InputValues = StageFunction->GetInputValues();

	// 1 input, the previous stage, does not have a value.
	if (Inputs.Num() != (InputValues.Num() + 1))
	{
		return;
	}

	TArray<FDMPropertyHandle> AllValuePropertyRows;

	for (int32 InputIndex = 0; InputIndex < InputValues.Num(); ++InputIndex)
	{
		FFunctionExpressionInput& Input = Inputs[InputIndex + 1];
		UDMMaterialValue* Value = InputValues[InputIndex];

		if (!IsValid(Value))
		{
			continue;
		}

		if (!Input.ExpressionInput)
		{
			continue;
		}

		TArray<FDMPropertyHandle> ValuePropertyRows;

		FDynamicMaterialEditorModule::GeneratorComponentPropertyRows(
			InComponentEditWidget, 
			Value, 
			ValuePropertyRows, 
			InOutProcessedObjects
		);

		if (ValuePropertyRows.Num() == 1)
		{
			ValuePropertyRows[0].NameOverride = FText::FromName(Input.ExpressionInput->InputName);
		}
		else
		{
			static const FText NameFormat = LOCTEXT("ValueFormat", "{0}[{1}]");

			for (int32 ValuePropertyIndex = 0; ValuePropertyIndex < ValuePropertyRows.Num(); ++ValuePropertyIndex)
			{
				ValuePropertyRows[ValuePropertyIndex].NameOverride = FText::Format(
					NameFormat,
					FText::FromName(Input.ExpressionInput->InputName),
					FText::AsNumber(ValuePropertyIndex + 1)
				);
			}
		}

		const FText Description = FText::FromString(Input.ExpressionInput->Description);

		for (FDMPropertyHandle& ValuePropertyRow : ValuePropertyRows)
		{
			ValuePropertyRow.NameToolTipOverride = Description;
		}

		AllValuePropertyRows.Append(ValuePropertyRows);
	}

	FDynamicMaterialEditorModule::GeneratorComponentPropertyRows(
		InComponentEditWidget, 
		StageFunction, 
		InOutPropertyRows, 
		InOutProcessedObjects
	);

	InOutPropertyRows.Append(AllValuePropertyRows);
}

#undef LOCTEXT_NAMESPACE

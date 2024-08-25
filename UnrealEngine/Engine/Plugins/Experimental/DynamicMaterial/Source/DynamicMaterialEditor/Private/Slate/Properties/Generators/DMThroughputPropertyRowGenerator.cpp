// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/Properties/Generators/DMThroughputPropertyRowGenerator.h"
#include "DynamicMaterialEditorModule.h"
#include "Components/DMMaterialStage.h"
#include "Components/DMMaterialStageInput.h"
#include "Components/DMMaterialStageThroughput.h"

const TSharedRef<FDMThroughputPropertyRowGenerator>& FDMThroughputPropertyRowGenerator::Get()
{
	static TSharedRef<FDMThroughputPropertyRowGenerator> Generator = MakeShared<FDMThroughputPropertyRowGenerator>();
	return Generator;
}

void FDMThroughputPropertyRowGenerator::AddComponentProperties(const TSharedRef<SDMComponentEdit>& InComponentEditWidget,
	UDMMaterialComponent* InComponent, TArray<FDMPropertyHandle>& InOutPropertyRows,
	TSet<UDMMaterialComponent*>& InOutProcessedObjects)
{
	if (!IsValid(InComponent))
	{
		return;
	}

	if (InOutProcessedObjects.Contains(InComponent))
	{
		return;
	}

	UDMMaterialStageThroughput* Throughput = Cast<UDMMaterialStageThroughput>(InComponent);

	if (!Throughput)
	{
		return;
	}

	InOutProcessedObjects.Add(Throughput);

	const TArray<FName>& ThroughputProperties = Throughput->GetEditableProperties();

	for (const FName& ThroughputProperty : ThroughputProperties)
	{
		if (InComponent->IsPropertyVisible(ThroughputProperty))
		{
			AddPropertyEditRows(InComponentEditWidget, InComponent, ThroughputProperty, InOutPropertyRows, InOutProcessedObjects);
		}
	}

	UDMMaterialStage* Stage = Throughput->GetStage();

	if (!Stage)
	{
		return;
	}

	InOutProcessedObjects.Add(Stage);

	static const FName StageInputsName = GET_MEMBER_NAME_CHECKED(UDMMaterialStage, Inputs);

	{
		const TArray<FDMMaterialStageConnector>& InputConnectors = Throughput->GetInputConnectors();
		const TArray<FDMMaterialStageConnection>& InputMap = Stage->GetInputConnectionMap();
		TArray<UDMMaterialStageInput*> Inputs = Stage->GetInputs();

		for (int32 InputIdx = 0; InputIdx < InputConnectors.Num(); ++InputIdx)
		{
			if (!Throughput->IsInputVisible(InputIdx) || !Throughput->CanChangeInput(InputIdx))
			{
				continue;
			}

			for (const FDMMaterialStageConnectorChannel& Channel : InputMap[InputIdx].Channels)
			{
				const int32 StageInputIdx = Channel.SourceIndex - FDMMaterialStageConnectorChannel::FIRST_STAGE_INPUT;

				if (Inputs.IsValidIndex(StageInputIdx))
				{
					FDynamicMaterialEditorModule::GeneratorComponentPropertyRows(InComponentEditWidget, Inputs[StageInputIdx], InOutPropertyRows, InOutProcessedObjects);
				}
			}
		}
	}

	const TArray<FName>& StageProperties = Stage->GetEditableProperties();

	for (const FName& StageProperty : StageProperties)
	{
		if (StageProperty != StageInputsName)
		{
			AddPropertyEditRows(InComponentEditWidget, Stage, StageProperty, InOutPropertyRows, InOutProcessedObjects);
			continue;
		}
	}
}

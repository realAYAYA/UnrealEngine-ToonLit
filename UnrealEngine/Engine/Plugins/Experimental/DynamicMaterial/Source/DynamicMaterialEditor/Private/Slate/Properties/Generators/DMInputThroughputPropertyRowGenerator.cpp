// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/Properties/Generators/DMInputThroughputPropertyRowGenerator.h"
#include "Components/DMMaterialStageThroughput.h"
#include "Components/DMMaterialSubStage.h"
#include "Components/MaterialStageInputs/DMMSIThroughput.h"

const TSharedRef<FDMInputThroughputPropertyRowGenerator>& FDMInputThroughputPropertyRowGenerator::Get()
{
	static TSharedRef<FDMInputThroughputPropertyRowGenerator> Generator = MakeShared<FDMInputThroughputPropertyRowGenerator>();
	return Generator;
}

void FDMInputThroughputPropertyRowGenerator::AddComponentProperties(const TSharedRef<SDMComponentEdit>& InComponentEditWidget,
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

	UDMMaterialStageInputThroughput* InputThroughput = Cast<UDMMaterialStageInputThroughput>(InComponent);

	if (!InputThroughput)
	{
		return;
	}

	InOutProcessedObjects.Add(InputThroughput);

	if (UDMMaterialSubStage* SubStage = InputThroughput->GetSubStage())
	{
		InOutProcessedObjects.Add(SubStage);
	}

	UDMMaterialStageThroughput* Throughput = InputThroughput->GetMaterialStageThroughput();

	if (!Throughput)
	{
		return;
	}

	FDMThroughputPropertyRowGenerator::AddComponentProperties(InComponentEditWidget, Throughput, InOutPropertyRows, InOutProcessedObjects);
}

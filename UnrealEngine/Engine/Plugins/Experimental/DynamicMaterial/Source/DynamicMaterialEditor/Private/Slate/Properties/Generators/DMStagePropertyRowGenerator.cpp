// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/Properties/Generators/DMStagePropertyRowGenerator.h"
#include "DynamicMaterialEditorModule.h"
#include "Components/DMMaterialComponent.h"
#include "Components/DMMaterialStage.h"
#include "Components/DMMaterialStageSource.h"
#include "Slate/SDMComponentEdit.h"

const TSharedRef<FDMStagePropertyRowGenerator>& FDMStagePropertyRowGenerator::Get()
{
	static TSharedRef<FDMStagePropertyRowGenerator> Generator = MakeShared<FDMStagePropertyRowGenerator>();
	return Generator;
}

void FDMStagePropertyRowGenerator::AddComponentProperties(const TSharedRef<SDMComponentEdit>& InComponentEditWidget, UDMMaterialComponent* InComponent,
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

	UDMMaterialStage* Stage = Cast<UDMMaterialStage>(InComponent);

	if (!Stage)
	{
		return;
	}

	UDMMaterialStageSource* Source = Stage->GetSource();

	if (!Source)
	{
		return;
	}

	FDynamicMaterialEditorModule::GeneratorComponentPropertyRows(InComponentEditWidget, Source, InOutPropertyRows, InOutProcessedObjects);
	FDMComponentPropertyRowGenerator::AddComponentProperties(InComponentEditWidget, Stage, InOutPropertyRows, InOutProcessedObjects);
}
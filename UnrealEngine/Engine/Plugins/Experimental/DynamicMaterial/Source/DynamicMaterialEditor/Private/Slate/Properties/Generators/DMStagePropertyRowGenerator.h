// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Slate/Properties/Generators/DMComponentPropertyRowGenerator.h"

class FDMStagePropertyRowGenerator : public FDMComponentPropertyRowGenerator
{
public:
	static const TSharedRef<FDMStagePropertyRowGenerator>& Get();

	FDMStagePropertyRowGenerator() = default;
	virtual ~FDMStagePropertyRowGenerator() override = default;

	virtual void AddComponentProperties(const TSharedRef<SDMComponentEdit>& InComponentEditWidget, UDMMaterialComponent* InComponent,
		TArray<FDMPropertyHandle>& InOutPropertyRows, TSet<UDMMaterialComponent*>& InOutProcessedObjects) override;
};

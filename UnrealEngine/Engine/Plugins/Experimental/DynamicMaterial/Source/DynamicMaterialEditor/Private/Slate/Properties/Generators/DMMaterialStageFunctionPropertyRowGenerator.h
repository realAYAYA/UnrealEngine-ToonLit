// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Slate/Properties/Generators/DMComponentPropertyRowGenerator.h"

class FDMMaterialStageFunctionPropertyRowGenerator : public FDMComponentPropertyRowGenerator
{
public:
	static const TSharedRef<FDMMaterialStageFunctionPropertyRowGenerator>& Get();

	FDMMaterialStageFunctionPropertyRowGenerator() = default;
	virtual ~FDMMaterialStageFunctionPropertyRowGenerator() override = default;

	virtual void AddComponentProperties(const TSharedRef<SDMComponentEdit>& InComponentEditWidget, UDMMaterialComponent* InComponent,
		TArray<FDMPropertyHandle>& InOutPropertyRows, TSet<UDMMaterialComponent*>& InOutProcessedObjects) override;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Slate/Properties/Generators/DMThroughputPropertyRowGenerator.h"

class FDMInputThroughputPropertyRowGenerator : public FDMThroughputPropertyRowGenerator
{
public:
	static const TSharedRef<FDMInputThroughputPropertyRowGenerator>& Get();

	FDMInputThroughputPropertyRowGenerator() = default;
	virtual ~FDMInputThroughputPropertyRowGenerator() override = default;

	virtual void AddComponentProperties(const TSharedRef<SDMComponentEdit>& InComponentEditWidget, UDMMaterialComponent* InComponent,
		TArray<FDMPropertyHandle>& InOutPropertyRows, TSet<UDMMaterialComponent*>& InOutProcessedObjects) override;
};

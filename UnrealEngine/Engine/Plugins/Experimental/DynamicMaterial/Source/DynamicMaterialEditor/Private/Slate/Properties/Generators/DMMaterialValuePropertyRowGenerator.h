// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Slate/Properties/Generators/DMComponentPropertyRowGenerator.h"

class FDMMaterialValuePropertyRowGenerator : public FDMComponentPropertyRowGenerator
{
public:
	static const TSharedRef<FDMMaterialValuePropertyRowGenerator>& Get();

	FDMMaterialValuePropertyRowGenerator() = default;
	virtual ~FDMMaterialValuePropertyRowGenerator() override = default;

	virtual void AddComponentProperties(const TSharedRef<SDMComponentEdit>& InComponentEditWidget, UDMMaterialComponent* InComponent,
		TArray<FDMPropertyHandle>& InOutPropertyRows, TSet<UDMMaterialComponent*>& InOutProcessedObjects) override;
};

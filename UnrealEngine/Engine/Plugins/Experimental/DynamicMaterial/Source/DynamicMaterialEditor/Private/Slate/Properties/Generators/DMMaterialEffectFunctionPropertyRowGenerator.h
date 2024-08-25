// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Slate/Properties/Generators/DMComponentPropertyRowGenerator.h"

class FDMMaterialEffectFunctionPropertyRowGenerator : public FDMComponentPropertyRowGenerator
{
public:
	static const TSharedRef<FDMMaterialEffectFunctionPropertyRowGenerator>& Get();

	FDMMaterialEffectFunctionPropertyRowGenerator() = default;
	virtual ~FDMMaterialEffectFunctionPropertyRowGenerator() override = default;

	virtual void AddComponentProperties(const TSharedRef<SDMComponentEdit>& InComponentEditWidget, UDMMaterialComponent* InComponent,
		TArray<FDMPropertyHandle>& InOutPropertyRows, TSet<UDMMaterialComponent*>& InOutProcessedObjects) override;
};

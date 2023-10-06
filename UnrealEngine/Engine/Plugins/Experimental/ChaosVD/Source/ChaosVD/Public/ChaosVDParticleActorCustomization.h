// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "IDetailCustomization.h"

/** Custom details panel for the ChaosVD Particle Actor */
class FChaosVDParticleActorCustomization : public IDetailCustomization
{
public:
	FChaosVDParticleActorCustomization();

	inline static FName ChaosVDCategoryName = FName("Particle Data");
	inline static FName ChaosVDVisualizationCategoryName = FName("Viewport Visualization Flags");

	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	TSet<FName> AllowedCategories;
};

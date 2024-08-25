// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "IDetailCustomization.h"

/**
 * Customization view for CVD Geometry component classes 
 */
class FChaosVDGeometryComponentCustomization : public IDetailCustomization
{
public:
	FChaosVDGeometryComponentCustomization();

	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	TSet<FName> AllowedCategories;
};

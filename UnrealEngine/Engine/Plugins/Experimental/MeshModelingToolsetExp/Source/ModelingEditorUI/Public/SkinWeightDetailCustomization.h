// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "UObject/WeakObjectPtr.h"

class USkinWeightsPaintToolProperties;

class FSkinWeightDetailCustomization : public IDetailCustomization
{
public:

	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable(new FSkinWeightDetailCustomization);
	}

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:

	void AddBrushUI(IDetailLayoutBuilder& DetailBuilder);
	void AddSelectionUI(IDetailLayoutBuilder& DetailBuilder);

	IDetailLayoutBuilder* CurrentDetailBuilder;
	TWeakObjectPtr<USkinWeightsPaintToolProperties> SkinToolSettings;

	static float WeightSliderWidths;
	static float WeightEditingLabelsPercent;
	static float WeightEditVerticalPadding;
	static float WeightEditHorizontalPadding;
};

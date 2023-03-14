// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveformEditorDetailsCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Sound/SoundWave.h"

static const FLazyName TransformationsCategoryName("Waveform Processing");

void FWaveformTransformationsDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{	
	TArray<FName> CategoryNames;
	DetailLayout.GetCategoryNames(CategoryNames);

	for (FName& CategoryName : CategoryNames)
	{
		if (CategoryName != TransformationsCategoryName)
		{
			DetailLayout.HideCategory(CategoryName);
		}
	}

	IDetailCategoryBuilder& CategoryBuilder = DetailLayout.EditCategory("Waveform Processing");
	CategoryBuilder.InitiallyCollapsed(false);
	CategoryBuilder.RestoreExpansionState(true);
}
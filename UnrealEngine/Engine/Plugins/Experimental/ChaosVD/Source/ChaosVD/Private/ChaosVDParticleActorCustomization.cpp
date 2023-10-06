// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDParticleActorCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"

FChaosVDParticleActorCustomization::FChaosVDParticleActorCustomization()
{
	AllowedCategories.Add(FChaosVDParticleActorCustomization::ChaosVDCategoryName);
	AllowedCategories.Add(FChaosVDParticleActorCustomization::ChaosVDVisualizationCategoryName);
}

TSharedRef<IDetailCustomization> FChaosVDParticleActorCustomization::MakeInstance()
{
	return MakeShareable( new FChaosVDParticleActorCustomization );
}

void FChaosVDParticleActorCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{

	IDetailCategoryBuilder& YourCategory = DetailBuilder.EditCategory(ChaosVDVisualizationCategoryName, FText::GetEmpty(), ECategoryPriority::Important);
	// Hide everything as the only thing we want to show in these actors is the Recorded debug data
	TArray<FName> CurrentCategoryNames;
	DetailBuilder.GetCategoryNames(CurrentCategoryNames);
	for (const FName& CategoryToHide : CurrentCategoryNames)
	{
		if (!AllowedCategories.Contains(CategoryToHide))
		{
			DetailBuilder.HideCategory(CategoryToHide);
		}
	}

	DetailBuilder.EditCategory(ChaosVDCategoryName).InitiallyCollapsed(false);
}

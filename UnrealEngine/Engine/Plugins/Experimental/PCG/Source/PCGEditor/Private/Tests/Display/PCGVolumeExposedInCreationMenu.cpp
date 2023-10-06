// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/PCGTestsCommon.h"
#include "IPlacementModeModule.h"

#if WITH_AUTOMATION_TESTS

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGVolumeExposedInCreationMenu, FPCGTestBaseClass, "Editor.Plugins.Tools.PCG.PCGVolumeExposedInCreationMenu", PCGTestsCommon::TestFlags)

bool FPCGVolumeExposedInCreationMenu::RunTest(const FString& Parameters)
{
	// create menu queries place mode categories to dislplay 
	const IPlacementModeModule& PlacementModeModule = IPlacementModeModule::Get();
	// PCG volume is located in Volumes category
	const FName PlacementModeCategoryHandle = TEXT("Volumes");
	FString PCGDisplayVolumeName = TEXT("PCGVolume");
	TArray<TSharedPtr<FPlaceableItem>>OutItems;


	PlacementModeModule.GetItemsForCategory(PlacementModeCategoryHandle, OutItems);
	bool VolumeCategoryFound = OutItems.ContainsByPredicate([&PCGDisplayVolumeName](const TSharedPtr<FPlaceableItem>& Item) {
		return Item->DisplayName.ToString().Contains(PCGDisplayVolumeName);
		});

	TestTrue(TEXT("PCGVolume is not found in Create Menu"), VolumeCategoryFound);
	return true;
}

#endif 

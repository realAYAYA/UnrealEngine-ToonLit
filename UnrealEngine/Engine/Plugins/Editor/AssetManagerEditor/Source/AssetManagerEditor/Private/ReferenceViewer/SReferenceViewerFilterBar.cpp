// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReferenceViewer/SReferenceViewerFilterBar.h"

#include "CoreMinimal.h"

void SReferenceViewerFilterBar::SaveSettings()
{
	TArray<FilterState> SaveFilters;
	if (UReferenceViewerSettings* Settings = GetMutableDefault<UReferenceViewerSettings>())
	{
		// Only save & load user filters, not autofilters
		if (!Settings->AutoUpdateFilters())
		{
			for (TSharedRef<SAssetFilter> CurrentAssetFilter : AssetFilters)
			{
				FTopLevelAssetPath AssetFilterPath = CurrentAssetFilter->GetCustomClassFilterData()->GetClassPathName();
				SaveFilters.Add(FilterState(AssetFilterPath, CurrentAssetFilter->IsEnabled()));
			}
			Settings->SetUserFilters(SaveFilters);
		}
	}
}

void SReferenceViewerFilterBar::LoadSettings() 
{
	if (UReferenceViewerSettings* Settings = GetMutableDefault<UReferenceViewerSettings>())
	{
		// Only save & load user filters, not autofilters
		if (!Settings->AutoUpdateFilters())
		{
			TArray<FilterState> SavedFilters = Settings->GetUserFilters();
			RemoveAllFilters();
			for (FilterState& State : SavedFilters)
			{
				if (DoesAssetTypeFilterExist(State.FilterPath))
				{
					SetAssetTypeFilterCheckState(State.FilterPath, ECheckBoxState::Checked);
					ToggleAssetTypeFilterEnabled(State.FilterPath, State.bIsEnabled);
				}
			}
		}
	}
}


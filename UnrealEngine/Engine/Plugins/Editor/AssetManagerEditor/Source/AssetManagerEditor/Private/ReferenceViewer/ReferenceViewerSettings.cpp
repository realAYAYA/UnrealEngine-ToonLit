// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReferenceViewer/ReferenceViewerSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ReferenceViewerSettings)

bool UReferenceViewerSettings::IsSearchDepthLimited() const
{
	return bLimitSearchDepth;
}

bool UReferenceViewerSettings::IsSearchBreadthLimited() const
{
	return bLimitSearchBreadth;
}

bool UReferenceViewerSettings::IsShowSoftReferences() const
{
	return bIsShowSoftReferences;
}

bool UReferenceViewerSettings::IsShowHardReferences() const
{
	return bIsShowHardReferences;
}

bool UReferenceViewerSettings::IsShowFilteredPackagesOnly() const
{
	return bIsShowFilteredPackagesOnly;
}

bool UReferenceViewerSettings::IsCompactMode() const
{
	return bIsCompactMode;
}

bool UReferenceViewerSettings::IsShowDuplicates() const
{
	return bIsShowDuplicates;
}

bool UReferenceViewerSettings::IsShowEditorOnlyReferences() const
{
	return bIsShowEditorOnlyReferences;
}

bool UReferenceViewerSettings::IsShowManagementReferences() const
{
	return bIsShowManagementReferences;
}

bool UReferenceViewerSettings::IsShowSearchableNames() const
{
	return bIsShowSearchableNames;
}

bool UReferenceViewerSettings::IsShowCodePackages() const
{
	return bIsShowCodePackages;
}

bool UReferenceViewerSettings::IsShowReferencers() const
{
	return bIsShowReferencers;
}

bool UReferenceViewerSettings::IsShowDependencies() const
{
	return bIsShowDependencies;
}

void UReferenceViewerSettings::SetSearchDepthLimitEnabled(bool newEnabled)
{
	bLimitSearchDepth = newEnabled;
	SaveConfig();
}

void UReferenceViewerSettings::SetSearchBreadthLimitEnabled(bool newEnabled)
{
	bLimitSearchBreadth = newEnabled;
	SaveConfig();
}

void UReferenceViewerSettings::SetShowSoftReferencesEnabled(bool newEnabled)
{
	bIsShowSoftReferences = newEnabled;
	SaveConfig();
}

void UReferenceViewerSettings::SetShowHardReferencesEnabled(bool newEnabled)
{
	bIsShowHardReferences = newEnabled;
	SaveConfig();
}

void UReferenceViewerSettings::SetShowFilteredPackagesOnlyEnabled(bool newEnabled)
{
	bIsShowFilteredPackagesOnly = newEnabled;
	SaveConfig();
}

void UReferenceViewerSettings::SetCompactModeEnabled(bool newEnabled)
{
	bIsCompactMode = newEnabled;
	SaveConfig();
}

void UReferenceViewerSettings::SetShowDuplicatesEnabled(bool newEnabled)
{
	bIsShowDuplicates = newEnabled;
	SaveConfig();
}

void UReferenceViewerSettings::SetShowEditorOnlyReferencesEnabled(bool newEnabled)
{
	bIsShowEditorOnlyReferences = newEnabled;
	SaveConfig();
}

void UReferenceViewerSettings::SetShowManagementReferencesEnabled(bool newEnabled)
{
	bIsShowManagementReferences = newEnabled;
	SaveConfig();
}

void UReferenceViewerSettings::SetShowSearchableNames(bool newEnabled)
{
	bIsShowSearchableNames = newEnabled;
	SaveConfig();
}

void UReferenceViewerSettings::SetShowCodePackages(bool newEnabled)
{
	bIsShowCodePackages = newEnabled;
	SaveConfig();
}

void UReferenceViewerSettings::SetShowReferencers(const bool newEnabled)
{
	bIsShowReferencers = newEnabled;
	SaveConfig();
}

void UReferenceViewerSettings::SetShowDependencies(const bool newEnabled)
{
	bIsShowDependencies = newEnabled;
	SaveConfig();
}

int32 UReferenceViewerSettings::GetSearchDependencyDepthLimit() const
{
	return MaxSearchDependencyDepth;
}

void UReferenceViewerSettings::SetSearchDependencyDepthLimit(int32 NewDepthLimit, bool bSaveConfig)
{
	MaxSearchDependencyDepth = FMath::Max(NewDepthLimit, 0);
	if (bSaveConfig)
	{
		SaveConfig();
	}
}

int32 UReferenceViewerSettings::GetSearchReferencerDepthLimit() const
{
	return MaxSearchReferencerDepth;
}

void UReferenceViewerSettings::SetSearchReferencerDepthLimit(int32 NewDepthLimit, bool bSaveConfig)
{
	MaxSearchReferencerDepth = FMath::Max(NewDepthLimit, 0);
	if (bSaveConfig)
	{
		SaveConfig();
	}
}

int32 UReferenceViewerSettings::GetSearchBreadthLimit() const
{
	return MaxSearchBreadth;
}

void UReferenceViewerSettings::SetSearchBreadthLimit(int32 NewBreadthLimit)
{
	MaxSearchBreadth = FMath::Max(NewBreadthLimit, 0);
	SaveConfig();
}

bool UReferenceViewerSettings::GetEnableCollectionFilter() const
{
	return bEnableCollectionFilter;
}

void UReferenceViewerSettings::SetEnableCollectionFilter(bool bEnabled)
{
	bEnableCollectionFilter = bEnabled;
	SaveConfig();
}

bool UReferenceViewerSettings::IsShowPath() const
{
	return bIsShowPath;
}

void UReferenceViewerSettings::SetShowPathEnabled(bool bEnabled)
{
	bIsShowPath = bEnabled;
	SaveConfig();
}

bool UReferenceViewerSettings::GetFiltersEnabled() const
{
	return bFiltersEnabled;
}

void UReferenceViewerSettings::SetFiltersEnabled(bool newEnabled)
{
	bFiltersEnabled = newEnabled;
	SaveConfig();
}

bool UReferenceViewerSettings::AutoUpdateFilters() const
{
	return bAutoUpdateFilters;
}

void UReferenceViewerSettings::SetAutoUpdateFilters(bool bEnabled)
{
	bAutoUpdateFilters = bEnabled;
	SaveConfig();
}

const TArray<FilterState>& UReferenceViewerSettings::GetUserFilters() const
{
	return UserFilters;	
}

void UReferenceViewerSettings::SetUserFilters(TArray<FilterState>& InFilters)
{
	UserFilters = InFilters;
	SaveConfig();
}


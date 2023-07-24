// Copyright Epic Games, Inc. All Rights Reserved.

#include "Filters/SAssetFilterBar.h"

FFilterBarSettings* FFilterBarBase::GetMutableConfig()
{
	if(FilterBarIdentifier.IsNone())
	{
		return nullptr;
	}

	UFilterBarConfig* FilterBarConfig = GetMutableDefault<UFilterBarConfig>();
	return &UFilterBarConfig::Get()->FilterBars.FindOrAdd(FilterBarIdentifier);
}

const FFilterBarSettings* FFilterBarBase::GetConstConfig() const
{
	if(FilterBarIdentifier.IsNone())
	{
		return nullptr;
	}

	const UFilterBarConfig* FilterBarConfig = GetDefault<UFilterBarConfig>();
	return UFilterBarConfig::Get()->FilterBars.Find(FilterBarIdentifier);
}

void FFilterBarBase::SaveConfig()
{
	UFilterBarConfig::Get()->SaveEditorConfig();
}

void FFilterBarBase::InitializeConfig()
{
	UFilterBarConfig::Initialize();

	UFilterBarConfig::Get()->LoadEditorConfig();

	// Call GetMutableConfig to force create a config for this filter bar if the user specified FilterBarIdentifier
	FFilterBarSettings* FilterBarConfig = GetMutableConfig();
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementOutlinerFilter.h"

#include "TypedElementOutlinerMode.h"

FTEDSOutlinerFilter::FTEDSOutlinerFilter(const FName& InFilterName, TSharedPtr<FFilterCategory> InCategory, FTypedElementOutlinerMode* InTEDSOutlinerMode, const TypedElementDataStorage::FQueryDescription& InFilterQuery)
	: FFilterBase(InCategory)
	, FilterName(InFilterName)
	, TEDSOutlinerMode(InTEDSOutlinerMode)
	, FilterQuery(InFilterQuery)
{
	
}

FString FTEDSOutlinerFilter::GetName() const
{
	return FilterName.ToString();
}

FText FTEDSOutlinerFilter::GetDisplayName() const
{
	return FText::FromName(FilterName);
}

FText FTEDSOutlinerFilter::GetToolTipText() const
{
	return FText::FromName(FilterName);
}

FLinearColor FTEDSOutlinerFilter::GetColor() const
{
	return FLinearColor();	
}

FName FTEDSOutlinerFilter::GetIconName() const
{
	return FName();
}

bool FTEDSOutlinerFilter::IsInverseFilter() const
{
	return false;
}

void FTEDSOutlinerFilter::ActiveStateChanged(bool bActive)
{
	if(bActive)
	{
		TEDSOutlinerMode->AddExternalQuery(FilterName, FilterQuery);
	}
	else
	{
		TEDSOutlinerMode->RemoveExternalQuery(FilterName);
	}
}

void FTEDSOutlinerFilter::ModifyContextMenu(FMenuBuilder& MenuBuilder)
{
	
}

void FTEDSOutlinerFilter::SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) const
{
	
}

void FTEDSOutlinerFilter::LoadSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString)
{
	
}

bool FTEDSOutlinerFilter::PassesFilter(SceneOutliner::FilterBarType InItem) const
{
	return true; // The filter is applied through a TEDS query and this is just a dummy to activate it, so we can simply return true
}

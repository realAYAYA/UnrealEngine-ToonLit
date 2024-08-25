// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleEditorData.h"

#include "Algo/Find.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleFaderGroup.h"
#include "Library/DMXEntityFixturePatch.h"


void UDMXControlConsoleEditorData::AddUserFilter(const FString& FilterLabel, const FString& FilterString, const FLinearColor FilterColor, bool bIsEnabled)
{
	FDMXControlConsoleEditorUserFilter NewUserFilter;
	NewUserFilter.FilterLabel = FilterLabel;
	NewUserFilter.FilterString = FilterString;
	NewUserFilter.FilterColor = FilterColor;
	NewUserFilter.bIsEnabled = bIsEnabled;

	FiltersCollection.UserFilters.Add(NewUserFilter);

	OnUserFiltersChanged.Broadcast();
}

void UDMXControlConsoleEditorData::RemoveUserFilter(const FString& FilterLabel)
{
	FiltersCollection.UserFilters.RemoveAll(
		[FilterLabel](const FDMXControlConsoleEditorUserFilter& UserFilter)
		{
			return UserFilter.FilterLabel == FilterLabel;
		});

	OnUserFiltersChanged.Broadcast();
}

FDMXControlConsoleEditorUserFilter* UDMXControlConsoleEditorData::FindUserFilter(const FString& FilterLabel)
{
	return Algo::FindBy(FiltersCollection.UserFilters, FilterLabel, &FDMXControlConsoleEditorUserFilter::FilterLabel);
}

void UDMXControlConsoleEditorData::UpdateFilters(UDMXControlConsoleData* ControlConsoleData)
{
	if (!ControlConsoleData)
	{
		return;
	}

	// Reset all filter strings array
	FiltersCollection.AttributeNameFilterStrings.Reset();
	FiltersCollection.UniverseIDFilterStrings.Reset();
	FiltersCollection.FixtureIDFilterStrings.Reset();

	const TArray<UDMXControlConsoleFaderGroup*> AllFaderGroups = ControlConsoleData->GetAllFaderGroups();
	for (const UDMXControlConsoleFaderGroup* FaderGroup : AllFaderGroups)
	{
		if (!FaderGroup)
		{
			continue;
		}

		const UDMXEntityFixturePatch* FixturePatch = FaderGroup->GetFixturePatch();
		if (!FixturePatch)
		{
			continue;
		}

		// Add Universe ID filters
		const int32 UniverseID = FixturePatch->GetUniverseID();
		const FString UniverseIDAsString = TEXT("Uni ") + FString::FromInt(UniverseID);
		FiltersCollection.UniverseIDFilterStrings.AddUnique(UniverseIDAsString);

		// Add Fixture ID filters
		int32 FixtureID;
		if (FixturePatch->FindFixtureID(FixtureID))
		{
			const FString FIDAsString = FString::FromInt(FixtureID);
			FiltersCollection.FixtureIDFilterStrings.AddUnique(FIDAsString);
		}

		// Add Attribute Name filters
		const TMap<FDMXAttributeName, FDMXFixtureFunction> AttributeNameToFixtureFuncionMap = FixturePatch->GetAttributeFunctionsMap();
		for (const TTuple<FDMXAttributeName, FDMXFixtureFunction>& AttributeNameToFixtureFuncion : AttributeNameToFixtureFuncionMap)
		{
			const FName& AttributeName = AttributeNameToFixtureFuncion.Key.Name;
			const FString AttributeNameAsString = AttributeName.ToString();
			FiltersCollection.AttributeNameFilterStrings.AddUnique(AttributeNameAsString);
		}

		FDMXFixtureMatrix FixtureMatrix;
		if (FixturePatch->GetMatrixProperties(FixtureMatrix))
		{
			const TArray<FDMXFixtureCellAttribute>& CellAttriubutes = FixtureMatrix.CellAttributes;
			for (const FDMXFixtureCellAttribute& CellAttribute : CellAttriubutes)
			{
				const FName& AttributeName = CellAttribute.Attribute.Name;
				const FString AttributeNameAsString = AttributeName.ToString();
				FiltersCollection.AttributeNameFilterStrings.AddUnique(AttributeNameAsString);
			}
		}
	}
}

const TArray<FDMXControlConsoleEditorUserFilter>& UDMXControlConsoleEditorData::GetUserFilters() const
{
	return FiltersCollection.UserFilters;
}

const TArray<FString>& UDMXControlConsoleEditorData::GetAttributeNameFilters() const
{
	return FiltersCollection.AttributeNameFilterStrings;
}

const TArray<FString>& UDMXControlConsoleEditorData::GetUniverseIDFilters() const
{
	return FiltersCollection.UniverseIDFilterStrings;
}

const TArray<FString>& UDMXControlConsoleEditorData::GetFixtureIDFilters() const
{
	return FiltersCollection.FixtureIDFilterStrings;
}

void UDMXControlConsoleEditorData::SetFaderGroupsViewMode(EDMXControlConsoleEditorViewMode ViewMode)
{
	FaderGroupsViewMode = ViewMode;
	OnFaderGroupsViewModeChanged.Broadcast();
}

void UDMXControlConsoleEditorData::SetFadersViewMode(EDMXControlConsoleEditorViewMode ViewMode)
{
	FadersViewMode = ViewMode;
	OnFadersViewModeChanged.Broadcast();
}

void UDMXControlConsoleEditorData::ToggleAutoGroupActivePatches()
{
	bAutoGroupActivePatches = !bAutoGroupActivePatches;

	OnAutoGroupStateChanged.Broadcast();
}

void UDMXControlConsoleEditorData::ToggleAutoSelectActivePatches()
{
	bAutoSelectActivePatches = !bAutoSelectActivePatches;
}

void UDMXControlConsoleEditorData::ToggleAutoSelectFilteredElements()
{
	bAutoSelectFilteredElements = !bAutoSelectFilteredElements;
}

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXControlConsoleEditorDataBase.h"
#include "Widgets/SDMXReadOnlyFixturePatchList.h"

#include "DMXControlConsoleEditorData.generated.h"

class UDMXControlConsoleData;


/** Enum for DMX Control Console control modes */
UENUM()
enum class EDMXControlConsoleEditorControlMode : uint8
{
	Relative,
	Absolute
};

/** Enum for DMX Control Console view modes */
UENUM()
enum class EDMXControlConsoleEditorViewMode : uint8
{
	Collapsed,
	Expanded
};

/** Enum for DMX Control Console value types */
UENUM()
enum class EDMXControlConsoleEditorValueType : uint8
{
	Byte,
	Normalized
};

/** Struct which describes a User Filter */
USTRUCT()
struct FDMXControlConsoleEditorUserFilter
{
	GENERATED_BODY()

	/** The filter name displayed in the editor */
	UPROPERTY()
	FString FilterLabel;

	/** The filter string used in the filtering system */
	UPROPERTY()
	FString FilterString;

	/** The color showed by the filter in the editor */
	UPROPERTY()
	FLinearColor FilterColor = FLinearColor::White;

	/** True if the filter is enabled */
	UPROPERTY()
	bool bIsEnabled = false;
};

/** Struct for collecting DMX Control Console filter strings */
USTRUCT()
struct FDMXControlConsoleEditorFiltersCollection
{
	GENERATED_BODY()

	/** The array of custom filters created by the user */
	UPROPERTY()
	TArray<FDMXControlConsoleEditorUserFilter> UserFilters;

	/** The array of attribute name filter strings based on the current Control Console Data */
	UPROPERTY()
	TArray<FString> AttributeNameFilterStrings;

	/** The array of universe id filter strings based on the current Control Console Data */
	UPROPERTY()
	TArray<FString> UniverseIDFilterStrings;

	/** The array of fixture id filter strings based on the current Control Console Data */
	UPROPERTY()
	TArray<FString> FixtureIDFilterStrings;
};

/** Control Console container class for editor data */
UCLASS()
class UDMXControlConsoleEditorData
	: public UDMXControlConsoleEditorDataBase
{
	GENERATED_BODY()

public:
	/** Adds a new User Filter with the given parameters */
	void AddUserFilter(const FString& FilterLabel, const FString& FilterString, const FLinearColor FilterColor, bool bIsEnabled);

	/** Removes all the User Filters with the given filter name */
	void RemoveUserFilter(const FString& FilterLabel);

	/** Finds the User Filter which matches the given filter label, or nullptr if none exists */
	FDMXControlConsoleEditorUserFilter* FindUserFilter(const FString& FilterLabel);

	/** Updates the filters collection based on the given Control Console Data */
	void UpdateFilters(UDMXControlConsoleData* ControlConsoleData);

	/** Gets the array of the User created filter strings. */
	const TArray<FDMXControlConsoleEditorUserFilter>& GetUserFilters() const;

	/** Gets the array of Attribute Name filter strings besed on the current Control Console Data */
	const TArray<FString>& GetAttributeNameFilters() const;

	/** Gets the array of Universe ID filter strings besed on the current Control Console Data */
	const TArray<FString>& GetUniverseIDFilters() const;

	/** Gets the array of Fixture ID filter strings besed on the current Control Console Data */
	const TArray<FString>& GetFixtureIDFilters() const;

	/** Gets the current Control Mode for Faders */
	EDMXControlConsoleEditorControlMode GetControlMode() const { return ControlMode; }

	/** Sets the current Control Mode for Faders */
	void SetControlMode(EDMXControlConsoleEditorControlMode NewControlMode) { ControlMode = NewControlMode; }

	/** Gets the current View Mode for Fader Groups */
	EDMXControlConsoleEditorViewMode GetFaderGroupsViewMode() const { return FaderGroupsViewMode; }

	/** Sets the current View Mode for Fader Groups */
	void SetFaderGroupsViewMode(EDMXControlConsoleEditorViewMode ViewMode);

	/** Gets the current View Mode for Faders */
	EDMXControlConsoleEditorViewMode GetFadersViewMode() const { return FadersViewMode; }

	/** Sets the current View Mode for Faders */
	void SetFadersViewMode(EDMXControlConsoleEditorViewMode ViewMode);

	/** Gets the current Value Type for Faders */
	EDMXControlConsoleEditorValueType GetValueType() const { return ValueType; }

	/** Sets the current Value Type for Faders */
	void SetValueType(EDMXControlConsoleEditorValueType NewValueType) { ValueType = NewValueType; }

	/** Gets the current auto-grouping state for the activated Fader Groups */
	bool GetAutoGroupActivePatches() const { return bAutoGroupActivePatches; }

	/** Gets the current auto-selection state for the activated Fader Groups */
	bool GetAutoSelectActivePatches() const { return bAutoSelectActivePatches; }

	/** Gets the current auto-selection state for the filtered Elements */
	bool GetAutoSelectFilteredElements() const { return bAutoSelectFilteredElements; }

	/** Toggles the auto-grouping state for the activated Fader Groups */
	void ToggleAutoGroupActivePatches();

	/** Toggles the auto-selection state for the activated Fader Groups */
	void ToggleAutoSelectActivePatches();

	/** Toggles the auto-selection state for the filtered Elements */
	void ToggleAutoSelectFilteredElements();

	/** Returns a delegate broadcast whenever the list of User Filters has been changed */
	FSimpleMulticastDelegate& GetOnUserFiltersChanged() { return OnUserFiltersChanged; }

	/** Returns a delegate broadcast whenever the Fader Groups view mode is changed */
	FSimpleMulticastDelegate& GetOnFaderGroupsViewModeChanged() { return OnFaderGroupsViewModeChanged; }

	/** Returns a delegate broadcast whenever the Faders view mode is changed */
	FSimpleMulticastDelegate& GetOnFadersViewModeChanged() { return OnFadersViewModeChanged; }

	/** Returns a delegate broadcast whenever the auto-group state is changed */
	FSimpleMulticastDelegate& GetOnAutoGroupStateChanged() { return OnAutoGroupStateChanged; }

	/** Fixture Patch List default descriptor */
	UPROPERTY()
	FDMXReadOnlyFixturePatchListDescriptor FixturePatchListDescriptor;

private:
	/** Called when the list of User Filters has changed */
	FSimpleMulticastDelegate OnUserFiltersChanged;

	/** Called when the Fader Groups view mode is changed */
	FSimpleMulticastDelegate OnFaderGroupsViewModeChanged;

	/** Called when the Faders view mode is changed */
	FSimpleMulticastDelegate OnFadersViewModeChanged;

	/** Called when the auto-gorup state has changed */
	FSimpleMulticastDelegate OnAutoGroupStateChanged;

	/** Collection of filters based on the Control Console Data */
	UPROPERTY()
	FDMXControlConsoleEditorFiltersCollection FiltersCollection;

	/** Current control mode for Faders widgets */
	UPROPERTY()
	EDMXControlConsoleEditorControlMode ControlMode = EDMXControlConsoleEditorControlMode::Absolute;

	/** Current view mode for FaderGroupView widgets*/
	UPROPERTY()
	EDMXControlConsoleEditorViewMode FaderGroupsViewMode = EDMXControlConsoleEditorViewMode::Expanded;

	/** Current view mode for Faders widgets */
	UPROPERTY()
	EDMXControlConsoleEditorViewMode FadersViewMode = EDMXControlConsoleEditorViewMode::Collapsed;
	
	/** Current value type for Faders widgets */
	UPROPERTY()
	EDMXControlConsoleEditorValueType ValueType = EDMXControlConsoleEditorValueType::Byte;

	UPROPERTY()
	/** True if the Fader Groups from activated Fixture Patches must be grouped by default */
	bool bAutoGroupActivePatches = false;

	UPROPERTY()
	/** True if the Fader Groups from activated Fixture Patches must be selected by default */
	bool bAutoSelectActivePatches = false;

	UPROPERTY()
	/** True if the filtered Elements must be selected by default */
	bool bAutoSelectFilteredElements = false;
};

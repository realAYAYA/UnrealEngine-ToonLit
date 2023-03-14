// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Views/List/ObjectMixerEditorListRow.h"

class OBJECTMIXEREDITOR_API IObjectMixerEditorListFilter
{
public:

	enum class EObjectMixerEditorListFilterMatchType
	{
		// Filters of this type are placed into a group in which if any of the group's filters pass, the row is considered passing
		MatchAny,
		// Filters of this type are placed into a group in which all of the group's filters must pass for the row to be considered passing
		MatchAll
	};
	
	virtual ~IObjectMixerEditorListFilter() = default;

	virtual FString GetFilterName() const
	{
		return "";
	}

	/** Return true if the filter should be able to be toggled on/off in the Show Options. Can still be toggled by code. */
	virtual bool IsUserToggleable() const
	{
		return false;
	}

	/** Returns localized text to display for when this filter' active state is defined by a toggle button. */
	virtual FText GetFilterButtonLabel() const
	{
		return FText::GetEmpty();
	}
	
	/** Returns localized text to display when the user mouses over a toggle button that defines whether this filter is active. */
	virtual FText GetFilterButtonToolTip() const
	{
		return FText::GetEmpty();
	}
	
	void SetFilterActive(const bool bNewEnabled)
	{
		bIsFilterActive = bNewEnabled;
	}

	void ToggleFilterActive()
	{
		bIsFilterActive = !bIsFilterActive;
	}

	bool GetIsFilterActive() const
	{
		return bIsFilterActive;
	}

	void SetFilterMatchType(const EObjectMixerEditorListFilterMatchType InType)
	{
		FilterMatchType = InType;
	}

	EObjectMixerEditorListFilterMatchType GetFilterMatchType() const
	{
		return FilterMatchType;
	}

	virtual bool DoesItemPassFilter(const FObjectMixerEditorListRowPtr& InItem) const
	{
		return false;
	}

private:

	bool bIsFilterActive = false;
	EObjectMixerEditorListFilterMatchType FilterMatchType = EObjectMixerEditorListFilterMatchType::MatchAny;
			
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineTypes.h"
#include "Widgets/SCompoundWidget.h"

class SWrapBox;
class UDMXControlConsoleEditorModel;


namespace UE::DMX::Private
{
	class FDMXControlConsoleEditorToolbar;
	class FDMXControlConsoleFilterModel;

	/** Enum for DMX Control Console filter categories */
	enum class EDMXControlConsoleFilterCategory : uint8
	{
		User,
		AttributeName,
		UniverseID,
		FixtureID
	};

	/** View for displaying the filters collection of a DMX Control Console */
	class SDMXControlConsoleEditorFiltersView
		: public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDMXControlConsoleEditorFiltersView)
			{}

		SLATE_END_ARGS()

		/** Constructs the widget */
		void Construct(const FArguments& InArgs, const TSharedPtr<FDMXControlConsoleEditorToolbar> EditorToolbar, UDMXControlConsoleEditorModel* InEditorModel);

	private:
		/** Requests the refreshing of the filters view */
		void RequestRefresh();

		/** Updates all the filter models based on the Control Console Editor Data */
		void UpdateFilterButtons();

		/** Updates the user filter models based on the Control Console Editor Data */
		void UpdateUserFilterButtons();

		/** Updates the attribute name filter models based on the Control Console Editor Data */
		void UpdateAttributeNameFilterButtons();

		/** Updates the universe ID filter models based on the Control Console Editor Data */
		void UpdateUniverseIDFilterButtons();

		/** Updates the fixture ID filter models based on the Control Console Editor Data */
		void UpdateFixtureIDFilterButtons();

		/** Called whenever the state of a filter has changed */
		void OnFilterStateChanged(TSharedPtr<FDMXControlConsoleFilterModel> FilterModel);

		/** Called to disable all filters */
		void OnDisableAllFilters();

		/** Sets the visibility of a filters box, depending on the given filter category */
		void OnSetFiltersBoxVisibility(bool bIsNotVisible, EDMXControlConsoleFilterCategory FilterCategory);

		/** Timer handle in use while refreshing the filters view is requested but not carried out yet */
		FTimerHandle RefreshFiltersViewTimerHandle;

		/** Container box widget for the User filters */
		TSharedPtr<SWrapBox> UserFiltersBox;

		/** Container box widget for the Attribute Name filters */
		TSharedPtr<SWrapBox> AttributeNameFiltersBox;

		/** Container box widget for the Universe ID filters */
		TSharedPtr<SWrapBox> UniverseIDFiltersBox;

		/** Container box widget for the Fixture ID filters */
		TSharedPtr<SWrapBox> FixtureIDFiltersBox;

		/** The array of models used to describe all the filters in the Control Console */
		TArray<TSharedPtr<FDMXControlConsoleFilterModel>> FilterModels;

		/** Weak reference to the editor Toolbar */
		TWeakPtr<FDMXControlConsoleEditorToolbar> WeakToolbarPtr;

		/** Weak reference to the Control Console Editor Model */
		TWeakObjectPtr<UDMXControlConsoleEditorModel> EditorModel;
	};
}

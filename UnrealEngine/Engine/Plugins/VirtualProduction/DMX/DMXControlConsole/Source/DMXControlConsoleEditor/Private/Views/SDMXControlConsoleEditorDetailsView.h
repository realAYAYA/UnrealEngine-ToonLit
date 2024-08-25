// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "UObject/WeakObjectPtrTemplates.h"

class IDetailsView;
class FDelegateHandle;
class SDockTab;
class UDMXControlConsoleEditorModel;


namespace UE::DMX::Private
{
	/** View for displaying the details of the elements in the edited Control Console */
	class SDMXControlConsoleEditorDetailsView
		: public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDMXControlConsoleEditorDetailsView)
			{}

		SLATE_END_ARGS()

		/** Destructor */
		~SDMXControlConsoleEditorDetailsView();

		/** Constructs the widget */
		void Construct(const FArguments& InArgs, UDMXControlConsoleEditorModel* InEditorModel);

	private:
		/** Requests to update the Details Views on the next tick */
		void RequestUpdateDetailsViews();

		/** Updates the Details Views */
		void ForceUpdateDetailsViews();

		/** Called when the active tab in the editor changes */
		void OnActiveTabChanged(TSharedPtr<SDockTab> PreviouslyActive, TSharedPtr<SDockTab> NewlyActivated);

		/** Searches this widget's parents to see if it's a child of InDockTab */
		bool IsWidgetInTab(TSharedPtr<SDockTab> InDockTab, TSharedPtr<SWidget> InWidget) const;

		/** True if slate throttling is currently disabled */
		bool bThrottleDisabled = false;

		/** Delegate handle bound to the FGlobalTabmanager::OnActiveTabChanged delegate */
		FDelegateHandle OnActiveTabChangedDelegateHandle;

		/** Timer handle in use while updating details views is requested but not carried out yet */
		FTimerHandle UpdateDetailsViewTimerHandle;

		/** Shows details of the current selected Fader Group Controllers */
		TSharedPtr<IDetailsView> FaderGroupControllersDetailsView;

		/** Shows details of the current selected Fader Groups */
		TSharedPtr<IDetailsView> FaderGroupsDetailsView;

		/** Shows details of the current selected Element Controllers */
		TSharedPtr<IDetailsView> ElementControllersDetailsView;

		/** Shows details of the current selected Faders */
		TSharedPtr<IDetailsView> FadersDetailsView;

		/** Weak reference to the Control Console editor model */
		TWeakObjectPtr<UDMXControlConsoleEditorModel> EditorModel;
	};
}

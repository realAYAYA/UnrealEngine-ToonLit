// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Analytics/DMXEditorToolAnalyticsProvider.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "UObject/GCObject.h"

enum class EDMXControlConsoleStopDMXMode : uint8;
class FSpawnTabArgs;
class FTabManager;
class SDockableTab;
class UDMXControlConsole;
class UDMXControlConsoleData;
class UDMXControlConsoleEditorData;
class UDMXControlConsoleEditorLayouts;
class UDMXControlConsoleEditorModel;


namespace UE::DMX::Private
{
	class SDMXControlConsoleEditorDetailsView;
	class SDMXControlConsoleEditorDMXLibraryView;
	class SDMXControlConsoleEditorFiltersView;
	class SDMXControlConsoleEditorLayoutView;
	class FDMXControlConsoleEditorToolbar;

	/** Implements an Editor toolkit for Control Console. */
	class FDMXControlConsoleEditorToolkit
		: public FAssetEditorToolkit
		, public FGCObject
	{
	public:
		FDMXControlConsoleEditorToolkit();
		virtual ~FDMXControlConsoleEditorToolkit();

		/**
		 * Edits the specified control console object.
		 *
		 * @param Mode The tool kit mode.
		 * @param InitToolkitHost
		 * @param ObjectToEdit The control console object to edit.
		 */
		void InitControlConsoleEditor(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost>& InitToolkitHost, UDMXControlConsole* InControlConsole);

		/** Returns the edited Control Console */
		UDMXControlConsole* GetControlConsole() const { return ControlConsole; }

		/** Returns the edited Control Console Data */
		UDMXControlConsoleData* GetControlConsoleData() const;

		/** Returns the edited Control Console Editor Data */
		UDMXControlConsoleEditorData* GetControlConsoleEditorData() const;

		/** Returns the edited Control Console Layouts */
		UDMXControlConsoleEditorLayouts* GetControlConsoleLayouts() const;

		/** Returns the Control Console Editor Model, if valid */
		UDMXControlConsoleEditorModel* GetControlConsoleEditorModel() const { return EditorModel; }

		/** Removes all selected elements from DMX Control Console */
		void RemoveAllSelectedElements();

		/** Clears the DMX Control Console and all its elements */
		void ClearAll();

		/** Resets all the elements in the Control Console to their default values */
		void ResetToDefault();

		/** Resets all the elements in the Control Console to zero */
		void ResetToZero();

		/** Name of the DMX Library View Tab */
		static const FName DMXLibraryViewTabID;

		/** Name of the Layout View Tab */
		static const FName LayoutViewTabID;

		/** Name of the Details View Tab */
		static const FName DetailsViewTabID;

		/** Name of the Filters View Tab */
		static const FName FiltersViewTabID;

	protected:
		//~ Begin FAssetEditorToolkit Interface
		virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
		virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
		virtual const FSlateBrush* GetDefaultTabIcon() const override;
		//~ End FAssetEditorToolkit Interface

		//~ Begin IToolkit Interface
		virtual FText GetBaseToolkitName() const override;
		virtual FName GetToolkitFName() const override;
		virtual FLinearColor GetWorldCentricTabColorScale() const override { return FLinearColor(0.0f, 0.0f, 0.2f, 0.5f); }
		virtual FString GetWorldCentricTabPrefix() const override;
		//~ End IToolkit Interface

		// FGCObject interface
		virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
		virtual FString GetReferencerName() const override;
		// End of FGCObject interface

	private:
		/** Internally initializes the toolkit */
		void InitializeInternal(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost>& InitToolkitHost, const FGuid& MessageLogGuid);

		/** Generates all the views of the asset toolkit */
		void GenerateInternalViews();

		/** Generates the DMX Library View for this Control Console instance */
		TSharedRef<SDMXControlConsoleEditorDMXLibraryView> GenerateDMXLibraryView();

		/** Generates the Layout View for this Control Console instance */
		TSharedRef<SDMXControlConsoleEditorLayoutView> GenerateLayoutView();

		/** Generates the Details View for this Control Console instance */
		TSharedRef<SDMXControlConsoleEditorDetailsView> GenerateDetailsView();

		/** Generates the Filters View for this Control Console instance */
		TSharedRef<SDMXControlConsoleEditorFiltersView> GenerateFiltersView();

		/** Spawns the DMX Library View */
		TSharedRef<SDockTab> SpawnTab_DMXLibraryView(const FSpawnTabArgs& Args);

		/** Spawns the Layout View */
		TSharedRef<SDockTab> SpawnTab_LayoutView(const FSpawnTabArgs& Args);

		/** Spawns the Details View */
		TSharedRef<SDockTab> SpawnTab_DetailsView(const FSpawnTabArgs& Args);

		/** Spawns the Filters View */
		TSharedRef<SDockTab> SpawnTab_FiltersView(const FSpawnTabArgs& Args);

		/** Setups the asset toolkit's commands */
		void SetupCommands();

		/** Extends the asset toolkit's toolbar */
		void ExtendToolbar();

		/** Starts to play DMX */
		void PlayDMX();

		/** Returns true if the console currently sends DMX */
		bool IsPlayingDMX() const;

		/** Pauses playing DMX. Current DMX values will still be sent at a lower rate. */
		void PauseDMX();

		/** Stops playing DMX */
		void StopPlayingDMX();

		/** Toggles between playing and pausing DMX */
		void TogglePlayPauseDMX();

		/** Toggles between playing and stopping DMX */
		void TogglePlayStopDMX();

		/** Sets the stop mode for the asset being edited */
		void SetStopDMXMode(EDMXControlConsoleStopDMXMode StopDMXMode);

		/** Returns true if console uses tested stop mode */
		bool IsUsingStopDMXMode(EDMXControlConsoleStopDMXMode TestStopMode) const;

		/** True when sending DMX is paused */
		bool bPaused = false;

		/** Reference to this asset toolkit's toolbar */
		TSharedPtr<FDMXControlConsoleEditorToolbar> Toolbar;

		/** The DMX Library View instance */
		TSharedPtr<SDMXControlConsoleEditorDMXLibraryView> DMXLibraryView;

		/** The Layout View instance */
		TSharedPtr<SDMXControlConsoleEditorLayoutView> LayoutView;

		/** The Details View instance */
		TSharedPtr<SDMXControlConsoleEditorDetailsView> DetailsView;

		/** The Filters View instance */
		TSharedPtr<SDMXControlConsoleEditorFiltersView> FiltersView;

		/** The Editor Model for the Control Console this toolkit is based on */
		TObjectPtr<UDMXControlConsoleEditorModel> EditorModel;

		/** The Control Console object this toolkit is based on */
		TObjectPtr<UDMXControlConsole> ControlConsole;

		/** The analytics provider for this tool */
		UE::DMX::FDMXEditorToolAnalyticsProvider AnalyticsProvider;
	};
}

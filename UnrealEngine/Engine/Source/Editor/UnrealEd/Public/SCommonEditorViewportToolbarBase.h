// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "SViewportToolBar.h"
#include "IPreviewProfileController.h"

// This is the interface that the host of a SCommonEditorViewportToolbarBase must implement
class ICommonEditorViewportToolbarInfoProvider
{
public:
	// Get the viewport widget
	virtual TSharedRef<class SEditorViewport> GetViewportWidget() = 0;

// 	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
// 	TSharedPtr<FExtender> LevelEditorExtenders = LevelEditorModule.GetMenuExtensibilityManager()->GetAllExtenders();
	virtual TSharedPtr<FExtender> GetExtenders() const = 0;

	// Called to inform the host that a button was clicked (typically used to focus on a particular viewport in a multi-viewport setup)
	virtual void OnFloatingButtonClicked() = 0;
};

namespace CommonEditorViewportUtils
{
	struct FShowMenuCommand
	{
		TSharedPtr<FUICommandInfo> ShowMenuItem;
		FText LabelOverride;

		FShowMenuCommand(TSharedPtr<FUICommandInfo> InShowMenuItem, const FText& InLabelOverride)
			: ShowMenuItem(InShowMenuItem)
			, LabelOverride(InLabelOverride)
		{
		}

		FShowMenuCommand(TSharedPtr<FUICommandInfo> InShowMenuItem)
			: ShowMenuItem(InShowMenuItem)
		{
		}
	};

	static inline void FillShowMenu(class FMenuBuilder& MenuBuilder, TArray<FShowMenuCommand> MenuCommands, int32 EntryOffset)
	{
		// Generate entries for the standard show flags
		// Assumption: the first 'n' entries types like 'Show All' and 'Hide All' buttons, so insert a separator after them
		for (int32 EntryIndex = 0; EntryIndex < MenuCommands.Num(); ++EntryIndex)
		{
			MenuBuilder.AddMenuEntry(MenuCommands[EntryIndex].ShowMenuItem, NAME_None, MenuCommands[EntryIndex].LabelOverride);
			if (EntryIndex == EntryOffset - 1)
			{
				MenuBuilder.AddMenuSeparator();
			}
		}
	}
}

/**
 * A viewport toolbar widget for an asset or level editor that is placed in a viewport
 */
class SCommonEditorViewportToolbarBase : public SViewportToolBar
{
public:
	SLATE_BEGIN_ARGS(SCommonEditorViewportToolbarBase)
		: _AddRealtimeButton(false)
		, _PreviewProfileController(nullptr)
		{}

		SLATE_ARGUMENT(bool, AddRealtimeButton)
		SLATE_ARGUMENT(TSharedPtr<IPreviewProfileController>, PreviewProfileController) // Should be null if the Preview doesn't require profile.
	SLATE_END_ARGS()

	UNREALED_API virtual ~SCommonEditorViewportToolbarBase();

	UNREALED_API void Construct(const FArguments& InArgs, TSharedPtr<class ICommonEditorViewportToolbarInfoProvider> InInfoProvider);

	/** Build the screen percentage menu. */
	static UNREALED_API void ConstructScreenPercentageMenu(FMenuBuilder& MenuBuilder, class FEditorViewportClient* ViewportClient);

private:
	/**
	 * Returns the label for the "Camera" tool bar menu, which changes depending on the viewport type
	 *
	 * @return	Label to use for this menu label
	 */
	UNREALED_API FText GetCameraMenuLabel() const;


	/**
	 * Returns the label for the "View" tool bar menu, which changes depending on viewport show flags
	 *
	 * @return	Label to use for this menu label
	 */
	UNREALED_API FText GetViewMenuLabel() const;

	/**
	 * Generates the toolbar options menu content 
	 *
	 * @return The widget containing the options menu content
	 */
	UNREALED_API TSharedRef<SWidget> GenerateOptionsMenu() const;

	/**
	 * Generates the toolbar camera menu content 
	 *
	 * @return The widget containing the view menu content
	 */
	UNREALED_API TSharedRef<SWidget> GenerateCameraMenu() const;

	/**
	 * Generates the toolbar view menu content 
	 *
	 * @return The widget containing the view menu content
	 */
	UNREALED_API TSharedRef<SWidget> GenerateViewMenu() const;

	/**
	 * Generates the toolbar show menu content 
	 *
	 * @return The widget containing the show menu content
	 */
	UNREALED_API virtual TSharedRef<SWidget> GenerateShowMenu() const;

	/**
	 * Returns the initial visibility of the view mode options widget 
	 *
	 * @return The visibility value
	 */
	UNREALED_API EVisibility GetViewModeOptionsVisibility() const;

	/**
	 * Generates the toolbar view param menu content 
	 *
	 * @return The widget containing the show menu content
	 */
	UNREALED_API TSharedRef<SWidget> GenerateViewModeOptionsMenu() const;

	/**
	 * @return The widget containing the perspective only FOV window.
	 */
	UNREALED_API TSharedRef<SWidget> GenerateFOVMenu() const;

	/** Called by the FOV slider in the perspective viewport to get the FOV value */
	UNREALED_API float OnGetFOVValue() const;

	/**
	 * @return The widget containing the far view plane slider.
	 */
	UNREALED_API TSharedRef<SWidget> GenerateFarViewPlaneMenu() const;

	/** Called by the far view plane slider in the perspective viewport to get the far view plane value */
	UNREALED_API float OnGetFarViewPlaneValue() const;

	/** Called when the far view plane slider is adjusted in the perspective viewport */
	UNREALED_API void OnFarViewPlaneValueChanged( float NewValue );

	/** Called when we click the realtime warning */
	UNREALED_API FReply OnRealtimeWarningClicked();
	/** Called to determine if we should show the realtime warning */
	UNREALED_API EVisibility GetRealtimeWarningVisibility() const;

protected:
	// Merges the extender list from the host with the specified extender and returns the results
	UNREALED_API TSharedPtr<FExtender> GetCombinedExtenderList(TSharedRef<FExtender> MenuExtender) const;

	/** Gets the extender for the view menu */
	UNREALED_API virtual TSharedPtr<FExtender> GetViewMenuExtender() const;

	UNREALED_API void CreateViewMenuExtensions(FMenuBuilder& MenuBuilder);

	/** Extension allowing derived classes to add to the options menu.*/	
	virtual void ExtendOptionsMenu(FMenuBuilder& OptionsMenuBuilder) const {}

	/** Extension allowing derived classes to add to left-aligned portion of the toolbar slots.*/
	virtual void ExtendLeftAlignedToolbarSlots(TSharedPtr<SHorizontalBox> MainBoxPtr, TSharedPtr<SViewportToolBar> ParentToolBarPtr) const {}

protected:
	// Returns the info provider for this viewport
	UNREALED_API ICommonEditorViewportToolbarInfoProvider& GetInfoProvider() const;

	// Get the viewport client
	UNREALED_API class FEditorViewportClient& GetViewportClient() const;

protected:
	// Creates the view menu widget (override point for children)
	UNREALED_API virtual TSharedRef<class SEditorViewportViewMenu> MakeViewMenu();

	UNREALED_API FText GetScalabilityWarningLabel() const;
	UNREALED_API EVisibility GetScalabilityWarningVisibility() const;
	UNREALED_API TSharedRef<SWidget> GetScalabilityWarningMenuContent() const;
	virtual bool GetShowScalabilityMenu() const
	{
		return false;
	}
	/** Called when the FOV slider is adjusted in the perspective viewport */
	UNREALED_API virtual void OnFOVValueChanged(float NewValue) const;

	/** Called when the ScreenPercentage slider is adjusted in the viewport */
	UNREALED_API void OnScreenPercentageValueChanged(int32 NewValue);

	/** Update the list of asset viewer profiles displayed by the combo box. */
	UNREALED_API void UpdateAssetViewerProfileList();
	UNREALED_API void UpdateAssetViewerProfileSelection();

	/** Invoked when the asset viewer profile combo box selection changes. */
	UNREALED_API void OnAssetViewerProfileComboBoxSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type /*SelectInfo*/);

	/** Creates and returns the asset viewr profile combo box.*/
	UNREALED_API TSharedRef<SWidget> MakeAssetViewerProfileComboBox();

private:
	/** The viewport that we are in */
	TWeakPtr<class ICommonEditorViewportToolbarInfoProvider> InfoProviderPtr;

	/** Interface to set/get/list the preview profiles. */
	TSharedPtr<IPreviewProfileController> PreviewProfileController;

	/** List of advanced preview profiles to fill up the Profiles combo box. */
	TArray<TSharedPtr<FString>> AssetViewerProfileNames;

	/** Displays/Selects the active advanced viewer profile. */
	TSharedPtr<STextComboBox> AssetViewerProfileComboBox;
};


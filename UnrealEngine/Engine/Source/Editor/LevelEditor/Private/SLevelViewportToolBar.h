// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SLevelViewport.h"
#include "SViewportToolBar.h"

class ACameraActor;
class FExtender;
class SExtensionPanel;
class SActionableMessageViewportWidget;
class STransformViewportToolBar;
class FMenuBuilder;
class UToolMenu;
struct FToolMenuSection;

/**
 * A level viewport toolbar widget that is placed in a viewport
 */
class SLevelViewportToolBar : public SViewportToolBar
{
public:
	SLATE_BEGIN_ARGS( SLevelViewportToolBar ){}
		SLATE_ARGUMENT( TSharedPtr<class SLevelViewport>, Viewport )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );

	/** @return Whether the given viewmode is supported. */ 
	virtual bool IsViewModeSupported(EViewModeIndex ViewModeIndex) const;

	/** @return Level editor viewport client. */ 
	FLevelEditorViewportClient* GetLevelViewportClient() const;

	/** Fills view menu */
	void FillViewMenu(UToolMenu* InMenu);

private:
	/**
	 * Returns the label for the "Camera" tool bar menu, which changes depending on the viewport type
	 *
	 * @return	Label to use for this menu label
	 */
	FText GetCameraMenuLabel() const;

	/* Returns the label icon for the Camera tool bar menu, which changes depending on viewport type 
	 *
	 * @return	Label icon to use for this menu label
	 *
	 */
	const FSlateBrush* GetCameraMenuLabelIcon() const;

	/**
	 * Returns the label for the "View" tool bar menu, which changes depending on viewport show flags
	 *
	 * @return	Label to use for this menu label
	 */
	FText GetViewMenuLabel() const;

	/**
	 * Returns the label for the "Device Profile Preview" tool bar menu, which changes depending on the preview type.
	 *
	 * @return	Label to use for this menu label.
	 */
	FText GetDevicePreviewMenuLabel() const;

	/**
	 * Returns the label icon for the "Device Preview" tool bar menu
	 *
	 * @return	Label icon to use for this menu label
	 */
	const FSlateBrush* GetDevicePreviewMenuLabelIcon() const;

	/**
	 * Returns the label icon for the "View" tool bar menu, which changes depending on viewport show flags
	 *
	 * @return	Label icon to use for this menu label
	 */
	const FSlateBrush* GetViewMenuLabelIcon() const;

	/** @return Returns true, only if this tool bar's viewport is the "current" level editing viewport */
	bool IsCurrentLevelViewport() const;

	/** @return Returns true if this viewport is the perspective viewport */
	bool IsPerspectiveViewport() const;

	/**
	 * Generates the toolbar device profile simulation menu content .
	 *
	 * @return The widget containing the options menu content.
	 */
	TSharedRef<SWidget> GenerateDevicePreviewMenu() const;

	/** Fills device preview menu */
	void FillDevicePreviewMenu(UToolMenu* Menu) const;

	/**
	 * Generates the sub  menu for different device profile previews.
	 *
	 * @param Menu - The menu.
	 * @param InDeviceProfiles - The array of device profiles.
	 */
	void MakeDevicePreviewSubMenu(UToolMenu* Menu, TArray< class UDeviceProfile* > Profiles);

	/**
	 * Set the level profile, and save the selection to an .ini file.
	 *
	 * @param DeviceProfileName - The selected device profile
	 */
	void SetLevelProfile( FString DeviceProfileName );

	/**
	 * Generates the toolbar view mode options menu content 
	 *
	 * @return The widget containing the options menu content
	 */
	TSharedRef<SWidget> GenerateOptionsMenu();

	/** Fills options menu */
	void FillOptionsMenu(UToolMenu* Menu);

	/**
	 * Generates the toolbar camera menu content 
	 *
	 * @return The widget containing the view menu content
	 */
	TSharedRef<SWidget> GenerateCameraMenu() const;

	/** Fills camera menu */
	void FillCameraMenu(UToolMenu* Menu) const;

	/**
	 * Generates menu entries for placed cameras (e.g CameraActors
	 *
	 * @param Menu	The menu to add menu entries to
	 * @param Cameras	The list of cameras to add
	 */
	void GeneratePlacedCameraMenuEntries(UToolMenu* Menu, TArray<ACameraActor*> Cameras) const;

	/**
	 * Generates menu entries for placed cameras (e.g CameraActors
	 *
	 * @param Section	The menu section to add menu entries to
	 * @param Cameras	The list of cameras to add
	 */
	void GeneratePlacedCameraMenuEntries(FToolMenuSection& Section, TArray<ACameraActor*> Cameras) const;

	/**
	 * Generates menu entries for changing the type of the viewport
	 *
	 * @param Menu	The menu to add menu entries to
	 */
	void GenerateViewportTypeMenu(UToolMenu* Menu) const;

	/**
	 * Generates menu entries for changing the type of the viewport
	 *
	 * @param Section	The menu section to add menu entries to
	 */
	void GenerateViewportTypeMenu(FToolMenuSection& Section) const;

	/**
	 * Generates menu entries for spawning cameras at the current viewport
	 *
	 * @param Menu	The menu to add menu entries to
	 */
	void GenerateCameraSpawnMenu(UToolMenu* Menu) const;

	/**
	 * Generates the toolbar view menu content 
	 *
	 * @return The widget containing the view menu content
	 */
	TSharedRef<SWidget> GenerateViewMenu() const;

	/**
	 * Generates the toolbar show menu content 
	 *
	 * @return The widget containing the show menu content
	 */
	TSharedRef<SWidget> GenerateShowMenu() const;

	/** Fills the toolbar show menu content */
	void FillShowMenu(UToolMenu* Menu) const;

	/**
	 * Returns the initial visibility of the view mode options widget 
	 *
	 * @return The visibility value
	 */
	EVisibility GetViewModeOptionsVisibility() const;

	/** Get the name of the viewmode options menu */
	FText GetViewModeOptionsMenuLabel() const;

	/**
	 * Generates the toolbar view param menu content 
	 *
	 * @return The widget containing the show menu content
	 */
	TSharedRef<SWidget> GenerateViewModeOptionsMenu() const;

	/**
	 * @return The widget containing the perspective only FOV window.
	 */
	TSharedRef<SWidget> GenerateFOVMenu() const;

	/** Called by the FOV slider in the perspective viewport to get the FOV value */
	float OnGetFOVValue() const;

	/** Called when the FOV slider is adjusted in the perspective viewport */
	void OnFOVValueChanged( float NewValue );

	/**
	 * @return The widget containing the far view plane slider.
	 */
	TSharedRef<SWidget> GenerateFarViewPlaneMenu() const;

	/** Called by the far view plane slider in the perspective viewport to get the far view plane value */
	float OnGetFarViewPlaneValue() const;

	/** Called when the far view plane slider is adjusted in the perspective viewport */
	void OnFarViewPlaneValueChanged( float NewValue );

	bool IsLandscapeLODSettingChecked(int32 Value) const;
	void OnLandscapeLODChanged(int32 NewValue);

	FReply OnRealtimeWarningClicked();
	EVisibility GetRealtimeWarningVisibility() const;

	FText GetScalabilityWarningLabel() const;
	EVisibility GetScalabilityWarningVisibility() const;
	TSharedRef<SWidget> GetScalabilityWarningMenuContent() const;

	double OnGetHLODInEditorMinDrawDistanceValue() const;
	void OnHLODInEditorMinDrawDistanceValueChanged(double NewValue) const;
	double OnGetHLODInEditorMaxDrawDistanceValue() const;
	void OnHLODInEditorMaxDrawDistanceValueChanged(double NewValue) const;

private:
	/**
	 * Generates the toolbar show layers menu content 
	 *
	 * @param Menu	The tool menu
	 */
	static void FillShowLayersMenu(UToolMenu* Menu, TWeakPtr<class SLevelViewport> Viewport );

	/**
	 * Generates 'show foliage types' menu content for a viewport
	 *
	 * @param Menu	The tool menu
	 * @param Viewport		target vieport
	 */
	static void FillShowFoliageTypesMenu(UToolMenu* Menu, TWeakPtr<class SLevelViewport> Viewport);

	/**
	 * Generates 'Show HLODs' menu content for a viewport
	 *
	 * @param Menu		The tool menu
	 * @param Viewport	Target vieport
	 */
	void FillShowHLODsMenu(UToolMenu* Menu) const;

	/** Generates the layout sub-menu content */
	void GenerateViewportConfigsMenu(UToolMenu* Menu) const;

	/** Gets the world we are editing */
	TWeakObjectPtr<UWorld> GetWorld() const;

	/** Gets the extender for the view menu */
	TSharedPtr<FExtender> GetViewMenuExtender();

	/** Called when the user disables realtime override from the toolbar */
	void OnDisableRealtimeOverride();
	bool IsRealtimeOverrideToggleVisible() const;
	FText GetRealtimeOverrideTooltip() const;

	float GetTransformToolbarWidth() const;

private:
	/** The viewport that we are in */
	TWeakPtr<class SLevelViewport> Viewport;

	/** STransformViewportToolBar menu */
	TSharedPtr<STransformViewportToolBar> TransformToolbar;

	/** Viewport widget for warning messages */
	TSharedPtr<SActionableMessageViewportWidget> ActionableMessageViewportWidget;

	/** The previous max STransformViewportToolBar width to allow deterministic size calculations */
	mutable float TransformToolbar_CachedMaxWidth = 0.0f;
};


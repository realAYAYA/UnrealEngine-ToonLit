// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Animation/CurveSequence.h"
#include "Styling/SlateColor.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Framework/Commands/UICommandInfo.h"
#include "SWorldPartitionViewportWidget.h"
#include "EditorViewportClient.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SWindow.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "SAssetEditorViewport.h"
#include "EditorModeManager.h"
#include "IAssetViewport.h"
#include "LevelEditorViewport.h"

class FLevelEditorViewportClient;
class FLevelViewportLayout;
class FSceneViewport;
class FUICommandList;
class ILevelEditor;
class SActorPreview;
class SCaptureRegionWidget;
class SGameLayerManager;
class UFoliageType;
enum class EMapChangeType : uint8;
enum ELabelAnchorMode : int;

/**
 * Encapsulates an SViewport and an SLevelViewportToolBar
 */
class LEVELEDITOR_API SLevelViewport : public SAssetEditorViewport, public IAssetViewport
{
public:
	SLATE_BEGIN_ARGS( SLevelViewport )
		{}

		SLATE_ARGUMENT( TWeakPtr<ILevelEditor>, ParentLevelEditor )
		SLATE_ARGUMENT( TSharedPtr<FLevelEditorViewportClient>, LevelEditorViewportClient )
	SLATE_END_ARGS()

	SLevelViewport();
	~SLevelViewport();


	/**
	 * Constructs the viewport widget                   
	 */
	void Construct(const FArguments& InArgs, const FAssetEditorViewportConstructionArgs& InConstructionArgs);

	/**
	 * Constructs the widgets for the viewport overlay
	 */
	void ConstructViewportOverlayContent();

	/**
	 * Constructs the level editor viewport client
	 */
	void ConstructLevelEditorViewportClient(FLevelEditorViewportInstanceSettings& ViewportInstanceSettings);

	/**
	 * @return true if the viewport is visible. false otherwise                  
	 */
	virtual bool IsVisible() const override;

	/** @return true if this viewport is in a foregrounded tab */
	bool IsInForegroundTab() const;

	/**
	 * @return The editor client for this viewport
	 */
	const FLevelEditorViewportClient& GetLevelViewportClient() const 
	{		
		return *LevelViewportClient;
	}

	FLevelEditorViewportClient& GetLevelViewportClient() 
	{		
		return *LevelViewportClient;
	}

	virtual FEditorViewportClient& GetAssetViewportClient() override
	{
		return *LevelViewportClient;
	}

	/**
	 * Sets Slate keyboard focus to this viewport
	 */
	void SetKeyboardFocusToThisViewport();

	/**
	 * @return The list of commands on the viewport that are bound to delegates                    
	 */
	const TSharedPtr<FUICommandList>& GetCommandList() const { return CommandList; }

	/** Saves this viewport's config to ULevelEditorViewportSettings */
	void SaveConfig(const FString& ConfigName) const;

	/** IAssetViewport Interface */
	virtual void StartPlayInEditorSession( UGameViewportClient* PlayClient, const bool bInSimulateInEditor ) override;
	virtual void EndPlayInEditorSession() override;
	virtual void SwapViewportsForSimulateInEditor() override;
	virtual void SwapViewportsForPlayInEditor() override;
	virtual void OnSimulateSessionStarted() override;
	virtual void OnSimulateSessionFinished() override;
	virtual void RegisterGameViewportIfPIE() override;
	virtual bool HasPlayInEditorViewport() const override; 
	virtual FViewport* GetActiveViewport() override;
	TSharedPtr<FSceneViewport> GetSharedActiveViewport() const override {return ActiveViewport;};
	virtual TSharedRef< const SWidget> AsWidget() const override { return AsShared(); }
	virtual TSharedRef< SWidget> AsWidget() override { return AsShared(); }
	virtual TWeakPtr< SViewport > GetViewportWidget() override { return ViewportWidget; }
	virtual void AddOverlayWidget( TSharedRef<SWidget> OverlaidWidget ) override;
	virtual void RemoveOverlayWidget( TSharedRef<SWidget> OverlaidWidget ) override;



	/** SEditorViewport Interface */
	virtual void OnFocusViewportToSelection() override;
	virtual EVisibility GetTransformToolbarVisibility() const override;
	virtual UWorld* GetWorld() const override;
	virtual void ToggleInViewportContextMenu() override;

	virtual void HideInViewportContextMenu() override;
	virtual bool CanToggleInViewportContextMenu() override;

	static void EnableInViewportMenu();
	FMargin GetContextMenuPadding() const;
	/**
	 * Called when the maximize command is executed                   
	 */
	FReply OnToggleMaximize();

	/**
	 * @return true if this viewport is maximized, false otherwise
	 */
	bool IsMaximized() const;

	/**
	 * Attempts to switch this viewport into immersive mode
	 *
	 * @param	bWantImmersive Whether to switch to immersive mode, or switch back to normal mode
	 * @param	bAllowAnimation	True to allow animation when transitioning, otherwise false
	 */
	void MakeImmersive( const bool bWantImmersive, const bool bAllowAnimation ) override;

	/**
	 * @return true if this viewport is in immersive mode, false otherwise
	 */
	bool IsImmersive () const override;
	
	/**
	 * Called to get the visibility of the viewport's 'Restore from Immersive' button. Returns EVisibility::Collapsed when not in immersive mode
	 */
	EVisibility GetCloseImmersiveButtonVisibility() const;
		
	/**
	 * Called to get the visibility of the viewport's maximize/minimize toggle button. Returns EVisibility::Collapsed when in immersive mode
	 */
	EVisibility GetMaximizeToggleVisibility() const;

	/**
	 * @return true if the active viewport is currently being used for play in editor
	 */
	bool IsPlayInEditorViewportActive() const;

	/**
	 * Called on all viewports, when actor selection changes.
	 * 
	 * @param NewSelection	List of objects that are now selected
	 */
	void OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh=false);

	/**
	 * Called on all viewports, when element selection changes.
	 * 
	 * @param SelectionSet  New selection
	 * @param bForceRefresh Force refresh
	 */
	void OnElementSelectionChanged(const UTypedElementSelectionSet* SelectionSet, bool bForceRefresh);

	/**
	 * Called when game view should be toggled
	 */
	void ToggleGameView() override;

	/**
	 * @return true if we can toggle game view
	 */
	bool CanToggleGameView() const;

	/**
	 * @return true if we are in game view                   
	 */
	bool IsInGameView() const override;

	/**
	 * Toggles layer visibility in this viewport
	 *
	 * @param LayerID					Index of the layer
	 */
	void ToggleShowLayer( FName LayerName );

	/**
	 * Checks if a layer is visible in this viewport
	 *
	 * @param LayerID					Index of the layer
	 */
	bool IsLayerVisible( FName LayerName ) const;

	/**
	 * Toggles foliage type visibility in this viewport
	 *
	 * @param FoliageType	Target foliage type
	 */
	void ToggleShowFoliageType(TWeakObjectPtr<class UFoliageType> FoliageType);

	/**
	 * Toggles all foliage types visibility
	 *
	 * @param Visible	true if foliage types should be visible, false otherwise
	 */
	void ToggleAllFoliageTypes(bool bVisible);

	/**
	 * Checks if a foliage type is visible in this viewport
	 *
	 * @param FoliageType	Target foliage type
	 */
	bool IsFoliageTypeVisible(TWeakObjectPtr<class UFoliageType> FoliageType) const;


	/** Called to lock/unlock the actor from the viewport's context menu */
	void OnActorLockToggleFromMenu(AActor* Actor);

	/**
	 * @return true if the actor is locked to the viewport
	 */
	bool IsActorLocked(const TWeakObjectPtr<AActor> Actor) const;

	/**
	 * @return true if an actor is locked to the viewport
	 */
	bool IsAnyActorLocked() const;

	/**
	 * @return true if the viewport is locked to selected actor
	 */
	bool IsSelectedActorLocked() const;

	/**
	 * Toggles enabling the exact camera view when locking a viewport to a camera
	 */
	void ToggleActorPilotCameraView();

	/**
	 * Check whether locked camera view is enabled
	 */
	bool IsLockedCameraViewEnabled() const;

	/**
	 * Sets whether the viewport should allow cinematic control
	 * 
	 * @param Whether the viewport should allow cinematic control.
     */
	void SetAllowsCinematicControl(bool bAllow);

	/**
	 * @return Whether the viewport allows cinematic control.
     */
	bool GetAllowsCinematicControl() const;

	/**
	 * @return the fixed width that a column returned by CreateActorLockSceneOutlinerColumn expects to be
	 */
	static float GetActorLockSceneOutlinerColumnWidth();

	/**
	 * @return a new custom column for a scene outliner that indicates whether each actor is locked to this viewport
	 */
	TSharedRef< class ISceneOutlinerColumn > CreateActorLockSceneOutlinerColumn( class ISceneOutliner& SceneOutliner ) const;

	/** Called when Preview Selected Cameras preference is changed.*/
	void OnPreviewSelectedCamerasChange();

	/**
	 * Set the device profile name
	 *
	 * @param ProfileName The profile name to set
	 */
	void SetDeviceProfileString( const FString& ProfileName );

	/**
	 * @return true if the in profile name matches the set profile name
	 */
	bool IsDeviceProfileStringSet( FString ProfileName ) const;

	/**
	 * @return the name of the selected device profile
	 */
	FString GetDeviceProfileString( ) const;

	/** Get the parent level editor for this viewport */
	TWeakPtr<ILevelEditor> GetParentLevelEditor() const { return ParentLevelEditor; }

	/** @return whether the the actor editor context should be displayed for this viewport */
	virtual bool IsActorEditorContextVisible() const;

	/** Called to get the screen percentage preview text */
	FText GetCurrentScreenPercentageText() const;

	UE_DEPRECATED(5.1, "GetCurrentLevelTextVisibility not used anymore.")
	virtual EVisibility GetCurrentLevelTextVisibility() const { return EVisibility::Collapsed; }
	UE_DEPRECATED(5.1, "GetCurrentLevelButtonVisibility not used anymore.")
	virtual EVisibility GetCurrentLevelButtonVisibility() const { return EVisibility::Collapsed; }

	/** @return The visibility of the current level text display */
	virtual EVisibility GetSelectedActorsCurrentLevelTextVisibility() const;

	/** Called to get the text for the level the currently selected actor or actors are in. */
	FText GetSelectedActorsCurrentLevelText(bool bDrawOnlyLabel) const;

	/** @return The visibility of the current screen percentage text display */
	EVisibility GetCurrentScreenPercentageVisibility() const;

	/** @return The visibility of the viewport controls popup */
	virtual EVisibility GetViewportControlsVisibility() const;

	/**
	 * Called to get the visibility of the level viewport toolbar
	 */
	virtual EVisibility GetToolBarVisibility() const;

	/**
	 * Sets the current layout on the parent tab that this viewport belongs to.
	 * 
	 * @param ConfigurationName		The name of the layout (for the names in namespace LevelViewportConfigurationNames)
	 */
	UE_DEPRECATED(5.1, "Moved to internal handling by FViewportTabContent. See FViewportTabContent::BindCommonViewportCommands")
	void OnSetViewportConfiguration(FName ConfigurationName);

	/**
	 * Returns whether the named layout is currently selected on the parent tab that this viewport belongs to.
	 *
	 * @param ConfigurationName		The name of the layout (for the names in namespace LevelViewportConfigurationNames)
	 * @return						True, if the named layout is currently active
	 */
	UE_DEPRECATED(5.1, "Moved to internal handling by FViewportTabContent. See FViewportTabContent::BindCommonViewportCommands")
	bool IsViewportConfigurationSet(FName ConfigurationName) const;

	/** Get this level viewport widget's type within its parent layout */
	UE_DEPRECATED(5.1, "Moved to internal handling by FViewportTabContent. See FViewportTabContent::BindCommonViewportCommands")
	FName GetViewportTypeWithinLayout() const;

	/** Set this level viewport widget's type within its parent layout */
	UE_DEPRECATED(5.1, "Moved to internal handling by FViewportTabContent. See FViewportTabContent::BindCommonViewportCommands")
	void SetViewportTypeWithinLayout(FName InLayoutType);

	/** Activates the specified viewport type in the layout, if it's not already, or reverts to default if it is. */
	UE_DEPRECATED(5.1, "Moved to internal handling by FViewportTabContent. See FViewportTabContent::BindCommonViewportCommands")
	void ToggleViewportTypeActivationWithinLayout(FName InLayoutType);

	/** Checks if the specified layout type matches our current viewport type. */
	UE_DEPRECATED(5.1, "Moved to internal handling by FViewportTabContent. See FViewportTabContent::BindCommonViewportCommands")
	bool IsViewportTypeWithinLayoutEqual(FName InLayoutType);

	/** For the specified actor, See if we're forcing a preview */
	bool IsActorAlwaysPreview(TWeakObjectPtr<AActor> Actor) const;

	/** For the specified actor, toggle Pinned/Unpinned of it's ActorPreview */
	void SetActorAlwaysPreview(TWeakObjectPtr<AActor> PreviewActor, bool bAlwaysPreview = true);

	/** For the specified actor, toggle Pinned/Unpinned of it's ActorPreview */
	void ToggleActorPreviewIsPinned(TWeakObjectPtr<AActor> PreviewActor);

	/** For the specified actor, toggle whether the panel is detached from it*/
	void ToggleActorPreviewIsPanelDetached(TWeakObjectPtr<AActor> PreviewActor);

	/** See if the specified actor's ActorPreview is pinned or not */
	bool IsActorPreviewPinned(TWeakObjectPtr<AActor> PreviewActor);

	/** See if the specified actor's ActorPreview is detached from actor */
	bool IsActorPreviewDetached(TWeakObjectPtr<AActor> PreviewActor);

	/** Actions to perform whenever the viewports floating buttons are pressed */
	void OnFloatingButtonClicked();

	/** Get the visibility for viewport toolbar */
	EVisibility GetToolbarVisibility() const;

	/** Get the visibility for items considered to be part of the 'full' viewport toolbar */
	EVisibility GetFullToolbarVisibility() const { return (bShowToolbarAndControls && bShowFullToolbar) ? EVisibility::Visible : EVisibility::Collapsed; }

	/** Unpin and close all actor preview windows */
	void RemoveAllPreviews(const bool bRemoveFromDesktopViewport = true);

	/**
	 * Called to set a bookmark
	 *
	 * @param BookmarkIndex	The index of the bookmark to set
	 */
	void OnSetBookmark( int32 BookmarkIndex );

	/**
	 * Called to jump to a bookmark
	 *
	 * @param BookmarkIndex	The index of the bookmark to jump to
	 */
	void OnJumpToBookmark( int32 BookmarkIndex );

	/**
	 * Called to clear a bookmark
	 *
	 * @param BookmarkIndex The index of the bookmark to clear
	 */
	void OnClearBookmark( int32 BookmarkIndex );

	/**
	 * Called to clear all bookmarks
	 */
	void OnClearAllBookmarks();

	/**
	 * Called to Compact Bookmarks.
	 */
	void OnCompactBookmarks();

	/** 
	 * Returns the config key associated with this viewport. 
	 * This is what is used when loading/saving per viewport settings. 
	 * If a plugin extends a LevelViewport menu, they'll be able to identify it and match their settings accordingly
	 */
	FName GetConfigKey() const { return ConfigKey; }

protected:
	/** SEditorViewport interface */
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;

	virtual void OnIncrementPositionGridSize() override;
	virtual void OnDecrementPositionGridSize() override;
	virtual void OnIncrementRotationGridSize() override;
	virtual void OnDecrementRotationGridSize() override;
	virtual const FSlateBrush* OnGetViewportBorderBrush() const override;
	virtual FSlateColor OnGetViewportBorderColorAndOpacity() const override;
	virtual EVisibility OnGetViewportContentVisibility() const override;
	virtual EVisibility OnGetFocusedViewportIndicatorVisibility() const override;
	virtual void BindCommands() override;
private:
	/** Flag to know if we need to update the previews which is handled in the tick. */
	bool bNeedToUpdatePreviews;

	/** Loads this viewport's config from the ini file */
	FLevelEditorViewportInstanceSettings LoadLegacyConfigFromIni(const FString& ConfigKey, const FLevelEditorViewportInstanceSettings& InDefaultSettings);

	/** Called when a property is changed */
	void HandleViewportSettingChanged(FName PropertyName);

	/**
	 * Called when the advanced settings should be opened.
	 */
	void OnAdvancedSettings();

	/**
	 * Called when immersive mode is toggled by the user
	 */
	void OnToggleImmersive();

	/** Called when moving tabs in and out of a sidebar is activated by the user */
	void OnToggleSidebarTabs();

	/**
	* Called to determine whether the maximize mode of current viewport can be toggled
	*/
	bool CanToggleMaximizeMode() const;

	/**
	* Called to toggle maximize mode of current viewport
	*/
	void OnToggleMaximizeMode();

	/** Starts previewing any selected camera actors using live "PIP" sub-views */
	void PreviewSelectedCameraActors(const bool bPreviewInDesktopViewport = true);

	/**
	 * Called to create a cameraActor in the currently selected perspective viewport
	 */
	void OnCreateCameraActor(UClass *InClass);

	/**
	 * Called to bring up the screenshot UI
	 */
	void OnTakeHighResScreenshot();

	/**
	 * Called to check currently selected editor viewport is a perspective one
	 */
	bool IsPerspectiveViewport() const;

	/**
	 * Toggles all volume classes visibility
	 *
	 * @param Visible					true if volumes should be visible, false otherwise
	 */
	void OnToggleAllVolumeActors( bool bVisible );

	/**
	 * Toggles volume classes visibility
	 *
	 * @param VolumeID					Index of the volume class
	 */
	void ToggleShowVolumeClass( int32 VolumeID );

	/**
	 * Checks if volume class is visible in this viewport
	 *
	 * @param VolumeID					Index of the volume class
	 */
	bool IsVolumeVisible( int32 VolumeID ) const;

	/**
	 * Toggles all layers visibility
	 *
	 * @param Visible					true if layers should be visible, false otherwise
	 */
	void OnToggleAllLayers( bool bVisible );

	/**
	 * Toggles all sprite categories visibility
	 *
	 * @param Visible					true if sprites should be visible, false otherwise
	 */
	void OnToggleAllSpriteCategories( bool bVisible );

	/**
	 * Toggles sprite category visibility in this viewport
	 *
	 * @param CategoryID				Index of the category
	 */
	void ToggleSpriteCategory( int32 CategoryID );

	/**
	 * Checks if sprite category is visible in this viewport
	 *
	 * @param CategoryID				Index of the category
	 */
	bool IsSpriteCategoryVisible( int32 CategoryID ) const;

	/**
	 * Toggles all Stat commands visibility
	 *
	 * @param Visible					true if Stats should be visible, false otherwise
	 */
	void OnToggleAllStatCommands(bool bVisible);

	/**
	 * Called when show flags for this viewport should be reset to default, or the saved settings
	 */
	void OnUseDefaultShowFlags(bool bUseSavedDefaults = false);

	/**
	 * Called to toggle allowing sequencer to use this viewport to preview in
	 */
	void OnToggleAllowCinematicPreview();

	/**
	 * @return true if this viewport allows cinematics to be previewed in it                   
	 */
	bool AllowsCinematicPreview() const;

	/** Find currently selected actor in the level script.  */
	void FindSelectedInLevelScript();
	
	/** Can we find the currently selected actor in the level script. */
	bool CanFindSelectedInLevelScript() const;

	/** Called to clear the current actor lock */
	void OnActorUnlock();

	/**
	 * @return true if clearing the current actor lock is a valid input
	 */
	bool CanExecuteActorUnlock() const;

	/** Called to lock the viewport to the currently selected actor */
	void OnActorLockSelected();

	/**
	 * @return true if clearing the setting the actor lock to the selected actor is a valid input
	 */
	bool CanExecuteActorLockSelected() const;

	/**
	 * Called when the viewport should be redrawn
	 *
	 * @param bInvalidateHitProxies	Whether or not to invalidate hit proxies
	 */
	void RedrawViewport( bool bInvalidateHitProxies );

	/** An internal handler for dagging dropable objects into the viewport. */
	bool HandleDragObjects(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent);

	/** An internal handler for dropping objects into the viewport. 
	 *	@param DragDropEvent		The drag event.
	 *	@param bCreateDropPreview	If true, a drop preview actor will be spawned instead of a normal actor.
	 */
	bool HandlePlaceDraggedObjects(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent, bool bCreateDropPreview);

	/**
	 * Tries to get assets from a drag and drop event.
	 * 
	 * @param DragDropEvent		Event to get assets from.
	 * @param AssetDataArray	Asets will be added here.
	 */
	void GetAssetsFromDrag(const FDragDropEvent& DragDropEvent, TArray<FAssetData>& AssetDataArray);

	/** SWidget Interface */
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual void OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual void OnDragLeave( const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	/** End of SWidget interface */

	/**
	 * Bound event Triggered via FLevelViewportCommands::ApplyMaterialToActor, attempts to apply a material selected in the content browser
	 * to an actor being hovered over in the Editor viewport.
	 */ 
	void OnApplyMaterialToViewportTarget();

	/**
	 * Binds commands for our toolbar options menu
	 *
	 * @param CommandList	The list to bind commands to
	 */
	void BindOptionCommands( FUICommandList& CommandList );

	/**
	 * Binds commands for our toolbar view menu
	 *
	 * @param CommandList	The list to bind commands to
	 */
	void BindViewCommands( FUICommandList& CommandList );

	/**
	 * Binds commands for our toolbar show menu
	 *
	 * @param CommandList	The list to bind commands to
	 */
	void BindShowCommands( FUICommandList& CommandList );

	/**
	 * Binds commands for our drag-drop context menu
	 *
	 * @param CommandList	The list to bind commands to
	 */
	void BindDropCommands( FUICommandList& CommandList );

	/**
	* Binds commands for our stat menu, also used as a delegate listener
	*
	* @param InMenuItem		The menu item we need to bind
	* @param InCommandName	The command used by the functions
	*/
	void BindStatCommand(const TSharedPtr<FUICommandInfo> InMenuItem, const FString& InCommandName);
	
	/**
	 * Called to build the viewport context menu when the user is Drag Dropping from the Content Browser
	 */
	TSharedRef< SWidget > BuildViewportDragDropContextMenu();

	/**
	 * Called when a map is changed (loaded,saved,new map, etc)
	 */
	void OnMapChanged( UWorld* World, EMapChangeType MapChangeType );

	/** Called in response to an actor being deleted in the level */
	void OnLevelActorsRemoved(AActor* InActor);

	/** Gets whether the locked icon should be shown in the viewport because it is locked to an actor */
	EVisibility GetLockedIconVisibility() const;

	/** Gets the locked icon tooltip text showing the meaning of the icon and the name of the locked actor */
	FText GetLockedIconToolTip() const;

	/**
	 * Starts previewing the specified actors.  If the supplied list of actors is empty, turns off the preview.
	 *
	 * @param	ActorsToPreview		List of actors to draw previews for
	 */
	void PreviewActors( const TArray< AActor* >& ActorsToPreview, const bool bPreviewInDesktopViewport = true);

	/** Called every frame to update any actor preview viewports we may have */
	void UpdateActorPreviewViewports();

	/**
	 * Removes a specified actor preview from the list
	 *
	 * @param PreviewIndex Array index of the preview to remove.
	 */
	void RemoveActorPreview( int32 PreviewIndex, AActor* Actor = nullptr, const bool bRemoveFromDesktopViewport = true);
	
	/** Returns true if this viewport is the active viewport and can process UI commands */
	bool CanProduceActionForCommand(const TSharedRef<const FUICommandInfo>& Command) const;

	/** Called when undo is executed */
	void OnUndo();

	/** Called when undo is executed */
	void OnRedo();

	/** @return Whether or not undo can be executed */
	bool CanExecuteUndo() const;

	/** @return Whether or not redo can be executed */
	bool CanExecuteRedo() const;

	/** @return Whether the mouse capture label is visible */
	EVisibility GetMouseCaptureLabelVisibility() const;

	/** @return The current color & opacity for the mouse capture label */
	FLinearColor GetMouseCaptureLabelColorAndOpacity() const;

	/** @return The current text for the mouse capture label */
	FText GetMouseCaptureLabelText() const;

	/** Show the mouse capture label with the specified vertical and horizontal alignment */
	void ShowMouseCaptureLabel(ELabelAnchorMode AnchorMode);

	/** Hide the mouse capture label */
	void HideMouseCaptureLabel();

	/** Resets view flags when a new level is created or opened */
	void ResetNewLevelViewFlags();

	/** Gets the active scene viewport for the game */
	FSceneViewport* GetGameSceneViewport() const;

	/** Called when the user toggles the full toolbar */
	void OnToggleShowFullToolbar() { bShowFullToolbar = !bShowFullToolbar; }

	/** Check whether we should display the full toolbar or not */
	bool ShouldShowFullToolbar() const { return bShowFullToolbar; }

	/** Handle any level viewport changes on entering PIE or simulate */
	void TransitionToPIE(bool bIsSimulating);

	/** Handle any level viewport changes on leaving PIE or simulate */
	void TransitionFromPIE(bool bIsSimulating);

	/** Get the stretch type of the viewport */
	EStretch::Type OnGetScaleBoxStretch() const;

	/** Get the SViewport size */
	FVector2D GetSViewportSize() const;

	/** Updates the real-time overrride applied to the viewport */
	void OnPerformanceSettingsChanged(UObject* Obj, struct FPropertyChangedEvent& ChangeEvent);

private:
	/** Tab which this viewport is located in */
	TWeakPtr<class FLevelViewportLayout> ParentLayout;

	/** Pointer to the parent level editor for this viewport */
	TWeakPtr<ILevelEditor> ParentLevelEditor;

	/** Viewport overlay widget exposed to game systems when running play-in-editor */
	TSharedPtr<SOverlay> PIEViewportOverlayWidget;

	TSharedPtr<SGameLayerManager> GameLayerManager;

	/** Viewport horizontal box used internally for drawing actor previews on top of the level viewport */
	TSharedPtr<SHorizontalBox> ActorPreviewHorizontalBox;

	/** Active Slate viewport for rendering and I/O (Could be a pie viewport)*/
	TSharedPtr<class FSceneViewport> ActiveViewport;

	/**
	 * Inactive Slate viewport for rendering and I/O
	 * If this is valid there is a pie viewport and this is the previous level viewport scene viewport 
	 */
	TSharedPtr<class FSceneViewport> InactiveViewport;

	/**
	 * When in PIE this will contain the editor content for the viewport widget (toolbar). It was swapped
	 * out for GameUI content
	 */
	TSharedPtr<SWidget> InactiveViewportWidgetEditorContent;

	/** 
	 *  When PIE is active, the handle for the change feature level delegate
	 */
	FDelegateHandle PIEPreviewFeatureLevelChangedHandle;

	/** Level viewport client */
	TSharedPtr<FLevelEditorViewportClient> LevelViewportClient;

	/** The brush to use if this viewport is in debug mode */
	const FSlateBrush* DebuggingBorder;
	/** The brush to use for a black background */
	const FSlateBrush* BlackBackground;
	/** The brush to use when transitioning into Play in Editor mode */
	const FSlateBrush* StartingPlayInEditorBorder;
	/** The brush to use when transitioning into Simulate mode */
	const FSlateBrush* StartingSimulateBorder;
	/** The brush to use when returning back to the editor from PIE or SIE mode */
	const FSlateBrush* ReturningToEditorBorder;
	/** The brush to use when the viewport is not maximized */
	const FSlateBrush* NonMaximizedBorder;
	/** Array of objects dropped during the OnDrop event */
	TArray<UObject*> DroppedObjects;

	/** Caching off of the DragDropEvent Local Mouse Position grabbed from OnDrop */
	FVector2D CachedOnDropLocalMousePos;

	/** Weak pointer to the highres screenshot dialog if it's open. Will become invalid if UI is closed whilst the viewport is open */
	TWeakPtr<class SWindow> HighResScreenshotDialog;

	/** Pointer to the capture region widget in the viewport overlay. Enabled by the high res screenshot UI when capture region selection is required */
	TSharedPtr<class SCaptureRegionWidget> CaptureRegionWidget;

	/** Types of transition effects we support */
	struct EViewTransition
	{
		enum Type
		{
			/** No transition */
			None = 0,

			/** Fade in from black */
			FadingIn,

			/** Entering PIE */
			StartingPlayInEditor,

			/** Entering SIE */
			StartingSimulate,

			/** Leaving either PIE or SIE */
			ReturningToEditor
		};
	};

	/** Type of transition we're currently playing */
	EViewTransition::Type ViewTransitionType;

	/** Animation progress within current view transition */
	FCurveSequence ViewTransitionAnim;

	/** True if we want to kick off a transition animation but are waiting for the next tick to do so */
	bool bViewTransitionAnimPending;

	/** The current device profile string */
	FString DeviceProfile;

	/** The current viewport config key */
	FName ConfigKey;

	/**
	 * Contains information about an actor being previewed within this viewport
	 */
	class FViewportActorPreview
	{

	public:

		FViewportActorPreview() 
			: bIsPinned(false)
			, bIsPanelDetached(false)
		{}

		void ToggleIsPinned()
		{
			bIsPinned = !bIsPinned;
		}

		void ToggleIsPanelDetached()
		{
			bIsPanelDetached = !bIsPanelDetached;
		}

		/** The Actor that is the center of attention. */
		TWeakObjectPtr< AActor > Actor;

		/** Level viewport client for our preview viewport */
		TSharedPtr< FLevelEditorViewportClient > LevelViewportClient;

		/** The scene viewport */
		TSharedPtr< FSceneViewport > SceneViewport;

		/** Slate widget that represents this preview in the viewport */
		TSharedPtr< SActorPreview > PreviewWidget;

		/** Whether or not this actor preview will remain on screen if the actor is deselected */
		bool bIsPinned;	

		/** Whether this actor preview is displayed in a detached panel */
		bool bIsPanelDetached;
	};

	/** List of actor preview objects */
	TArray< FViewportActorPreview > ActorPreviews;

	/** Storage for actors we always want to preview.  This comes from MU transactions .*/
	TSet<TWeakObjectPtr<AActor>> AlwaysPreviewActors;

	/** The slot index in the SOverlay for the PIE mouse control label */
	int32 PIEOverlaySlotIndex;

	/** Separate curve to control fading out the PIE mouse control label */
	FCurveSequence PIEOverlayAnim;

	/** Whether the PIE view has focus so we can track when to reshow the mouse control label */
	bool bPIEHasFocus;

	/** Whether the PIE view contains focus (even if not captured), if so we disable throttling. */
	bool bPIEContainsFocus;

	/** The users value for allowing throttling, we restore this value when we lose focus. */
	int32 UserAllowThrottlingValue;

	/** Whether to show toolbar or buttons */
	bool bShowToolbarAndControls;

	/** Whether to show a full toolbar, or a compact one */
	bool bShowFullToolbar;
	TSharedPtr<class SWidget> InViewportMenuWrapper;
	bool bIsInViewportMenuShowing;
	bool bIsInViewportMenuInitialized;
	TSharedPtr<class SInViewportDetails> InViewportMenu;
	static bool bInViewportMenuEnabled;

	TSharedPtr<SWorldPartitionViewportWidget> WorldPartitionViewportWidget;

protected:
	void LockActorInternal(AActor* NewActorToLock);

public:
	static bool GetCameraInformationFromActor(AActor* Actor, FMinimalViewInfo& out_CameraInfo);

	static bool CanGetCameraInformationFromActor(AActor* Actor);
};

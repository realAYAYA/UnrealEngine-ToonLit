// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Styling/SlateColor.h"
#include "Input/Reply.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SViewport.h"
#include "UnrealWidgetFwd.h"
#include "EditorViewportClient.h"

class FActiveTimerHandle;
class FSceneViewport;
class FUICommandList;
class SViewport;
struct FSlateBrush;
struct FToolMenuContext;

class UNREALED_API SEditorViewport
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEditorViewport)
		: _ViewportSize(SViewport::FArguments::GetDefaultViewportSize())
	{ }
	SLATE_ATTRIBUTE(FVector2D, ViewportSize);
	SLATE_END_ARGS()
	
	SEditorViewport();
	virtual ~SEditorViewport();

	void Construct( const FArguments& InArgs );

	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual bool SupportsKeyboardFocus() const override;
	virtual FReply OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent ) override;
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	
	/**
	 * @return True if the viewport is being updated in realtime
	 */
	bool IsRealtime() const;

	/** @return True if the viewport is currently visible */
	virtual bool IsVisible() const;

	/** 
	 * Invalidates the viewport to ensure it is redrawn during the next tick. 
	 * This is implied every frame while the viewport IsRealtime().
	 */
	void Invalidate();

	/** Toggles realtime on/off for the viewport. Slate tick/paint is ensured when realtime is on. */
	void OnToggleRealtime();

	/**
	 * Sets whether this viewport can render directly to the back buffer.  Advanced use only
	 * 
	 * @param	bInRenderDirectlyToWindow	Whether we should be able to render to the back buffer
	 */
	void SetRenderDirectlyToWindow( const bool bInRenderDirectlyToWindow );

	/**
	 * Sets whether stereo rendering is allowed for this viewport.  Advanced use only
	 * 
	 * @param	bInEnableStereoRendering	Whether stereo rendering should be allowed for this viewport
	 */
	void EnableStereoRendering( const bool bInEnableStereoRendering );

	/**
	 * @return true if the specified coordinate system the active one active
	 */
	virtual bool IsCoordSystemActive( ECoordSystem CoordSystem ) const;

	/**
	 * Cycles between world and local coordinate systems
	 */
	virtual void OnCycleCoordinateSystem();
	
	/** @return The viewport command list */
	const TSharedPtr<FUICommandList> GetCommandList() const { return CommandList; }

	TSharedPtr<FEditorViewportClient> GetViewportClient() const { return Client; }

	/**
	 * @return The current FSceneViewport shared pointer
	 */
	TSharedPtr<FSceneViewport> GetSceneViewport() { return SceneViewport; }

	/**
	 * Controls the visibility of the widget transform toolbar, if there is an associated toolbar
	 */
	virtual EVisibility GetTransformToolbarVisibility() const;

	/** Build the exposure menu using EV100 settings */
	TSharedRef<SWidget> BuildFixedEV100Menu()  const;

	/**
 * Called when the user wants to show the in-viewport context menu
 */
	virtual void ToggleInViewportContextMenu() {}
	virtual void HideInViewportContextMenu() {}
	virtual void UpdateInViewportMenuLocation(const FVector2D InLocation);
	virtual bool CanToggleInViewportContextMenu() { return false; }

///////////////////////////////////////////////////////////////////////////////
// begin feature level control functions block
///////////////////////////////////////////////////////////////////////////////
private:
	/** Called to get the feature level preview text */
	FText GetCurrentFeatureLevelPreviewText(bool bDrawOnlyLabel) const;

	/** Helper function that, for some FeatureLevel argument, will retrieve the required shader platform */
	EShaderPlatform GetShaderPlatformHelper(const ERHIFeatureLevel::Type InFeatureLevel) const;

protected:
	/** @return The visibility of the current feature level preview text display */
	EVisibility GetCurrentFeatureLevelPreviewTextVisibility() const;

	/** @return true if realtime can be toggled (it cannot be toggled directly if there is an override in place) */
	bool CanToggleRealtime() const;

	/** call this function to build a 'text' widget that can display the present feature level */
	TSharedRef<SWidget> BuildFeatureLevelWidget() const;
///////////////////////////////////////////////////////////////////////////////
// end feature level control functions block
///////////////////////////////////////////////////////////////////////////////

	/** Called by the fixed EV100 slider to get the fixed EV100 value */
	float OnGetFixedEV100Value() const;

	/** Called when fixed EV100 slider is adjusted */
	void OnFixedEV100ValueChanged( float NewValue );

	/** Called to know whether the fixed EV100 slider is enabled. */
	bool IsFixedEV100Enabled() const;


	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() = 0;

	// Implement this to add a viewport toolbar to the inside top of the viewport
	virtual TSharedPtr<SWidget> MakeViewportToolbar() { return TSharedPtr<SWidget>(nullptr); }

	// Implement this to add an arbitrary set of toolbars or other overlays to the inside of the viewport
	virtual void PopulateViewportOverlays(TSharedRef<SOverlay> Overlay) { }

	virtual void BindCommands();
	virtual const FSlateBrush* OnGetViewportBorderBrush() const { return NULL; }
	virtual FSlateColor OnGetViewportBorderColorAndOpacity() const { return FLinearColor::Black; }
	
	/**
	 * @return The visibility of widgets in the viewport (e.g, menus).  Note this is not the visibility of the scene rendered in the viewport                                                              
	 */
	virtual EVisibility OnGetViewportContentVisibility() const;

	/**
	 * @return The visibility of the viewport focus indicator.                                                             
	 */
	virtual EVisibility OnGetFocusedViewportIndicatorVisibility() const { return EVisibility::Collapsed; }

	/** UI Command delegate bindings */
	void OnToggleStats();

	/**
	 * Toggles Stat command visibility in this viewport
	 *
	 * @param CommandName				Name of the command
	 */
	virtual void ToggleStatCommand(FString CommandName);

	/**
	 * Checks if Stat command is visible in this viewport
	 *
	 * @param CommandName				Name of the command
	 */
	virtual bool IsStatCommandVisible(FString CommandName) const;

	/**
	 * Toggles a show flag in this viewport
	 *
	 * @param EngineShowFlagIndex	the ID to toggle
	 */
	void ToggleShowFlag( uint32 EngineShowFlagIndex );

	/**
	 * Checks if a show flag is enabled in this viewport
	 *
	 * @param EngineShowFlagIndex	the ID to check
	 * @return true if the show flag is enabled, false otherwise
	 */
	bool IsShowFlagEnabled( uint32 EngineShowFlagIndex ) const;

	/** Changes the auto exposure setting for this viewport */
	void ChangeExposureSetting();

	/** Checks if auto exposure setting is selected */
	bool IsExposureSettingSelected() const;
	
	virtual void OnScreenCapture();
	virtual void OnScreenCaptureForProjectThumbnail();
	virtual bool DoesAllowScreenCapture() { return true; }
	
	/**
	 * Changes the snapping grid size
	 */
	virtual void OnIncrementPositionGridSize() {};
	virtual void OnDecrementPositionGridSize() {};
	virtual void OnIncrementRotationGridSize() {};
	virtual void OnDecrementRotationGridSize() {};

	/**
	 * @return true if the specified widget mode is active
	 */
	virtual bool IsWidgetModeActive( UE::Widget::EWidgetMode Mode ) const;

	/**
	 * @return true if the translate/rotate mode widget is visible 
	 */
	virtual bool IsTranslateRotateModeVisible() const;

	/**
	* @return true if the 2d mode widget is visible
	*/
	virtual bool Is2DModeVisible() const;

	/**
	 * Moves between widget modes
	 */
	virtual void OnCycleWidgetMode();

	/**
	 * Called when the user wants to focus the viewport to the current selection
	 */
	virtual void OnFocusViewportToSelection(){}

	/** Gets the world this viewport is for */
	virtual UWorld* GetWorld() const;

	/**
	 * Called when surface snapping has been enabled/disabled
	 */
	static void OnToggleSurfaceSnap();

	/**
	 * Called to test whether surface snapping is enabled or not
	 */
	static bool OnIsSurfaceSnapEnabled();

protected:
	TSharedPtr<SOverlay> ViewportOverlay;

	/** Viewport that renders the scene provided by the viewport client */
	TSharedPtr<FSceneViewport> SceneViewport;
	
	/** Widget where the scene viewport is drawn in */
	TSharedPtr<SViewport> ViewportWidget;
	
	/** The client responsible for setting up the scene */
	TSharedPtr<FEditorViewportClient> Client;
	
	/** Commandlist used in the viewport (Maps commands to viewport specific actions) */
	TSharedPtr<FUICommandList> CommandList;
	
	/** The last time the viewport was ticked (for visibility determination) */
	double LastTickTime;

	FVector2D InViewportContextMenuLocation;

private:
	/** Ensures a Slate tick/paint pass when the viewport is realtime or was invalidated this frame */
	EActiveTimerReturnType EnsureTick( double InCurrentTime, float InDeltaTime );

	/** Gets the visibility of the active viewport border */
	EVisibility GetActiveBorderVisibility() const;
private:
	/** The handle to the active timer */
	TWeakPtr<FActiveTimerHandle> ActiveTimerHandle;

	/** Whether the viewport needs to update, even without input or realtime (e.g. inertial camera movement) */
	bool bInvalidated;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Package.h"
#include "FrameNumberDisplayFormat.h"
#include "SequencerSettings.generated.h"

struct FPropertyChangedEvent;
enum class EAutoChangeMode : uint8;
enum class EAllowEditsMode : uint8;
enum class EKeyGroupMode : uint8;
enum class EMovieSceneKeyInterpolation : uint8;

namespace UE::Sequencer
{
	enum class EViewDensity;
}

UENUM()
enum ESequencerSpawnPosition : int
{
	/** Origin. */
	SSP_Origin UMETA(DisplayName="Origin"),

	/** Place in Front of Camera. */
	SSP_PlaceInFrontOfCamera UMETA(DisplayName="Place in Front of Camera"),
};

UENUM()
enum ESequencerZoomPosition : int
{
	/** Playhead. */
	SZP_CurrentTime UMETA(DisplayName="Playhead"),

	/** Mouse Position. */
	SZP_MousePosition UMETA(DisplayName="Mouse Position"),
};

UENUM(BlueprintType)
enum ESequencerLoopMode : int
{
	/** No Looping. */
	SLM_NoLoop UMETA(DisplayName="No Looping"),

	/** Loop Playback Range. */
	SLM_Loop UMETA(DisplayName="Loop"),

	/** Loop Selection Range. */
	SLM_LoopSelectionRange UMETA(DisplayName="Loop Selection Range"),
};

/** Empty class used to house multiple named USequencerSettings */
UCLASS()
class SEQUENCER_API USequencerSettingsContainer
	: public UObject
{
public:
	GENERATED_BODY()

	/** Get or create a settings object for the specified name */
	template<class T> 
	static T* GetOrCreate(const TCHAR* InName)
	{
		static const TCHAR* SettingsContainerName = TEXT("SequencerSettingsContainer");

		auto* Outer = FindObject<USequencerSettingsContainer>(GetTransientPackage(), SettingsContainerName);
		if (!Outer)
		{
			Outer = NewObject<USequencerSettingsContainer>(GetTransientPackage(), USequencerSettingsContainer::StaticClass(), SettingsContainerName);
			Outer->AddToRoot();
		}
	
		T* Inst = FindObject<T>( Outer, InName );
		if (!Inst)
		{
			Inst = NewObject<T>( Outer, T::StaticClass(), InName );
			Inst->LoadConfig();
		}

		return Inst;
	}
};

/** Struct for storing reorderable and hidden/visible outliner columns */
USTRUCT(BlueprintType)
struct FColumnVisibilitySetting
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category=General)
	FName ColumnName;

	UPROPERTY(EditAnywhere, Category=General)
	bool bIsVisible;

	bool operator==(const FColumnVisibilitySetting& Other) const
	{
		return ColumnName == Other.ColumnName && bIsVisible == Other.bIsVisible;
	}

	FColumnVisibilitySetting(FName InColumnName, bool InbIsVisible)
		: ColumnName(InColumnName)
		, bIsVisible(InbIsVisible)
	{}

	FColumnVisibilitySetting()
		: ColumnName(NAME_None)
		, bIsVisible(false)
	{}
};

/** Serializable options for sequencer. */
UCLASS(config=EditorPerProjectUserSettings, PerObjectConfig)
class SEQUENCER_API USequencerSettings
	: public UObject
{
public:
	GENERATED_UCLASS_BODY()

	DECLARE_MULTICAST_DELEGATE( FOnEvaluateSubSequencesInIsolationChanged );
	DECLARE_MULTICAST_DELEGATE( FOnShowSelectedNodesOnlyChanged );
	DECLARE_MULTICAST_DELEGATE_OneParam( FOnAllowEditsModeChanged, EAllowEditsMode );
	DECLARE_MULTICAST_DELEGATE(FOnLoopStateChanged);
	DECLARE_MULTICAST_DELEGATE(FOnTimeDisplayFormatChanged);

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	/** Gets the current auto change mode. */
	EAutoChangeMode GetAutoChangeMode() const;
	/** Sets the current auto change mode. */
	void SetAutoChangeMode(EAutoChangeMode AutoChangeMode);

	/** Gets the current allow edits mode. */
	EAllowEditsMode GetAllowEditsMode() const;
	/** Sets the current auto-key mode. */
	void SetAllowEditsMode(EAllowEditsMode AllowEditsMode);
	/** Gets the multicast delegate which is run whenever the allow edits mode is changed. */
	FOnAllowEditsModeChanged& GetOnAllowEditsModeChanged() { return OnAllowEditsModeChangedEvent; }

	/** Returns what channels will get keyed when one channel changes */
	EKeyGroupMode GetKeyGroupMode() const;
	/** Sets which channels are keyed when a channel is keyed */
	void SetKeyGroupMode(EKeyGroupMode);

	/** Get the default Interpolation type for newly created keyframes if the channel does not already have keyframes */
	EMovieSceneKeyInterpolation GetKeyInterpolation() const;
	/** Sets default key interpolation for creating new keys on empty channels */
	void SetKeyInterpolation(EMovieSceneKeyInterpolation InKeyInterpolation);

	/** Get initial spawn position. */
	ESequencerSpawnPosition GetSpawnPosition() const;
	/** Set initial spawn position. */
	void SetSpawnPosition(ESequencerSpawnPosition InSpawnPosition);

	/** Get whether to create spawnable cameras. */
	bool GetCreateSpawnableCameras() const;
	/** Set whether to create spawnable cameras. */
	void SetCreateSpawnableCameras(bool bInCreateSpawnableCameras);

	/** Gets whether or not to show the time range slider. */
	bool GetShowRangeSlider() const;
	/** Sets whether or not to show frame numbers. */
	void SetShowRangeSlider(bool InbShowRangeSlider);

	/** Gets whether or not snapping is enabled. */
	bool GetIsSnapEnabled() const;
	/** Sets whether or not snapping is enabled. */
	void SetIsSnapEnabled(bool InbIsSnapEnabled);

	/** Gets whether or not to snap key times to the interval. */
	bool GetSnapKeyTimesToInterval() const;
	/** Sets whether or not to snap keys to the interval. */
	void SetSnapKeyTimesToInterval(bool InbSnapKeyTimesToInterval);

	/** Gets whether or not to snap keys to other keys. */
	bool GetSnapKeyTimesToKeys() const;
	/** Sets whether or not to snap keys to other keys. */
	void SetSnapKeyTimesToKeys(bool InbSnapKeyTimesToKeys);

	/** Gets whether or not to snap sections to the interval. */
	bool GetSnapSectionTimesToInterval() const;
	/** Sets whether or not to snap sections to the interval. */
	void SetSnapSectionTimesToInterval(bool InbSnapSectionTimesToInterval);

	/** Gets whether or not to snap sections to other sections. */
	bool GetSnapSectionTimesToSections() const;
	/** sets whether or not to snap sections to other sections. */
	void SetSnapSectionTimesToSections( bool InbSnapSectionTimesToSections );

	/** @return true if keys and sections should be kept within the playback range when moving them */
	bool GetSnapKeysAndSectionsToPlayRange() const;
	/** Set whether or not keys and sections should be kept within the playback range when moving them */
	void SetSnapKeysAndSectionsToPlayRange(bool bInSnapKeysAndSectionsToPlayRange);

	/** Gets whether or not to snap the play time to keys while scrubbing. */
	bool GetSnapPlayTimeToKeys() const;
	/** Sets whether or not to snap the play time to keys while scrubbing. */
	void SetSnapPlayTimeToKeys(bool InbSnapPlayTimeToKeys);

	/** Gets whether or not to snap the play time to section bounds while scrubbing. */
	bool GetSnapPlayTimeToSections() const;
	/** Sets whether or not to snap the play time to section bounds while scrubbing. */
	void SetSnapPlayTimeToSections(bool InbSnapPlayTimeToSections);

	/** Gets whether or not to snap the play time to markers while scrubbing. */
	bool GetSnapPlayTimeToMarkers() const;
	/** Sets whether or not to snap the play time to markers while scrubbing. */
	void SetSnapPlayTimeToMarkers(bool InbSnapPlayTimeToMarkers);

	/** Gets whether or not to snap the play time to the interval while scrubbing. */
	bool GetSnapPlayTimeToInterval() const;
	/** Sets whether or not to snap the play time to the interval while scrubbing. */
	void SetSnapPlayTimeToInterval(bool InbSnapPlayTimeToInterval);

	/** Gets whether or not to snap the play time to the pressed key. */
	bool GetSnapPlayTimeToPressedKey() const;
	/** Sets whether or not to snap the play time to the pressed key. */
	void SetSnapPlayTimeToPressedKey(bool InbSnapPlayTimeToPressedKey);

	/** Gets whether or not to snap the play time to the dragged key. */
	bool GetSnapPlayTimeToDraggedKey() const;
	/** Sets whether or not to snap the play time to the dragged key. */
	void SetSnapPlayTimeToDraggedKey(bool InbSnapPlayTimeToDraggedKey);

	/** Gets the snapping interval for curve values. */
	float GetCurveValueSnapInterval() const;
	/** Sets the snapping interval for curve values. */
	void SetCurveValueSnapInterval(float InCurveValueSnapInterval);

	/** Gets the state for spacing between grid lines */
	TOptional<float> GetGridSpacing() const;
	/** Sets the grid line spacing state */
	void SetGridSpacing(TOptional<float> InGridSpacing);

	/** Gets whether or not to snap curve values to the interval. */
	bool GetSnapCurveValueToInterval() const;
	/** Sets whether or not to snap curve values to the interval. */
	void SetSnapCurveValueToInterval(bool InbSnapCurveValueToInterval);

	/** Gets whether or not to show selected nodes only. */
	bool GetShowSelectedNodesOnly() const;
	/** Sets whether or not to show selected nodes only. */
	void SetShowSelectedNodesOnly(bool Visible);
	FOnShowSelectedNodesOnlyChanged& GetOnShowSelectedNodesOnlyChanged() { return OnShowSelectedNodesOnlyChangedEvent; }

	/** Gets whether left mouse drag does marquee select instead of camera orbit and ALT always moves the camera */
	bool GetLeftMouseDragDoesMarquee() const;
	/** Sets whether left mouse drag does marquee select instead of camera orbit and ALT always moves the camera */
	void SetLeftMouseDragDoesMarque(bool bDoMarque);

	/** Get zoom in/out position (mouse position or current time). */
	ESequencerZoomPosition GetZoomPosition() const;
	/** Set zoom in/out position (mouse position or current time). */
	void SetZoomPosition(ESequencerZoomPosition InZoomPosition);

	/** Gets whether or not auto-scroll is enabled when playing. */
	bool GetAutoScrollEnabled() const;
	/** Sets whether or not auto-scroll is enabled when playing. */
	void SetAutoScrollEnabled(bool bInAutoScrollEnabled);
	
	/** Gets whether or not to link the curve editor time range. */
	bool GetLinkCurveEditorTimeRange() const;
	/** Sets whether or not to link the curve editor time range. */
	void SetLinkCurveEditorTimeRange(bool InbLinkCurveEditorTimeRange);

	/** Return true if we are to synchronize the curve editor and sequencer trees */
	bool ShouldSyncCurveEditorSelection() const { return bSynchronizeCurveEditorSelection; }
	/** Assign whether we are to synchronize the curve editor and sequencer trees */
	void SyncCurveEditorSelection(bool bInSynchronizeCurveEditorSelection);

	/** Return true if we should filter the curve editor tree to only nodes that are relevant to the current sequencer selection */
	bool ShouldIsolateToCurveEditorSelection() const { return bIsolateCurveEditorToSelection; }
	/** Assign whether we should filter the curve editor tree to only nodes that are relevant to the current sequencer selection */
	void IsolateCurveEditorToSelection(bool bInIsolateCurveEditorToSelection);

	/** Gets the loop mode. */
	ESequencerLoopMode GetLoopMode() const;
	/** Sets the loop mode. */
	void SetLoopMode(ESequencerLoopMode InLoopMode);

	/** @return true if the cursor reset when navigating in and out of subsequences, false otherwise */
	bool ShouldResetPlayheadWhenNavigating() const;
	/** Set whether or not the cursor should be reset when navigating in and out of subsequences */
	void SetResetPlayheadWhenNavigating(bool bInResetPlayheadWhenNavigating);

	/** @return true if the cursor should be kept within the playback (or subsequence/shot) range while scrubbing in sequencer, false otherwise */
	bool ShouldKeepCursorInPlayRangeWhileScrubbing() const;
	/** Set whether or not the cursor should be kept within the playback (or subsequence/shot) range while scrubbing in sequencer */
	void SetKeepCursorInPlayRangeWhileScrubbing(bool bInKeepCursorInPlayRangeWhileScrubbing);

	/** @return true if the playback range should be synced to the section bounds, false otherwise */
	bool ShouldKeepPlayRangeInSectionBounds() const;
	/** Set whether or not the playback range should be synced to the section bounds */
	void SetKeepPlayRangeInSectionBounds(bool bInKeepPlayRangeInSectionBounds);

	/** Get the number of digits we should zero-pad to when showing frame numbers in sequencer */
	uint8 GetZeroPadFrames() const;
	/** Set the number of digits we should zero-pad to when showing frame numbers in sequencer */
	void SetZeroPadFrames(uint8 InZeroPadFrames);

	/** Get the number of frames to increment when jumping forwards/backwards */
	FFrameNumber GetJumpFrameIncrement() const;
	/** Set the number of frames to increment when jumping forwards/backwards */
	void SetJumpFrameIncrement(FFrameNumber InJumpFrameIncrement);

	/** @return true if showing layer bars */
	bool GetShowLayerBars() const;
	/** Set whether to show layer bars */ 
	void SetShowLayerBars(bool bInShowLayerBars);

	/** @return true if showing key bars */
	bool GetShowKeyBars() const;
	/** Set whether to show key bars */ 
	void SetShowKeyBars(bool bInShowKeyBars);

	/** @return true if key areas are infinite */
	bool GetInfiniteKeyAreas() const;
	/** Set whether to show channel colors */
	void SetInfiniteKeyAreas(bool bInInfiniteKeyAreas);

	/** @return true if showing channel colors for the key bars */
	bool GetShowChannelColors() const;
	/** Set whether to show channel colors for the key bars */
	void SetShowChannelColors(bool bInShowChannelColors);

	/** @return true if showing the info button in the playback controls */
	bool GetShowInfoButton() const;
	/** Set whether to show the info button in the playback controls */
	void SetShowInfoButton(bool bInShowInfoButton);

	/** @return true if showing tick lines */
	bool GetShowTickLines() const;
	/** Set whether to show status bar */
	void SetShowTickLines(bool bInDrawTickLines);

	/** @return true if showing sequencer toolbar */
	bool GetShowSequencerToolbar() const;
	/** Set whether to show sequencer toolbar bar */
	void SetShowSequencerToolbar(bool bInDrawTickLines);

	/** @return Whether the given channel has curve extents */
	bool HasKeyAreaCurveExtents(const FString& ChannelName) const;
	/** @ Remove curve extents for the given channel */
	void RemoveKeyAreaCurveExtents(const FString& ChannelName);
	/** @return Get the key area curve extents for the given channel */
	void GetKeyAreaCurveExtents(const FString& ChannelName, double& InMin, double& InMax) const;
	/** Set the key area curve extents for the given channel */
	void SetKeyAreaCurveExtents(const FString& ChannelName, double InMin, double InMax);

	/** @return The key area height when showing curves */
	float GetKeyAreaHeightWithCurves() const;
	/** Set the key area height when showing curves */
	void SetKeyAreaHeightWithCurves(float InKeyAreaHeightWithCurves);

	/** @return The tolerance to use when reducing keys */
	float GetReduceKeysTolerance() const;
	/** Set the tolerance to use when reducing keys */
	void SetReduceKeysTolerance(float InReduceKeysTolerance);

	/** @return true if deleting keys that fall beyond the section range when trimming */
	bool GetDeleteKeysWhenTrimming() const;
	/** Set whether to delete keys that fall beyond the section range when trimming */
	void SetDeleteKeysWhenTrimming(bool bInDeleteKeysWhenTrimming);

	/** @return true if disable sections when baking */
	bool GetDisableSectionsAfterBaking() const;
	/** Set whether to disable sections when baking, as opposed to deleting */
	void SetDisableSectionsAfterBaking(bool bInDisableSectionsAfterBaking);

	/** @return the section color tints */
	TArray<FColor> GetSectionColorTints() const;
	/** Set the section color tints */
	void SetSectionColorTints(const TArray<FColor>& InSectionColorTints);

	/** @return Whether to playback in clean mode (game view, hide viewport UI) */
	bool GetCleanPlaybackMode() const;
	/** Toggle whether to playback in clean mode */
	void SetCleanPlaybackMode(bool bInCleanPlaybackMode);

	/** @return Whether to activate realtime viewports when in sequencer */
	bool ShouldActivateRealtimeViewports() const;
	/** Toggle whether to allow possession of PIE viewports */
	void SetActivateRealtimeViewports(bool bInActivateRealtimeViewports);

	/** Gets whether or not track defaults will be automatically set when modifying tracks. */
	bool GetAutoSetTrackDefaults() const;
	/** Sets whether or not track defaults will be automatically set when modifying tracks. */
	void SetAutoSetTrackDefaults(bool bInAutoSetTrackDefaults);

	/** @return Whether to show debug vis */
	bool ShouldShowDebugVisualization() const;
	/** Toggle whether to show debug vis */
	void SetShowDebugVisualization(bool bInShowDebugVisualization);

	/** @return Whether to evaluate sub sequences in isolation */
	bool ShouldEvaluateSubSequencesInIsolation() const;
	/** Set whether to evaluate sub sequences in isolation */
	void SetEvaluateSubSequencesInIsolation(bool bInEvaluateSubSequencesInIsolation);
	/** Gets the multicast delegate which is run whenever evaluate sub sequences in isolation is changed. */
	FOnEvaluateSubSequencesInIsolationChanged& GetOnEvaluateSubSequencesInIsolationChanged() { return OnEvaluateSubSequencesInIsolationChangedEvent; }

	/** @return Whether to rerun construction scripts on bound actors every frame */
	bool ShouldRerunConstructionScripts() const;
	/** Set whether to rerun construction scripts on bound actors every frame */
	void SetRerunConstructionScripts(bool bInRerunConstructionScripts);

	/** Snaps a time value in seconds to the currently selected interval. */
	float SnapTimeToInterval(float InTimeValue) const;

	/** Check whether to show pre and post roll in sequencer */
	bool ShouldShowPrePostRoll() const;
	/** Toggle whether to show pre and post roll in sequencer */
	void SetShouldShowPrePostRoll(bool bInVisualizePreAndPostRoll);

	/** Check whether whether to recompile the director blueprint when the sequence is evaluated (if one exists) */
	bool ShouldCompileDirectorOnEvaluate() const;
	/** Assign whether whether to recompile the director blueprint when the sequence is evaluated (if one exists) */
	void SetCompileDirectorOnEvaluate(bool bInCompileDirectorOnEvaluate);

	uint32 GetTrajectoryPathCap() const { return TrajectoryPathCap; }

	FOnLoopStateChanged& GetOnLoopStateChanged();

	FOnTimeDisplayFormatChanged& GetOnTimeDisplayFormatChanged();

	/** What format should we display the UI controls in when representing time in a sequence? */
	EFrameNumberDisplayFormats GetTimeDisplayFormat() const { return FrameNumberDisplayFormat; }
	/** Sets the time display format to the specified type. */
	void SetTimeDisplayFormat(EFrameNumberDisplayFormats InFormat);

	/** What movie renderer to use */
	FString GetMovieRendererName() const { return MovieRendererName; }
	/** Sets the movie renderer to use */
	void SetMovieRendererName(const FString& InMovieRendererName);

	/** Gets whether or not to expand the outliner tree view when a child element is selected (from outside of the tree view). */
	bool GetAutoExpandNodesOnSelection() const { return bAutoExpandNodesOnSelection; }
	/** Sets whether or not to expand the outliner tree view when a child element is selected (from outside of the tree view). */
	void SetAutoExpandNodesOnSelection(bool bInAutoExpandNodesOnSelection);


	/**
	 * Gets whether unlocking a camera cut track should return the viewport to its original location, or keep it where
	 * the camera cut was.
	 */
	bool GetRestoreOriginalViewportOnCameraCutUnlock() const { return bRestoreOriginalViewportOnCameraCutUnlock; }
	/**
	 * Sets whether unlocking a camera cut track should return the viewport to its original location, or keep it where
	 * the camera cut was.
	 */
	void SetRestoreOriginalViewportOnCameraCutUnlock(bool bInRestoreOriginalViewportOnCameraCutUnlock);

	/** Gets the tree view width percentage */
	float GetTreeViewWidth() const { return TreeViewWidth; }
	/** Sets the tree view width percentage */
	void SetTreeViewWidth(float InTreeViewWidth);

	/** Gets the saved view density */
	UE::Sequencer::EViewDensity GetViewDensity() const;
	/** Sets the saved view density */
	void SetViewDensity(FName InViewDensity);

	/** Gets whether the given track filter is enabled */
	bool IsTrackFilterEnabled(const FString& TrackFilter) const;
	/** Sets whether the track filter should be enabled/disabled */
	void SetTrackFilterEnabled(const FString& TrackFilter, bool bEnabled);

	/** Get outliner column visibility in display order */
	TArray<FColumnVisibilitySetting> GetOutlinerColumnSettings() const { return ColumnVisibilitySettings; }
	/** Sets the visibility of outliner columns in display order */
	void SetOutlinerColumnVisibility(const TArray<FColumnVisibilitySetting>& InColumnVisibilitySettings);

protected:

	/** The auto change mode (auto-key, auto-track or none). */
	UPROPERTY( config, EditAnywhere, Category=Keyframing )
	EAutoChangeMode AutoChangeMode;

	/** Allow edits mode. */
	UPROPERTY( config, EditAnywhere, Category=Keyframing )
	EAllowEditsMode AllowEditsMode;

	/**Key group mode. */
	UPROPERTY(config, EditAnywhere, Category = Keyframing)
	EKeyGroupMode KeyGroupMode;

	/** The interpolation type for newly created keyframes */
	UPROPERTY( config, EditAnywhere, Category=Keyframing )
	EMovieSceneKeyInterpolation KeyInterpolation;

	/** Whether or not track defaults will be automatically set when modifying tracks. */
	UPROPERTY( config, EditAnywhere, Category=Keyframing, meta = (ToolTip = "When setting keys on properties and transforms automatically update the track default values used when there are no keys."))
	bool bAutoSetTrackDefaults;

	/** The default location of a spawnable when it is first dragged into the viewport from the content browser. */
	UPROPERTY( config, EditAnywhere, Category=General )
	TEnumAsByte<ESequencerSpawnPosition> SpawnPosition;

	/** Enable or disable creating of spawnable cameras whenever cameras are created. */
	UPROPERTY( config, EditAnywhere, Category=General )
	bool bCreateSpawnableCameras;

	/** Show the in/out range in the timeline with respect to the start/end range. */
	UPROPERTY( config, EditAnywhere, Category=Timeline )
	bool bShowRangeSlider;

	/** Enable or disable snapping in the timeline. */
	UPROPERTY( config, EditAnywhere, Category=Snapping )
	bool bIsSnapEnabled;

	/** Enable or disable snapping keys to the time snapping interval. */
	UPROPERTY( config, EditAnywhere, Category=Snapping )
	bool bSnapKeyTimesToInterval;

	/** Enable or disable snapping keys to other keys. */
	UPROPERTY( config, EditAnywhere, Category=Snapping )
	bool bSnapKeyTimesToKeys;
	
	/** Enable or disable snapping sections to the time snapping interval. */
	UPROPERTY( config, EditAnywhere, Category=Snapping )
	bool bSnapSectionTimesToInterval;

	/** Enable or disable snapping sections to other sections. */
	UPROPERTY( config, EditAnywhere, Category=Snapping )
	bool bSnapSectionTimesToSections;

	/** Enable or disable keeping keys and sections in the playback range. */
	UPROPERTY(config, EditAnywhere, Category = Timeline)
	bool bSnapKeysAndSectionsToPlayRange;

	/** Enable or disable snapping the playhead to keys while scrubbing. */
	UPROPERTY( config, EditAnywhere, Category=Snapping, meta = (DisplayName = "Snap Playhead to Keys"))
	bool bSnapPlayTimeToKeys;

	/** Enable or disable snapping the playhead to section bounds while scrubbing. */
	UPROPERTY( config, EditAnywhere, Category=Snapping, meta = (DisplayName = "Snap Playhead to Sections"))
	bool bSnapPlayTimeToSections;

	/** Enable or disable snapping the playhead to markers while scrubbing. */
	UPROPERTY( config, EditAnywhere, Category=Snapping, meta = (DisplayName = "Snap Playhead to Markers"))
	bool bSnapPlayTimeToMarkers;

	/** Enable or disable snapping the playhead to the time snapping interval while scrubbing. */
	UPROPERTY( config, EditAnywhere, Category=Snapping, meta = (DisplayName = "Snap Playhead to Interval"))
	bool bSnapPlayTimeToInterval;

	/** Enable or disable snapping the playhead to the pressed key. */
	UPROPERTY( config, EditAnywhere, Category=Snapping, meta = (DisplayName = "Snap Playhead to Pressed Key"))
	bool bSnapPlayTimeToPressedKey;

	/** Enable or disable snapping the playhead to the dragged key. */
	UPROPERTY( config, EditAnywhere, Category=Snapping, meta = (DisplayName = "Snap Playhead to Dragged Key"))
	bool bSnapPlayTimeToDraggedKey;

	/** The curve value interval to snap to. */
	float CurveValueSnapInterval;

	/** grid line spacing state */
	TOptional<float> GridSpacing;

	/** Enable or disable snapping the curve value to the curve value interval. */
	UPROPERTY( config, EditAnywhere, Category=Snapping )
	bool bSnapCurveValueToInterval;

	/** Only show selected nodes in the tree view. */
	UPROPERTY( config, EditAnywhere, Category=General )
	bool bShowSelectedNodesOnly;

	/** Defines whether to jump back to the start of the sequence when a recording is started */
	UPROPERTY(config, EditAnywhere, Category=General)
	bool bRewindOnRecord;

	/** Defines whether left mouse drag does marquee select instead of camera orbit */
	UPROPERTY(config, EditAnywhere, Category = General)
	bool bLeftMouseDragDoesMarquee;

	/** Whether to zoom in on the current position or the current time in the timeline. */
	UPROPERTY( config, EditAnywhere, Category=Timeline )
	TEnumAsByte<ESequencerZoomPosition> ZoomPosition;

	/** Enable or disable auto scroll in the timeline when playing. */
	UPROPERTY( config, EditAnywhere, Category=Timeline )
	bool bAutoScrollEnabled;

	/** Enable or disable linking the curve editor time range to the sequencer timeline's time range. */
	UPROPERTY( config, EditAnywhere, Category=CurveEditor )
	bool bLinkCurveEditorTimeRange;

	/** When enabled, changing the sequencer tree selection will also select the relevant nodes in the curve editor tree if possible. */
	UPROPERTY( config, EditAnywhere, Category=CurveEditor )
	bool bSynchronizeCurveEditorSelection;

	/** When enabled, changing the sequencer tree selection will isolate (auto-filter) the selected nodes in the curve editor. */
	UPROPERTY( config, EditAnywhere, Category=CurveEditor )
	bool bIsolateCurveEditorToSelection;

	/** The loop mode of the playback in timeline. */
	UPROPERTY( config )
	TEnumAsByte<ESequencerLoopMode> LoopMode;

	/** Enable or disable resetting the playhead when navigating in and out of subsequences. */
	UPROPERTY(config, EditAnywhere, Category = Timeline, meta = (DisplayName = "Reset Playhead When Navigating"))
	bool bResetPlayheadWhenNavigating;

	/** Enable or disable keeping the playhead in the current playback range while scrubbing. */
	UPROPERTY(config, EditAnywhere, Category = Timeline, meta = (DisplayName = "Keep Playhead in Play Range While Scrubbing"))
	bool bKeepCursorInPlayRangeWhileScrubbing;

	/** Enable or disable keeping the playback range constrained to the section bounds. */
	UPROPERTY( config, EditAnywhere, Category=Timeline )
	bool bKeepPlayRangeInSectionBounds;

	/** The number of zeros to pad the frame numbers by. */
	UPROPERTY( config, EditAnywhere, Category=Timeline )
	uint8 ZeroPadFrames;

	/** The number of frames to jump by when jumping forward or backwards. */
	UPROPERTY( config, EditAnywhere, Category=Timeline )
	FFrameNumber JumpFrameIncrement;

	/** Enable or disable the layer bars to edit keyframes in bulk. */
	UPROPERTY( config, EditAnywhere, Category=Timeline )
	bool bShowLayerBars;

	/** Enable or disable key bar connections. */
	UPROPERTY( config, EditAnywhere, Category=Timeline )
	bool bShowKeyBars;

	/** Enable or disable setting key area sections as infinite by default. */
	UPROPERTY( config, EditAnywhere, Category=Timeline )
	bool bInfiniteKeyAreas;

	/** Enable or disable displaying channel bar colors for the key bars. */
	UPROPERTY(config, EditAnywhere, Category = Timeline)
	bool bShowChannelColors;

	/** Enable or disable displaying the info button in the playback controls. */
	UPROPERTY(config, EditAnywhere, Category = Timeline)
	bool bShowInfoButton;

	/** Enable or disable displaying the tick lines. */
	UPROPERTY(config, EditAnywhere, Category = Timeline)
	bool bShowTickLines;

	/** Enable or disable displaying the sequencer toolbar. */
	UPROPERTY(config, EditAnywhere, Category = Timeline)
	bool bShowSequencerToolbar;

	/** The key area curve extents, stored per channel name */
	UPROPERTY(config, EditAnywhere, Category = Timeline)
	FString KeyAreaCurveExtents;

	/** The key area height when showing curves */
	UPROPERTY(config, EditAnywhere, Category = Timeline)
	float KeyAreaHeightWithCurves;

	/** The tolerance to use when reducing keys */
	UPROPERTY(config, EditAnywhere, Category = Timeline)
	float ReduceKeysTolerance;

	/** Enable or disable deleting keys that fall beyond the section range when trimming. */
	UPROPERTY(config, EditAnywhere, Category = Timeline)
	bool bDeleteKeysWhenTrimming;

	/** Whether to disable sections after baking as opposed to deleting. */
	UPROPERTY(config, EditAnywhere, Category = Timeline)
	bool bDisableSectionsAfterBaking;

	/** Section color tints */
	UPROPERTY(config, EditAnywhere, Category = General)
	TArray<FColor> SectionColorTints;

	/** When enabled, sequencer will playback in clean mode (game view, hide viewport UI) */
	UPROPERTY(config, EditAnywhere, Category = General)
	bool bCleanPlaybackMode;

	/** When enabled, sequencer will activate 'Realtime' in viewports */
	UPROPERTY(config, EditAnywhere, Category = General)
	bool bActivateRealtimeViewports;

	/** When enabled, entering a sub sequence will evaluate that sub sequence in isolation, rather than from the root sequence */
	UPROPERTY(config, EditAnywhere, Category=Playback)
	bool bEvaluateSubSequencesInIsolation;

	/** When enabled, construction scripts will be rerun on bound actors for every frame */
	UPROPERTY(config, EditAnywhere, Category=Playback)
	bool bRerunConstructionScripts;

	/** Enable or disable showing of debug visualization. */
	UPROPERTY( config, EditAnywhere, Category=General )
	bool bShowDebugVisualization;

	/** Enable or disable showing of pre and post roll visualization. */
	UPROPERTY( config, EditAnywhere, Category=General )
	bool bVisualizePreAndPostRoll;

	/** Whether to recompile the director blueprint when the sequence is evaluated (if one exists) */
	UPROPERTY(config, EditAnywhere, Category=General)
	bool bCompileDirectorOnEvaluate;

	/** Specifies the maximum number of keys to draw when rendering trajectories in viewports */
	UPROPERTY(config, EditAnywhere, Category = General)
	uint32 TrajectoryPathCap;

	/** What format do we display time in to the user? */
	UPROPERTY(config, EditAnywhere, Category=General)
	EFrameNumberDisplayFormats FrameNumberDisplayFormat;

	/** Which movie renderer to use */
	UPROPERTY(config, EditAnywhere, Category=General)
	FString MovieRendererName;

	/** Whether to expand the sequencer tree view when a child element is selected (from outside of the tree view). */
	UPROPERTY(config, EditAnywhere, Category = General)
	bool bAutoExpandNodesOnSelection;

	/**
	 * Whether unlocking a camera cut track should return the viewport to its original location, or keep it where the
	 * camera cut was.
	 * WARNING: Disabling this will make previewing camera cut blends useless, since it will blend to the same position.
	 */
	UPROPERTY(config, EditAnywhere, Category=General)
	bool bRestoreOriginalViewportOnCameraCutUnlock;

	/** The tree view width percentage */
	UPROPERTY(config, EditAnywhere, Category = General)
	float TreeViewWidth;

	UPROPERTY(config, EditAnywhere, Category = General)
	FName ViewDensity;

	/** The track filters that are enabled */
	UPROPERTY(config, EditAnywhere, Category = General)
	TArray<FString> TrackFilters;

	/** List of all columns and their visibility, in the order to be displayed in the outliner view */
	UPROPERTY(config, EditAnywhere, Category = General)
	TArray<FColumnVisibilitySetting> ColumnVisibilitySettings;

	FOnEvaluateSubSequencesInIsolationChanged OnEvaluateSubSequencesInIsolationChangedEvent;
	FOnShowSelectedNodesOnlyChanged OnShowSelectedNodesOnlyChangedEvent;
	FOnAllowEditsModeChanged OnAllowEditsModeChangedEvent;
	FOnLoopStateChanged OnLoopStateChangedEvent;
	FOnTimeDisplayFormatChanged OnTimeDisplayFormatChangedEvent;
};

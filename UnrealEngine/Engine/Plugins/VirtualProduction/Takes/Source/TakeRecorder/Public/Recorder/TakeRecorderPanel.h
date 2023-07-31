// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/FrameRate.h"
#include "TakeRecorderPanel.generated.h"

class UTakePreset;
class ULevelSequence;
class UTakeMetaData;
class UTakeRecorderSources;

class STakeRecorderTabContent;

UENUM()
enum class ETakeRecorderPanelMode : uint8
{
	/** The panel is setting up a new recording */
	NewRecording,
	/** The panel is setting up recording into an existing level sequence */
	RecordingInto,
	/** The panel is editing a Take Preset asset */
	EditingPreset,
	/** The panel is reviewing a previously recorded take */
	ReviewingRecording,
};

/**
 * Take recorder UI panel interop object
 */
UCLASS()
class TAKERECORDER_API UTakeRecorderPanel : public UObject
{
public:

	GENERATED_BODY()

	/**
	 * Get the mode that the panel is currently in
	 */
	UFUNCTION(BlueprintCallable, Category="Take Recorder|Panel")
	ETakeRecorderPanelMode GetMode() const;

	/**
	 * Setup this panel such that it is ready to start recording using the specified
	 * take preset as a template for the recording.
	 */
	UFUNCTION(BlueprintCallable, Category="Take Recorder|Panel", DisplayName="Set Mode (Recording w/ Take Preset)")
	void SetupForRecording_TakePreset(UTakePreset* TakePresetAsset);

	/**
	 * Setup this panel such that it is ready to start recording using the specified
	 * level sequence asset as a template for the recording.
	 */
	UFUNCTION(BlueprintCallable, Category="Take Recorder|Panel", DisplayName="Set Mode (Recording w/ Level Sequence)")
	void SetupForRecording_LevelSequence(ULevelSequence* LevelSequenceAsset);

	/**
	 * Setup this panel such that it is ready to start recording using the specified
	 * level sequence asset to record into.
	 */
	UFUNCTION(BlueprintCallable, Category="Take Recorder|Panel", DisplayName="Set Mode (Recording into Level Sequence)")
	void SetupForRecordingInto_LevelSequence(ULevelSequence* LevelSequenceAsset);

	/**
	 * Setup this panel as an editor for the specified take preset asset.
	 */
	UFUNCTION(BlueprintCallable, Category="Take Recorder|Panel", DisplayName="Set Mode (Editing Take Preset)")
	void SetupForEditing(UTakePreset* TakePreset);

	/**
	 * Setup this panel as a viewer for a previously recorded take.
	 */
	UFUNCTION(BlueprintCallable, Category="Take Recorder|Panel", DisplayName="Set Mode (Read-Only Level Sequence)")
	void SetupForViewing(ULevelSequence* LevelSequenceAsset);

	UE_DEPRECATED(4.25, "Please use ClearPendingTake instead")
	UFUNCTION(BlueprintCallable, Category = "Take Recorder|Panel", DisplayName = "New Take", meta = (DeprecatedFunction, DeprecationMessage = "Please use ClearPendingTake instead"))
	void NewTake() { ClearPendingTake(); }

	/*
	 * Clear the pending take level sequence
	 */
	UFUNCTION(BlueprintCallable, Category = "Take Recorder|Panel", DisplayName = "Clear Pending Take")
	void ClearPendingTake();

	/**
	 * Access the level sequence for this take
	 */
	UFUNCTION(BlueprintCallable, Category="Take Recorder|Panel")
	ULevelSequence* GetLevelSequence() const;


	/**
	 * Access the last level sequence that was recorded
	 */
	UFUNCTION(BlueprintCallable, Category="Take Recorder|Panel")
	ULevelSequence* GetLastRecordedLevelSequence() const;


	/**
	 * Access take meta data for this take
	 */
	UFUNCTION(BlueprintCallable, Category="Take Recorder|Panel")
	UTakeMetaData* GetTakeMetaData() const;


	/**
	 * Access the frame rate for this take
	 */
	UFUNCTION(BlueprintCallable, Category="Take Recorder|Panel")
	FFrameRate GetFrameRate() const;


	/**
	* Set the frame rate for this take
	*/
	UFUNCTION(BlueprintCallable, Category = "Take Recorder|Panel")
	void SetFrameRate(FFrameRate InFrameRate);


	/**
	* Set if the frame rate is set from the Timecode frame rate
	*/
	UFUNCTION(BlueprintCallable, Category = "Take Recorder|Panel")
	void SetFrameRateFromTimecode(bool  bInFromTimecode);


	/**
	 * Access the sources that are to be (or were) used for recording this take
	 */
	UFUNCTION(BlueprintCallable, Category="Take Recorder|Panel")
	UTakeRecorderSources* GetSources() const;


	/**
	 * Start recording with the current take
	 */
	UFUNCTION(BlueprintCallable, Category="Take Recorder|Panel")
	void StartRecording() const;


	/**
	 * Stop recording with the current take
	 */
	UFUNCTION(BlueprintCallable, Category="Take Recorder|Panel")
	void StopRecording() const;

	/**
	 * Whether the panel is ready to start recording
	 */
	UFUNCTION(BlueprintCallable, Category="Take Recorder|Panel")
	bool CanStartRecording(FText& OutErrorText) const;

public:

	/*~ Native interface ~*/

	/**
	 * Initialize this object with a weak pointer to the tab content from which all the UI state can be retrieved
	 */
	void InitializePanel(TWeakPtr<STakeRecorderTabContent> InTabContent);

	/**
	 * Check whether this panel is still open or not
	 */
	bool IsPanelOpen() const;

	/**
	 * Invalidate this object by reporting that it is no longer open. Any subsequent scripting interactions will result in an error
	 */
	void ClosePanel();

private:

	bool ValidateTabContent() const;

	TWeakPtr<STakeRecorderTabContent> WeakTabContent;
};

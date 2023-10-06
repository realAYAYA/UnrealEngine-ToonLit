// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosVDRecording.h"
#include "Templates/SharedPointer.h"
#include "Delegates/DelegateCombinations.h"
#include "Delegates/Delegate.h"

struct FChaosVDTrackInfo;
class FChaosVDScene;
struct FChaosVDRecording;
class FChaosVDPlaybackController;
class FString;

DECLARE_MULTICAST_DELEGATE_OneParam(FChaosVDPlaybackControllerUpdated, TWeakPtr<FChaosVDPlaybackController>)
DECLARE_MULTICAST_DELEGATE_ThreeParams(FChaosVDPlaybackControllerFrameUpdated, TWeakPtr<FChaosVDPlaybackController>, const FChaosVDTrackInfo*, FGuid);

/** Enum with the available game track types*/
enum class EChaosVDTrackType : int32
{
	Invalid,
	Game,
	Solver
};

/** Data that represents the current state of a track and ID info*/
struct FChaosVDTrackInfo
{
	int32 TrackID = INDEX_NONE;
	EChaosVDTrackType TrackType = EChaosVDTrackType::Invalid;
	int32 CurrentFrame = INDEX_NONE;
	int32 CurrentStep = INDEX_NONE;
	int32 LockedOnStep = INDEX_NONE;
	int32 MaxFrames = INDEX_NONE;
	FString TrackName;
};

/** Flags used to control how the unload of a recording is performed */
enum class EChaosVDUnloadRecordingFlags : uint8
{
	None = 0, 
	BroadcastChanges = 1 << 0,
	Silent = 1 << 1
};
ENUM_CLASS_FLAGS(EChaosVDUnloadRecordingFlags)

typedef TMap<int32, TSharedPtr<FChaosVDTrackInfo>> TrackInfoByIDMap;

/** Loads,unloads and owns a Chaos VD recording file */
class FChaosVDPlaybackController : public TSharedFromThis<FChaosVDPlaybackController>
{
public:

	/** ID used for the Game Track */
	static constexpr int32 GameTrackID  = 0 ;

	FChaosVDPlaybackController(const TWeakPtr<FChaosVDScene>& InSceneToControl);
	~FChaosVDPlaybackController();

	/** Loads a recording using a Trace Session Name */
	bool LoadChaosVDRecordingFromTraceSession(const FString& InSessionName);

	/** Unloads the currently loaded recording
	 * @param UnloadOptions Options flags to change the steps performed during the unload
	 */
	void UnloadCurrentRecording(EChaosVDUnloadRecordingFlags UnloadOptions = EChaosVDUnloadRecordingFlags::BroadcastChanges);

	/** Returns true if the controller has a valid recording loaded*/
	bool IsRecordingLoaded() const { return LoadedRecording.IsValid(); }

	/** Returns a weak ptr to the Scene this controller is controlling during playback */
	TWeakPtr<FChaosVDScene> GetControllerScene() { return SceneToControl; }

	/**
	 * Moves a track of the recording to the specified step and frame numbers
	 * @param InstigatorID ID of the ControllerInstigator that requested the move
	 * @param TrackType Type of the track to to move
	 * @param InTrackID ID of the track to move
	 * @param FrameNumber Frame number to go
	 * @param Step Step number to go
	 */
	void GoToTrackFrame(FGuid InstigatorID, EChaosVDTrackType TrackType, int32 InTrackID, int32 FrameNumber, int32 Step);

	/**
	 * Gets the number of available steps in a track at the specified frame
	 * @param TrackType Type of the track to evaluate
	 * @param InTrackID ID of the track to evaluate
	 * @param FrameNumber Frame number to evaluate
	 * @return Number of available steps
	 */
	int32 GetTrackStepsNumberAtFrame(EChaosVDTrackType TrackType, int32 InTrackID, int32 FrameNumber) const;

	/**
	 * Gets the available steps container in a track at the specified frame
	 * @param TrackType Type of the track to evaluate
	 * @param InTrackID ID of the track to evaluate
	 * @param FrameNumber Frame number to evaluate
	 * @return Ptr to the steps data container
	 */
	const FChaosVDStepsContainer* GetTrackStepsDataAtFrame(EChaosVDTrackType TrackType, int32 InTrackID, int32 FrameNumber) const;

	/**
	 * Gets the number of available frames for the specified track
	 * @param TrackType Type of the track to evaluate
	 * @param InTrackID ID of the track to evaluate
	 * @return Number of available frames
	 */
	int32 GetTrackFramesNumber(EChaosVDTrackType TrackType, int32 InTrackID) const;
	
	/**
	 * Gets the current frame number at which the specified track is at
	 * @param TrackType Type of the track to evaluate
	 * @param InTrackID ID of the track to evaluate
	 * @return Current frame number for the track
	 */
	int32 GetTrackCurrentFrame(EChaosVDTrackType TrackType, int32 InTrackID) const;

	/**
	 * Gets the number of available frames for the specified track
	 * @param TrackType Type of the track to evaluate
	 * @param InTrackID ID of the track to evaluate
	 * @return Number of available frames
	 */
	int32 GetTrackCurrentStep(EChaosVDTrackType TrackType, int32 InTrackID) const;

	/**
	 * Gets the index number of the last step available (available steps -1)
	 * @param TrackType Type of the track to evaluate
	 * @param InTrackID ID of the track to evaluate
	 * @return Number of the last step
	 */
	int32 GetTrackLastStepAtFrame(EChaosVDTrackType TrackType, int32 InTrackID, int32 InFrameNumber) const;

	/** Converts the current frame number of a track, to a frame number in other tracks space time
	 * @param FromTrack Track info with the current frame number we want to convert
	 * @param ToTrack Track info we want to use to convert the frame to
	 * @return Converted Frame Number
	 */
	int32 ConvertCurrentFrameToOtherTrackFrame(const FChaosVDTrackInfo* FromTrack, const FChaosVDTrackInfo* ToTrack);

	/**
	 * Gets all the ids of the tracks, of the specified type, that are available available on the loaded recording
	 * @param TrackType Type of the tracks we are interested in
	 * @param OutTrackInfo Array where any found track info data will be added
	 */

	void GetAvailableTracks(EChaosVDTrackType TrackType, TArray<TSharedPtr<FChaosVDTrackInfo>>& OutTrackInfo);
	
	/**
	 * Gets all the ids of the tracks, of the specified type, that are available available on the loaded recording, at a specified frame
	 * @param TrackTypeToFind Type of the tracks we are interested in
	 * @param OutTrackInfo Array where any found track info data will be added
	 * @param TrackFrameInfo Ptr to the track info with the current frame to evaluate
	 */
	void GetAvailableTrackInfosAtTrackFrame(EChaosVDTrackType TrackTypeToFind, TArray<TSharedPtr<FChaosVDTrackInfo>>& OutTrackInfo, const FChaosVDTrackInfo* TrackFrameInfo);

	/**
	 * Gets the track info of the specified type with the specified ID
	 * @param TrackType Type of the track to find
	 * @param TrackID ID of the track to find
	 * @return Ptr to the found track info data - Null if nothing was found
	 */
	const FChaosVDTrackInfo* GetTrackInfo(EChaosVDTrackType TrackType, int32 TrackID);

	/**
	 * Gets the track info of the specified type with the specified ID
	 * @param TrackType Type of the track to find
	 * @param TrackID ID of the track to find
	 * @return Ptr to the found track info data - Null if nothing was found.
	 */
	FChaosVDTrackInfo* GetMutableTrackInfo(EChaosVDTrackType TrackType, int32 TrackID);


	/**
	 * Locks the steps timeline of a given track so each time you move between frames, it will automatically scrub to the locked in step
	 * @param TrackType Type of the track to find
	 * @param TrackID ID of the track to find
	 */
	void LockTrackInCurrentStep(EChaosVDTrackType TrackType, int32 TrackID);

	/**
	 * UnLocks the steps timeline of a given track so each time you move between frames, it will automatically scrub to the default step
	 * @param TrackType Type of the track to find
	 * @param TrackID ID of the track to find
	 */
	void UnlockTrackStep(EChaosVDTrackType TrackType, int32 TrackID);

	/** Returns a weak ptr pointer to the loaded recording */
	TWeakPtr<FChaosVDRecording> GetCurrentRecording() { return LoadedRecording; }

	/** Called when data on the recording being controlled gets updated internally or externally (for example, during Trace Analysis)*/
	FChaosVDPlaybackControllerUpdated& OnDataUpdated() { return ControllerUpdatedDelegate; }

	/** Called when a frame on a track is updated */
	FChaosVDPlaybackControllerFrameUpdated& OnTrackFrameUpdated() { return ControllerFrameUpdatedDelegate; }

protected:

	/** Updates (or adds) solvers data from the loaded recording to the solver tracks */
	void UpdateSolverTracksData();

	/** Updates the controlled scene with the loaded data at specified game frame */
	void GoToRecordedGameFrame(int32 FrameNumber, FGuid InstigatorID);

	/** Updates the controlled scene with the loaded data at specified solver frame and solver step */
	void GoToRecordedSolverStep(int32 InTrackID, int32 FrameNumber, int32 Step, FGuid InstigatorID);

	/** Handles any data changes on the loaded recording - Usually called during Trace analysis */
	void HandleCurrentRecordingUpdated();

	/** Finds the closest Key frame to the provided frame number, and plays all the following frames until the specified frame number (no inclusive) */
	void PlayFromClosestKeyFrame(int32 InTrackID, int32 FrameNumber, FChaosVDScene& InSceneToControl);

	/** Map containing all track info, by track type*/
	TMap<EChaosVDTrackType, TrackInfoByIDMap> TrackInfoPerType;

	/** Ptr to the loaded recording */
	TSharedPtr<FChaosVDRecording> LoadedRecording;

	/**Ptr to the current Chaos VD Scene this controller controls*/
	TWeakPtr<FChaosVDScene> SceneToControl;

	/** Delegate called when the data on the loaded recording changes */
	FChaosVDPlaybackControllerUpdated ControllerUpdatedDelegate;

	/** Delegate called when the a in a track changes */
	FChaosVDPlaybackControllerFrameUpdated ControllerFrameUpdatedDelegate;
};

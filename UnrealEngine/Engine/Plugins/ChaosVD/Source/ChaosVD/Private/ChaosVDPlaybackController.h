// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosVDRecording.h"
#include "Containers/Queue.h"
#include "Containers/Ticker.h"
#include "Templates/SharedPointer.h"
#include "Delegates/DelegateCombinations.h"
#include "Delegates/Delegate.h"
#include "HAL/ThreadSafeBool.h"

class IChaosVDPlaybackControllerInstigator;
class UChaosVDEditorSettings;
struct FChaosVDTraceSessionDescriptor;
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
	bool bIsReSimulated = false;
};

struct FChaosVDQueuedTrackInfoUpdate
{
	FChaosVDTrackInfo TrackInfo;
	FGuid InstigatorID;
};

struct FChaosVDGeometryDataUpdate
{
	Chaos::FConstImplicitObjectPtr NewGeometry;
	uint32 GeometryID;
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
class FChaosVDPlaybackController : public TSharedFromThis<FChaosVDPlaybackController>, public FTSTickerObjectBase
{
public:

	/** ID used for the Game Track */
	static constexpr int32 GameTrackID  = 0 ;
	static constexpr int32 InvalidFrameRateOverride  = -1 ;
	static constexpr float FallbackFrameTime = 1.0f / 60.0f ;

	FChaosVDPlaybackController(const TWeakPtr<FChaosVDScene>& InSceneToControl);
	virtual ~FChaosVDPlaybackController() override;

	/** Loads a recording using a CVD Trace Session Descriptor */
	bool LoadChaosVDRecordingFromTraceSession(const FChaosVDTraceSessionDescriptor& InSessionDescriptor);

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
	void GoToTrackFrame_AssumesLocked(FGuid InstigatorID, EChaosVDTrackType TrackType, int32 InTrackID, int32 FrameNumber, int32 Step);

	/**
	 * Gets the number of available steps in a track at the specified frame
	 * @param TrackType Type of the track to evaluate
	 * @param InTrackID ID of the track to evaluate
	 * @param FrameNumber Frame number to evaluate
	 * @return Number of available steps
	 */
	int32 GetTrackStepsNumberAtFrame_AssumesLocked(EChaosVDTrackType TrackType, int32 InTrackID, int32 FrameNumber) const;

	/**
	 * Gets the available steps container in a track at the specified frame
	 * @param TrackType Type of the track to evaluate
	 * @param InTrackID ID of the track to evaluate
	 * @param FrameNumber Frame number to evaluate
	 * @return Ptr to the steps data container
	 */
	const FChaosVDStepsContainer* GetTrackStepsDataAtFrame_AssumesLocked(EChaosVDTrackType TrackType, int32 InTrackID, int32 FrameNumber) const;

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
	
	virtual bool Tick(float DeltaTime) override;

	/** Returns true if we are playing a live debugging session */
	bool IsPlayingLiveSession() const;

	/** Updates the loaded recording state to indicate is not longer receiving live updates */
	void HandleDisconnectedFromSession();

	void RequestPause() { bPauseRequested = true; }
	void RequestUnpause() { bPauseRequested = false; }
	bool HasPauseRequest() const { return bPauseRequested; }
	void RequestStop(const IChaosVDPlaybackControllerInstigator& InPlaybackInstigator);

	/** Returns the ID of the current playback instigator that is controlling the playback */
	const FGuid& GetPlaybackInstigatorWithExclusiveControlsID() const { return CurrentPlaybackInstigator; }

	bool AcquireExclusivePlaybackControls(const IChaosVDPlaybackControllerInstigator& InPlaybackInstigator);
	bool ReleaseExclusivePlaybackControls(const IChaosVDPlaybackControllerInstigator& InPlaybackInstigator);

	float GetFrameTimeOverride() const;
	float GetFrameTimeForTrack(EChaosVDTrackType TrackType, int32 TrackID, const FChaosVDTrackInfo& TrackInfo) const;

	void UpdateTrackVisibility(EChaosVDTrackType Type, int32 TrackID, bool bNewVisibility);

protected:

	/** Updates (or adds) solvers data from the loaded recording to the solver tracks */
	void UpdateSolverTracksData();

	/** Updates the controlled scene with the loaded data at specified game frame */
	void GoToRecordedGameFrame_AssumesLocked(int32 FrameNumber, FGuid InstigatorID, int32 Attempts = 0);

	/** Updates the controlled scene with the loaded data at specified solver frame and solver step */
	void GoToRecordedSolverStep_AssumesLocked(int32 InTrackID, int32 FrameNumber, int32 Step, FGuid InstigatorID, int32 Attempts = 0);

	/** Handles any data changes on the loaded recording - Usually called during Trace analysis */
	void HandleCurrentRecordingUpdated();

	/** Finds the closest Key frame to the provided frame number, and plays all the following frames until the specified frame number (no inclusive) */
	void PlayFromClosestKeyFrame_AssumesLocked(int32 InTrackID, int32 FrameNumber, FChaosVDScene& InSceneToControl) const;

	/** Add the provided track info update to the queue. The update will be broadcast in the game thread */
	void EnqueueTrackInfoUpdate(const FChaosVDTrackInfo& InTrackInfo, FGuid InstigatorID);

	/** Add the provided Geometry info data to the queue. The update will be broadcast in the game thread */
	void EnqueueGeometryDataUpdate(const Chaos::FConstImplicitObjectPtr& NewGeometry, const uint32 GeometryID);

	void HandleFrameRateOverrideSettingsChanged(UChaosVDEditorSettings* CVDSettings);

	void PlaySolverStepData(int32 TrackID, const TSharedRef<FChaosVDScene>& InSceneToControlSharedPtr, const FChaosVDSolverFrameData& InSolverFrameData, int32 StepIndex);

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

	/** Set to true when the recording data controlled by this Playback Controller is updated, the update delegate will be called on the GT */
	std::atomic<bool> bHasPendingGTUpdateBroadcast;

	/** Last seen Platform Cycle on which the loaded recording was updated */
	uint64 RecordingLastSeenTimeUpdatedAsCycle = 0;

	/** Queue with a copy of all Track Info Updates that needs to be done in the Game thread */
	TQueue<FChaosVDQueuedTrackInfoUpdate, EQueueMode::Mpsc> TrackInfoUpdateGTQueue;

	/** Queue with a all the new geometry data that needs to be processed in the Game thread */
	TQueue<FChaosVDGeometryDataUpdate, EQueueMode::Mpsc> GeometryDataUpdateGTQueue;

	bool bPlayedFirstFrame = false;

	int32 MaxFramesLaggingBehindDuringLiveSession = 50;
	int32 MinFramesLaggingBehindDuringLiveSession = 5;

	int32 CurrentFrameRateOverride = InvalidFrameRateOverride;

	bool bPauseRequested = false;

	FDelegateHandle RecordingStoppedHandle;

	FGuid CurrentPlaybackInstigator;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDPlaybackController.h"

#include "ChaosVDEditorSettings.h"
#include "ChaosVDModule.h"
#include "ChaosVDPlaybackControllerInstigator.h"
#include "ChaosVDRecording.h"
#include "ChaosVDRuntimeModule.h"
#include "ChaosVDScene.h"
#include "Actors/ChaosVDSolverInfoActor.h"
#include "Misc/MessageDialog.h"
#include "Trace/ChaosVDTraceManager.h"
#include "Trace/ChaosVDTraceProvider.h"
#include "TraceServices/Model/AnalysisSession.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

namespace Chaos::VisualDebugger::Cvars
{
	static bool bPlayAllPreviousFrameSteps = true;
	static FAutoConsoleVariableRef CVarChaosVDbPlayAllPreviousFrameSteps(
		TEXT("p.Chaos.VD.Tool.PlayAllPreviousFrameSteps"),
		bPlayAllPreviousFrameSteps,
		TEXT("If true, each time we get playback a solver frame in a specific stage, we will play all the previous steps from that frame in sequence to ensure we have the correct visualization for what happened in that frame."));
}

FChaosVDPlaybackController::FChaosVDPlaybackController(const TWeakPtr<FChaosVDScene>& InSceneToControl)
{
	SceneToControl = InSceneToControl;

	RecordingStoppedHandle = FChaosVDRuntimeModule::Get().RegisterRecordingStopCallback(FChaosVDRecordingStateChangedDelegate::FDelegate::CreateRaw(this, &FChaosVDPlaybackController::HandleDisconnectedFromSession));

	if (UChaosVDEditorSettings* Settings = GetMutableDefault<UChaosVDEditorSettings>())
	{
		Settings->OnPlaybackSettingsChanged().AddRaw(this, &FChaosVDPlaybackController::HandleFrameRateOverrideSettingsChanged);
		HandleFrameRateOverrideSettingsChanged(Settings);
	}

	CurrentPlaybackInstigator = IChaosVDPlaybackControllerInstigator::InvalidGuid;
}

FChaosVDPlaybackController::~FChaosVDPlaybackController()
{
	// There is a chance the Runtime module is unloaded by now if we had the tool open and we are closing the editor
	if (FChaosVDRuntimeModule::IsLoaded())
	{
		FChaosVDRuntimeModule::Get().RemoveRecordingStopCallback(RecordingStoppedHandle);
	}
	
	if (UChaosVDEditorSettings* Settings = GetMutableDefault<UChaosVDEditorSettings>())
	{
		Settings->OnPlaybackSettingsChanged().RemoveAll(this);
	}

	UnloadCurrentRecording(EChaosVDUnloadRecordingFlags::Silent);
}

bool FChaosVDPlaybackController::LoadChaosVDRecordingFromTraceSession(const FChaosVDTraceSessionDescriptor& InSessionDescriptor)
{
	if (!ensure(!InSessionDescriptor.SessionName.IsEmpty()))
	{
		return false;
	}

	if (LoadedRecording.IsValid())
	{
		UnloadCurrentRecording();
	}

	if (const TSharedPtr<const TraceServices::IAnalysisSession> TraceSession = FChaosVDModule::Get().GetTraceManager()->GetSession(InSessionDescriptor.SessionName))
	{
		if (const FChaosVDTraceProvider* ChaosVDProvider = TraceSession->ReadProvider<FChaosVDTraceProvider>(FChaosVDTraceProvider::ProviderName))
		{
			LoadedRecording = ChaosVDProvider->GetRecordingForSession();
		}
	}

	if (!ensure(LoadedRecording.IsValid()))
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("LoadRecordingFailedMessage", "Failed to load the selected CVD recording. Please see the logs for more details... "));

		return false;
	}

	LoadedRecording->SetIsLive(InSessionDescriptor.bIsLiveSession);

	HandleCurrentRecordingUpdated();

	LoadedRecording->OnGeometryDataLoaded().AddRaw(this, &FChaosVDPlaybackController::EnqueueGeometryDataUpdate);
	
	if (TSharedPtr<FChaosVDScene> ScenePtr = SceneToControl.Pin())
	{
		ScenePtr->LoadedRecording = LoadedRecording;
	}
	
	bHasPendingGTUpdateBroadcast = true;

	return true;
}

void FChaosVDPlaybackController::UnloadCurrentRecording(EChaosVDUnloadRecordingFlags UnloadOptions)
{
	RecordingLastSeenTimeUpdatedAsCycle = 0;

	TrackInfoUpdateGTQueue.Empty();
	
	if (LoadedRecording.IsValid())
	{
		LoadedRecording.Reset();
	}

	// This will make sure the cached data used by the UI is up to date.
	// It already handles internally an unloaded recording, in which case the cached data will be properly reset

	HandleCurrentRecordingUpdated();

	if (const TSharedPtr<FChaosVDScene> SceneToControlSharedPtr = SceneToControl.Pin())
	{
		if (SceneToControlSharedPtr->IsInitialized())
		{
			SceneToControlSharedPtr->CleanUpScene();
		}	
	}

	if (EnumHasAnyFlags(UnloadOptions, EChaosVDUnloadRecordingFlags::BroadcastChanges))
	{
		bHasPendingGTUpdateBroadcast = true;
	}

	bPlayedFirstFrame = false;
}

void FChaosVDPlaybackController::PlayFromClosestKeyFrame_AssumesLocked(const int32 InTrackID, const int32 FrameNumber, FChaosVDScene& InSceneToControl) const
{
	if (!LoadedRecording.IsValid())
	{
		return;
	}

	const int32 KeyFrameNumber = LoadedRecording->FindFirstSolverKeyFrameNumberFromFrame_AssumesLocked(InTrackID, FrameNumber);
	if (KeyFrameNumber < 0)
	{
		// This can happen during live debugging as we miss some of the events at the beginning.
		// Loading a trace file that was recorded as part of a live session, will have the same issue.
		UE_LOG(LogChaosVDEditor, Warning, TEXT("[%s] Failed to find a keyframe close to frame [%d] of track [%d]"), ANSI_TO_TCHAR(__FUNCTION__), FrameNumber, InTrackID);
		return;
	}

	// Instead of playing back each delta frame since the key frame, generate a new solver frame with all the deltas collapsed in one
	// This increases the tool performance while scrubbing or live debugging if there are few keyframes
	const int32 LastFrameToEvaluateIndex = FrameNumber - 1;
	FChaosVDSolverFrameData CollapsedFrameData;
	LoadedRecording->CollapseSolverFramesRange_AssumesLocked(InTrackID, KeyFrameNumber, LastFrameToEvaluateIndex, CollapsedFrameData);

	if (CollapsedFrameData.SolverSteps.Num() > 0)
	{
		InSceneToControl.UpdateFromRecordedStepData(InTrackID, CollapsedFrameData.SolverSteps[0], CollapsedFrameData);
	}
}

void FChaosVDPlaybackController::EnqueueTrackInfoUpdate(const FChaosVDTrackInfo& InTrackInfo, FGuid InstigatorID)
{
	// This will be used in the Game Thread on the first tick after this was added, so we need to make a copy of the state right now
	FChaosVDQueuedTrackInfoUpdate InfoUpdate;
	InfoUpdate.TrackInfo = InTrackInfo;
	InfoUpdate.InstigatorID = InstigatorID;

	// TODO: FChaosVDTrackInfo has the track name as an string which will be copied as well.
	// We should move the names to another structure as these don't change often.
	// It wasn't an issue for now, but now this copy might have an impact due to the number of updates there could be done.
	
	TrackInfoUpdateGTQueue.Enqueue(InfoUpdate);
}

void FChaosVDPlaybackController::EnqueueGeometryDataUpdate(const Chaos::FConstImplicitObjectPtr& NewGeometry, const uint32 GeometryID)
{	
	GeometryDataUpdateGTQueue.Enqueue({NewGeometry, GeometryID });
}

void FChaosVDPlaybackController::HandleFrameRateOverrideSettingsChanged(UChaosVDEditorSettings* CVDSettings)
{
	if (!CVDSettings)
	{
		return;
	}

	CurrentFrameRateOverride = CVDSettings->bPlaybackAtRecordedFrameRate ? InvalidFrameRateOverride : CVDSettings->TargetFrameRateOverride;
}

void FChaosVDPlaybackController::PlaySolverStepData(int32 TrackID, const TSharedRef<FChaosVDScene>& InSceneToControlSharedPtr, const FChaosVDSolverFrameData& InSolverFrameData, int32 StepIndex)
{
	if (InSolverFrameData.SolverSteps.IsValidIndex(StepIndex))
	{
		InSceneToControlSharedPtr->UpdateFromRecordedStepData(TrackID, InSolverFrameData.SolverSteps[StepIndex], InSolverFrameData);
	}
	else
	{
		// This is common if we stop PIE, change worlds, and PIE again without stopping the recording
		UE_LOG(LogChaosVDEditor, Verbose, TEXT("[%s] Tried to scrub to an invalid step | Step Number [%d] ..."), ANSI_TO_TCHAR(__FUNCTION__), StepIndex);
	}
}

void FChaosVDPlaybackController::GoToRecordedSolverStep_AssumesLocked(const int32 InTrackID, const int32 FrameNumber, const int32 Step, FGuid InstigatorID, int32 Attempts)
{
	if (const TSharedPtr<FChaosVDScene> SceneToControlSharedPtr = SceneToControl.Pin())
	{
		if (ensure(LoadedRecording.IsValid()))
		{
			TSharedPtr<FChaosVDTrackInfo> CurrentTrackInfo;
			if (TrackInfoByIDMap* TrackInfoByID = TrackInfoPerType.Find(EChaosVDTrackType::Solver))
			{
				if (const TSharedPtr<FChaosVDTrackInfo>* TrackInfo = TrackInfoByID->Find(InTrackID))
				{
					CurrentTrackInfo = *TrackInfo;
				}
			}

			if (!ensure(CurrentTrackInfo.IsValid()))
			{
				UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Track info for track ID [%d]. We can't continue..."), ANSI_TO_TCHAR(__FUNCTION__), InTrackID);
				return;
			}

			if (FChaosVDSolverFrameData* SolverFrameData = LoadedRecording->GetSolverFrameData_AssumesLocked(InTrackID, FrameNumber))
			{
				const int32 FrameDiff = FrameNumber - CurrentTrackInfo->CurrentFrame;
				constexpr int32 FrameDriftTolerance = 1;

				// If we go back, even for one single step and the particles that changed are not in the prev step, we have no data to restore their changed values.
				// So for now if we are going backwards, always play from closest keyframe.
				// TODO: Implement a less expensive way of handle these cases.
				// We should keep the previous state of each loaded particle so if when going back they are not in the new delta we are evaluating, (and were not destroyed)
				// we can just re-apply that last known state.
				const bool bNeedsToPlayFromKeyframe = FrameDiff < 0 || FMath::Abs(FrameDiff) > FrameDriftTolerance;

				if (bNeedsToPlayFromKeyframe || CurrentTrackInfo->CurrentFrame == 0)
				{
					// As Frames are recorded as delta, we need to make sure of playing back all the deltas since the closest keyframe
					PlayFromClosestKeyFrame_AssumesLocked(InTrackID, FrameNumber, *SceneToControlSharedPtr.Get());
				}

				const int32 StepDiff = Step - CurrentTrackInfo->CurrentStep;
				const bool bNeedsPlayPreviousSteps = CurrentTrackInfo->CurrentFrame != FrameNumber || StepDiff < 0 || FMath::Abs(StepDiff) > FrameDriftTolerance;

				if (Chaos::VisualDebugger::Cvars::bPlayAllPreviousFrameSteps && bNeedsPlayPreviousSteps)
				{
					for (int32 StepIndex = 0; StepIndex <= Step; StepIndex++)
					{
						PlaySolverStepData(InTrackID, SceneToControlSharedPtr.ToSharedRef(), *SolverFrameData, StepIndex);
					}
				}
				else
				{
					PlaySolverStepData(InTrackID, SceneToControlSharedPtr.ToSharedRef(), *SolverFrameData, Step);
				}

				CurrentTrackInfo->CurrentFrame = FrameNumber;
				CurrentTrackInfo->CurrentStep = Step;
				CurrentTrackInfo->bIsReSimulated = SolverFrameData->bIsResimulated;
			
				EnqueueTrackInfoUpdate(*CurrentTrackInfo.Get(), InstigatorID);
			}
		}
	}
	else
	{
		ensureMsgf(false, TEXT("GoToRecordedStep Called without a valid scene to control"));	
	}
}

void FChaosVDPlaybackController::GoToRecordedGameFrame_AssumesLocked(const int32 FrameNumber, FGuid InstigatorID, int32 Attempts)
{
	if (const TSharedPtr<FChaosVDScene> SceneToControlSharedPtr = SceneToControl.Pin())
	{
		if (ensure(LoadedRecording.IsValid()))
		{
			if (TrackInfoByIDMap* TrackInfoByID = TrackInfoPerType.Find(EChaosVDTrackType::Game))
			{
				const TSharedPtr<FChaosVDTrackInfo>* TrackInfoSharedPtrPtr = TrackInfoByID->Find(GameTrackID);
				if (!ensure(TrackInfoSharedPtrPtr))
				{
					return;
				}
				
				if (const TSharedPtr<FChaosVDTrackInfo> TrackInfoSharedPtr = *TrackInfoSharedPtrPtr)
				{
					if (const FChaosVDGameFrameData* FoundGameFrameData = LoadedRecording->GetGameFrameData_AssumesLocked(FrameNumber))
					{
						TArray<int32> AvailableSolversID;
						LoadedRecording->GetAvailableSolverIDsAtGameFrameNumber_AssumesLocked(FrameNumber, AvailableSolversID);

						SceneToControlSharedPtr->HandleEnterNewGameFrame(FrameNumber, AvailableSolversID, *FoundGameFrameData);

						for (const int32 SolverID : AvailableSolversID)
						{
							// When Scrubbing the timeline by game frames instead of solvers, try to go to the first solver frame on the first platform cycle of the game frame.
							// Game Frames are not in sync with Solver Frames and Solver steps.
							const int32 SolverFrameNumber = LoadedRecording->GetLowestSolverFrameNumberAtCycle_AssumesLocked(SolverID, FoundGameFrameData->FirstCycle);
	
							const int32 StepNumber = GetTrackLastStepAtFrame(EChaosVDTrackType::Solver, SolverID, SolverFrameNumber);

							GoToTrackFrame_AssumesLocked(InstigatorID, EChaosVDTrackType::Solver, SolverID, SolverFrameNumber, StepNumber);
						}
					}
					
					TrackInfoSharedPtr->CurrentFrame = FrameNumber;
					EnqueueTrackInfoUpdate(*TrackInfoSharedPtr.Get(), InstigatorID);
				}
			}
		}
	}
}

void FChaosVDPlaybackController::GoToTrackFrame(FGuid InstigatorID, EChaosVDTrackType TrackType, int32 InTrackID, int32 FrameNumber, int32 Step)
{
	if (!ensure(LoadedRecording.IsValid()))
	{
		return;
	}

	FReadScopeLock ReadLock(LoadedRecording->GetRecordingDataLock());
	GoToTrackFrame_AssumesLocked(InstigatorID, TrackType, InTrackID, FrameNumber, Step);
}

void FChaosVDPlaybackController::GoToTrackFrame_AssumesLocked(FGuid InstigatorID, EChaosVDTrackType TrackType, int32 InTrackID, int32 FrameNumber, int32 Step)
{
	switch (TrackType)
	{
	case EChaosVDTrackType::Game:
		GoToRecordedGameFrame_AssumesLocked(FrameNumber, InstigatorID);
		break;
	case EChaosVDTrackType::Solver:
		GoToRecordedSolverStep_AssumesLocked(InTrackID, FrameNumber, Step, InstigatorID);
		break;
	default:
		ensure(false);
		break;
	}
}

int32 FChaosVDPlaybackController::GetTrackStepsNumberAtFrame_AssumesLocked(EChaosVDTrackType TrackType, const int32 InTrackID, const int32 FrameNumber) const
{
	if (!LoadedRecording.IsValid())
	{
		return INDEX_NONE;
	}
	
	switch (TrackType)
	{
		case EChaosVDTrackType::Game:
			// Game Tracks do not have steps
			return 0;
			break;
		case EChaosVDTrackType::Solver:
			{
				if (const FChaosVDSolverFrameData* FrameData = LoadedRecording->GetSolverFrameData_AssumesLocked(InTrackID, FrameNumber))
				{
					return FrameData->SolverSteps.Num() > 0 ? FrameData->SolverSteps.Num() : INDEX_NONE;
				}
				else
				{
					return INDEX_NONE;
				}
				break;
			}
	default:
		return INDEX_NONE;
		break;
	}
}

const FChaosVDStepsContainer* FChaosVDPlaybackController::GetTrackStepsDataAtFrame_AssumesLocked(EChaosVDTrackType TrackType, int32 InTrackID, int32 FrameNumber) const
{
	if (!LoadedRecording.IsValid())
	{
		return nullptr;
	}
	
	switch (TrackType)
	{
	case EChaosVDTrackType::Game:
		// Game Tracks do not have steps
		return nullptr;
		break;
	case EChaosVDTrackType::Solver:
		{
			if (const FChaosVDSolverFrameData* FrameData = LoadedRecording->GetSolverFrameData_AssumesLocked(InTrackID, FrameNumber))
			{
				return &FrameData->SolverSteps;
			}
			else
			{
				return nullptr;
			}
			break;
		}
	default:
		return nullptr;
		break;
	}
}

int32 FChaosVDPlaybackController::GetTrackFramesNumber(EChaosVDTrackType TrackType, const int32 InTrackID) const
{
	if (!LoadedRecording.IsValid())
	{
		return INDEX_NONE;
	}
	
	switch (TrackType)
	{
		case EChaosVDTrackType::Game:
		{
			// There is only one game track so no ID is needed
			int32 GameFrames = LoadedRecording->GetAvailableGameFramesNumber();
			return GameFrames > 0 ? GameFrames : INDEX_NONE;
			break;
		}

		case EChaosVDTrackType::Solver:
		{
			int32 SolverFrames = LoadedRecording->GetAvailableSolverFramesNumber(InTrackID);
			return  SolverFrames > 0 ? SolverFrames : INDEX_NONE;
			break;
		}
		default:
			return INDEX_NONE;
			break;
	}
}

int32 FChaosVDPlaybackController::ConvertCurrentFrameToOtherTrackFrame(const FChaosVDTrackInfo* FromTrack, const FChaosVDTrackInfo* ToTrack)
{
	// Each track is on a different "space time", because it's source data ticked at different rates when was recorded, and some start/end at different points on time
	// But all the recorded frame data on all of them use Platform Cycles as timestamps
	// This method wraps specialized methods in the recording object to convert between these spaces. For example Game frame 1500 could be frame 5 on a specific solver
	// And Frame 5 of that solver could be frame 30 on another solver.

	if (FromTrack == nullptr || ToTrack == nullptr)
	{
		ensureMsgf(false, TEXT("One of provided track infos is not valid"));
		return INDEX_NONE;
	}
	
	if (!ensure(LoadedRecording.IsValid()))
    {
    	return INDEX_NONE;
    }

	const bool bBothTracksHaveSameID = FromTrack->TrackID == ToTrack->TrackID;
	const bool bBothTracksHaveSameType = FromTrack->TrackType == ToTrack->TrackType;
	if (bBothTracksHaveSameType && bBothTracksHaveSameID)
	{
		return FromTrack->CurrentFrame;
	}

	switch (FromTrack->TrackType)
	{
	case EChaosVDTrackType::Game:
		{
			// Convert from Game Frame to Solver Frame
			return LoadedRecording->GetLowestSolverFrameNumberGameFrame(FromTrack->TrackID, FromTrack->CurrentFrame);
			break;
		}
		
	case EChaosVDTrackType::Solver:
		{
			if (ToTrack->TrackType == EChaosVDTrackType::Solver)
			{
				ensureMsgf(false, TEXT("Frame conversion between solver tracks is not supported yet"));
				return INDEX_NONE;
			}
			return LoadedRecording->GetLowestGameFrameAtSolverFrameNumber(FromTrack->TrackID, FromTrack->CurrentFrame);
			break;
		}
	default:
		ensure(false);
		return INDEX_NONE;
		break;
	}
}

int32 FChaosVDPlaybackController::GetTrackCurrentFrame(EChaosVDTrackType TrackType, const int32 InTrackID) const
{
	if (const TrackInfoByIDMap* TrackInfoByID = TrackInfoPerType.Find(TrackType))
	{
		const TSharedPtr<FChaosVDTrackInfo>* TrackInfoPtrPtr = TrackInfoByID->Find(InTrackID);
		if (const TSharedPtr<FChaosVDTrackInfo> TrackInfoSharedPtr = TrackInfoPtrPtr ? *TrackInfoPtrPtr : nullptr)
		{
			return TrackInfoSharedPtr->CurrentFrame;
		}
	}

	return INDEX_NONE;
}

int32 FChaosVDPlaybackController::GetTrackCurrentStep(EChaosVDTrackType TrackType, const int32 InTrackID) const
{
	if (const TrackInfoByIDMap* TrackInfoByID = TrackInfoPerType.Find(TrackType))
	{
		const TSharedPtr<FChaosVDTrackInfo>* TrackInfoPtrPtr = TrackInfoByID->Find(InTrackID);
		if (const TSharedPtr<FChaosVDTrackInfo> TrackInfoSharedPtr = TrackInfoPtrPtr ? *TrackInfoPtrPtr : nullptr)
		{
			return TrackInfoSharedPtr->CurrentStep;
		}
	}

	return INDEX_NONE;
}

int32 FChaosVDPlaybackController::GetTrackLastStepAtFrame(EChaosVDTrackType TrackType, int32 InTrackID, int32 InFrameNumber) const
{
	switch (TrackType)
	{
		case EChaosVDTrackType::Solver:
		{
			const int32 AvailableSteps = GetTrackStepsNumberAtFrame_AssumesLocked(EChaosVDTrackType::Solver, InTrackID, InFrameNumber);
			return AvailableSteps == INDEX_NONE ? INDEX_NONE: AvailableSteps -1;
			break;
		}
		case EChaosVDTrackType::Game:
		default:
			ensureMsgf(false, TEXT("Unsuported Track Type"));
			return INDEX_NONE;
			break;
	}
	
}

const FChaosVDTrackInfo* FChaosVDPlaybackController::GetTrackInfo(EChaosVDTrackType TrackType, int32 TrackID)
{
	return GetMutableTrackInfo(TrackType, TrackID);
}

FChaosVDTrackInfo* FChaosVDPlaybackController::GetMutableTrackInfo(EChaosVDTrackType TrackType, int32 TrackID)
{
	if (const TrackInfoByIDMap* TrackInfoByID = TrackInfoPerType.Find(TrackType))
	{
		const TSharedPtr<FChaosVDTrackInfo>* TrackInfoPtrPtr = TrackInfoByID->Find(TrackID);
		if (const TSharedPtr<FChaosVDTrackInfo> TrackInfoSharedPtr = TrackInfoPtrPtr ? *TrackInfoPtrPtr : nullptr)
		{
			return TrackInfoSharedPtr.Get();
		}
	}

	return nullptr;
}

void FChaosVDPlaybackController::LockTrackInCurrentStep(EChaosVDTrackType TrackType, int32 TrackID)
{
	if (FChaosVDTrackInfo* TrackInfo = GetMutableTrackInfo(TrackType, TrackID))
	{
		TrackInfo->LockedOnStep = TrackInfo->CurrentStep;
	}
}

void FChaosVDPlaybackController::UnlockTrackStep(EChaosVDTrackType TrackType, int32 TrackID)
{
	if (FChaosVDTrackInfo* TrackInfo = GetMutableTrackInfo(TrackType, TrackID))
	{
		TrackInfo->LockedOnStep = INDEX_NONE;
	}
}

void FChaosVDPlaybackController::GetAvailableTracks(EChaosVDTrackType TrackType, TArray<TSharedPtr<FChaosVDTrackInfo>>& OutTrackInfo)
{
	OutTrackInfo.Reset();
	TrackInfoPerType.FindOrAdd(TrackType).GenerateValueArray(OutTrackInfo);
}

void FChaosVDPlaybackController::GetAvailableTrackInfosAtTrackFrame(EChaosVDTrackType TrackTypeToFind, TArray<TSharedPtr<FChaosVDTrackInfo>>& OutTrackInfo, const FChaosVDTrackInfo* TrackFrameInfo)
{
	OutTrackInfo.Reset();

	if (!LoadedRecording.IsValid())
	{
		return;
	}

	if (TrackFrameInfo == nullptr)
	{
		return;
	}

	int32 CorrectedFrameNumber = INDEX_NONE;
	switch (TrackFrameInfo->TrackType)
	{
		case EChaosVDTrackType::Game:
			{
				CorrectedFrameNumber = TrackFrameInfo->CurrentFrame;
			}
			break;
		case EChaosVDTrackType::Solver:
			{
				CorrectedFrameNumber = LoadedRecording->GetLowestGameFrameAtSolverFrameNumber(TrackFrameInfo->TrackID, TrackFrameInfo->CurrentFrame);
				break;
			}
		default:
			ensure(false);
			break;
	}

	TArray<int32> AvailableSolversID;
	LoadedRecording->GetAvailableSolverIDsAtGameFrameNumber(CorrectedFrameNumber, AvailableSolversID);
	
	TrackInfoByIDMap& TrackInfoMap = TrackInfoPerType.FindOrAdd(TrackTypeToFind);
	for (const int32 SolverID : AvailableSolversID)
	{
		// The recording might have the solver data available added because it was added the trace analysis thread, 
		// but the playback controller didn't process it in the game thread yet
		if (const TSharedPtr<FChaosVDTrackInfo>* SolverTrackInfo = TrackInfoMap.Find(SolverID))
		{
			OutTrackInfo.Add(*SolverTrackInfo);
		}	
	}
}

bool FChaosVDPlaybackController::Tick(float DeltaTime)
{	
	if (!GeometryDataUpdateGTQueue.IsEmpty())
	{
		FChaosVDGeometryDataUpdate GeometryDataUpdate;
		while (GeometryDataUpdateGTQueue.Dequeue(GeometryDataUpdate))
		{
			if (const TSharedPtr<FChaosVDScene> ScenePtr = SceneToControl.Pin())
			{
				ScenePtr->HandleNewGeometryData(GeometryDataUpdate.NewGeometry, GeometryDataUpdate.GeometryID);
			}
		}
	}

	const TWeakPtr<FChaosVDPlaybackController> ThisWeakPtr = DoesSharedInstanceExist() ? AsWeak() : nullptr;
	if (!ThisWeakPtr.IsValid())
	{
		return true;
	}


	const bool bIsRecordingLoaded = LoadedRecording.IsValid();

	if (bIsRecordingLoaded)
	{
		uint64 CurrentLastUpdatedTime = LoadedRecording->GetLastUpdatedTimeAsCycle();
		if (CurrentLastUpdatedTime != RecordingLastSeenTimeUpdatedAsCycle)
		{
			RecordingLastSeenTimeUpdatedAsCycle = CurrentLastUpdatedTime;

			HandleCurrentRecordingUpdated();
		}
	}

	if (bHasPendingGTUpdateBroadcast)
	{
		ControllerUpdatedDelegate.Broadcast(ThisWeakPtr);
		bHasPendingGTUpdateBroadcast = false;
	}

	if (!TrackInfoUpdateGTQueue.IsEmpty())
	{
		FChaosVDQueuedTrackInfoUpdate TrackInfoUpdate;
		while (TrackInfoUpdateGTQueue.Dequeue(TrackInfoUpdate))
		{
			OnTrackFrameUpdated().Broadcast(ThisWeakPtr, &TrackInfoUpdate.TrackInfo, TrackInfoUpdate.InstigatorID);
		}
	}

	if (bIsRecordingLoaded)
	{
		// Load at least the first frame
		if (!bPlayedFirstFrame)
		{
			if (LoadedRecording->GetAvailableSolversNumber_AssumesLocked() > 0)
			{
				constexpr int32 GameFrameToLoad = 0;
				constexpr int32 Step = 0;
				GoToTrackFrame(IChaosVDPlaybackControllerInstigator::InvalidGuid, EChaosVDTrackType::Game, GameTrackID, GameFrameToLoad, Step);
				bPlayedFirstFrame = true;
			}
		}

		// If we are live, make sure we don't lag too much behind
		if (!bPauseRequested && IsPlayingLiveSession())
		{
			if (const FChaosVDTrackInfo* GameTrackInfo = GetTrackInfo(EChaosVDTrackType::Game, GameTrackID))
			{
				const int32 CurrentFrameDeltaFromLast = FMath::Abs(GameTrackInfo->MaxFrames - GameTrackInfo->CurrentFrame);
				if (CurrentFrameDeltaFromLast > MaxFramesLaggingBehindDuringLiveSession)
				{
					// Playing the middle point between last and the threshold. We don't want to play the last available frame as it could be incomplete,
					// and we don't want to go to close to the threshold.
					const int32 GameFrameToLoad = LoadedRecording->GetAvailableGameFramesNumber() - MinFramesLaggingBehindDuringLiveSession;
					constexpr int32 Step = 0;
					GoToTrackFrame(IChaosVDPlaybackControllerInstigator::InvalidGuid, EChaosVDTrackType::Game, GameTrackID, GameFrameToLoad, Step);
				}
			}
		}
	}

	return true;
}

bool FChaosVDPlaybackController::IsPlayingLiveSession() const
{
	return LoadedRecording.IsValid() ? LoadedRecording->IsLive() : false;
}

void FChaosVDPlaybackController::HandleDisconnectedFromSession()
{
	if (LoadedRecording.IsValid())
	{
		LoadedRecording->SetIsLive(false);
	}

	// Queue a general update in the Game Thread
	bHasPendingGTUpdateBroadcast = true;
}

void FChaosVDPlaybackController::RequestStop(const IChaosVDPlaybackControllerInstigator& InPlaybackInstigator)
{
	constexpr int32 FrameNumber = 0;
	constexpr int32 StepNumber = 0;
	GoToTrackFrame(InPlaybackInstigator.GetInstigatorID(), EChaosVDTrackType::Game, GameTrackID, FrameNumber, StepNumber);
}

bool FChaosVDPlaybackController::AcquireExclusivePlaybackControls(const IChaosVDPlaybackControllerInstigator& InPlaybackInstigator)
{
	const FGuid& InstigatorID = InPlaybackInstigator.GetInstigatorID();
	if (InstigatorID == CurrentPlaybackInstigator || CurrentPlaybackInstigator == IChaosVDPlaybackControllerInstigator::InvalidGuid)
	{
		CurrentPlaybackInstigator = InPlaybackInstigator.GetInstigatorID();
		bHasPendingGTUpdateBroadcast = true;
		return true;
	}

	return false;
}

bool FChaosVDPlaybackController::ReleaseExclusivePlaybackControls(const IChaosVDPlaybackControllerInstigator& InPlaybackInstigator)
{
	const FGuid& InstigatorID = InPlaybackInstigator.GetInstigatorID();
	if (InstigatorID == CurrentPlaybackInstigator || CurrentPlaybackInstigator == IChaosVDPlaybackControllerInstigator::InvalidGuid)
	{
		CurrentPlaybackInstigator = IChaosVDPlaybackControllerInstigator::InvalidGuid;
		bHasPendingGTUpdateBroadcast = true;
		return true;
	}

	return false;
}

float FChaosVDPlaybackController::GetFrameTimeOverride() const
{
	constexpr int32 MinimumFrameRateOverride = 1;
	return CurrentFrameRateOverride >= MinimumFrameRateOverride ? 1.0f / static_cast<float>(CurrentFrameRateOverride) : InvalidFrameRateOverride;
}

float FChaosVDPlaybackController::GetFrameTimeForTrack(EChaosVDTrackType TrackType, int32 TrackID, const FChaosVDTrackInfo& TrackInfo) const
{
	const float TargetFrameTimeOverride = GetFrameTimeOverride();
	const bool bHastFrameRateOverride = !FMath::IsNearlyEqual(TargetFrameTimeOverride, FChaosVDPlaybackController::InvalidFrameRateOverride);
	if (bHastFrameRateOverride)
	{
		return TargetFrameTimeOverride;
	}

	float CurrentTargetFrameTime = FallbackFrameTime;
	if (LoadedRecording)
	{
		switch(TrackType)
		{
			case EChaosVDTrackType::Solver:
				{
					if (const FChaosVDSolverFrameData* FrameData = LoadedRecording->GetSolverFrameData_AssumesLocked(TrackID, TrackInfo.CurrentFrame))
					{
						CurrentTargetFrameTime = FrameData->GetFrameTime();
					}
					break;
				}
			case EChaosVDTrackType::Game:
				{
					if (const FChaosVDGameFrameData* FrameData = LoadedRecording->GetGameFrameData_AssumesLocked(TrackInfo.CurrentFrame))
					{
						CurrentTargetFrameTime = FrameData->GetFrameTime();
					}

					break;
				}
			default:
				break;
		}
	}

	return CurrentTargetFrameTime;
}

void FChaosVDPlaybackController::UpdateTrackVisibility(EChaosVDTrackType Type, int32 TrackID, bool bNewVisibility)
{
	switch (Type)
	{
		case EChaosVDTrackType::Solver:
			{
				if (const TSharedPtr<FChaosVDScene> ScenePtr = SceneToControl.Pin())
				{
					if (AChaosVDSolverInfoActor* SolverActorInfo = ScenePtr->GetSolverInfoActor(TrackID))
					{
						SolverActorInfo->SetIsTemporarilyHiddenInEditor(!bNewVisibility);
					}
				}
				break;
			}
		case EChaosVDTrackType::Game:
		default:
			ensure(false);
			break;
	}
}

void FChaosVDPlaybackController::UpdateSolverTracksData()
{
	if (!LoadedRecording.IsValid())
	{
		// If the recording is no longer valid, clear any existing solver track info data so the UI can be updated accordingly
		if (TrackInfoByIDMap* SolverTracks = TrackInfoPerType.Find(EChaosVDTrackType::Solver))
		{
			SolverTracks->Empty();
		}

		return;
	}

	const TMap<int32, TArray<FChaosVDSolverFrameData>>& SolversByID = LoadedRecording->GetAvailableSolvers_AssumesLocked();
	for (const TPair<int32, TArray<FChaosVDSolverFrameData>>& SolverIDPair : SolversByID)
	{
		TSharedPtr<FChaosVDTrackInfo>& SolverTrackInfo = TrackInfoPerType[EChaosVDTrackType::Solver].FindOrAdd(SolverIDPair.Key);

		if (!SolverTrackInfo.IsValid())
		{
			SolverTrackInfo = MakeShared<FChaosVDTrackInfo>();
			SolverTrackInfo->CurrentFrame = 0;
			SolverTrackInfo->CurrentStep = 0;
		};
		
		SolverTrackInfo->TrackID = SolverIDPair.Key;
		SolverTrackInfo->MaxFrames = GetTrackFramesNumber(EChaosVDTrackType::Solver, SolverIDPair.Key);
		SolverTrackInfo->TrackName = LoadedRecording->GetSolverName(SolverIDPair.Key);
		SolverTrackInfo->TrackType = EChaosVDTrackType::Solver;
	}
}

void FChaosVDPlaybackController::HandleCurrentRecordingUpdated()
{
	// These two tracks should always exist
	TrackInfoPerType.FindOrAdd(EChaosVDTrackType::Game);
	TrackInfoPerType.FindOrAdd(EChaosVDTrackType::Solver);

	// Same for the Game Track, needs to always exist
	TSharedPtr<FChaosVDTrackInfo>& GameTrackInfo = TrackInfoPerType[EChaosVDTrackType::Game].FindOrAdd(GameTrackID);
	if (!GameTrackInfo.IsValid())
	{
		GameTrackInfo = MakeShared<FChaosVDTrackInfo>();
		GameTrackInfo->CurrentFrame = 0;
		GameTrackInfo->CurrentStep = 0;
	}

	GameTrackInfo->MaxFrames = LoadedRecording.IsValid() ? LoadedRecording->GetAvailableGameFrames_AssumesLocked().Num() : INDEX_NONE;
	GameTrackInfo->TrackType = EChaosVDTrackType::Game;

	// Each time the recording is updated, populate or update the existing solver tracks data
	UpdateSolverTracksData();

	bHasPendingGTUpdateBroadcast = true;
}

#undef LOCTEXT_NAMESPACE

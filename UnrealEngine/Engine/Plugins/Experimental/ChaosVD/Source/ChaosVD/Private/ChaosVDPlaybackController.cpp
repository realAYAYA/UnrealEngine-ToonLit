// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDPlaybackController.h"

#include "ChaosVDModule.h"
#include "ChaosVDPlaybackControllerInstigator.h"
#include "ChaosVDRecording.h"
#include "ChaosVDScene.h"
#include "Trace/ChaosVDTraceManager.h"
#include "Trace/ChaosVDTraceProvider.h"
#include "TraceServices/Model/AnalysisSession.h"

FChaosVDPlaybackController::FChaosVDPlaybackController(const TWeakPtr<FChaosVDScene>& InSceneToControl)
{
	SceneToControl = InSceneToControl;
}

FChaosVDPlaybackController::~FChaosVDPlaybackController()
{
	UnloadCurrentRecording(EChaosVDUnloadRecordingFlags::Silent);
}

bool FChaosVDPlaybackController::LoadChaosVDRecordingFromTraceSession(const FString& InSessionName)
{
	if (InSessionName.IsEmpty())
	{
		return false;
	}

	if (LoadedRecording.IsValid())
	{
		UnloadCurrentRecording();
	}

	if (const TSharedPtr<const TraceServices::IAnalysisSession> TraceSession = FChaosVDModule::Get().GetTraceManager()->GetSession(InSessionName))
	{
		if (const FChaosVDTraceProvider* ChaosVDProvider = TraceSession->ReadProvider<FChaosVDTraceProvider>(FChaosVDTraceProvider::ProviderName))
		{
			LoadedRecording = ChaosVDProvider->GetRecordingForSession();
		}
	}

	if (!LoadedRecording.IsValid())
	{
		return false;
	}

	LoadedRecording->OnRecordingUpdated().AddRaw(this, &FChaosVDPlaybackController::HandleCurrentRecordingUpdated);

	HandleCurrentRecordingUpdated();

	for (TMap<int32, TArray<FChaosVDSolverFrameData>>::TConstIterator SolversIterator = LoadedRecording->GetAvailableSolvers().CreateConstIterator(); SolversIterator; ++SolversIterator)
	{
		constexpr int32 FrameNumber = 0;
		constexpr int32 StepNumber = 0;
		GoToTrackFrame(IChaosVDPlaybackControllerInstigator::InvalidGuid, EChaosVDTrackType::Solver, SolversIterator->Key, FrameNumber, StepNumber);
	}

	LoadedRecording->OnGeometryDataLoaded().AddLambda([this](const TSharedPtr<const Chaos::FImplicitObject>& NewGeometry, const uint32 GeometryID)
	{
		if (const TSharedPtr<FChaosVDScene> ScenePtr = SceneToControl.Pin())
		{
			ScenePtr->HandleNewGeometryData(NewGeometry, GeometryID);
		}
	});
	
	if (TSharedPtr<FChaosVDScene> ScenePtr = SceneToControl.Pin())
	{
		ScenePtr->LoadedRecording = LoadedRecording;
	}
	
	OnDataUpdated().Broadcast(AsWeak());

	return true;
}

void FChaosVDPlaybackController::UnloadCurrentRecording(EChaosVDUnloadRecordingFlags UnloadOptions)
{
	if (LoadedRecording.IsValid())
	{
		LoadedRecording->OnRecordingUpdated().RemoveAll(this);
		LoadedRecording.Reset();
	}

	TrackInfoPerType.Reset();

	if (const TSharedPtr<FChaosVDScene> SceneToControlSharedPtr = SceneToControl.Pin())
	{
		if (SceneToControlSharedPtr->IsInitialized())
		{
			SceneToControlSharedPtr->CleanUpScene();
		}	
	}

	if (EnumHasAnyFlags(UnloadOptions, EChaosVDUnloadRecordingFlags::BroadcastChanges))
	{
		const TWeakPtr<FChaosVDPlaybackController> ThisWeakPtr = DoesSharedInstanceExist() ? AsWeak() : nullptr;
		OnDataUpdated().Broadcast(ThisWeakPtr);
	}
}

void FChaosVDPlaybackController::PlayFromClosestKeyFrame(const int32 InTrackID, const int32 FrameNumber, FChaosVDScene& InSceneToControl)
{
	const int32 KeyFrameNumber = LoadedRecording->FindFirstSolverKeyFrameNumberFromFrame(InTrackID, FrameNumber);

	if (!ensure(KeyFrameNumber >= 0))
	{
		return;
	}

	for (int32 CurrentFrameNumber = KeyFrameNumber; CurrentFrameNumber < FrameNumber; CurrentFrameNumber++)
	{				
		if (const FChaosVDSolverFrameData* SolverFrameData = LoadedRecording->GetSolverFrameData(InTrackID, CurrentFrameNumber))
		{
			const int32 LastStepNumber = SolverFrameData->SolverSteps.Num() - 1;

			if (ensure(SolverFrameData->SolverSteps.IsValidIndex(LastStepNumber)))
			{
				InSceneToControl.UpdateFromRecordedStepData(InTrackID, SolverFrameData->DebugName, SolverFrameData->SolverSteps[LastStepNumber], *SolverFrameData);
			}
		}
		else
		{
			UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Failed to read solver frame data for frame [%d] in track [%d]"), ANSI_TO_TCHAR(__FUNCTION__), CurrentFrameNumber, InTrackID);
		}
	}
}

void FChaosVDPlaybackController::GoToRecordedSolverStep(const int32 InTrackID, const int32 FrameNumber, const int32 Step, FGuid InstigatorID)
{
	if (const TSharedPtr<FChaosVDScene> SceneToControlSharedPtr = SceneToControl.Pin())
	{
		if (ensure(LoadedRecording.IsValid()))
		{
			const TSharedPtr<const TraceServices::IAnalysisSession> TraceSession = FChaosVDModule::Get().GetTraceManager()->GetSession(LoadedRecording->SessionName);
			if (!ensure(TraceSession))
			{
				return;	
			}

			// TODO: This will not cover a future case where the recording is not own/populated by Trace
			// If in the future we decide implement the CVD format standalone with streaming support again,
			// we will need to add a lock to the file. A feature that might need that is Recording Clips
			TraceServices::FAnalysisSessionReadScope SessionReadScope(*TraceSession);

			TSharedPtr<FChaosVDTrackInfo> CurrentTrackInfo;
			if (TrackInfoByIDMap* TrackInfoByID = TrackInfoPerType.Find(EChaosVDTrackType::Solver))
			{
				if (TSharedPtr<FChaosVDTrackInfo>* TrackInfo = TrackInfoByID->Find(InTrackID))
				{
					CurrentTrackInfo = *TrackInfo;
				}
			}
			
			if (!ensure(CurrentTrackInfo.IsValid()))
			{
				UE_LOG(LogChaosVDEditor, Warning, TEXT("[%s] Track info for track ID [%d]. We can't continue..."), ANSI_TO_TCHAR(__FUNCTION__), InTrackID);
				return;
			}

			const int32 FrameDiff = FrameNumber - CurrentTrackInfo->CurrentFrame;
			constexpr int32 FrameDriftTolerance = 1;
			if (FMath::Abs(FrameDiff) > FrameDriftTolerance || CurrentTrackInfo->CurrentFrame == 0)
			{
				// As Frames are recorded as delta, we need to make sure of playing back all the deltas since the closest keyframe
				PlayFromClosestKeyFrame(InTrackID, FrameNumber, *SceneToControlSharedPtr.Get());
			}

			const FChaosVDSolverFrameData* SolverFrameData = LoadedRecording->GetSolverFrameData(InTrackID, FrameNumber);
			if (CurrentTrackInfo->LockedOnStep != INDEX_NONE)
			{
				// If this track is locked to a specific step, we need to play back the previous steps on the current frame, because not all steps capture the same data.
				// For example, Particles positions are fully captured in the first sub-step and the last one
				for (int32 StepIndex = 0; StepIndex <= CurrentTrackInfo->LockedOnStep; StepIndex++)
				{
					if (SolverFrameData && ensure(SolverFrameData->SolverSteps.IsValidIndex(CurrentTrackInfo->LockedOnStep)))
					{
						SceneToControlSharedPtr->UpdateFromRecordedStepData(InTrackID, SolverFrameData->DebugName, SolverFrameData->SolverSteps[StepIndex], *SolverFrameData);
					}
					else
					{
						UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Tried to scrub to an invalid step | Step Number [%d] ..."), ANSI_TO_TCHAR(__FUNCTION__), CurrentTrackInfo->LockedOnStep);
						return;
					}
				}
			}
			else
			{
				if (SolverFrameData && ensure(SolverFrameData->SolverSteps.IsValidIndex(Step)))
				{
					SceneToControlSharedPtr->UpdateFromRecordedStepData(InTrackID, SolverFrameData->DebugName, SolverFrameData->SolverSteps[Step], *SolverFrameData);
				}
				else
				{
					UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Tried to scrub to an invalid step | Step Number [%d] ..."), ANSI_TO_TCHAR(__FUNCTION__), CurrentTrackInfo->LockedOnStep);
				}
			}
			

			CurrentTrackInfo->CurrentFrame = FrameNumber;
			CurrentTrackInfo->CurrentStep = Step;

			OnTrackFrameUpdated().Broadcast(AsWeak(), CurrentTrackInfo.Get(), InstigatorID);
		}
	}
	else
	{
		ensureMsgf(false, TEXT("GoToRecordedStep Called without a valid scene to control"));	
	}
}

void FChaosVDPlaybackController::GoToRecordedGameFrame(const int32 FrameNumber, FGuid InstigatorID)
{
	if (const TSharedPtr<FChaosVDScene> SceneToControlSharedPtr = SceneToControl.Pin())
	{
		if (ensure(LoadedRecording.IsValid()))
		{
			if (const FChaosVDGameFrameData* FrameData = LoadedRecording->GetGameFrameData(FrameNumber))
			{
				if (TrackInfoByIDMap* TrackInfoByID = TrackInfoPerType.Find(EChaosVDTrackType::Game))
				{
					TArray<int32> AvailableSolversID;
					LoadedRecording->GetAvailableSolverIDsAtGameFrameNumber(FrameNumber, AvailableSolversID);

					SceneToControlSharedPtr->HandleEnterNewGameFrame(FrameNumber, AvailableSolversID);

					for (int32 SolverID : AvailableSolversID)
					{
						// When Scrubbing the timeline by game frames instead of solvers, try to go to the first solver frame on the first platform cycle of the game frame.
						// Game Frames are not in sync with Solver Frames and Solver steps.
						const int32 SolverFrameNumber = LoadedRecording->GetLowestSolverFrameNumberAtCycle(SolverID, FrameData->FirstCycle);
	
						const int32 StepNumber = GetTrackLastStepAtFrame(EChaosVDTrackType::Solver, SolverID,SolverFrameNumber);

						GoToTrackFrame(InstigatorID, EChaosVDTrackType::Solver, SolverID, SolverFrameNumber, StepNumber);
					}

					TSharedPtr<FChaosVDTrackInfo>* TrackInfoPtrPtr = TrackInfoByID->Find(GameTrackID);
					if (TSharedPtr<FChaosVDTrackInfo> TrackInfoSharedPtr = TrackInfoPtrPtr? *TrackInfoPtrPtr : nullptr)
					{
						TrackInfoSharedPtr->CurrentFrame = FrameNumber;
						OnTrackFrameUpdated().Broadcast(AsWeak(), TrackInfoSharedPtr.Get(), InstigatorID);
					}
				}
			}
		}
	}
}

void FChaosVDPlaybackController::GoToTrackFrame(FGuid InstigatorID, EChaosVDTrackType TrackType, int32 InTrackID, int32 FrameNumber, int32 Step)
{
	switch (TrackType)
	{
	case EChaosVDTrackType::Game:
		GoToRecordedGameFrame(FrameNumber, InstigatorID);
		break;
	case EChaosVDTrackType::Solver:
		GoToRecordedSolverStep(InTrackID, FrameNumber, Step, InstigatorID);
		break;
	default:
		ensure(false);
		break;
	}
}

int32 FChaosVDPlaybackController::GetTrackStepsNumberAtFrame(EChaosVDTrackType TrackType, const int32 InTrackID, const int32 FrameNumber) const
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
			if (const FChaosVDSolverFrameData* FrameData = LoadedRecording->GetSolverFrameData(InTrackID, FrameNumber))
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

const FChaosVDStepsContainer* FChaosVDPlaybackController::GetTrackStepsDataAtFrame(EChaosVDTrackType TrackType, int32 InTrackID, int32 FrameNumber) const
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
			if (const FChaosVDSolverFrameData* FrameData = LoadedRecording->GetSolverFrameData(InTrackID, FrameNumber))
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
			const int32 AvailableSteps = GetTrackStepsNumberAtFrame(EChaosVDTrackType::Solver, InTrackID, InFrameNumber);
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
	for (int32 SolverID : AvailableSolversID)
	{
		OutTrackInfo.Add(TrackInfoMap.FindChecked(SolverID));
	}
}

void FChaosVDPlaybackController::UpdateSolverTracksData()
{
	const TMap<int32, TArray<FChaosVDSolverFrameData>>& SolversByID = LoadedRecording->GetAvailableSolvers();
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
	};

	GameTrackInfo->MaxFrames = LoadedRecording->GetAvailableGameFrames().Num();
	GameTrackInfo->TrackType = EChaosVDTrackType::Game;

	// Each time the recording is updated, populate or update the existing solver tracks data
	UpdateSolverTracksData();

	OnDataUpdated().Broadcast(AsWeak());
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "HistoryEdition/HistoryEdition.h"

#include "ConcertLogGlobal.h"
#include "ConcertSyncSessionDatabase.h"
#include "IConcertServer.h"

#include "Misc/ScopeExit.h"

namespace UE::ConcertSyncCore
{
	static FConcertSessionFilter BuildFilterFrom(const TSet<FActivityID>& ToDelete);

	TSet<FActivityID> CombineRequirements(const FHistoryAnalysisResult& ToDelete)
	{
		TSet<FActivityID> Result;
		for (const FActivityID ActivityID : ToDelete.HardDependencies)
		{
			Result.Add(ActivityID);
		}
		for (const FActivityID ActivityID : ToDelete.PossibleDependencies)
		{
			Result.Add(ActivityID);
		}
		return Result;
	}
	
	FOperationErrorResult DeleteActivitiesInArchivedSession(const TSharedRef<IConcertServer>& Server, const FGuid& ArchivedSessionId, const TSet<FActivityID>& ToDelete)
	{
		const FString SessionId = ArchivedSessionId.ToString(EGuidFormats::DigitsWithHyphens);
		UE_LOG(LogConcert, Log, TEXT("Deleting %d activities from session %s..."), ToDelete.Num(), *SessionId);
		TOptional<FString> ErrorMessage;
		ON_SCOPE_EXIT
		{
			if (ErrorMessage)
			{
				UE_LOG(LogConcert, Error, TEXT("Finished deleting activities from %s with error: %s."), *SessionId, **ErrorMessage);
			}
			else
			{
				UE_LOG(LogConcert, Log, TEXT("Finished deleting activities from %s successfully."), *SessionId);
			}
		};
		
		const TOptional<FConcertSessionInfo> DeletedSessionInfo = Server->GetArchivedSessionInfo(ArchivedSessionId);
		if (!DeletedSessionInfo)
		{
			ErrorMessage = FString::Printf(TEXT("Session ID %s does not resolve to any archived session!"), *ArchivedSessionId.ToString());
			return FOperationErrorResult::MakeError(FText::FromString(*ErrorMessage));
		}

		// Restore the session while skipping all to be deleted activities
		FText FailureReason;
		const FConcertSessionFilter Filter = BuildFilterFrom(ToDelete);
		// Do not use same name in case user already created a live session from this archived session (would fail in that case) 
		FConcertSessionInfo OverrideInfo = *DeletedSessionInfo;
		OverrideInfo.SessionName = DeletedSessionInfo->SessionName + FGuid::NewGuid().ToString();
		const TSharedPtr<IConcertServerSession> LiveSession = Server->RestoreSession(ArchivedSessionId, OverrideInfo, Filter, FailureReason);
		if (!LiveSession)
		{
			ErrorMessage = FailureReason.ToString();
			return FOperationErrorResult::MakeError(FailureReason);
		}
		ON_SCOPE_EXIT
		{
			Server->DestroySession(LiveSession->GetId(), FailureReason);
		};

		// The archived session must be removed before it can be overwritten
		if (!Server->DestroySession(ArchivedSessionId, FailureReason))
		{
			ErrorMessage = FailureReason.ToString();
			return FOperationErrorResult::MakeError(FailureReason); 
		}

		if (!Server->ArchiveSession(LiveSession->GetId(), DeletedSessionInfo->SessionName, Filter, FailureReason, ArchivedSessionId).IsValid())
		{
			ErrorMessage = FailureReason.ToString();
			return FOperationErrorResult::MakeError(FailureReason); 
		}
		
		return FOperationErrorResult::MakeSuccess(); 
	}

	static FConcertSessionFilter BuildFilterFrom(const TSet<FActivityID>& ToDelete)
	{
		FConcertSessionFilter Result;
		Result.ActivityIdsToExclude.Reserve(ToDelete.Num());
		for (const FActivityID ActivityID : ToDelete)
		{
			Result.ActivityIdsToExclude.Add(ActivityID);
		}
		return Result;
	}
}


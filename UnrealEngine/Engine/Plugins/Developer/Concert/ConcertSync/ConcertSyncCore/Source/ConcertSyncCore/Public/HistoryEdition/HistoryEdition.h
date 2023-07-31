// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HistoryAnalysis.h"

class IConcertServer;
class IConcertSyncServer;

namespace UE::ConcertSyncCore
{
	struct FOperationErrorResult
	{
		TOptional<FText> ErrorMessage;

		explicit FOperationErrorResult(TOptional<FText> ErrorMessage = {})
			: ErrorMessage(ErrorMessage)
		{}

		static FOperationErrorResult MakeSuccess() { return FOperationErrorResult(); }
		static FOperationErrorResult MakeError(FText Error) { return FOperationErrorResult(MoveTemp(Error)); }

		bool WasSuccessful() const { return !ErrorMessage.IsSet(); }
		bool HadError() const { return !WasSuccessful(); }
	};

	/** Utility functions that converts FHistoryDeletionRequirements into a single TSet. */
	CONCERTSYNCCORE_API TSet<FActivityID> CombineRequirements(const FHistoryAnalysisResult& ToDelete);

	/**
	 * Deletes the given sessions in ToDelete from ArchivedSessionDatabase.
	 *
	 * There is no direct functionality for removing activities in the database; the operation is as follows:
	 *	1. Restore the archived session with a session filter
	 *	2. Delete the archived session
	 *	3. Archive the live session created in step 1 with the old guid.
	 */
	CONCERTSYNCCORE_API FOperationErrorResult DeleteActivitiesInArchivedSession(const TSharedRef<IConcertServer>& Server, const FGuid& ArchivedSessionId, const TSet<FActivityID>& ToDelete);
}

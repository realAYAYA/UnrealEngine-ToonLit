// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FPlasticSourceControlChangelistState;
class FPlasticSourceControlRevision;
class FPlasticSourceControlState;
typedef TSharedRef<class FPlasticSourceControlBranch, ESPMode::ThreadSafe> FPlasticSourceControlBranchRef;

namespace PlasticSourceControlParsers
{

class FSmartLockInfoParser
{
public:
	explicit FSmartLockInfoParser(const FString& InResult);

	FString Repository;
	int32 ItemId;
	FDateTime Date;
	FString BranchName;
	FString Status;
	FString Owner;
	FString Filename;
};

class FPlasticMergeConflictParser
{
public:
	explicit FPlasticMergeConflictParser(const FString& InResult);

	FString Filename;
	FString BaseChangeset;
	FString SourceChangeset;
};

/**
	* Helper struct for RemoveRedundantErrors()
	*/
struct FRemoveRedundantErrors
{
	explicit FRemoveRedundantErrors(const FString& InFilter)
		: Filter(InFilter)
	{
	}

	bool operator()(const FString& String) const
	{
		if (String.Contains(Filter))
		{
			return true;
		}

		return false;
	}

	/** The filter string we try to identify in the reported error */
	FString Filter;
};

bool ParseWorkspaceInfo(TArray<FString>& InResults, FString& OutBranchName, FString& OutRepositoryName, FString& OutServerUrl);

bool GetChangesetFromWorkspaceStatus(const TArray<FString>& InResults, int32& OutChangeset);

void ParseFileStatusResult(TArray<FString>&& InFiles, const TArray<FString>& InResults, TArray<FPlasticSourceControlState>& OutStates);

void ParseDirectoryStatusResult(const FString& InDir, const TArray<FString>& InResults, TArray<FPlasticSourceControlState>& OutStates);

void ParseFileinfoResults(const TArray<FString>& InResults, TArray<FPlasticSourceControlState>& InOutStates);

bool ParseHistoryResults(const bool bInUpdateHistory, const FString& InResults, TArray<FPlasticSourceControlState>& InOutStates);

bool ParseUpdateResults(const FString& InResults, TArray<FString>& OutFiles);
bool ParseUpdateResults(const TArray<FString>& InResults, TArray<FString>& OutFiles);

FText ParseCheckInResults(const TArray<FString>& InResults);

bool ParseChangelistsResults(const FString& Results, TArray<FPlasticSourceControlChangelistState>& OutChangelistsStates, TArray<TArray<FPlasticSourceControlState>>& OutCLFilesStates);

bool ParseShelveDiffResult(const FString InWorkspaceRoot, TArray<FString>&& InResults, FPlasticSourceControlChangelistState& InOutChangelistsState);
bool ParseShelveDiffResults(const FString InWorkspaceRoot, TArray<FString>&& InResults, TArray<FPlasticSourceControlRevision>& OutBaseRevisions);

bool ParseShelvesResults(const FString& InResults, TArray<FPlasticSourceControlChangelistState>& InOutChangelistsStates);
bool ParseShelvesResult(const FString& InResults, FString& OutComment, FDateTime& OutDate, FString& OutOwner);

bool ParseBranchesResults(const FString& InResults, TArray<FPlasticSourceControlBranchRef>& OutBranches);

bool ParseMergeResults(const FString& InResult, TArray<FString>& OutFiles);

} // namespace PlasticSourceControlParsers

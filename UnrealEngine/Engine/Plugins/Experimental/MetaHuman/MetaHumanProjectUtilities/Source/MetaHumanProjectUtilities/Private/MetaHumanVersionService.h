// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "MetaHumanTypes.h"
#include "Internationalization/Text.h"

// Gets live data from the cloud about the current deployed versions of MetaHumans
namespace UE::MetaHumanVersionService
{
	// Represents a Release Note
	struct FReleaseNoteData
	{
		FText Title; // Title for the note
		FMetaHumanVersion Version; // MHC release that this note relates to
		FText Note; // Main body of the release note
		FText Detail; // Further detail on the release
	};

	// Given a MHC release, give the UE version it relates to as a simple FString i.e. 1.2.0 => "5.0"
	const FString& UEVersionFromMhVersion(const FMetaHumanVersion& Version);

	// Returns all ReleaseNotes
	TArray<TSharedRef<FReleaseNoteData>> GetReleaseNotesForVersionUpgrade(const FMetaHumanVersion& FromVersion, const FMetaHumanVersion& ToVersion);

	// Override the URL to use to connect to the version service.
	void SetServiceUrl(const FString &ServiceUrl);

	// Starts asynchronous retrieval of cloud data
	void Init();
}

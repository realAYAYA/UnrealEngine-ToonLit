// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Clients/LiveLinkHubUEClientInfo.h"
#include "CoreTypes.h"
#include "LiveLinkPresetTypes.h"
#include "Misc/Guid.h"
#include "Subjects/LiveLinkHubSubjectSessionConfig.h"


#include "LiveLinkHubSessionData.generated.h"

/** Live link hub session data which can be serialized to disk. */
USTRUCT()
struct FLiveLinkHubSessionData
{
	GENERATED_BODY()

	UPROPERTY()
	/** Subject configs for this session. */
	FLiveLinkHubSubjectSessionConfig SubjectsConfig;
};

/** Live link hub session data that can be saved to disk. */
USTRUCT()
struct FLiveLinkHubPersistedSessionData : public FLiveLinkHubSessionData
{
	GENERATED_BODY()

	FLiveLinkHubPersistedSessionData() = default;

	FLiveLinkHubPersistedSessionData(FLiveLinkHubSessionData SessionData)
		: FLiveLinkHubSessionData(MoveTemp(SessionData))
	{
	}

	/** Live link hub sources. */
	UPROPERTY()
	TArray<FLiveLinkSourcePreset> Sources;

	/** Live link hub subjects. */
	UPROPERTY()
	TArray<FLiveLinkSubjectPreset> Subjects;

	/** Live link hub client info. */
	UPROPERTY()
	TArray<FLiveLinkHubUEClientInfo> Clients;
};

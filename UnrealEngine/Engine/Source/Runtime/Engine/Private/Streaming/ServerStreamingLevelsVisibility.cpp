// Copyright Epic Games, Inc. All Rights Reserved.

#include "Streaming/ServerStreamingLevelsVisibility.h"
#include "Net/UnrealNetwork.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"

AServerStreamingLevelsVisibility::AServerStreamingLevelsVisibility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = false;
#if WITH_EDITORONLY_DATA
	bListedInSceneOutliner = false;
#endif
}

AServerStreamingLevelsVisibility* AServerStreamingLevelsVisibility::SpawnServerActor(UWorld* InWorld)
{
	const ENetMode NetMode = InWorld->GetNetMode();
	if (InWorld->IsGameWorld() && (NetMode == NM_DedicatedServer || NetMode == NM_ListenServer))
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = AServerStreamingLevelsVisibility::StaticClass()->GetFName();
		SpawnParams.OverrideLevel = InWorld->PersistentLevel;
		if (AServerStreamingLevelsVisibility* ServerStreamingLevelVisibility = InWorld->SpawnActor<AServerStreamingLevelsVisibility>(AServerStreamingLevelsVisibility::StaticClass(), SpawnParams))
		{
			for (ULevelStreaming* StreamingLevel : InWorld->GetStreamingLevels())
			{
				if (StreamingLevel && StreamingLevel->IsLevelVisible())
				{
					ServerStreamingLevelVisibility->SetIsVisible(StreamingLevel->GetWorldAssetPackageFName(), true);
				}
			}
			return ServerStreamingLevelVisibility;
		}
	}
	return nullptr;
}

bool AServerStreamingLevelsVisibility::Contains(const FName& InLevelPackageName) const
{
	return ServerVisibleLevelNames.Contains(InLevelPackageName);
}

void AServerStreamingLevelsVisibility::SetIsVisible(const FName& InLevelPackageName, bool bInIsVisible)
{
	if (ensure(GetLocalRole() == ROLE_Authority))
	{
		if (bInIsVisible)
		{
			ServerVisibleLevelNames.Add(InLevelPackageName);
		}
		else
		{
			ServerVisibleLevelNames.Remove(InLevelPackageName);
		}
	}
}
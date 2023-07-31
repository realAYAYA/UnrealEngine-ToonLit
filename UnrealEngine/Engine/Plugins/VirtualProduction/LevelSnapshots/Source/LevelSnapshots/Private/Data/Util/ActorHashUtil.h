// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HashSettings.h"

class AActor;
class UObject;
struct FActorSnapshotData;
struct FWorldSnapshotData;
struct FActorSnapshotHash;

namespace UE::LevelSnapshots::Private
{
	/** Updated by project settings */
	extern FHashSettings GHashSettings;
	
	/**
	 * Computes and populates hash data for actor.
	 */
	void PopulateActorHash(FActorSnapshotHash& HashData, AActor* WorldActor);

	/**
	 * Computes hash for actor and compares it to saved data.
	 * @return Whether the computed hash matches the saved hash.
	 */
	bool HasMatchingHash(const FActorSnapshotHash& ActorData, AActor* WorldActor);
}

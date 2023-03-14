// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/NonNullPointer.h"
#include "UObject/SoftObjectPath.h"

class AActor;
class UObject;
struct FWorldSnapshotData;

namespace UE::LevelSnapshots
{
	using FDeserializedActorGetter = TFunctionRef<TOptional<TNonNullPtr<AActor>>()>;
	
	struct FCanRecreateActorParams
	{
		UWorld* EditorWorld;

		UClass* Class;
		FSoftObjectPath ActorPath;
		EObjectFlags ObjectFlags;
		const FWorldSnapshotData& WorldData;
		
		/** If you need access to the actor's data, you can call this function. */
		FDeserializedActorGetter DeserializeFromSnapshotFunc;
	};
	
	/**
	 * Can decide whether a given actor can be added to or removed from the world.
	 */
	class LEVELSNAPSHOTS_API IActorSnapshotFilter
	{
	public:

		enum class EFilterResult
		{
			/* The object in question is included but only if nobody else returned Disallow.  */
			Allow,
			/* Let other filters decide. If every override return DoNotCare, default snapshot behaviour is applied. */
			DoNotCare,
			/* The object in question is never suitable and is not included. Other filters cannot override this. */
			Disallow
		};

		/** Whether the actor is allowed to be recreated. Called when diffing to world. */
		virtual EFilterResult CanRecreateActor(const FCanRecreateActorParams& Params) { return EFilterResult::DoNotCare; }

		/** Whether the actor is allowed to be removed. Called when diffing to world. */
		virtual EFilterResult CanDeleteActor(const AActor* EditorActor) { return EFilterResult::DoNotCare; }

		
		virtual ~IActorSnapshotFilter() = default;
	};
}
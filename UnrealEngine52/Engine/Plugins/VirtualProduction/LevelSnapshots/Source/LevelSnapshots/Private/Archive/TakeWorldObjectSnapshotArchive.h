// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SnapshotArchive.h"

struct FObjectSnapshotData;
struct FWorldSnapshotData;
class UObject;

namespace UE::LevelSnapshots::Private
{
	/* Used when we're taking a snapshot of the world. */
	class FTakeWorldObjectSnapshotArchive final : public FSnapshotArchive
	{
		using Super = FSnapshotArchive;
	public:

		static void TakeSnapshot(FObjectSnapshotData& InObjectData, FWorldSnapshotData& InSharedData, UObject* InOriginalObject);

		//~ Begin FSnapshotArchive Interface
		virtual bool ShouldSkipProperty(const FProperty* InProperty) const override;
		//~ End FSnapshotArchive Interface

	protected:
	
		//~ Begin FSnapshotArchive Interface
		virtual UObject* ResolveObjectDependency(int32 ObjectIndex, UObject* CurrentValue) const { checkNoEntry(); return nullptr; }
		//~ End FSnapshotArchive Interface

	private:
	
		FTakeWorldObjectSnapshotArchive(FObjectSnapshotData& InObjectData, FWorldSnapshotData& InSharedData, UObject* InOriginalObject);

		UObject* Archetype;
	};
}
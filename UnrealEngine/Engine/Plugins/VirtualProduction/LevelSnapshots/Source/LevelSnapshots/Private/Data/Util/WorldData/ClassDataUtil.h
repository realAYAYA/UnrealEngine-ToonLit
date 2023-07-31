// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SnapshotUtilTypes.h"
#include "Templates/NonNullPointer.h"
#include "UObject/ObjectMacros.h"

struct FPropertySelection;
class AActor;
class UObject;
struct FActorSnapshotData;
struct FClassSnapshotData;
struct FComponentSnapshotData;
struct FObjectSnapshotData;
struct FPropertySelectionMap;
struct FSnapshotDataCache;
struct FSoftClassPath;
struct FSubobjectSnapshotData;
struct FWorldSnapshotData;

namespace UE::LevelSnapshots::Private
{
	struct FSubobjectArchetypeFallbackInfo
	{
		UObject* SubobjectOuter;
		FName SubobjectName;
		EObjectFlags SubobjectFlags;
	};
	
	/** Gets the actor CDO that was saved when the snapshot was taken. */
	TOptional<TNonNullPtr<AActor>> GetActorClassDefault(FWorldSnapshotData& WorldData, FClassDataIndex ClassIndex, FSnapshotDataCache& Cache);
	/** Gets the archetype to use for constructing a subobject.*/
	TOptional<TNonNullPtr<UObject>> GetSubobjectArchetype(FWorldSnapshotData& WorldData, FClassDataIndex ClassIndex, FSnapshotDataCache& Cache, const FSubobjectArchetypeFallbackInfo& FallbackInfo);
	
	/** Gets an archetype that was saved when the snapshot was taken. Each subobject in actors can have their own archetypes. */
	TOptional<TNonNullPtr<FClassSnapshotData>> GetObjectArchetypeData(FWorldSnapshotData& WorldData, FClassDataIndex ClassIndex, FSnapshotDataCache& Cache, const FSubobjectArchetypeFallbackInfo& FallbackInfo);
	
	void SerializeClassDefaultsIntoSubobject(UObject* Object, FWorldSnapshotData& WorldData, FClassDataIndex ClassIndex, FSnapshotDataCache& Cache, const FSubobjectArchetypeFallbackInfo& FallbackInfo);
	void SerializeClassDefaultsIntoSubobject(UObject* Object, FClassSnapshotData& DataToSerialize, FWorldSnapshotData& WorldData);

	void SerializeSelectedClassDefaultsInto(UObject* Object, FWorldSnapshotData& WorldData, FClassDataIndex ClassIndex, FSnapshotDataCache& Cache, const FSubobjectArchetypeFallbackInfo& FallbackInfo, const FPropertySelection& PropertiesToRestore);

	FSoftClassPath GetClass(const FActorSnapshotData& Data, const FWorldSnapshotData& WorldData);
	FSoftClassPath GetClass(const FSubobjectSnapshotData& Data, const FWorldSnapshotData& WorldData);
	
	
	/** Adds a class default entry in the world data */
	FClassDataIndex AddClassArchetype(FWorldSnapshotData& WorldData, UObject* SavedObject);
}
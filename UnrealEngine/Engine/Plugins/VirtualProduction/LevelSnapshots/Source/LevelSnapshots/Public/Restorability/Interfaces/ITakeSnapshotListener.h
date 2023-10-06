// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class ULevelSnapshot;

namespace UE::LevelSnapshots
{
	struct FPostTakeSnapshotEventData;
	struct FPreTakeSnapshotEventData;

	struct FTakeObjectSnapshotEvent
	{
		UObject* Object;
	};

	struct FPreTakeObjectSnapshotEvent : FTakeObjectSnapshotEvent
	{};
	struct FPostTakeObjectSnapshotEvent : FTakeObjectSnapshotEvent
	{};

	/** Exposes callbacks for when a snapshot is taken */
	class LEVELSNAPSHOTS_API ITakeSnapshotListener
	{
	public:

		/** Called before an object's data is recorded by a snapshot. */
		virtual void PreTakeObjectSnapshot(const FPreTakeObjectSnapshotEvent& Params) {}
		/** Called after an object's data is recorded by a snapshot. */
		virtual void PostTakeObjectSnapshot(const FPostTakeObjectSnapshotEvent& Params) {}

		/** Called before a snapshot is taken for a world. */
		virtual void PreTakeSnapshot(const FPreTakeSnapshotEventData& Params) {}
		/** Called after a snapshot is taken for a world. */
		virtual void PostTakeSnapshot(const FPostTakeSnapshotEventData& Params) {}
		
		virtual ~ITakeSnapshotListener() = default;
	};
}

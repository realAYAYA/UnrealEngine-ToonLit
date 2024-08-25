// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/SoftObjectPath.h"

class UWorld;

namespace UE::ConcertSharedSlate { class IEditableReplicationStreamModel; }

namespace UE::MultiUserClient
{
	/**
	 * Handles a client opening a new level.
	 * Upon leaving a level, all replicated objects assigned to that client in that level are removed.
	 */
	class FChangeLevelHandler : public FNoncopyable
	{
	public:
		
		/**
		 * @param UpdatedModel The client model to update when the local editor changes maps. The caller ensures that it outlives the constructed object.
		 */
		FChangeLevelHandler(ConcertSharedSlate::IEditableReplicationStreamModel& UpdatedModel UE_LIFETIMEBOUND);
		~FChangeLevelHandler();

	private:

		/** The client model to update when the local editor changes maps. */
		ConcertSharedSlate::IEditableReplicationStreamModel& UpdatedModel;

		/** Path to the previously open world */
		FSoftObjectPath PreviousWorldPath;
		
		void OnWorldDestroyed(UWorld* World);
		void OnWorldAdded(UWorld* World) const;
	};
}

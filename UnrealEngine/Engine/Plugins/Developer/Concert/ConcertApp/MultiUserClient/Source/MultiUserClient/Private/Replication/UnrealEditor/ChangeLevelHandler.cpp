// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChangeLevelHandler.h"

#include "Replication/Editor/Model/IEditableReplicationStreamModel.h"

#include "Engine/Engine.h"
#include "Engine/World.h"

namespace UE::MultiUserClient
{
	FChangeLevelHandler::FChangeLevelHandler(ConcertSharedSlate::IEditableReplicationStreamModel& UpdatedModel)
		: UpdatedModel(UpdatedModel)
	{
		if (ensure(GEngine))
		{
			GEngine->OnWorldDestroyed().AddRaw(this, &FChangeLevelHandler::OnWorldDestroyed);
			GEngine->OnWorldAdded().AddRaw(this, &FChangeLevelHandler::OnWorldAdded);
		}
	}

	FChangeLevelHandler::~FChangeLevelHandler()
	{
		if (GEngine)
		{
			GEngine->OnWorldDestroyed().RemoveAll(this);
			GEngine->OnWorldAdded().RemoveAll(this);
		}
	}

	void FChangeLevelHandler::OnWorldDestroyed(UWorld* World)
	{
		// Remember the current map so when the next world
		if (World->WorldType == EWorldType::Editor)
		{
			PreviousWorldPath = FSoftObjectPath(World);
		}
	}

	void FChangeLevelHandler::OnWorldAdded(UWorld* World) const
	{
		// User reloaded the same map - keep around all settings
		if (PreviousWorldPath != World)
		{
			UpdatedModel.Clear();
		}
	}
}

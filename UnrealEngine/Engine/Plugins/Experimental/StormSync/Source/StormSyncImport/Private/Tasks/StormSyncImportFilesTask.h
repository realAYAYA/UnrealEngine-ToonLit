// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Tasks/IStormSyncImportTask.h"

/** Import files from local exported buffer implementation for tasks that need delayed execution */
class FStormSyncImportFilesTask final : public IStormSyncImportSubsystemTask
{
public:
	explicit FStormSyncImportFilesTask(const FString& InFilename) :
		Filename(InFilename)
	{
	}

	virtual void Run() override;

private:
	FString Filename;
};
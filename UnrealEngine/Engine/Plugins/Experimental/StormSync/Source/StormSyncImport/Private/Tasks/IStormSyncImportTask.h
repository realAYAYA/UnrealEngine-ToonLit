// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/** Interface for tasks that need delayed execution */
class IStormSyncImportSubsystemTask
{
public:
	virtual ~IStormSyncImportSubsystemTask() = default;

	virtual void Run() = 0;
};

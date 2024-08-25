// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tasks/StormSyncImportBufferTask.h"

#include "StormSyncImportLog.h"
#include "Subsystems/StormSyncImportSubsystem.h"

void FStormSyncImportBufferTask::Run()
{
	if (!Buffer.IsValid())
	{
		UE_LOG(LogStormSyncImport, Error, TEXT("FStormSyncImportBufferTask::Run failed on invalid buffer"));
		return;
	}

	UE_LOG(LogStormSyncImport, Display, TEXT("FStormSyncImportBufferTask::Run for buffer of size %d"), Buffer->Num());
	UStormSyncImportSubsystem::Get().PerformBufferImport(PackageDescriptor, MoveTemp(Buffer));
}

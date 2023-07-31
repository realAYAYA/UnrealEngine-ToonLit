// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetImportTask.h"

DEFINE_LOG_CATEGORY(LogAssetImportTask);

UAssetImportTask::UAssetImportTask()
{

}

const TArray<UObject*>& UAssetImportTask::GetObjects() const
{
	if (AsyncResults.IsValid())
	{
		AsyncResults->WaitUntilDone();
		return AsyncResults->GetImportedObjects();
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return ToRawPtrTArrayUnsafe(Result);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

bool UAssetImportTask::IsAsyncImportComplete() const
{
	return !AsyncResults.IsValid() || AsyncResults->GetStatus() == UE::Interchange::FImportResult::EStatus::Done;
}

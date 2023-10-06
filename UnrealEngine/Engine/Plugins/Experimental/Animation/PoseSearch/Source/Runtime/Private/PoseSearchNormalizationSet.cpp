// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchNormalizationSet.h"
#include "PoseSearch/PoseSearchDatabase.h"

#if WITH_EDITORONLY_DATA

void UPoseSearchNormalizationSet::AddUniqueDatabases(TArray<TObjectPtr<const UPoseSearchDatabase>>& UniqueDatabases) const
{
	for (const TObjectPtr<const UPoseSearchDatabase>& Database : Databases)
	{
		if (Database)
		{
			UniqueDatabases.AddUnique(Database);
		}
	}
}

#endif // WITH_EDITORONLY_DATA
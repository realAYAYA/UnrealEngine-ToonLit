// Copyright Epic Games, Inc. All Rights Reserved.

#include "HistoryEdition/ActivityDependencyEdge.h"

FString UE::ConcertSyncCore::LexToString(EActivityDependencyReason Reason)
{
	switch (Reason)
	{
		case EActivityDependencyReason::PackageCreation: return TEXT("PackageCreation");
		case EActivityDependencyReason::PackageRemoval: return TEXT("PackageRemoval");
		case EActivityDependencyReason::PackageRename: return TEXT("PackageRename");
		case EActivityDependencyReason::PackageEdited: return TEXT("PackageEdited");
		case EActivityDependencyReason::EditAfterPreviousPackageEdit: return TEXT("EditPossiblyDependsOnPackage");
		case EActivityDependencyReason::SubobjectCreation: return TEXT("SubobjectCreation");
		case EActivityDependencyReason::SubobjectRemoval: return TEXT("SubobjectRemoval");
		default:
			checkNoEntry();
			static_assert(static_cast<int32>(EActivityDependencyReason::Count) == 7, "Update this location when you change EActivityDependencyReason");
			return TEXT("Invalid");
	}
}

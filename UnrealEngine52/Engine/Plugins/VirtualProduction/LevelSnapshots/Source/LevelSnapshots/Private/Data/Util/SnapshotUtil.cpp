// Copyright Epic Games, Inc. All Rights Reserved.

#include "Util/SnapshotUtil.h"

#include "ActorSnapshotData.h"
#include "LevelSnapshotsLog.h"

#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

FSoftObjectPath UE::LevelSnapshots::Private::ExtractPathWithoutSubobjects(const FSoftObjectPath& ObjectPath)
{
	int32 ColonIndex = INDEX_NONE;
	const FString Path = ObjectPath.ToString();
	Path.FindChar(':', ColonIndex);
	return ColonIndex == INDEX_NONE
		? Path
		: Path.Left(ColonIndex);
}

FString UE::LevelSnapshots::Private::ExtractLastSubobjectName(const FSoftObjectPath& ObjectPath)
{
	const FString& SubPathString = ObjectPath.GetSubPathString();
	const int32 LastDotIndex = SubPathString.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	if (LastDotIndex == INDEX_NONE)
	{
		return SubPathString;
	}

	return SubPathString.RightChop(LastDotIndex + 1);
}

TOptional<FSoftObjectPath> UE::LevelSnapshots::Private::ExtractActorFromPath(const FSoftObjectPath& OriginalObjectPath, bool& bIsPathToActorSubobject)
{
	const static FString PersistentLevelString("PersistentLevel.");
	const int32 PersistentLevelStringLength = PersistentLevelString.Len();
	// /Game/MapName.MapName:PersistentLevel.StaticMeshActor_42.StaticMeshComponent becomes PersistentLevel.StaticMeshActor_42.StaticMeshComponent
	const FString& SubPathString = OriginalObjectPath.GetSubPathString();
	const int32 IndexOfPersistentLevelInfo = SubPathString.Find(PersistentLevelString, ESearchCase::CaseSensitive);
	if (IndexOfPersistentLevelInfo == INDEX_NONE)
	{
		return {};
	}

	// Example: /Game/MapName.MapName:PersistentLevel.StaticMeshActor_42.StaticMeshComponent
	const int32 DotAfterActorNameIndex = SubPathString.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromStart, IndexOfPersistentLevelInfo + PersistentLevelStringLength);
	const bool bPathIsToActor = DotAfterActorNameIndex == INDEX_NONE;
	
	bIsPathToActorSubobject = !bPathIsToActor;
	return bPathIsToActor ?
		// Example /Game/MapName.MapName:PersistentLevel.StaticMeshActor_42
		OriginalObjectPath
		:
		// Converts /Game/MapName.MapName:PersistentLevel.StaticMeshActor_42.StaticMeshComponent to /Game/MapName.MapName:PersistentLevel.StaticMeshActor_42
		FSoftObjectPath(OriginalObjectPath.GetAssetPath(), SubPathString.Left(DotAfterActorNameIndex));
}

bool UE::LevelSnapshots::Private::IsPathToWorldObject(const FSoftObjectPath& OriginalObjectPath)
{
	bool bDummy;
	return ExtractActorFromPath(OriginalObjectPath, bDummy).IsSet();
}

TOptional<int32> UE::LevelSnapshots::Private::FindDotAfterActorName(const FSoftObjectPath& OriginalObjectPath)
{
	const static FString PersistentLevelString("PersistentLevel.");
	const int32 PersistentLevelStringLength = PersistentLevelString.Len();
	// /Game/MapName.MapName:PersistentLevel.StaticMeshActor_42.StaticMeshComponent becomes PersistentLevel.StaticMeshActor_42.StaticMeshComponent
	const FString& SubPathString = OriginalObjectPath.GetSubPathString();
	const int32 IndexOfPersistentLevelInfo = SubPathString.Find(PersistentLevelString, ESearchCase::CaseSensitive);
	if (IndexOfPersistentLevelInfo == INDEX_NONE)
	{
		return {};
	}

	// Example: /Game/MapName.MapName:PersistentLevel.StaticMeshActor_42.StaticMeshComponent
	const int32 DotAfterActorNameIndex = SubPathString.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromStart, IndexOfPersistentLevelInfo + PersistentLevelStringLength);
	return DotAfterActorNameIndex != INDEX_NONE ? DotAfterActorNameIndex : TOptional<int32>();
}

TOptional<TNonNullPtr<FActorSnapshotData>> UE::LevelSnapshots::Private::FindSavedActorDataUsingObjectPath(TMap<FSoftObjectPath, FActorSnapshotData>& ActorData, const FSoftObjectPath& OriginalObjectPath, bool& bIsPathToActorSubobject)
{
	const TOptional<FSoftObjectPath> PathToActor = UE::LevelSnapshots::Private::ExtractActorFromPath(OriginalObjectPath, bIsPathToActorSubobject);
	if (!PathToActor.IsSet())
	{
		return {};	
	}
	
	FActorSnapshotData* Result = ActorData.Find(*PathToActor);
	UE_CLOG(Result == nullptr, LogLevelSnapshots, Warning, TEXT("Path %s looks like an actor path but no data was saved for it. Maybe it was a reference to an auto-generated actor, e.g. a brush or volume present in all worlds by default?"), *OriginalObjectPath.ToString());
	return Result ? TOptional<TNonNullPtr<FActorSnapshotData>>(Result) : TOptional<TNonNullPtr<FActorSnapshotData>>();  
}


/* Takes an existing path to an actor's subobjects and replaces the actor bit with the path to another actor.
 *
 * E.g. /Game/MapName.MapName:PersistentLevel.StaticMeshActor_42.StaticMeshComponent could become /Game/MapName.MapName:PersistentLevel.SomeOtherActor.StaticMeshComponent
 */
FSoftObjectPath UE::LevelSnapshots::Private::SetActorInPath(AActor* NewActor, const FSoftObjectPath& OriginalObjectPath)
{
	const static FString PersistentLevelString("PersistentLevel.");
	const int32 PersistentLevelStringLength = PersistentLevelString.Len();
	const FString& SubPathString = OriginalObjectPath.GetSubPathString();
	const int32 IndexOfPersistentLevelInfo = SubPathString.Find(PersistentLevelString, ESearchCase::CaseSensitive);
	if (IndexOfPersistentLevelInfo == INDEX_NONE)
	{
		return {};
	}

	const int32 DotAfterActorNameIndex = SubPathString.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromStart, IndexOfPersistentLevelInfo + PersistentLevelStringLength);

	const FSoftObjectPath PathToNewActor(NewActor);
	// PersistentLevel.StaticMeshActor_42.StaticMeshComponent becomes .StaticMeshComponent
	const FString PathAfterOriginalActor = SubPathString.Right(SubPathString.Len() - DotAfterActorNameIndex); 
	return FSoftObjectPath(PathToNewActor.GetAssetPath(), PathToNewActor.GetSubPathString() + PathAfterOriginalActor);
}
// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/ObjectUtils.h"

#include "Containers/UnrealString.h"
#include "UObject/SoftObjectPath.h"

namespace UE::ConcertSharedSlate::ObjectUtils
{
	bool IsActor(const FSoftObjectPath& SoftObjectPath)
	{
		// Example of an actor called floor
		// SoftObjectPath = { AssetPath = {PackageName = "/Game/Maps/SyncBoxLevel", AssetName = "SyncBoxLevel"}, SubPathString = "PersistentLevel.Floor" } }
		const FString& SubPathString = SoftObjectPath.GetSubPathString();

		constexpr int32 PersistentLevelStringLength = 16; // "PersistentLevel." has 16 characters
		const bool bIsWorldObject = SubPathString.Contains(TEXT("PersistentLevel."), ESearchCase::CaseSensitive);
		if (!bIsWorldObject)
		{
			// Not a path to a world object
			return {};
		}

		// Start search after the . behind PersistentLevel
		const int32 StartSearch = PersistentLevelStringLength + 1;
		const int32 IndexOfDotAfterActorName = SubPathString.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart, StartSearch);
		return IndexOfDotAfterActorName == INDEX_NONE;
	}
	
	TOptional<FSoftObjectPath> GetActorOf(const FSoftObjectPath& SoftObjectPath)
	{
		// Example of an actor called floor
		// SoftObjectPath = { AssetPath = {PackageName = "/Game/Maps/SyncBoxLevel", AssetName = "SyncBoxLevel"}, SubPathString = "PersistentLevel.Floor" } }
		const FString& SubPathString = SoftObjectPath.GetSubPathString();

		constexpr int32 PersistentLevelStringLength = 16; // "PersistentLevel." has 16 characters
		const bool bIsWorldObject = SubPathString.Contains(TEXT("PersistentLevel."), ESearchCase::CaseSensitive);
		if (!bIsWorldObject)
		{
			// Not a path to a world object
			return {};
		}

		// Start search after the . behind PersistentLevel
		const int32 StartSearch = PersistentLevelStringLength + 1;
		const int32 IndexOfDotAfterActorName = SubPathString.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart, StartSearch);
		if (IndexOfDotAfterActorName == INDEX_NONE)
		{
			// SoftObjectPath points to an actor
			return {};
		}

		const int32 NumToChopOffRight = SubPathString.Len() - IndexOfDotAfterActorName;
		const FString NewSubstring = SubPathString.LeftChop(NumToChopOffRight);
		const FSoftObjectPath PathToOwningActor(SoftObjectPath.GetAssetPath(), NewSubstring);
		return PathToOwningActor;
	}
	
	FString ExtractObjectDisplayStringFromPath(const FSoftObjectPath& Object)
	{
		// Subpath looks like this PersistentLevel.Actor.Component
		const FString& Subpath = Object.GetSubPathString();
		const int32 LastDotIndex = Subpath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (LastDotIndex == INDEX_NONE)
		{
			return {};
		}
		return Subpath.RightChop(LastDotIndex + 1);
	}
};


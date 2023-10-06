// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldFoldersImplementation.h"

#include "Containers/Map.h"
#include "WorldFolders.h"

FWorldFoldersImplementation::FWorldFoldersImplementation(UWorldFolders& InWorldFolders)
	: Owner(InWorldFolders)
{
}

UWorld* FWorldFoldersImplementation::GetWorld() const
{
	return Owner.GetWorld();
}

bool FWorldFoldersImplementation::ContainsFolder(const FFolder& InFolder) const
{
	return Owner.FoldersProperties.Contains(InFolder);
}
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Folder.h"

class UWorld;
class UWorldFolders;
struct FFolder;

class FWorldFoldersImplementation
{
public:
	FWorldFoldersImplementation(UWorldFolders& Owner);
	virtual ~FWorldFoldersImplementation() {}
	virtual bool AddFolder(const FFolder& InFolder) { return true; }
	virtual bool RemoveFolder(const FFolder& InFolder, bool bShouldDeleteFolder) { return true; }
	virtual bool RenameFolder(const FFolder& InOldFolder, const FFolder& InNewFolder) { return true; }
	virtual bool ContainsFolder(const FFolder& InFolder) const;
	UWorld* GetWorld() const;

protected:
	UWorldFolders& Owner;
};
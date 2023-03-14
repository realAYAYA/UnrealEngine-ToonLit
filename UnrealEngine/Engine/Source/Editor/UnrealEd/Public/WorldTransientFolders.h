// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorldFoldersImplementation.h"

// Class handling a list of transient actor folders of a world
class FWorldTransientFolders : public FWorldFoldersImplementation
{
	typedef FWorldFoldersImplementation Super;

public:
	FWorldTransientFolders(UWorldFolders& Owner) : Super(Owner) {}
	~FWorldTransientFolders() {}

	//~ Begin FWorldFoldersImplementation
	virtual bool RenameFolder(const FFolder& InOldFolder, const FFolder& InNewFolder) override;
	//~ End FWorldFoldersImplementation
};
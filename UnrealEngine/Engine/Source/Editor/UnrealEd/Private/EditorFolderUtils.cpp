// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorFolderUtils.h"

FName FEditorFolderUtils::GetLeafName(const FName& InPath)
{
	FString PathString = InPath.ToString();
	int32 LeafIndex = 0;
	if (PathString.FindLastChar('/', LeafIndex))
	{
		return FName(*PathString.RightChop(LeafIndex + 1));
	}
	else
	{
		return InPath;
	}
}

bool FEditorFolderUtils::PathIsChildOf(const FString& PotentialChild, const FString& Parent)
{
	const int32 ParentLen = Parent.Len();
	return
		PotentialChild.Len() > ParentLen&&
		PotentialChild[ParentLen] == '/' &&
		PotentialChild.Left(ParentLen) == Parent;
}

bool FEditorFolderUtils::PathIsChildOf(const FName& PotentialChild, const FName& Parent)
{
	return PathIsChildOf(PotentialChild.ToString(), Parent.ToString());
}
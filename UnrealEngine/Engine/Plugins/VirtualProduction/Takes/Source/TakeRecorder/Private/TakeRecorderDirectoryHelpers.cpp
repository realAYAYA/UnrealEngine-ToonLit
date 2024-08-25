// Copyright Epic Games, Inc. All Rights Reserved.

#include "TakeRecorderDirectoryHelpers.h"

#include "EditorDirectories.h"
#include "Misc/PackageName.h"


namespace UE::TakeRecorder::Private
{

bool IsValidPath(const FString& InPathBase)
{
	FString TestPath = InPathBase + TEXT("/") + "TestPackage";
	return !InPathBase.IsEmpty() && FPackageName::IsValidLongPackageName(TestPath, false);
}

/**
 * Splits the path into two parts the base (potentionally) and the sub-folders. For example,
 *
 *  /Game/Cinematics/Takes will be split to /Game and /Cinematics/Takes
 *
 ** */
TTuple<FString, FString> SplitPackagePath(const FString& InPath)
{
	int32 DelimIndex = InPath.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromStart, 1);

	FString RightPart;
	FString LeftPart;
	if (DelimIndex != INDEX_NONE)
	{
		RightPart = InPath.Right(InPath.Len() - DelimIndex);
		LeftPart = InPath.Left(DelimIndex);
	}
	else
	{
		LeftPart = InPath;
	}

	return {LeftPart, RightPart};
}

FString GetDefaultProjectPath()
{
	// Determine the starting path. Try to use the most recently used directory
	FString AssetPath;
	const FString DefaultFilesystemDirectory = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::NEW_ASSET);
	if (DefaultFilesystemDirectory.IsEmpty() || !FPackageName::TryConvertFilenameToLongPackageName(DefaultFilesystemDirectory, AssetPath))
	{
		return TEXT("/Game");
	}
	if (AssetPath.EndsWith(TEXT("/")))
	{
		return AssetPath.Left(AssetPath.Len()-1);
	}
	return AssetPath;
}

FString ResolvePathToProject(const FString& InPath)
{
	const FString FixedPathStr = InPath.StartsWith(TEXT("/")) ? InPath : TEXT("/") + InPath;
	const FString DefaultProjectPath = GetDefaultProjectPath();

	return DefaultProjectPath + FixedPathStr;
}

FString RemoveProjectFromPath(const FString& InPath)
{
	FString DefaultPath = GetDefaultProjectPath();
	if (InPath.StartsWith(TEXT("/Game")) || (!DefaultPath.IsEmpty() && InPath.StartsWith(DefaultPath)))
	{
		TTuple<FString, FString> Parts = SplitPackagePath(InPath);
		return Parts.Get<1>();
	}
	return InPath;
}

}

// Copyright Epic Games, Inc. All Rights Reserved.
#include "WorldPartition/ContentBundle/ContentBundlePaths.h"

#include "Engine/Level.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "WorldPartition/ContentBundle/ContentBundleBase.h"
#include "WorldPartition/ContentBundle/ContentBundleDescriptor.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
#include "PackageTools.h"
#endif

FString ContentBundlePaths::GetCookedContentBundleLevelFolder(const FContentBundleBase& ContentBundle)
{
	TStringBuilderWithBuffer<TCHAR, NAME_SIZE> GeneratedContentBundleLevelFolder;
	GeneratedContentBundleLevelFolder += TEXT("/");
	GeneratedContentBundleLevelFolder += ContentBundle.GetDescriptor()->GetPackageRoot();
	GeneratedContentBundleLevelFolder += TEXT("/CB/");
	GeneratedContentBundleLevelFolder += GetRelativeLevelFolder(ContentBundle);
	GeneratedContentBundleLevelFolder += TEXT("/");

	FString Result = *GeneratedContentBundleLevelFolder;
	FPaths::RemoveDuplicateSlashes(Result);
	return Result;
}

FString ContentBundlePaths::GetRelativeLevelFolder(const FContentBundleBase& ContentBundle)
{
	const UWorld* InjectedWorld = ContentBundle.GetInjectedWorld();
	FString WorldPackageName = InjectedWorld->GetPackage()->GetName();

	FString PackageRoot, PackagePath, PackageName;
	ensure(FPackageName::SplitLongPackageName(WorldPackageName, PackageRoot, PackagePath, PackageName));

	TStringBuilderWithBuffer<TCHAR, NAME_SIZE> RelativeLevelFolder;
	RelativeLevelFolder += PackagePath;
	RelativeLevelFolder += PackageName;
	RelativeLevelFolder += TEXT("/");

	return *RelativeLevelFolder;
}

#if WITH_EDITOR

FString ContentBundlePaths::MakeExternalActorPackagePath(const FString& ContentBundleExternalActorFolder, const FString& ActorName)
{
	const FString ContentBundleExternalActor = ULevel::GetActorPackageName(ContentBundleExternalActorFolder, EActorPackagingScheme::Reduced, *ActorName);
	check(IsAContentBundleExternalActorPackagePath(ContentBundleExternalActor));
	return ContentBundleExternalActor;
}

bool ContentBundlePaths::IsAContentBundleExternalActorPackagePath(FStringView InPackagePath)
{
	return GetContentBundleGuidFromExternalActorPackagePath(InPackagePath).IsValid();
}

FStringView ContentBundlePaths::GetRelativeExternalActorPackagePath(FStringView InContentBundleExternalActorPackagePath)
{
	FStringView RelativeContentBundlePath = GetActorPathRelativeToExternalActors(InContentBundleExternalActorPackagePath);
	if (!RelativeContentBundlePath.IsEmpty())
	{
		check(RelativeContentBundlePath.Left(GetContentBundleFolder().Len()).Equals(GetContentBundleFolder()));
		RelativeContentBundlePath = RelativeContentBundlePath.RightChop(GetContentBundleFolder().Len());
		if (!RelativeContentBundlePath.IsEmpty())
		{
			return RelativeContentBundlePath.RightChop(RelativeContentBundlePath.Find(TEXT("/")));
		}
	}
	
	return FStringView();
}

FGuid ContentBundlePaths::GetContentBundleGuidFromExternalActorPackagePath(FStringView InContentBundleExternalActorPackagePath)
{
	FGuid Result;

	FStringView RelativeContentBundlePath = GetActorPathRelativeToExternalActors(InContentBundleExternalActorPackagePath);
	if (!RelativeContentBundlePath.IsEmpty())
	{
		check(RelativeContentBundlePath.Left(GetContentBundleFolder().Len()).Equals(GetContentBundleFolder()));
		RelativeContentBundlePath = RelativeContentBundlePath.RightChop(GetContentBundleFolder().Len());
		if (!RelativeContentBundlePath.IsEmpty())
		{
			RelativeContentBundlePath= RelativeContentBundlePath.LeftChop(RelativeContentBundlePath.Len() - RelativeContentBundlePath.Find(TEXT("/")));
			verify(FGuid::Parse(FString(RelativeContentBundlePath), Result));
		}
	}

	return Result;
}

FStringView ContentBundlePaths::GetActorPathRelativeToExternalActors(FStringView InContentBundleExternalActorPackagePath)
{
	uint32 ExternalActorIdx = UE::String::FindFirst(InContentBundleExternalActorPackagePath, FPackagePath::GetExternalActorsFolderName(), ESearchCase::IgnoreCase);
	if (ExternalActorIdx != INDEX_NONE)
	{
		FStringView RelativeContentBundlePath = InContentBundleExternalActorPackagePath.RightChop(ExternalActorIdx + FCString::Strlen(FPackagePath::GetExternalActorsFolderName()));
		if (RelativeContentBundlePath.Left(GetContentBundleFolder().Len()).Equals(GetContentBundleFolder()))
		{
			return RelativeContentBundlePath;
		}
	}
	return FStringView();
}

bool ContentBundlePaths::BuildContentBundleExternalActorPath(const FString& InContenBundleMountPoint, const FGuid& InContentBundleGuid, FString& OutContentBundleRootPath)
{
	if (InContenBundleMountPoint.IsEmpty() || !InContentBundleGuid.IsValid())
	{
		return false;
	}

	TStringBuilderWithBuffer<TCHAR, NAME_SIZE> ContentBundleRootPathBuilder;
	ContentBundleRootPathBuilder += TEXT("/");
	ContentBundleRootPathBuilder += InContenBundleMountPoint;
	ContentBundleRootPathBuilder += GetContentBundleFolder();
	ContentBundleRootPathBuilder += InContentBundleGuid.ToString();
	ContentBundleRootPathBuilder += TEXT("/");
	
	OutContentBundleRootPath = *ContentBundleRootPathBuilder;

	return true;
}

bool ContentBundlePaths::BuildActorDescContainerPackagePath(const FString& InContenBundleMountPoint, const FGuid& InContentBundleGuid, const FString& InLevelPackagePath, FString& OutContainerPackagePath)
{
	FString ContentBundleRootPath;
	if (!BuildContentBundleExternalActorPath(InContenBundleMountPoint, InContentBundleGuid, ContentBundleRootPath))
	{
		return false;
	}

	FString LevelRoot, LevelPackagePath, LevelName;
	if (FPackageName::SplitLongPackageName(InLevelPackagePath, LevelRoot, LevelPackagePath, LevelName))
	{
		TStringBuilderWithBuffer<TCHAR, NAME_SIZE> ContentBundleActorDescContainerPackage;
		ContentBundleActorDescContainerPackage += ContentBundleRootPath;
		ContentBundleActorDescContainerPackage += LevelPackagePath;
		ContentBundleActorDescContainerPackage += LevelName;

		OutContainerPackagePath = UPackageTools::SanitizePackageName(*ContentBundleActorDescContainerPackage);
		return true;
	}

	return false;
}

FStringView ContentBundlePaths::GetRelativePath(FStringView InContentBundlePath)
{
	uint32 ContentBundleFolderIdx = UE::String::FindFirst(InContentBundlePath, GetContentBundleFolder(), ESearchCase::IgnoreCase);
	if (ContentBundleFolderIdx != INDEX_NONE)
	{
		FStringView RelativeContentBundlePath = InContentBundlePath.RightChop(ContentBundleFolderIdx + GetContentBundleFolder().Len());
		uint32 ContentBundleGuidEndIdx = UE::String::FindFirst(RelativeContentBundlePath, TEXT("/"), ESearchCase::IgnoreCase);
		if (ContentBundleGuidEndIdx != INDEX_NONE)
		{
			return RelativeContentBundlePath.RightChop(ContentBundleGuidEndIdx + 1); // + 1 to remove the "/"
		}
	}
	return FStringView();
}

bool ContentBundlePaths::IsAContentBundlePath(FStringView InContentBundlePath)
{
	return GetRelativePath(InContentBundlePath).Len() > 0;
}

#endif

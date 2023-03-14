// Copyright Epic Games, Inc. All Rights Reserved.
#include "WorldPartition/ContentBundle/ContentBundlePaths.h"

#include "Engine/Level.h"
#include "Engine/World.h"
#include "WorldPartition/ContentBundle/ContentBundleBase.h"
#include "WorldPartition/ContentBundle/ContentBundleDescriptor.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
#include "String/Find.h"
#endif

namespace ContentBundlePaths
{
	// ContentBundleCookedFolder is reduce to avoid reaching MAX_PATH
	constexpr FStringView ContentBundleCookedFolder = TEXTVIEW("/CB/");
}

FString ContentBundlePaths::GetCookedContentBundleLevelFolder(const FContentBundleBase& ContentBundle)
{
	TStringBuilderWithBuffer<TCHAR, NAME_SIZE> GeneratedContentBundleLevelFolder;
	GeneratedContentBundleLevelFolder += TEXT("/");
	GeneratedContentBundleLevelFolder += ContentBundle.GetDescriptor()->GetPackageRoot();
	GeneratedContentBundleLevelFolder += TEXT("/");
	GeneratedContentBundleLevelFolder += ContentBundlePaths::ContentBundleCookedFolder;
	GeneratedContentBundleLevelFolder += TEXT("/");
	
	// ContentBundleGuid is reduced to avoid reaching MAX_PATH
	ContentBundle.GetDescriptor()->GetGuid().AppendString(GeneratedContentBundleLevelFolder, EGuidFormats::Short); 

	GeneratedContentBundleLevelFolder += TEXT("/");
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
	check(IsAContentBundlePackagePath(ContentBundleExternalActor));
	return ContentBundleExternalActor;
}

bool ContentBundlePaths::IsAContentBundlePackagePath(FStringView InPackagePath)
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

#endif
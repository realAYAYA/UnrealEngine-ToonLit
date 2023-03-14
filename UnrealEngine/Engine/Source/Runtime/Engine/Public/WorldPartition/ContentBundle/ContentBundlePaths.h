// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Containers/StringView.h"

class FContentBundleBase;

namespace ContentBundlePaths
{
	constexpr FStringView ContentBundleFolder = TEXTVIEW("/ContentBundle/");
	constexpr FStringView GeneratedFolder = TEXTVIEW("/_Generated_/");

	// Returns "/ContentBundle/"
	constexpr FStringView GetContentBundleFolder() { return ContentBundleFolder; }
	
	// Returns "ContentBundle"
	constexpr FStringView GetContentBundleFolderName()
	{
		// Start at index 1 to remove the first "/", Lenght - 2 to remove the trailing "/"
		return FStringView(&GetContentBundleFolder()[1], GetContentBundleFolder().Len() - 2);
	}

	// Returns "/ContentBundle/"
	constexpr FStringView GetGeneratedFolder() { return GeneratedFolder; }
	
	// Returns "_Generated_"
	constexpr FStringView GetGeneratedFolderName()
	{
		// Start at index 1 to remove the first "/", Lenght - 2 to remove the trailing "/"
		return FStringView(&GetGeneratedFolder()[1], GetGeneratedFolder().Len() - 2);
	}

	// Return value uses a reduced amount of character to avoid reaching character limit when cooking 
	// return format is /{MountPoint}/CB/{ContentBundle_ShortUID}/{LevelPath}/{GeneratedFolder}/
	FString GetCookedContentBundleLevelFolder(const FContentBundleBase& ContentBundle);
	
	// return format is {LevelPath}
	FString GetRelativeLevelFolder(const FContentBundleBase& ContentBundle);

#if WITH_EDITOR
	// return an ExternalActor path following the format : /{MountPoint}/{ExternalActorFolder}/{ContentBundleFolder}/{ContentBundleUID}/{LevelPath}/{ExternalActorPackagePath}
	ENGINE_API FString MakeExternalActorPackagePath(const FString& ContentBundleExternalActorFolder, const FString& ActorName);

	// return true if InPackage follow format : //{MountPoint}/{ExternalActorFolder}/{ContentBundleFolder}/{ContentBundleUID}/, false otherwise
	ENGINE_API bool IsAContentBundlePackagePath(FStringView InPackagePath);

	// InContentBundleExternalActorPackagePath format is : /{MountPoint}/{ExternalActorFolder}/{ContentBundleFolder}/{ContentBundleUID}/{LevelPath}/{ExternalActorPackagePath}
	// return format is /{LevelPath}/{ExternalActorPackagePath}, empty otherwise
	ENGINE_API FStringView GetRelativeExternalActorPackagePath(FStringView InContentBundleExternalActorPackagePath);

	// InContentBundleExternalActorPackagePath format is : /{MountPoint}/{ExternalActorFolder}/{ContentBundleFolder}/{ContentBundleUID}/{LevelPath}/{ExternalActorPackagePath}
	// return format is {ContentBundleUID}, 0 otherwise
	ENGINE_API FGuid GetContentBundleGuidFromExternalActorPackagePath(FStringView InContentBundleExternalActorPackagePath);

	// InExternalActorPath format is : /{MountPoint}/{ExternalActorFolder}/{ContentBundleFolder}/{ContentBundleUID}/{LevelPath}/{ExternalActorPackagePath}
	// return format is /{ContentBundleFolder}/{ContentBundleUID}/{LevelPath}/{ExternalActorPackagePath}, empty otherwise
	ENGINE_API FStringView GetActorPathRelativeToExternalActors(FStringView InContentBundleExternalActorPackagePath);
#endif
}
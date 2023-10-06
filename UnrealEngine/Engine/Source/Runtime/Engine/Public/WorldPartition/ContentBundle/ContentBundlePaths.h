// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Containers/StringView.h"

class FContentBundleBase;

namespace ContentBundlePaths
{
	inline constexpr FStringView ContentBundleFolder = TEXTVIEW("/ContentBundle/");
	inline constexpr FStringView GeneratedFolder = TEXTVIEW("/_Generated_/");

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
	// return format is /{MountPoint}/{ContentBundleFolder}/{ContentBundleUID}/
	ENGINE_API bool BuildContentBundleExternalActorPath(const FString& InContenBundleMountPoint, const FGuid& InContentBundleGuid, FString& OutContentBundleRootPath);

	// return format is /{MountPoint}/{ContentBundleFolder}/{ContentBundleUID}/{LevelPath}/
	ENGINE_API bool BuildActorDescContainerPackagePath(const FString& InContenBundleMountPoint, const FGuid& InContentBundleGuid, const FString& InLevelPackagePath, FString& OutContainerPackagePath);

	// InContentBundlePath format is : */{ContentBundleFolder}/{ContentBundleUID}/{RelativePath}
	// return format is {RelativePath}, empty otherwise
	ENGINE_API FStringView GetRelativePath(FStringView InContentBundlePath);

	// return true if InPackage follow format : */{ContentBundleFolder}/{ContentBundleUID}/*, false otherwise
	ENGINE_API bool IsAContentBundlePath(FStringView InContentBundlePath);

	// return an ExternalActor path following the format : /{MountPoint}/{ExternalActorFolder}/{ContentBundleFolder}/{ContentBundleUID}/{LevelPath}/{ExternalActorPackagePath}
	ENGINE_API FString MakeExternalActorPackagePath(const FString& ContentBundleExternalActorFolder, const FString& ActorName);

	// return true if InPackage follow format : //{MountPoint}/{ExternalActorFolder}/{ContentBundleFolder}/{ContentBundleUID}/*, false otherwise
	ENGINE_API bool IsAContentBundleExternalActorPackagePath(FStringView InPackagePath);

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
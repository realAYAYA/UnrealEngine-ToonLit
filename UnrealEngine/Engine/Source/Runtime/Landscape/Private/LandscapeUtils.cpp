// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeUtils.h"

#include "Engine/Level.h"

#if WITH_EDITOR
#include "EditorDirectories.h"
#include "ObjectTools.h"
#endif

namespace UE::Landscape
{

bool DoesPlatformSupportEditLayers(EShaderPlatform InShaderPlatform)
{
	// Edit layers work on the GPU and are only available on SM5+ and in the editor : 
	return IsFeatureLevelSupported(InShaderPlatform, ERHIFeatureLevel::SM5)
		&& !IsConsolePlatform(InShaderPlatform)
		&& !IsMobilePlatform(InShaderPlatform);
}

#if WITH_EDITOR

FString GetSharedAssetsPath(const FString& InPath)
{
	FString Path = InPath + TEXT("_sharedassets/");

	if (Path.StartsWith("/Temp/"))
	{
		Path = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::LEVEL) / Path.RightChop(FString("/Temp/").Len());
	}

	return Path;
}

FString GetSharedAssetsPath(const ULevel* InLevel)
{
	return GetSharedAssetsPath(InLevel->GetOutermost()->GetName());
}

FString GetLayerInfoObjectPackageName(const ULevel* InLevel, const FName& InLayerName, FName& OutLayerObjectName)
{
	FString PackageName;
	FString PackageFilename;
	FString SharedAssetsPath = GetSharedAssetsPath(InLevel);
	int32 Suffix = 1;

	OutLayerObjectName = FName(*FString::Printf(TEXT("%s_LayerInfo"), *ObjectTools::SanitizeInvalidChars(*InLayerName.ToString(), INVALID_LONGPACKAGE_CHARACTERS)));
	FPackageName::TryConvertFilenameToLongPackageName(SharedAssetsPath / OutLayerObjectName.ToString(), PackageName);

	while (FPackageName::DoesPackageExist(PackageName, &PackageFilename))
	{
		OutLayerObjectName = FName(*FString::Printf(TEXT("%s_LayerInfo_%d"), *ObjectTools::SanitizeInvalidChars(*InLayerName.ToString(), INVALID_LONGPACKAGE_CHARACTERS), Suffix));
		if (!FPackageName::TryConvertFilenameToLongPackageName(SharedAssetsPath / OutLayerObjectName.ToString(), PackageName))
		{
			break;
		}

		Suffix++;
	}

	return PackageName;
}

#endif //!WITH_EDITOR

} // end namespace UE::Landscape
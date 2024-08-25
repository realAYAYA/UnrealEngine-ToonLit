// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanTypes.h"

#include "EditorAssetLibrary.h"
#include "MetaHumanProjectUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/MetaData.h"

FImportPaths::FImportPaths(FMetaHumanAssetImportDescription ImportDescription)
{
	SourceCharacterFilePath = FPaths::ConvertRelativePathToFull(ImportDescription.CharacterPath);
	FPaths::NormalizeDirectoryName(SourceCharacterFilePath);
	SourceMetaHumansFilePath = FPaths::GetPath(SourceCharacterFilePath);
	SourceRootFilePath = FPaths::GetPath(SourceMetaHumansFilePath);
	SourceCommonFilePath = FPaths::ConvertRelativePathToFull(ImportDescription.CommonPath);
	FPaths::NormalizeDirectoryName(SourceCommonFilePath);

	DestinationRootAssetPath = ImportDescription.DestinationPath;
	DestinationMetaHumansAssetPath = FPaths::Combine(DestinationRootAssetPath, MetaHumansFolderName);
	DestinationCharacterAssetPath = FPaths::Combine(DestinationMetaHumansAssetPath, ImportDescription.CharacterName);
	DestinationCommonAssetPath = FPaths::Combine(DestinationMetaHumansAssetPath, CommonFolderName);

	DestinationRootFilePath = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(DestinationRootAssetPath));
	DestinationMetaHumansFilePath = FPaths::Combine(DestinationRootFilePath, MetaHumansFolderName);
	DestinationCharacterFilePath = FPaths::Combine(DestinationMetaHumansFilePath, ImportDescription.CharacterName);
	DestinationCommonFilePath = FPaths::Combine(DestinationMetaHumansFilePath, CommonFolderName);
}

FMetaHumanVersion FMetaHumanVersion::ReadFromFile(const FString& VersionFilePath)
{
	// This is the old behaviour. We can probably do better than this.
	if (!IFileManager::Get().FileExists(*VersionFilePath))
	{
		return FMetaHumanVersion(TEXT("0.5.1"));
	}
	const FString VersionTag = TEXT("MetaHumanVersion");
	FString VersionInfoString;
	if (FFileHelper::LoadFileToString(VersionInfoString, *VersionFilePath))
	{
		TSharedPtr<FJsonObject> VersionInfoObject;
		if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(VersionInfoString), VersionInfoObject))
		{
			return FMetaHumanVersion(VersionInfoObject->GetStringField(VersionTag));
		}
	}
	// Invalid file
	return {};
}

FInstalledMetaHuman::FInstalledMetaHuman(const FString& Name, const FString& MetaHumansFilePath)
	: Name(Name)
	, MetaHumansFilePath(MetaHumansFilePath)
	, MetaHumansAssetPath(FPackageName::FilenameToLongPackageName(MetaHumansFilePath))
{
}

EQualityLevel FInstalledMetaHuman::GetQualityLevel() const
{
	const FString MetaHumanRootAsset = FPaths::Combine(MetaHumansAssetPath, Name, FString::Format(TEXT("BP_{0}.BP_{0}"), {Name}));

	static const FName MetaHumanAssetQualityLevelKey = TEXT("MHExportQuality");
	if (const UObject* Asset = LoadObject<UObject>(nullptr, *MetaHumanRootAsset))
	{
		if (const TMap<FName, FString>* Metadata = UMetaData::GetMapForObject(Asset))
		{
			if (const FString* AssetQualityMetaData = Metadata->Find(MetaHumanAssetQualityLevelKey))
			{
				if (*AssetQualityMetaData == TEXT("High"))
				{
					return EQualityLevel::High;
				}
				if (*AssetQualityMetaData == TEXT("Medium"))
				{
					return EQualityLevel::Medium;
				}
			}
		}
	}
	return EQualityLevel::Low;
}

TArray<FInstalledMetaHuman> FInstalledMetaHuman::GetInstalledMetaHumans(const FImportPaths& ImportPaths)
{
	TArray<FInstalledMetaHuman> FoundMetaHumans;
	const FString ProjectMetaHumanPath = FPaths::Combine(ImportPaths.DestinationMetaHumansFilePath, TEXT("*"));
	TArray<FString> DirectoryList;
	IFileManager::Get().FindFiles(DirectoryList, *ProjectMetaHumanPath, false, true);

	for (const FString& Directory : DirectoryList)
	{
		const FString CharacterName = FPaths::GetCleanFilename(Directory);
		if (UEditorAssetLibrary::DoesAssetExist(FPackageName::ObjectPathToPackageName(ImportPaths.CharacterNameToBlueprintAssetPath(CharacterName))))
		{
			FoundMetaHumans.Emplace(CharacterName, ImportPaths.DestinationMetaHumansFilePath);
		}
	}
	return FoundMetaHumans;
}

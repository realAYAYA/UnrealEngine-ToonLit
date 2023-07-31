// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Utilities/VersionInfoHandler.h"
#include "Widgets/SWindow.h"

struct FDHIData
{
    FString CharacterPath;
    FString CommonPath;
    FString RootPath;
    FString CharacterName;
};

class FImportDHI
{
private:
    FImportDHI() = default;
    static TSharedPtr<FImportDHI> ImportDHIInst;
    TSharedPtr<FDHIData> ParseDHIData(TSharedPtr<FJsonObject> AssetImportJson);

	bool MHInLevel(const FString& CharacterBPPath);

    TArray<FString> CheckVersionCompatibilty();
	FString ReadVersionFile(const FString& FilePath);

	TMap<FString, float> ParseVersionInfo(const FString& VersionInfoString);
	
	TMap<FString, TArray<FString>> AssetsToUpdate(TMap<FString, float> SourceVersionInfo, TMap<FString, float> ProjectVersionInfo);
	TMap<FString, TArray<FString>> AssetsToUpdate(TMap<FString, float> SourceVersionInfo);
	TMap<FString, TArray<FString>> AssetsToUpdate(const FString& SourceCommonDirectory);

	void CopyCommonFiles(TArray<FString> AssetsList, const FString& CommonPath,  bool bExistingAssets = false);
	
	TMap<FString, FString> ParseModifiedInfo(const FString& ModifiedInfoFilePath);
	TArray<FString> GetModifiedFileList(TMap<FString, FString> AssetsImportInfo, TArray<FString> FilesToUpdate);

	void WriteModificationFile(FImportTimeData& ImportTimeData);
	FImportTimeData ReadModificationData();

	const FString ModificationInfoFile = TEXT("MHAssetModified.txt");
	TArray<FString> MHCharactersInLevel();
	TArray<FString> MetahumansInLevel(TArray<FString> CharactersInProject);

	void UpdateImportTimeData(TArray<FString> AssetsAdded, FImportTimeData& CurrentImportTimestamp);

	TArray<FString> ListCharactersInProject();
	TArray<FString> CharactersOpenEditors();
	void CloseAssetEditors(TArray<FString> CharactersOpenInEditors);

	void RecompileAllCharacters();

	//Dialog implementation, add headers if going with this route.
	TSharedPtr<class SOVerwriteDialog> DialogWidget;
	TSharedPtr<SWindow> DialogWindow;
	
	void ShowDialog(const FString& SourcePath, const FString& DestinationPath,  const FString& Message, const FString & Footer);

public:
    static TSharedPtr<FImportDHI> Get();
    void ImportAsset(TSharedPtr<FJsonObject> AssetImportJson);

private:
    static void EnableMissingPlugins();
};

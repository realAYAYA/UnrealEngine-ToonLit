// Copyright Epic Games, Inc. All Rights Reserved.
#include "AssetImporters/DHIImport.h"
#include "MSAssetImportData.h"
#include "Utilities/MiscUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/ScopedSlowTask.h"
#include "EditorAssetLibrary.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Interfaces/IProjectManager.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Internationalization/Text.h"
#include "UObject/Object.h"

#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "AssetRegistry/AssetRegistryHelpers.h"

#include "Engine/AssetManager.h"
#include "UObject/Linker.h"
#include "PackageTools.h"

#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/PackageReload.h"
#include "Misc/DateTime.h"

#include "Editor/EditorEngine.h"
#include "Editor.h"

#include "Kismet/GameplayStatics.h"
#include "JsonObjectConverter.h"

#include "Engine/World.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "HAL/PlatformApplicationMisc.h"

#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SComboBox.h"
#include "Input/Reply.h"
#include "Widgets/Layout/SScrollBox.h"

#define LOCTEXT_NAMESPACE "DHIImport"






//Dialog implementation
class SOVerwriteDialog : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SOVerwriteDialog)
	{}
	/** Window in which this widget resides */
	SLATE_ATTRIBUTE(TSharedPtr<SWindow>, ParentWindow)
	SLATE_ARGUMENT(FString, SourcePath)
	SLATE_ARGUMENT(FString, DestinationPath)
	SLATE_ARGUMENT(FString, Message)
	SLATE_ARGUMENT(FString, Footer)
	
		
	SLATE_END_ARGS()
		
	void Construct(const FArguments& InArgs)
	{
		ParentWindow = InArgs._ParentWindow.Get();

		

		this->ChildSlot
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(20.0f, 40.0f, 20.0f, 10.0f)
			[

				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(FText::FromString(InArgs._Message))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(20.0f, 4.0f, 8.0f, 4.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.FillWidth(.75f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Text(FText::FromString(InArgs._SourcePath))
				]
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.FillWidth(.25f)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Copy Path")))
					.HAlign(HAlign_Center)
					.OnClicked(this, &SOVerwriteDialog::HandlePathCopy, InArgs._SourcePath)

				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(20.0f, 4.0f, 8.0f, 4.0f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(FText::FromString(TEXT("TO:")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(20.0f, 4.0f, 8.0f, 4.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.FillWidth(.75f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Text(FText::FromString(InArgs._DestinationPath))
				]
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.FillWidth(.25f)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Copy Path")))
					.HAlign(HAlign_Center)
					.OnClicked(this, &SOVerwriteDialog::HandlePathCopy, InArgs._DestinationPath)
				]
			]

			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(20.0f, 2.0f, 8.0f, 4.0f)
				[

					SNew(STextBlock)
					.AutoWrapText(true)
				.Text(FText::FromString(TEXT("then replace the existing files")))
				]

			/*+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				.Padding(2.0f)
				[
					SNew(SSeparator)
					.Orientation(Orient_Horizontal)
				]*/

			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(20.0f, 4.0f, 8.0f, 4.0f)
				[

					SNew(STextBlock)
					.AutoWrapText(true)
				.Text(FText::FromString(InArgs._Footer))
				]

			//Ok Button
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.0f, 4.0f, 8.0f, 4.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					SNew(STextBlock)
				]
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.FillWidth(.25f)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Ok")))
					.HAlign(HAlign_Center)
					.OnClicked(this, &SOVerwriteDialog::HandleOK)
				]
			]
		]
        ];
	}
	
private:
	TSharedPtr<SWindow>	ParentWindow;

	FReply HandlePathCopy(FString InPath)
	{
		
		FPlatformApplicationMisc::ClipboardCopy(*InPath);
		return FReply::Handled();
	}

	FReply HandleOK()
	{
		ParentWindow->RequestDestroyWindow();
		return FReply::Handled();
	}
};

TSharedPtr<FImportDHI> FImportDHI::ImportDHIInst;

TSharedPtr<FDHIData> FImportDHI::ParseDHIData(TSharedPtr<FJsonObject> AssetImportJson)
{
    TSharedPtr<FDHIData> DHIImportData = MakeShareable(new FDHIData);
    DHIImportData->CharacterPath = AssetImportJson->GetStringField(TEXT("characterPath"));
    DHIImportData->CharacterName = AssetImportJson->GetStringField(TEXT("folderName"));
    DHIImportData->CommonPath = AssetImportJson->GetStringField(TEXT("commonPath"));
    DHIImportData->CharacterPath = FPaths::Combine(DHIImportData->CharacterPath, DHIImportData->CharacterName);
    return DHIImportData;
}

TSharedPtr<FImportDHI> FImportDHI::Get()
{
    if (!ImportDHIInst.IsValid())
    {
        ImportDHIInst = MakeShareable(new FImportDHI);
    }
    return ImportDHIInst;
}

void FImportDHI::ImportAsset(TSharedPtr<FJsonObject> AssetImportJson)
{	
	
	
	
	//For future
	/*TArray<FString> CharactersInLevel = MHCharactersInLevel();

	if (CharactersInLevel.Num() > 0)
	{
		UWorld* CurrentWorld = GEngine->GetWorldContexts()[0].World();
		FString MapName = CurrentWorld->GetMapName();
		
		FString MHinLevelMessage = FString::Printf(TEXT("There are open MetaHumans in level %s. Please close the level and then try again."), *MapName);
		
		EAppReturnType::Type ContinueImport = FMessageDialog::Open(EAppMsgType::Ok, FText(FText::FromString(MHinLevelMessage)));
		return;
	}*/


	//TArray<FString> MetahumansOpen = CharactersOpenEditors();
	//if (MetahumansOpen.Num() > 0)
	//{
	//	FString MHOpenInEditorMessage = FString::Printf(TEXT("There are open MetaHumans in the asset editor. The asset editor will be automatically closed before importing the new MetaHuman and any unsaved changes will be lost."));

	//	EAppReturnType::Type ContinueImport = FMessageDialog::Open(EAppMsgType::OkCancel, FText(FText::FromString(MHOpenInEditorMessage)));
	//	
	//	if (ContinueImport != EAppReturnType::Ok)
	//	{
	//		return;
	//	}


	//	//Just in case to close the editors		
	//	CloseAssetEditors(MetahumansOpen);
	//	

	//}

	

	TSharedPtr<FDHIData> CharacterSourceData = ParseDHIData(AssetImportJson);
	FString BPName = TEXT("BP_") + CharacterSourceData->CharacterName;
	BPName += TEXT(".") + BPName;
	FString BPPath = FPaths::Combine(TEXT("/Game/MetaHumans/"), CharacterSourceData->CharacterName, BPName);

	FString DestinationMetahumanPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("MetaHumans")));
	FPaths::NormalizeDirectoryName(DestinationMetahumanPath);
	FString SourceMetahumanPath = FPaths::GetPath( CharacterSourceData->CharacterPath);
	FPaths::NormalizeDirectoryName(SourceMetahumanPath);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry");


	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	bool bShouldUpdateAll = false;

	FString CommonDestinationPath = FPaths::ProjectContentDir();
	FString MetaHumansRoot = FPaths::Combine(CommonDestinationPath, TEXT("MetaHumans"));

	FString RootFolder = TEXT("/Game/MetaHumans");
	FString CommonFolder = FPaths::Combine(RootFolder, TEXT("Common"));
	FString CharacterName = CharacterSourceData->CharacterName;
	FString CharacterDestination = FPaths::Combine(MetaHumansRoot, CharacterName);

	bool bIsNewCharacter = true;

	FString UpgradeFooter = TEXT("Note: Doing so may result in old MetaHumans to appear broken until they are redownloaded and files overwritten in the current project");
	
	FString CharacterOverwriteMessage = TEXT("The MetaHuman you are trying to import already exists in this project. Please close Unreal Engine and then drag and drop the MetaHumans folder manually\nFROM:");
	//FString CharacterOverwriteMessage = FString::Printf(TEXT("The MetaHuman you are trying to import already exists in this project. Please close UE and overwrite the MetaHumans folder manually with %s and the Common Files\nFROM:"), *CharacterName);
	FString CharacterOverwriteFooter = TEXT("Note: Replacing files may result in old MetaHumans to appear broken until they are redownloaded and files overwritten in the current project");


	FString CommonOverwriteMessage = TEXT("There are some changes to the common assets in this project. Please close the project and copy and overwrite");
	
	

	
	
	const FString SourceVersionFilePath = FPaths::Combine(FPaths::GetPath( CharacterSourceData->CharacterPath), TEXT("MHAssetVersions.txt"));
	TArray<FString> AssetsToUpdateList;
	TMap<FString, float> SourceAssetsVersionInfo;
	TMap<FString, TArray<FString>> AssetsStatus;
	const FString ProjectAssetsVersionPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("MetaHumans"), TEXT("MHAssetVersions.txt")));

	if (PlatformFile.FileExists(*SourceVersionFilePath))
	{
		const FString SourceVersionString = ReadVersionFile(SourceVersionFilePath);

		SourceAssetsVersionInfo = ParseVersionInfo(SourceVersionString);


		if (!PlatformFile.FileExists(*ProjectAssetsVersionPath))
		{
			bShouldUpdateAll = true;

			AssetsStatus = AssetsToUpdate(SourceAssetsVersionInfo);
			AssetsToUpdateList = AssetsStatus["Update"];

		}

		else
		{
			const FString ProjectVersionString = ReadVersionFile(ProjectAssetsVersionPath);
			TMap<FString, float> ProjectAssetsVersionInfo = ParseVersionInfo(ProjectVersionString);
			AssetsStatus = AssetsToUpdate(SourceAssetsVersionInfo, ProjectAssetsVersionInfo);

			AssetsToUpdateList = AssetsStatus["Update"];
			//Hanlde case where asset exists but its version info does
		}
	}
	// Case where source MH character doesn't have the version info.
	else
	{
		AssetsStatus = AssetsToUpdate(CharacterSourceData->CommonPath);
		AssetsToUpdateList = AssetsStatus["Update"];
	}

	//Check locally modified files
	const FString ModifiedInfoFilePath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("MetaHumans"), ModificationInfoFile));
	TArray<FString> LocallyModifiedFiles;
	bool bModificationInfoExists = false;
	TArray<FString> ModifiedFiles;
	
	if (PlatformFile.FileExists(*ModifiedInfoFilePath))
	{
		bModificationInfoExists = true;
		TMap<FString, FString> AssetsImportInfo = ParseModifiedInfo(ModifiedInfoFilePath);

		ModifiedFiles = GetModifiedFileList(AssetsImportInfo, AssetsToUpdateList);
	}


	// Get confirmation from the user to update the existing common assets
	if (AssetsToUpdateList.Num() > 0) 
	{
		FString AssetsUpdateMessage = TEXT("There are some changes to the Common assets in this project. If you continue importing this MetaHuman, your local changes to the following assets may be overwritten.\nContinue anyway?\n");

		for (FString AssetToUpdate : AssetsToUpdateList)
		{
			AssetsUpdateMessage += AssetToUpdate + TEXT("\n");
		}

		EAppReturnType::Type UpdateAssetsDialog = FMessageDialog::Open(EAppMsgType::YesNo, FText(FText::FromString(AssetsUpdateMessage)));
		if (UpdateAssetsDialog != EAppReturnType::Yes)
			return;
	}


	// Not part of ^
	/*if (AssetsToUpdateList.Num() > 0 ) 
	{
		ShowDialog(SourceMetahumanPath, DestinationMetahumanPath, CommonOverwriteMessage);
		return;
	}*/

	// Found files that were modified locally, inform user about these before continui
	//if (ModifiedFiles.Num() > 0)
	//{
	//	FString AssetsModifiedMessage = TEXT("There are some changes to the Common assets in this project. If you continue importing this MetaHuman, your local changes to the following assets may be overwritten.\nContinue anyway?\n");

	//	for (FString AssetToModify : ModifiedFiles)
	//	{
	//		AssetsModifiedMessage += AssetToModify + TEXT("\n");
	//	}

	//	EAppReturnType::Type UpdateAssetsDialog = FMessageDialog::Open(EAppMsgType::YesNo, FText(FText::FromString(AssetsModifiedMessage)));
	//	if (UpdateAssetsDialog == EAppReturnType::No)
	//	{
	//		for (FString AssetModified : ModifiedFiles)
	//		{
	//			if (AssetsStatus["Update"].Contains(AssetModified))
	//			{
	//				AssetsStatus["Update"].Remove(AssetModified);
	//			}
	//		}
	//	}
	//	else if (UpdateAssetsDialog == EAppReturnType::Yes)
	//	{
	//		// We need to catch the close dialog event and remove this condition
	//		// Do nothing
	//	}
	//	else
	//	{
	//		return;
	//	}
	//		
	//}


    bool bIsCharacterUE5 = false;
	
    if (PlatformFile.FileExists(*FPaths::Combine(CharacterSourceData->CharacterPath, TEXT("VersionInfo.txt"))))
    {
        bIsCharacterUE5 = true;
    }

    TArray<FString> IncompatibleCharacters = CheckVersionCompatibilty();

	//FString ProjectUpgradeMessage = TEXT("It looks like this project has UE4 MetaHumans in it, which are incompatible with UE5 MetaHumans and may result in some errors when they’re together in the same project. We highly recommend that you update all MetaHumans to be UE5 MetaHumans.\nTo continue importing this MetaHuman, please close the project and copy the following files from:");
	FString ProjectUpgradeMessage = TEXT("This project has following UE4 MetaHumans in it: ");
	


	for (FString IncompatibleCharacter : IncompatibleCharacters)
	{
		ProjectUpgradeMessage += TEXT("\n") + IncompatibleCharacter.Replace(TEXT("/"), TEXT(""));
	}

	ProjectUpgradeMessage += TEXT("\nthese are incompatible with UE5 MetaHumans and may result in some errors when they’re together in the same project. We highly recommend that you update all MetaHumans to be UE5 MetaHumans.\nTo continue importing this MetaHuman, please close the project and copy the following files from:");

	// This is where we decide based on the number of UE4 characters whether to show dialog or not
	if (IncompatibleCharacters.Num() > 0)
	{
		ShowDialog(SourceMetahumanPath, DestinationMetahumanPath, ProjectUpgradeMessage, UpgradeFooter);
		return;
	}


	// TODO: Removed
	if (PlatformFile.DirectoryExists(*CharacterDestination))
	{
		bIsNewCharacter = false;
		if (MHInLevel(BPPath))
		{
			EAppReturnType::Type ContinueImport = FMessageDialog::Open(EAppMsgType::Ok, FText(FText::FromString("This MetaHuman already exists in this level. In order to continue, you will need to close the level and import the MetaHuman into a new or different level.")));
			return;
		}
	}


	// TODO: Partially Removed
	if (PlatformFile.DirectoryExists(*CharacterDestination))
	{
		bIsNewCharacter = false;
		
	// 	EAppReturnType::Type ContinueImport = FMessageDialog::Open(EAppMsgType::YesNo, FText(FText::FromString("The MetaHuman you are trying to import already exists in this project. Do you want to overwrite them?")));
	// 	if (ContinueImport != EAppReturnType::Yes)
	// 		return;
	}

	/*if (PlatformFile.DirectoryExists(*CharacterDestination))
	{
		bIsNewCharacter = false;

		EAppReturnType::Type ContinueImport = FMessageDialog::Open(EAppMsgType::Ok, FText(FText::FromString("Note: Hair cards will be missing until you restart Unreal Engine.")));
		


	}*/


    /*if (IncompatibleCharacters.Num() > 0 && bIsCharacterUE5)
    {
        FString CharacterOutputString = TEXT("");
        for (FString CharacterFolderName : IncompatibleCharacters)
        {
            CharacterOutputString += TEXT("\n") + CharacterFolderName;
        }

        FString BaseMessage = TEXT("This project has UE4 MetaHumans, which are incompatible with UE5 MetaHumans in the same project. Adding a UE5 MetaHuman can result in breaking the existing UE4 MetaHumans and you will need to replace each one with a UE5 version. Continue anyway?");

        EAppReturnType::Type ContinueImport = FMessageDialog::Open(EAppMsgType::YesNo, FText(FText::FromString(BaseMessage)));
        if (ContinueImport != EAppReturnType::Yes)
            return;
    }*/

    TArray<FString> AssetsBasePath;
    AssetsBasePath.Add(TEXT("/Game/MetaHumans"));

    CommonDestinationPath = FPaths::Combine(MetaHumansRoot, TEXT("Common"));

	

	// NOTE: the RigLogic plugin (and maybe others) must be loaded and added to the project before loading the asset
	// otherwise we get rid of the RigLogic nodes, resulting in leaving the asset in an undefined state. In the context
	// of ControlRig assets, graphs will remove the RigLogic nodes if the plugin is not enabled because the
	// FRigUnit_RigLogic_Data won't be available
	EnableMissingPlugins();

    TArray<FString> SourceCommonFiles;
    PlatformFile.FindFilesRecursively(SourceCommonFiles, *CharacterSourceData->CommonPath, NULL);
    //TArray<FString> ExistingAssets = UEditorAssetLibrary::ListAssets(TEXT("/Game/MetaHumans/Common"));

    FString ProjectCommonPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("MetaHumans"), TEXT("Common")));
    FString SourceCommonPath = CharacterSourceData->CommonPath;
    FPaths::NormalizeDirectoryName(SourceCommonPath);

    TArray<FString> ProjectCommonFiles;
    PlatformFile.FindFilesRecursively(ProjectCommonFiles, *ProjectCommonPath, TEXT("uasset"));
    TArray<FString> ExistingAssetStrippedPaths;

    for (FString ProjectCommonFile : ProjectCommonFiles)
    {
        ExistingAssetStrippedPaths.Add(ProjectCommonFile.Replace(*ProjectCommonPath, TEXT("")));
    }

	PlatformFile.CreateDirectoryTree(*MetaHumansRoot);

    int8 CommonFilesCount = 0;

    TArray<FString> FilesToCopy;
    
	// Update existing common assets
	CopyCommonFiles(AssetsStatus["Update"], CharacterSourceData->CommonPath, true);

	// Add new common assets
	CopyCommonFiles(AssetsStatus["Add"], CharacterSourceData->CommonPath, false);
	
	// Add new character
	if (bIsNewCharacter)
	{
		
		AssetRegistryModule.Get().ScanPathsSynchronous(AssetsBasePath, true);

		PlatformFile.CreateDirectoryTree(*CharacterDestination);
		AssetsBasePath.Add("/Game/MetaHumans/" + CharacterSourceData->CharacterName);

		AssetRegistryModule.Get().ScanPathsSynchronous(AssetsBasePath, true);

		FString CharacterCopyMsg = TEXT("Importing : ") + CharacterName;
		FText CharacterCopyMsgDialogMessage = FText::FromString(CharacterCopyMsg);
		FScopedSlowTask CharacterLoadprogress(1.0f, CharacterCopyMsgDialogMessage, true);
		CharacterLoadprogress.MakeDialog();
		CharacterLoadprogress.EnterProgressFrame(1.0f);
		PlatformFile.CopyDirectoryTree(*CharacterDestination, *CharacterSourceData->CharacterPath, true);
	}

	//Overwrite existing character
	else {
		
		
		TArray<FString> SourceCharacterFiles;
		PlatformFile.FindFilesRecursively(SourceCharacterFiles, *CharacterSourceData->CharacterPath, NULL);
		
		TArray<UPackage*> PackagesToReload;

		TArray<UPackage*> BPsToReload;

		FString CharacterCopyMsg = TEXT("Re-Importing : ") + CharacterName;
		FText CharacterCopyMsgDialogMessage = FText::FromString(CharacterCopyMsg);
		FScopedSlowTask CharacterLoadprogress((float)SourceCharacterFiles.Num() * 3.0, CharacterCopyMsgDialogMessage);
		CharacterLoadprogress.MakeDialog();
		

		for (FString FileToCopy : SourceCharacterFiles)
		{
			FString NormalizedSourceFile = FileToCopy;			
			FString StrippedSourcePath = NormalizedSourceFile.Replace(*CharacterSourceData->CharacterPath, TEXT(""));



			FString DestinationFilePath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Metahumans"), CharacterSourceData->CharacterName, StrippedSourcePath));

			// Each load consists of potentially 3 "units" of work: a load, an optional package reload and an optional
			// BP reload. Start off assuming that the load will count as all three units of work and reduce that as we
			// find out which optional stages are required.
			float WorkDone = 3.0f;

			if (PlatformFile.FileExists(*DestinationFilePath))
			{
				
				FString AssetName = FPaths::GetBaseFilename(DestinationFilePath);

				//Remove the project path till content folder
				FString StrippedFilePath = DestinationFilePath.Replace(
					*FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()), 
					TEXT("")
				);

				StrippedFilePath = StrippedFilePath.Replace(TEXT(".uasset"), TEXT(""));
				StrippedFilePath += TEXT(".") + AssetName;

				FString AssetPackagePath = FPaths::Combine(TEXT("/Game"), StrippedFilePath);

				FAssetData GameAssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(AssetPackagePath));
				
				if (GameAssetData.IsAssetLoaded())
				{					

					UObject* ItemObject = GameAssetData.GetAsset();

					if (!ItemObject->GetPackage()->IsFullyLoaded())
					{
						FlushAsyncLoading();
						ItemObject->GetPackage()->FullyLoad();
					}

					if (Cast<UBlueprint>(ItemObject) != nullptr)
					{
						BPsToReload.Add(ItemObject->GetPackage());
						WorkDone -= 1.0;
					}

					ResetLoaders(ItemObject->GetPackage());
					
					PackagesToReload.Add(ItemObject->GetPackage());
					WorkDone -= 1.0;

				}
				
				
				
			}
			CharacterLoadprogress.EnterProgressFrame(WorkDone);
			IFileManager::Get().Copy(*DestinationFilePath, *FileToCopy, true, true);	
			
			

		}	

		SortPackagesForReload(PackagesToReload);
		UPackageTools::ReloadPackages(PackagesToReload);
		/*for (UPackage* CommonPackage : PackagesToReload)
		{

			TArray<UPackage*> PackagesList;
			PackagesList.Add(CommonPackage);
			
			CharacterLoadprogress.EnterProgressFrame();
			UPackageTools::ReloadPackages(PackagesList);

		}*/

		for (auto Package : BPsToReload)
		{
			
			CharacterLoadprogress.EnterProgressFrame();
			UObject* Obj = Package->FindAssetInPackage();
			if (UBlueprint* BPObject = Cast<UBlueprint>(Obj))
			{
				FKismetEditorUtilities::CompileBlueprint(BPObject, EBlueprintCompileOptions::SkipGarbageCollection);
				BPObject->PreEditChange(nullptr);
				BPObject->PostEditChange();
			}
		}

	}
	
	// Write all the incoming assets to the import time file info along wiht the current timestamp.
	if(!bModificationInfoExists) 
	{
		TArray<FString> FileList = AssetsStatus["Update"] ;
		FileList += AssetsStatus["Add"];

		FImportTimeData AssetsImportTimeData;
		FString CurrentTime = FDateTime::Now().ToString();

		for (FString AssetToWrite : FileList)
		{
			FAssetImportTime AssetImportInfo;
			AssetImportInfo.path = AssetToWrite;
			AssetImportInfo.time = CurrentTime;
			AssetsImportTimeData.assets.Add(AssetImportInfo);
		}

		WriteModificationFile(AssetsImportTimeData);
	}
	// Write info about updated and newly added information
	else 
	{
		FImportTimeData CurrentImportTimestamp = ReadModificationData();

		FString CurrentImportTime = FDateTime::Now().ToString();

		for (FAssetImportTime& AssetModificationInfo : CurrentImportTimestamp.assets)
		{
			if (AssetsStatus["Update"].Contains(AssetModificationInfo.path))
			{
				AssetModificationInfo.time = CurrentImportTime;
			}
		}

		for (FString AssetAdded : AssetsStatus["Add"])
		{
			FAssetImportTime AssetImportData;
			AssetImportData.path = AssetAdded;
			AssetImportData.time = CurrentImportTime;
			CurrentImportTimestamp.assets.Add(AssetImportData);
		}
		
		WriteModificationFile(CurrentImportTimestamp);
	}

    AssetRegistryModule.Get().ScanPathsSynchronous(AssetsBasePath, true);

	if (bShouldUpdateAll)
	{
		PlatformFile.CopyFile(*ProjectAssetsVersionPath, *SourceVersionFilePath);
	}

    FAssetData CharacterAssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(BPPath));

    // As part of our discussion to fix this, here was the explanation about why the asset has to be loaded:
    // "In UE4 we came across this issue where even if we did call the syncfolder on content browser, it would still
    // not show the character in content browser. So a workaround to make the character appear was to just load
    // the character."
	FString CharacterLoadMsg = TEXT("Loading : ") + CharacterName;
	FText CharacterLoadMsgDialogMessage = FText::FromString(CharacterLoadMsg);
	FScopedSlowTask CharacterMLoadprogress(1.0f, CharacterLoadMsgDialogMessage, true);
	CharacterMLoadprogress.MakeDialog();
	CharacterMLoadprogress.EnterProgressFrame(1.0f);
    UObject *CharacterObject = CharacterAssetData.GetAsset();

	if (!bShouldUpdateAll)
	{
		UpdateMHVersionInfo(AssetsStatus, SourceAssetsVersionInfo);
	}
	
    AssetUtils::FocusOnSelected(CharacterDestination);
}

void FImportDHI::EnableMissingPlugins()
{
    // TODO we should find a way to retrieve the required plugins from the metadata as RigLogic might not be the only one
    static const TArray<FString> NeededPluginNames({TEXT("RigLogic")});

    IPluginManager &PluginManager = IPluginManager::Get();
    IProjectManager &ProjectManager = IProjectManager::Get();

    for (const FString &PluginName : NeededPluginNames)
    {
        TSharedPtr<IPlugin> NeededPlugin = PluginManager.FindPlugin(PluginName);
        if (NeededPlugin.IsValid() && !NeededPlugin->IsEnabled())
        {
            FText FailMessage;
            bool bPluginEnabled = ProjectManager.SetPluginEnabled(NeededPlugin->GetName(), true, FailMessage);

            if (bPluginEnabled && ProjectManager.IsCurrentProjectDirty())
            {
                bPluginEnabled = ProjectManager.SaveCurrentProjectToDisk(FailMessage);
            }

            if (bPluginEnabled)
            {
                PluginManager.MountNewlyCreatedPlugin(NeededPlugin->GetName());
            }
            else
            {
                FMessageDialog::Open(EAppMsgType::Ok, FailMessage);
            }
        }
    }
}

// Check if the project contains any UE4 metahuman characters
TArray<FString> FImportDHI::CheckVersionCompatibilty()
{
    TArray<FString> IncompatibleCharacters;
    TArray<FString> DirectoryList = UEditorAssetLibrary::ListAssets(TEXT("/Game/Metahumans"), false, true);

    FString ProjectMetahumanPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("MetaHumans")));

    IPlatformFile &PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    for (FString FoundAsset : DirectoryList)
    {
        if (UEditorAssetLibrary::DoesDirectoryExist(FoundAsset))
        {
            FString CharacterName = FoundAsset.Replace(TEXT("/Game/Metahumans/"), TEXT(""));

            if (CharacterName == TEXT("Common") || CharacterName == TEXT("Common/"))
            {
                continue;
            }

            FString VersionFilePath = FPaths::Combine(ProjectMetahumanPath, CharacterName, TEXT("VersionInfo.txt"));

			FString BPName = TEXT("BP_") + CharacterName.Replace(TEXT("/"), TEXT(""));
			BPName += TEXT(".") + BPName;
			FString BPPath = FPaths::Combine(TEXT("/Game/MetaHumans/"), CharacterName, BPName);
			

            if (!PlatformFile.FileExists(*VersionFilePath) && UEditorAssetLibrary::DoesAssetExist(BPPath))
            {
                IncompatibleCharacters.Add(CharacterName);
            }
        }
    }

    return IncompatibleCharacters;
}

FString FImportDHI::ReadVersionFile(const FString& FilePath)
{
	FString VersionInfoString;
	FFileHelper::LoadFileToString(VersionInfoString, *FilePath);
	return VersionInfoString;
}

TMap<FString, float> FImportDHI::ParseVersionInfo(const FString& VersionInfoString)
{
	TMap<FString, float> VersionInfo;
	TSharedPtr<FJsonObject> AssetsVersionInfoObject = DeserializeJson(VersionInfoString);
	TArray<TSharedPtr<FJsonValue> > AssetsVersionInfoArray = AssetsVersionInfoObject->GetArrayField(TEXT("assets"));

	for (TSharedPtr<FJsonValue>  AssetVersionInfoObject : AssetsVersionInfoArray)
	{
		FString AssetPath = AssetVersionInfoObject->AsObject()->GetStringField(TEXT("path"));
		float AssetVersion = FCString::Atof(*AssetVersionInfoObject->AsObject()->GetStringField(TEXT("version")));
		VersionInfo.Add(AssetPath, AssetVersion);		
	}

	return VersionInfo;
}

TMap<FString, TArray<FString>> FImportDHI::AssetsToUpdate(TMap<FString, float> SourceVersionInfo, TMap<FString, float> ProjectVersionInfo)
{
	TMap<FString, TArray<FString>> AssetsList;
	TArray<FString> AssetsUpdateList;

	TArray<FString> AssetsSkipList;
	TArray<FString> NewAssetsList;

	for (auto& SourceAssetInfo : SourceVersionInfo)
	{
		if (ProjectVersionInfo.Contains(SourceAssetInfo.Key))
		{
			if (SourceAssetInfo.Value > ProjectVersionInfo[SourceAssetInfo.Key])
			{
				AssetsUpdateList.Add(SourceAssetInfo.Key);
			}
			else 
			{
				AssetsSkipList.Add(SourceAssetInfo.Key);
			}
		}

		else 
		{
			NewAssetsList.Add(SourceAssetInfo.Key);
		}
	}

	AssetsList.Add(TEXT("Update"), AssetsUpdateList);
	AssetsList.Add(TEXT("Skip"), AssetsSkipList);
	AssetsList.Add(TEXT("Add"), NewAssetsList);

	return AssetsList;
}

TMap<FString, TArray<FString>> FImportDHI::AssetsToUpdate(TMap<FString, float> SourceVersionInfo)
{
	TMap<FString, TArray<FString>> AssetsStatus;
	TArray<FString> AssetsUpdateList;

	TArray<FString> AssetsSkipList;
	TArray<FString> NewAssetsList;

	FString ProjectCommonPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	for (auto& SourceAssetInfo : SourceVersionInfo)
	{
		FString FilePath = FPaths::Combine(ProjectCommonPath, SourceAssetInfo.Key);

		if (!SourceAssetInfo.Key.StartsWith(TEXT("MetaHumans/Common/")))
		{
			continue;
		}

		if (PlatformFile.FileExists(*FilePath))
		{
			AssetsUpdateList.Add(SourceAssetInfo.Key);
		}
		else 
		{
			NewAssetsList.Add(SourceAssetInfo.Key);
		}
	}

	AssetsStatus.Add(TEXT("Update"), AssetsUpdateList);	
	AssetsStatus.Add(TEXT("Add"), NewAssetsList);

	return AssetsStatus;
}

TMap<FString, TArray<FString>> FImportDHI::AssetsToUpdate(const FString& SourceCommonDirectory)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	TMap<FString, TArray<FString>> AssetsStatus;
	TArray<FString> AssetsUpdateList;

	TArray<FString> AssetsSkipList;
	TArray<FString> NewAssetsList;

	FString DestinationCommonDirectory = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(
			FPaths::ProjectContentDir(),
			TEXT("MetaHumans"),
			TEXT("Common")
		)
	);

	TArray<FString> SourceCommonFiles;
	PlatformFile.FindFilesRecursively(SourceCommonFiles, *SourceCommonDirectory, NULL);

	TArray<FString> ProjectCommonFiles;
	PlatformFile.FindFilesRecursively(ProjectCommonFiles, *DestinationCommonDirectory, NULL);

	FString StrippedSourceDirectory = FPaths::GetPath(
		FPaths::GetPath(
			SourceCommonDirectory
		)
	);

	for (FString SourceCommonFile : SourceCommonFiles)
	{
		FString StrippedSourceFile = SourceCommonFile.Replace(*SourceCommonDirectory, TEXT(""));
		FString FormattedFilePath = SourceCommonFile.Replace(*StrippedSourceDirectory, TEXT(""));

		FString ProjectFilePath = FPaths::Combine(
			DestinationCommonDirectory,
			StrippedSourceFile
		);
		FPaths::NormalizeDirectoryName(FormattedFilePath);
		if (FormattedFilePath.StartsWith(TEXT("/")))
		{
			FormattedFilePath.RemoveAt(0,1);
		}


		if (PlatformFile.FileExists(*ProjectFilePath))
		{
			AssetsUpdateList.Add(FormattedFilePath);
		}
		else
		{
			NewAssetsList.Add(FormattedFilePath);
		}
	}

	AssetsStatus.Add(TEXT("Update"), AssetsUpdateList);
	AssetsStatus.Add(TEXT("Add"), NewAssetsList);

	return AssetsStatus;
}

void FImportDHI::CopyCommonFiles(TArray<FString> AssetsList, const FString& CommonPath, bool bExistingAssets)
{
	int32 CommonFilesCount = AssetsList.Num();
	
	FString CommonCopyMsg;
	if (bExistingAssets)
	{
		CommonCopyMsg = TEXT("Overwriting existing Common Assets.");
	}
	else
	{
		CommonCopyMsg = TEXT("Importing Common Assets.");
	}
	
	FText CommonCopyMsgDialogMessage = FText::FromString(CommonCopyMsg);

	float WorkMultiplier = 1.0;
	if (bExistingAssets)
	{
		WorkMultiplier = 3.0;
	}

	FScopedSlowTask AssetLoadprogress((float)CommonFilesCount * WorkMultiplier, CommonCopyMsgDialogMessage, true);
	AssetLoadprogress.MakeDialog();

	TArray<UPackage*> PackagesToReload;
	TArray<UPackage*> BPsToReload;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry");

	for (FString AssetToUpdate : AssetsList)
	{
		float WorkDone = 2.0f;
		// This removes the initial path from  eg "MetaHumans/Common/Common/MetaHuman_ControlRig.uasset"
		FString StrippedCommonFilePath = AssetToUpdate.Replace(TEXT("MetaHumans/Common"), TEXT(""));

		FString SourceFilePath = FPaths::Combine(CommonPath, StrippedCommonFilePath);

		FString DestinationFilePath = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(
				FPaths::ProjectContentDir(), 
				AssetToUpdate
			)
		);

		if (bExistingAssets)
		{
			// Getting the asset name to create the package name eg get MetaHuman_ControlRig
			FString AssetName = FPaths::GetBaseFilename(DestinationFilePath);

			FString FileExtension = FPaths::GetExtension(DestinationFilePath);

			if (FileExtension == TEXT("uasset"))
			{
				FString AssetRelativePath = AssetToUpdate.Replace(TEXT(".uasset"), TEXT(""));
				// Create a path like /Game/Metahumans/MetaHumans/Common/Common/MetaHuman_ControlRig.MetaHuman_ControlRig
				FString GameAssetPath = TEXT("/Game/") + AssetRelativePath + TEXT(".") + AssetName;

				FAssetData GameAssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(GameAssetPath));

				if (GameAssetData.IsAssetLoaded())
				{
					UObject* ItemObject = GameAssetData.GetAsset();
					
					if (!ItemObject->GetPackage()->IsFullyLoaded())
					{
						FlushAsyncLoading();
						ItemObject->GetPackage()->FullyLoad();
					}

					if (Cast<UBlueprint>(ItemObject) != nullptr)
					{
						BPsToReload.Add(ItemObject->GetPackage());
					}

					ResetLoaders(ItemObject->GetPackage());					
					
					PackagesToReload.Add(ItemObject->GetPackage());
					//WorkDone -= 1;
				}
			}
		}
		
		AssetLoadprogress.EnterProgressFrame(1.0);
		IFileManager::Get().Copy(*DestinationFilePath, *SourceFilePath, true, true);
	}

	if (bExistingAssets)
	{
		SortPackagesForReload(PackagesToReload);
		//UPackageTools::ReloadPackages(PackagesToReload);

		for (UPackage* CommonPackage : PackagesToReload)
		{
			TArray<UPackage*> PackagesList;
			PackagesList.Add(CommonPackage);
			AssetLoadprogress.EnterProgressFrame(2.0);
			UPackageTools::ReloadPackages(PackagesList);
		}
	}
}

void FImportDHI::WriteModificationFile(FImportTimeData& ImportTimeData)
{
	const FString AssetsModificationInfoPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("MetaHumans"), ModificationInfoFile));

	FString ModificationInfoString;
	
	FString OutputData;
	FJsonObjectConverter::UStructToJsonObjectString(ImportTimeData, OutputData);

	FFileHelper::SaveStringToFile(OutputData, *AssetsModificationInfoPath);
}

//Redundant function and this should replace ParseModifiedInfo.
FImportTimeData FImportDHI::ReadModificationData()
{
	const FString ModifiedInfoFilePath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("MetaHumans"), ModificationInfoFile));

	FString ModifiedFileInfoString;
	FFileHelper::LoadFileToString(ModifiedFileInfoString, *ModifiedInfoFilePath);

	FImportTimeData ModifiedInfoData;

	FJsonObjectConverter::JsonObjectStringToUStruct<FImportTimeData>(ModifiedFileInfoString, &ModifiedInfoData);

	return ModifiedInfoData;
}

// Returns the asset path of the blueorint of all characters that exist in the project
TArray<FString> FImportDHI::ListCharactersInProject()
{
	TArray<FString> CharactersInProject;
	
	TArray<FString> DirectoryList = UEditorAssetLibrary::ListAssets(TEXT("/Game/Metahumans"), false, true);

	FString ProjectMetahumanPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("MetaHumans")));

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	for (FString FoundAsset : DirectoryList)
	{
		if (UEditorAssetLibrary::DoesDirectoryExist(FoundAsset))
		{
			FString CharacterName = FoundAsset.Replace(TEXT("/Game/Metahumans/"), TEXT(""));

			if (CharacterName == TEXT("Common") || CharacterName == TEXT("Common/"))
			{
				continue;
			}

			CharacterName = CharacterName.Replace(TEXT("/"), TEXT(""));

			FString BPName = TEXT("BP_") + CharacterName;
			BPName += TEXT(".") + BPName;

			FString BPPath = FPaths::Combine(TEXT("/Game/MetaHumans/"), CharacterName, BPName);

			if (UEditorAssetLibrary::DoesAssetExist(BPPath))
			{
				CharactersInProject.Add(BPPath);
			}
		}
	}

	return CharactersInProject;
}


// List out all the characters that have editors open
TArray<FString> FImportDHI::CharactersOpenEditors()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FString> AssetsOpen;

	TArray<FString> CharactersInProject = ListCharactersInProject();

	for (FString CharacterInProject : CharactersInProject)
	{
		FAssetData CharacterAssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(CharacterInProject));

		if (CharacterAssetData.IsAssetLoaded())
		{
			UObject* CharacterObject = CharacterAssetData.GetAsset();
			if (GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorsForAsset(CharacterObject).Num() > 0)
			{
				AssetsOpen.Add(CharacterInProject);
			}
		}
	}

	return AssetsOpen;
}

void FImportDHI::CloseAssetEditors(TArray<FString> CharactersOpenInEditors)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry");

	for (FString CharacterAsset : CharactersOpenInEditors)
	{
		FAssetData CharacterAssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(CharacterAsset));
		
		UObject* CharacterObject = CharacterAssetData.GetAsset();

		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(CharacterObject);
	}
}

void FImportDHI::RecompileAllCharacters()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry");

	TArray<FString> CharactersInProject = ListCharactersInProject();

	for (FString CharacterInProject : CharactersInProject)
	{
		FAssetData GameAssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(CharacterInProject));
		UObject* ItemObject = GameAssetData.GetAsset();

		if (UBlueprint* BPObject = Cast<UBlueprint>(ItemObject))
		{
			FKismetEditorUtilities::CompileBlueprint(BPObject, EBlueprintCompileOptions::SkipGarbageCollection);
			BPObject->PreEditChange(nullptr);
			BPObject->PostEditChange();
		}
	}
}



void FImportDHI::ShowDialog(const FString& SourcePath, const FString& DestinationPath, const FString& Message, const FString& Footer)
{
	//GEditor->EditorAddModalWindow(DialogWindow.ToSharedRef());
	TSharedRef<SWindow> ModalWindow = SNew(SWindow)
		.Title(FText::FromString(TEXT("MetaHuman Alert")))
		.SizingRule(ESizingRule::FixedSize)
		.ClientSize(FVector2D(1000, 350))
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.HasCloseButton(true)
		.SupportsMinimize(false)
		.SupportsMaximize(false);

	TSharedRef<SOVerwriteDialog> MessageBox = SNew(SOVerwriteDialog)
		.ParentWindow(ModalWindow)
		.SourcePath(SourcePath)
		.Message(Message)
		.Footer(Footer)
		.DestinationPath(DestinationPath);
		
	ModalWindow->SetContent(MessageBox);

	GEditor->EditorAddModalWindow(ModalWindow);
}

//Check if any Metahuman is in level. 
TArray<FString> FImportDHI::MHCharactersInLevel()
{
	TArray<FString> CharactersInProject;
	TArray<FString> CharactersInLevel;
	//const FString MetahumanSourcePath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("MetaHumans")));

	TArray<FString> IncompatibleCharacters;
	TArray<FString> DirectoryList = UEditorAssetLibrary::ListAssets(TEXT("/Game/Metahumans"), false, true);

	FString ProjectMetahumanPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("MetaHumans")));

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	for (FString FoundAsset : DirectoryList)
	{
		if (UEditorAssetLibrary::DoesDirectoryExist(FoundAsset))
		{
			FString CharacterName = FoundAsset.Replace(TEXT("/Game/Metahumans/"), TEXT(""));

			if (CharacterName == TEXT("Common") || CharacterName == TEXT("Common/"))
			{
				continue;
			}

			CharacterName = CharacterName.Replace(TEXT("/"), TEXT(""));

			FString BPName = TEXT("BP_") + CharacterName;
			BPName += TEXT(".") + BPName;
			
			FString BPPath = FPaths::Combine(TEXT("/Game/MetaHumans/"), CharacterName, BPName);

			if (MHInLevel(BPPath))
			{
				CharactersInLevel.Add(CharacterName);
				
			}
		}
	}

	return CharactersInLevel;
}

// Should be replaced by ReadModificationData
TMap<FString, FString> FImportDHI::ParseModifiedInfo(const FString& ModifiedInfoFilePath)
{
	FString ModifiedInfoString;
	FFileHelper::LoadFileToString(ModifiedInfoString, *ModifiedInfoFilePath);

	TMap<FString, FString> ModifiedInfo;
	TSharedPtr<FJsonObject> ModifiedInfoObject = DeserializeJson(ModifiedInfoString);
	TArray<TSharedPtr<FJsonValue> > ModifiedInfoObjectArray = ModifiedInfoObject->GetArrayField(TEXT("assets"));

	for (TSharedPtr<FJsonValue> AssetModifiedInfoObject : ModifiedInfoObjectArray)
	{
		FString AssetPath = AssetModifiedInfoObject->AsObject()->GetStringField(TEXT("path"));
		FString ImportTime = AssetModifiedInfoObject->AsObject()->GetStringField(TEXT("time"));
		ModifiedInfo.Add(AssetPath, ImportTime);

	}
	
	return ModifiedInfo;
}

TArray<FString> FImportDHI::GetModifiedFileList(TMap<FString, FString> AssetsImportInfo, TArray<FString> FilesToUpdate)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	TArray<FString> LocallyModifiedFiles;
	FString ProjectContentPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());

	for (FString FileToUpdate : FilesToUpdate)
	{
		
		FString FilePath = FPaths::Combine(ProjectContentPath, FileToUpdate);
		
		if (PlatformFile.FileExists(*FilePath))
		{
			FDateTime FileModifiedTime = PlatformFile.GetTimeStampLocal(*FilePath);
			FDateTime FileImportTime;
			FDateTime::Parse(AssetsImportInfo[FileToUpdate], FileImportTime);
			
			if (FileModifiedTime > FileImportTime)
			{
				
				LocallyModifiedFiles.Add(FileToUpdate);
			}
		}
	}

	return LocallyModifiedFiles;
}


bool FImportDHI::MHInLevel(const FString& CharacterBPPath)
{
	FString CharacterPathInLevel = CharacterBPPath + TEXT("_C");
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GEngine->GetWorldContexts()[0].World(), AActor::StaticClass(), FoundActors);

	for (AActor* FoundActor : FoundActors)
	{
		FString ActorPath = FoundActor->GetClass()->GetPathName();

		if (ActorPath.Equals(CharacterPathInLevel))
		{

			return true;
		}
	}

	return false;
}


TArray<FString> FImportDHI::MetahumansInLevel(TArray<FString> CharactersInProject)
{
	TArray<FString> CharactersInLevel;
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GEngine->GetWorldContexts()[0].World(), AActor::StaticClass(), FoundActors);

	for (AActor* FoundActor : FoundActors)
	{
		FString ActorPath = FoundActor->GetClass()->GetPathName();

		if (CharactersInProject.Contains(ActorPath))
		{
			CharactersInLevel.Add(ActorPath);
		}
	}

	return CharactersInLevel;
}

void FImportDHI::UpdateImportTimeData(TArray<FString> AssetsUpdated, FImportTimeData& CurrentImportTimestamp)
{
	for (FString AssetUpdated : AssetsUpdated)
	{

	}
}

#undef LOCTEXT_NAMESPACE

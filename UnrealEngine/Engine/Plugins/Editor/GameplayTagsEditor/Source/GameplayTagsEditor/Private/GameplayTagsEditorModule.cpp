// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayTagsEditorModule.h"

#include "Misc/Paths.h"
#include "PropertyEditorModule.h"
#include "Factories/Factory.h"
#include "EdGraphUtilities.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "GameplayTagsManager.h"
#include "AssetRegistry/AssetData.h"
#include "Misc/ConfigCacheIni.h"
#include "GameplayTagsGraphPanelPinFactory.h"
#include "GameplayTagsGraphPanelNodeFactory.h"
#include "GameplayTagContainerCustomization.h"
#include "GameplayTagQueryCustomization.h"
#include "GameplayTagCustomization.h"
#include "GameplayTagsSettings.h"
#include "GameplayTagsSettingsCustomization.h"
#include "GameplayTagsModule.h"
#include "ISettingsModule.h"
#include "ISettingsEditorModule.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SNullWidget.h"
#include "Framework/Notifications/NotificationManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "ISourceControlModule.h"
#include "SourceControlHelpers.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "Stats/StatsMisc.h"
#include "UObject/UObjectHash.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "UObject/ObjectSaveContext.h"

#define LOCTEXT_NAMESPACE "GameplayTagEditor"


class FGameplayTagsEditorModule
	: public IGameplayTagsEditorModule
{
public:

	// IModuleInterface

	virtual void StartupModule() override
	{
		FCoreDelegates::OnPostEngineInit.AddRaw(this, &FGameplayTagsEditorModule::OnPostEngineInit);
	}
	
	void OnPostEngineInit()
	{
		// Register the details customizer
		{
			FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertyModule.RegisterCustomPropertyTypeLayout("GameplayTagContainer", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGameplayTagContainerCustomizationPublic::MakeInstance));
			PropertyModule.RegisterCustomPropertyTypeLayout("GameplayTag", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGameplayTagCustomizationPublic::MakeInstance));
			PropertyModule.RegisterCustomPropertyTypeLayout("GameplayTagQuery", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGameplayTagQueryCustomization::MakeInstance));
			PropertyModule.RegisterCustomPropertyTypeLayout("GameplayTagCreationWidgetHelper", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGameplayTagCreationWidgetHelperDetails::MakeInstance));

			PropertyModule.RegisterCustomClassLayout(UGameplayTagsSettings::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FGameplayTagsSettingsCustomization::MakeInstance));

			PropertyModule.NotifyCustomizationModuleChanged();
		}

		TSharedPtr<FGameplayTagsGraphPanelPinFactory> GameplayTagsGraphPanelPinFactory = MakeShareable(new FGameplayTagsGraphPanelPinFactory());
		FEdGraphUtilities::RegisterVisualPinFactory(GameplayTagsGraphPanelPinFactory);

		TSharedPtr<FGameplayTagsGraphPanelNodeFactory> GameplayTagsGraphPanelNodeFactory = MakeShareable(new FGameplayTagsGraphPanelNodeFactory());
		FEdGraphUtilities::RegisterVisualNodeFactory(GameplayTagsGraphPanelNodeFactory);

		// These objects are not UDeveloperSettings because we only want them to register if the editor plugin is enabled

		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->RegisterSettings("Project", "Project", "GameplayTags",
				LOCTEXT("GameplayTagSettingsName", "GameplayTags"),
				LOCTEXT("GameplayTagSettingsNameDesc", "GameplayTag Settings"),
				GetMutableDefault<UGameplayTagsSettings>()
			);
		}

		GameplayTagPackageName = FGameplayTag::StaticStruct()->GetOutermost()->GetFName();
		GameplayTagStructName = FGameplayTag::StaticStruct()->GetFName();

		// Hook into notifications for object re-imports so that the gameplay tag tree can be reconstructed if the table changes
		if (GIsEditor)
		{
			GEditor->GetEditorSubsystem<UImportSubsystem>()->OnAssetPostImport.AddRaw(this, &FGameplayTagsEditorModule::OnObjectReimported);
			FEditorDelegates::OnEditAssetIdentifiers.AddRaw(this, &FGameplayTagsEditorModule::OnEditGameplayTag);
			IGameplayTagsModule::OnTagSettingsChanged.AddRaw(this, &FGameplayTagsEditorModule::OnEditorSettingsChanged);
			UPackage::PackageSavedWithContextEvent.AddRaw(this, &FGameplayTagsEditorModule::OnPackageSaved);
		}
	}

	void OnObjectReimported(UFactory* ImportFactory, UObject* InObject)
	{
		UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

		// Re-construct the gameplay tag tree if the base table is re-imported
		if (GIsEditor && !IsRunningCommandlet() && InObject && Manager.GameplayTagTables.Contains(Cast<UDataTable>(InObject)))
		{
			Manager.EditorRefreshGameplayTagTree();
		}
	}

	virtual void ShutdownModule() override
	{
		FCoreDelegates::OnPostEngineInit.RemoveAll(this);

		// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
		// we call this function before unloading the module.

		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->UnregisterSettings("Project", "Project", "GameplayTags");
			SettingsModule->UnregisterSettings("Project", "Project", "GameplayTags Developer");
		}

		if (GEditor)
		{
			GEditor->GetEditorSubsystem<UImportSubsystem>()->OnAssetPostImport.RemoveAll(this);
		}
		FEditorDelegates::OnEditAssetIdentifiers.RemoveAll(this);
		IGameplayTagsModule::OnTagSettingsChanged.RemoveAll(this);
		UPackage::PackageSavedWithContextEvent.RemoveAll(this);
	}

	void OnEditorSettingsChanged()
	{
		// This is needed to make networking changes as well, so let's always refresh
		UGameplayTagsManager::Get().EditorRefreshGameplayTagTree();

		// Attempt to migrate the settings if needed
		MigrateSettings();
	}

	void WarnAboutRestart()
	{
		ISettingsEditorModule* SettingsEditorModule = FModuleManager::GetModulePtr<ISettingsEditorModule>("SettingsEditor");
		if (SettingsEditorModule)
		{
			SettingsEditorModule->OnApplicationRestartRequired();
		}
	}

	void OnPackageSaved(const FString& PackageFileName, UPackage* Package, FObjectPostSaveContext ObjectSaveContext)
	{
		if (GIsEditor && !ObjectSaveContext.IsProceduralSave())
		{
			UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

			bool bRefreshGameplayTagTree = false;

			TArray<UObject*> Objects;
			const bool bIncludeNestedObjects = false;
			GetObjectsWithPackage(Package, Objects, bIncludeNestedObjects);
			for (UObject* Entry : Objects)
			{
				if (UDataTable* DataTable = Cast<UDataTable>(Entry))
				{
					if (Manager.GameplayTagTables.Contains(DataTable))
					{
						bRefreshGameplayTagTree = true;
						break;
					}
				}
			}

			// Re-construct the gameplay tag tree if a data table is saved (presumably with modifications).
			if (bRefreshGameplayTagTree)
			{
				Manager.EditorRefreshGameplayTagTree();
			}
		}
	}

	void OnEditGameplayTag(TArray<FAssetIdentifier> AssetIdentifierList)
	{
		// If any of these are gameplay tags, open up tag viewer
		for (FAssetIdentifier Identifier : AssetIdentifierList)
		{
			if (Identifier.IsValue() && Identifier.PackageName == GameplayTagPackageName && Identifier.ObjectName == GameplayTagStructName)
			{
				if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
				{
					// TODO: Select tag maybe?
					SettingsModule->ShowViewer("Project", "Project", "GameplayTags");
				}
				return;
			}
		}
	}

	void ShowNotification(const FText& TextToDisplay, float TimeToDisplay, bool bLogError = false)
	{
		FNotificationInfo Info(TextToDisplay);
		Info.ExpireDuration = TimeToDisplay;

		FSlateNotificationManager::Get().AddNotification(Info);

		// Also log if error
		if (bLogError)
		{
			UE_LOG(LogGameplayTags, Error, TEXT("%s"), *TextToDisplay.ToString())
		}
	}

	void MigrateSettings()
	{
		UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

		FString DefaultEnginePath = FString::Printf(TEXT("%sDefaultEngine.ini"), *FPaths::SourceConfigDir());

		UGameplayTagsSettings* Settings = GetMutableDefault<UGameplayTagsSettings>();
		
		// The refresh has already set the in memory version of this to be correct, just need to save it out now
		if (!GConfig->GetSectionPrivate(TEXT("GameplayTags"), false, true, DefaultEnginePath))
		{
			// Already migrated or no data
			return;
		}

		// Check out defaultengine
		GameplayTagsUpdateSourceControl(DefaultEnginePath);

		// Delete gameplay tags section entirely. This modifies the disk version
		GConfig->EmptySection(TEXT("GameplayTags"), DefaultEnginePath);

		FConfigSection* PackageRedirects = GConfig->GetSectionPrivate(TEXT("/Script/Engine.Engine"), false, false, DefaultEnginePath);

		if (PackageRedirects)
		{
			for (FConfigSection::TIterator It(*PackageRedirects); It; ++It)
			{
				if (It.Key() == TEXT("+GameplayTagRedirects"))
				{
					It.RemoveCurrent();
				}
			}
		}

		// This will remove comments, etc. It is expected for someone to diff this before checking in to manually fix it
		GConfig->Flush(false, DefaultEnginePath);

		// Write out gameplaytags.ini
		GameplayTagsUpdateSourceControl(Settings->GetDefaultConfigFilename());
		Settings->TryUpdateDefaultConfigFile();

		GConfig->LoadFile(Settings->GetDefaultConfigFilename());
		
		// Write out all other tag lists
		TArray<const FGameplayTagSource*> Sources;

		Manager.FindTagSourcesWithType(EGameplayTagSourceType::TagList, Sources);
		Manager.FindTagSourcesWithType(EGameplayTagSourceType::RestrictedTagList, Sources);

		for (const FGameplayTagSource* Source : Sources)
		{
			UGameplayTagsList* TagList = Source->SourceTagList;
			if (TagList)
			{
				GameplayTagsUpdateSourceControl(TagList->ConfigFileName);
				TagList->TryUpdateDefaultConfigFile(TagList->ConfigFileName);

				// Reload off disk
				GConfig->LoadFile(TagList->ConfigFileName);

				// Explicitly remove user tags section
				GConfig->EmptySection(TEXT("UserTags"), TagList->ConfigFileName);
			}
		}

		ShowNotification(LOCTEXT("MigrationText", "Migrated Tag Settings, check DefaultEngine.ini before checking in!"), 10.0f);
	}

	void GameplayTagsUpdateSourceControl(const FString& RelativeConfigFilePath)
	{
		FString ConfigPath = FPaths::ConvertRelativePathToFull(RelativeConfigFilePath);

		if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*ConfigPath))
		{
			return;
		}

		if (ISourceControlModule::Get().IsEnabled())
		{
			FText ErrorMessage;

			if (!SourceControlHelpers::CheckoutOrMarkForAdd(ConfigPath, FText::FromString(ConfigPath), NULL, ErrorMessage))
			{
				ShowNotification(ErrorMessage, 3.0f);
			}
		}
		else
		{
			if (!FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*ConfigPath, false))
			{
				ShowNotification(FText::Format(LOCTEXT("FailedToMakeWritable", "Could not make {0} writable."), FText::FromString(ConfigPath)), 3.0f);
			}
		}
	}

	bool DeleteTagRedirector(const FName& TagToDelete)
	{
		UGameplayTagsSettings* Settings = GetMutableDefault<UGameplayTagsSettings>();
		UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

		for (int32 i = 0; i < Settings->GameplayTagRedirects.Num(); i++)
		{
			if (Settings->GameplayTagRedirects[i].OldTagName == TagToDelete)
			{
				Settings->GameplayTagRedirects.RemoveAt(i);

				GameplayTagsUpdateSourceControl(Settings->GetDefaultConfigFilename());
				Settings->TryUpdateDefaultConfigFile();
				GConfig->LoadFile(Settings->GetDefaultConfigFilename());

				Manager.EditorRefreshGameplayTagTree();

				ShowNotification(FText::Format(LOCTEXT("RemoveTagRedirect", "Deleted tag redirect {0}"), FText::FromName(TagToDelete)), 5.0f);

				WarnAboutRestart();

				TSharedPtr<FGameplayTagNode> FoundNode = Manager.FindTagNode(TagToDelete);

				ensureMsgf(!FoundNode.IsValid() || FoundNode->GetCompleteTagName() == TagToDelete, TEXT("Failed to delete redirector %s!"), *TagToDelete.ToString());

				return true;
			}
		}

		return false;
	}

	virtual bool AddNewGameplayTagToINI(const FString& NewTag, const FString& Comment, FName TagSourceName, bool bIsRestrictedTag, bool bAllowNonRestrictedChildren) override
	{
		UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

		if (NewTag.IsEmpty())
		{
			return false;
		}

		if (Manager.ShouldImportTagsFromINI() == false)
		{
			return false;
		}

		UGameplayTagsSettings*			Settings = GetMutableDefault<UGameplayTagsSettings>();
		UGameplayTagsDeveloperSettings* DevSettings = GetMutableDefault<UGameplayTagsDeveloperSettings>();

		FText ErrorText;
		FString FixedString;
		if (!Manager.IsValidGameplayTagString(NewTag, &ErrorText, &FixedString))
		{
			ShowNotification(FText::Format(LOCTEXT("AddTagFailure_BadString", "Failed to add gameplay tag {0}: {1}, try {2} instead!"), FText::FromString(NewTag), ErrorText, FText::FromString(FixedString)), 10.0f, true);
			return false;
		}

		FName NewTagName = FName(*NewTag);

		// Delete existing redirector
		DeleteTagRedirector(NewTagName);

		// Already in the list as an explicit tag, ignore. Note we want to add if it is in implicit tag. (E.g, someone added A.B.C then someone tries to add A.B)
		if (Manager.IsDictionaryTag(NewTagName))
		{
			ShowNotification(FText::Format(LOCTEXT("AddTagFailure_AlreadyExists", "Failed to add gameplay tag {0}, already exists!"), FText::FromString(NewTag)), 10.0f, true);

			return false;
		}

		if (bIsRestrictedTag)
		{
			// restricted tags can't be children of non-restricted tags
			FString AncestorTag = NewTag;
			bool bWasSplit = NewTag.Split(TEXT("."), &AncestorTag, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			while (bWasSplit)
			{
				if (Manager.IsDictionaryTag(FName(*AncestorTag)))
				{
					FString TagComment;
					FName Source;
					bool bIsExplicit;
					bool bIsRestricted;
					bool bAllowsNonRestrictedChildren;

					Manager.GetTagEditorData(*AncestorTag, TagComment, Source, bIsExplicit, bIsRestricted, bAllowsNonRestrictedChildren);
					if (bIsRestricted)
					{
						break;
					}
					ShowNotification(FText::Format(LOCTEXT("AddRestrictedTagFailure", "Failed to add restricted gameplay tag {0}, {1} is not a restricted tag"), FText::FromString(NewTag), FText::FromString(AncestorTag)), 10.0f, true);

					return false;
				}

				bWasSplit = AncestorTag.Split(TEXT("."), &AncestorTag, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			}
		}
		else
		{
			// non-restricted tags can only be children of restricted tags if the restricted tag allows it
			FString AncestorTag = NewTag;
			bool bWasSplit = NewTag.Split(TEXT("."), &AncestorTag, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			while (bWasSplit)
			{
				if (Manager.IsDictionaryTag(FName(*AncestorTag)))
				{
					FString TagComment;
					FName Source;
					bool bIsExplicit;
					bool bIsRestricted;
					bool bAllowsNonRestrictedChildren;

					Manager.GetTagEditorData(*AncestorTag, TagComment, Source, bIsExplicit, bIsRestricted, bAllowsNonRestrictedChildren);
					if (bIsRestricted)
					{
						if (bAllowsNonRestrictedChildren)
						{
							break;
						}

						ShowNotification(FText::Format(LOCTEXT("AddTagFailure_RestrictedTag", "Failed to add gameplay tag {0}, {1} is a restricted tag and does not allow non-restricted children"), FText::FromString(NewTag), FText::FromString(AncestorTag)), 10.0f, true);

						return false;
					}
				}

				bWasSplit = AncestorTag.Split(TEXT("."), &AncestorTag, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			}
		}

		if ((TagSourceName == NAME_None || TagSourceName == FGameplayTagSource::GetDefaultName()) && DevSettings && !DevSettings->DeveloperConfigName.IsEmpty())
		{
			// Try to use developer config file
			TagSourceName = FName(*FString::Printf(TEXT("%s.ini"), *DevSettings->DeveloperConfigName));
		}

		if (TagSourceName == NAME_None)
		{
			// If not set yet, set to default
			TagSourceName = FGameplayTagSource::GetDefaultName();
		}

		const FGameplayTagSource* TagSource = Manager.FindTagSource(TagSourceName);

		if (!TagSource)
		{
			// Create a new one
			TagSource = Manager.FindOrAddTagSource(TagSourceName, EGameplayTagSourceType::TagList);
		}

		bool bSuccess = false;
		if (TagSource)
		{
			UObject* TagListObj = nullptr;
			FString ConfigFileName;

			if (bIsRestrictedTag && TagSource->SourceRestrictedTagList)
			{
				URestrictedGameplayTagsList* RestrictedTagList = TagSource->SourceRestrictedTagList;
				TagListObj = RestrictedTagList;
				RestrictedTagList->RestrictedGameplayTagList.AddUnique(FRestrictedGameplayTagTableRow(FName(*NewTag), Comment, bAllowNonRestrictedChildren));
				RestrictedTagList->SortTags();
				ConfigFileName = RestrictedTagList->ConfigFileName;
				bSuccess = true;
			}
			else if (TagSource->SourceTagList)
			{
				UGameplayTagsList* TagList = TagSource->SourceTagList;
				TagListObj = TagList;
				TagList->GameplayTagList.AddUnique(FGameplayTagTableRow(FName(*NewTag), Comment));
				TagList->SortTags();
				ConfigFileName = TagList->ConfigFileName;
				bSuccess = true;
			}

			GameplayTagsUpdateSourceControl(ConfigFileName);

			// Check source control before and after writing, to make sure it gets created or checked out

			TagListObj->TryUpdateDefaultConfigFile(ConfigFileName);
			GameplayTagsUpdateSourceControl(ConfigFileName);
			GConfig->LoadFile(ConfigFileName);
		}
		
		if (!bSuccess)
		{
			ShowNotification(FText::Format(LOCTEXT("AddTagFailure", "Failed to add gameplay tag {0} to dictionary {1}!"), FText::FromString(NewTag), FText::FromName(TagSourceName)), 10.0f, true);

			return false;
		}

		{
			FString PerfMessage = FString::Printf(TEXT("ConstructGameplayTagTree GameplayTag tables after adding new tag"));
			SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)

			Manager.EditorRefreshGameplayTagTree();
		}

		return true;
	}

	virtual bool DeleteTagFromINI(TSharedPtr<FGameplayTagNode> TagNodeToDelete) override
	{
		FName TagName = TagNodeToDelete->GetCompleteTagName();

		UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
		UGameplayTagsSettings* Settings = GetMutableDefault<UGameplayTagsSettings>();

		FString Comment;
		FName TagSourceName;
		bool bTagIsExplicit;
		bool bTagIsRestricted;
		bool bTagAllowsNonRestrictedChildren;

		if (DeleteTagRedirector(TagName))
		{
			return true;
		}
		
		if (!Manager.GetTagEditorData(TagName, Comment, TagSourceName, bTagIsExplicit, bTagIsRestricted, bTagAllowsNonRestrictedChildren))
		{
			ShowNotification(FText::Format(LOCTEXT("RemoveTagFailureNoTag", "Cannot delete tag {0}, does not exist!"), FText::FromName(TagName)), 10.0f, true);
			return false;
		}

		ensure(bTagIsRestricted == TagNodeToDelete->IsRestrictedGameplayTag());

		const FGameplayTagSource* TagSource = Manager.FindTagSource(TagSourceName);

		// Check if the tag is implicitly defined
		if (!bTagIsExplicit || !TagSource)
		{
			ShowNotification(FText::Format(LOCTEXT("RemoveTagFailureNoSource", "Cannot delete tag {0} as it is implicit, remove children manually"), FText::FromName(TagName)), 10.0f, true);
			return false;
		}
		
		if (bTagIsRestricted && !TagSource->SourceRestrictedTagList)
		{
			ShowNotification(FText::Format(LOCTEXT("RemoveTagFailureBadSource", "Cannot delete tag {0} from source {1}, remove manually"), FText::FromName(TagName), FText::FromName(TagSourceName)), 10.0f, true);
			return false;
		}

		if (!bTagIsRestricted && !TagSource->SourceTagList)
		{
			ShowNotification(FText::Format(LOCTEXT("RemoveTagFailureBadSource", "Cannot delete tag {0} from source {1}, remove manually"), FText::FromName(TagName), FText::FromName(TagSourceName)), 10.0f, true);
			return false;
		}

		FGameplayTag ActualTag = Manager.RequestGameplayTag(TagName);
		FGameplayTagContainer ChildTags = Manager.RequestGameplayTagChildrenInDictionary(ActualTag);

		TArray<FName> TagsThatWillBeDeleted;

		TagsThatWillBeDeleted.Add(TagName);

		FGameplayTag ParentTag = ActualTag.RequestDirectParent();
		while (ParentTag.IsValid() && !Manager.FindTagNode(ParentTag)->IsExplicitTag())
		{
			// See if there are more children than the one we are about to delete
			FGameplayTagContainer ParentChildTags = Manager.RequestGameplayTagChildrenInDictionary(ParentTag);

			ensure(ParentChildTags.HasTagExact(ActualTag));
			if (ParentChildTags.Num() == 1)
			{
				// This is the only tag, add to deleted list
				TagsThatWillBeDeleted.Add(ParentTag.GetTagName());
				ParentTag = ParentTag.RequestDirectParent();
			}
			else
			{
				break;
			}
		}

		for (FName TagNameToDelete : TagsThatWillBeDeleted)
		{
			// Verify references
			FAssetIdentifier TagId = FAssetIdentifier(FGameplayTag::StaticStruct(), TagName);
			TArray<FAssetIdentifier> Referencers;

			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
			AssetRegistryModule.Get().GetReferencers(TagId, Referencers, UE::AssetRegistry::EDependencyCategory::SearchableName);

			if (Referencers.Num() > 0)
			{
				ShowNotification(FText::Format(LOCTEXT("RemoveTagFailureBadSource_Referenced", "Cannot delete tag {0}, still referenced by {1} and possibly others"), FText::FromName(TagNameToDelete), FText::FromString(Referencers[0].ToString())), 10.0f, true);

				return false;
			}
		}

		// Passed, delete and save
		const FString& ConfigFileName = bTagIsRestricted ? TagSource->SourceRestrictedTagList->ConfigFileName : TagSource->SourceTagList->ConfigFileName;
		int32 TagListSize = bTagIsRestricted ? TagSource->SourceRestrictedTagList->RestrictedGameplayTagList.Num() : TagSource->SourceTagList->GameplayTagList.Num();

		for (int32 i = 0; i < TagListSize; i++)
		{
			bool bRemoved = false;
			if (bTagIsRestricted)
			{
				if (TagSource->SourceRestrictedTagList->RestrictedGameplayTagList[i].Tag == TagName)
				{
					TagSource->SourceRestrictedTagList->RestrictedGameplayTagList.RemoveAt(i);
					TagSource->SourceRestrictedTagList->TryUpdateDefaultConfigFile(ConfigFileName);
					bRemoved = true;
				}
			}
			else
			{
				if (TagSource->SourceTagList->GameplayTagList[i].Tag == TagName)
				{
					TagSource->SourceTagList->GameplayTagList.RemoveAt(i);
					TagSource->SourceTagList->TryUpdateDefaultConfigFile(ConfigFileName);
					bRemoved = true;
				}
			}

			if (bRemoved)
			{
				GameplayTagsUpdateSourceControl(ConfigFileName);
				GConfig->LoadFile(ConfigFileName);

				// See if we still live due to child tags

				if (ChildTags.Num() > 0)
				{
					ShowNotification(FText::Format(LOCTEXT("RemoveTagChildrenExist", "Deleted explicit tag {0}, still exists implicitly due to children"), FText::FromName(TagName)), 5.0f);
				}
				else
				{
					ShowNotification(FText::Format(LOCTEXT("RemoveTag", "Deleted tag {0}"), FText::FromName(TagName)), 5.0f);
				}

				// This invalidates all local variables, need to return right away
				Manager.EditorRefreshGameplayTagTree();

				return true;
			}
		}

		ShowNotification(FText::Format(LOCTEXT("RemoveTagFailureNoTag", "Cannot delete tag {0}, does not exist!"), FText::FromName(TagName)), 10.0f, true);
		
		return false;
	}

	virtual bool UpdateTagInINI(const FString& TagToUpdate, const FString& Comment, bool bIsRestrictedTag, bool bAllowNonRestrictedChildren) override
	{
		FName TagName = FName(*TagToUpdate);

		UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
		UGameplayTagsSettings* Settings = GetMutableDefault<UGameplayTagsSettings>();

		FString OldComment;
		FName TagSourceName;
		bool bTagIsExplicit;
		bool bTagWasRestricted;
		bool bTagDidAllowNonRestrictedChildren;

		bool bSuccess = false;

		if (Manager.GetTagEditorData(TagName, OldComment, TagSourceName, bTagIsExplicit, bTagWasRestricted, bTagDidAllowNonRestrictedChildren))
		{
			if (const FGameplayTagSource* TagSource = Manager.FindTagSource(TagSourceName))
			{
				// if we're disallowing non-restricted children make sure we don't already have some
				if (bTagDidAllowNonRestrictedChildren && !bAllowNonRestrictedChildren)
				{
					FGameplayTag ActualTag = Manager.RequestGameplayTag(TagName);
					FGameplayTagContainer ChildTags = Manager.RequestGameplayTagDirectDescendantsInDictionary(ActualTag, EGameplayTagSelectionType::NonRestrictedOnly);
					if (!ChildTags.IsEmpty())
					{
						ShowNotification(LOCTEXT("ToggleAllowNonRestrictedChildrenFailure", "Cannot prevent non-restricted children since some already exist! Delete them first."), 10.0f, true);
						return false;
					}
				}

				UObject* TagListObj = nullptr;
				FString ConfigFileName;

				if (bIsRestrictedTag && TagSource->SourceRestrictedTagList)
				{
					URestrictedGameplayTagsList* RestrictedTagList = TagSource->SourceRestrictedTagList;
					TagListObj = RestrictedTagList;
					ConfigFileName = RestrictedTagList->ConfigFileName;

					for (int32 i = 0; i < RestrictedTagList->RestrictedGameplayTagList.Num(); i++)
					{
						if (RestrictedTagList->RestrictedGameplayTagList[i].Tag == TagName)
						{
							RestrictedTagList->RestrictedGameplayTagList[i].bAllowNonRestrictedChildren = bAllowNonRestrictedChildren;
							bSuccess = true;
							break;
						}
					}
				}

				if (bSuccess)
				{
					// Check source control before and after writing, to make sure it gets created or checked out
					GameplayTagsUpdateSourceControl(ConfigFileName);
					TagListObj->TryUpdateDefaultConfigFile(ConfigFileName);
					GameplayTagsUpdateSourceControl(ConfigFileName);

					GConfig->LoadFile(ConfigFileName);
				}

			}
		}

		return bSuccess;
	}

	virtual bool RenameTagInINI(const FString& TagToRename, const FString& TagToRenameTo) override
	{
		FName OldTagName = FName(*TagToRename);
		FName NewTagName = FName(*TagToRenameTo);

		UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
		UGameplayTagsSettings* Settings = GetMutableDefault<UGameplayTagsSettings>();

		FString OldComment, NewComment;
		FName OldTagSourceName, NewTagSourceName;
		bool bTagIsExplicit;
		bool bTagIsRestricted;
		bool bTagAllowsNonRestrictedChildren;

		// Delete existing redirector
		DeleteTagRedirector(NewTagName);
		DeleteTagRedirector(OldTagName);

		if (Manager.GetTagEditorData(OldTagName, OldComment, OldTagSourceName, bTagIsExplicit, bTagIsRestricted, bTagAllowsNonRestrictedChildren))
		{
			// Add new tag if needed
			if (!Manager.GetTagEditorData(NewTagName, NewComment, NewTagSourceName, bTagIsExplicit, bTagIsRestricted, bTagAllowsNonRestrictedChildren))
			{
				if (!AddNewGameplayTagToINI(TagToRenameTo, OldComment, OldTagSourceName, bTagIsRestricted, bTagAllowsNonRestrictedChildren))
				{
					// Failed to add new tag, so fail
					return false;
				}
			}

			// Delete old tag if possible, still make redirector if this fails
			const FGameplayTagSource* OldTagSource = Manager.FindTagSource(OldTagSourceName);

			if (OldTagSource && OldTagSource->SourceTagList)
			{
				UGameplayTagsList* TagList = OldTagSource->SourceTagList;

				for (int32 i = 0; i < TagList->GameplayTagList.Num(); i++)
				{
					if (TagList->GameplayTagList[i].Tag == OldTagName)
					{
						TagList->GameplayTagList.RemoveAt(i);

						TagList->TryUpdateDefaultConfigFile(TagList->ConfigFileName);
						GameplayTagsUpdateSourceControl(TagList->ConfigFileName);
						GConfig->LoadFile(TagList->ConfigFileName);

						break;
					}
				}
			}
			else
			{
				ShowNotification(FText::Format(LOCTEXT("RenameFailure", "Tag {0} redirector was created but original tag was not destroyed as it has children"), FText::FromString(TagToRename)), 10.0f, true);
			}
		}

		// Add redirector no matter what
		FGameplayTagRedirect Redirect;
		Redirect.OldTagName = OldTagName;
		Redirect.NewTagName = NewTagName;

		Settings->GameplayTagRedirects.AddUnique(Redirect);

		GameplayTagsUpdateSourceControl(Settings->GetDefaultConfigFilename());
		Settings->TryUpdateDefaultConfigFile();
		GConfig->LoadFile(Settings->GetDefaultConfigFilename());

		ShowNotification(FText::Format(LOCTEXT("AddTagRedirect", "Renamed tag {0} to {1}"), FText::FromString(TagToRename), FText::FromString(TagToRenameTo)), 3.0f);

		Manager.EditorRefreshGameplayTagTree();

		WarnAboutRestart();

		return true;
	}

	virtual bool AddTransientEditorGameplayTag(const FString& NewTransientTag) override
	{
		UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

		if (NewTransientTag.IsEmpty())
		{
			return false;
		}

		Manager.TransientEditorTags.Add(*NewTransientTag);

		{
			FString PerfMessage = FString::Printf(TEXT("ConstructGameplayTagTree GameplayTag tables after adding new transient tag"));
			SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)

			Manager.EditorRefreshGameplayTagTree();
		}

		return true;
	}

	virtual bool AddNewGameplayTagSource(const FString& NewTagSource, const FString& RootDirToUse = FString()) override
	{
		UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

		if (NewTagSource.IsEmpty())
		{
			return false;
		}

		Manager.FindOrAddTagSource(FName(*NewTagSource), EGameplayTagSourceType::TagList, RootDirToUse);

		IGameplayTagsModule::OnTagSettingsChanged.Broadcast();

		return true;
	}

	TSharedRef<SWidget> MakeGameplayTagContainerWidget(FOnSetGameplayTagContainer OnSetTag, TSharedPtr<FGameplayTagContainer> GameplayTagContainer, const FString& FilterString) override
	{
		if (!GameplayTagContainer.IsValid())
		{
			return SNullWidget::NullWidget;
		}

		TArray<SGameplayTagWidget::FEditableGameplayTagContainerDatum> EditableContainers;
		EditableContainers.Emplace(nullptr, GameplayTagContainer.Get());

		// This will keep the shared ptr and delegate alive as long as the widget is
		SGameplayTagWidget::FOnTagChanged OnChanged = SGameplayTagWidget::FOnTagChanged::CreateLambda([OnSetTag, GameplayTagContainer]()
		{
			OnSetTag.Execute(*GameplayTagContainer.Get());
		});

		return SNew(SGameplayTagWidget, EditableContainers)
			.Filter(FilterString)
			.ReadOnly(false)
			.MultiSelect(true)
			.OnTagChanged(OnChanged);
	}

	TSharedRef<SWidget> MakeGameplayTagWidget(FOnSetGameplayTag OnSetTag, TSharedPtr<FGameplayTag> GameplayTag, const FString& FilterString) override
	{
		if (!GameplayTag.IsValid())
		{
			return SNullWidget::NullWidget;
		}

		// Make a wrapper tag container
		TSharedPtr<FGameplayTagContainer> GameplayTagContainer = MakeShareable(new FGameplayTagContainer(*GameplayTag.Get()));

		TArray<SGameplayTagWidget::FEditableGameplayTagContainerDatum> EditableContainers;
		EditableContainers.Emplace(nullptr, GameplayTagContainer.Get());

		// This will keep the shared ptr and delegate alive as long as the widget is
		SGameplayTagWidget::FOnTagChanged OnChanged = SGameplayTagWidget::FOnTagChanged::CreateLambda([OnSetTag, GameplayTag, GameplayTagContainer]()
		{
			*GameplayTag.Get() = GameplayTagContainer->First();
			OnSetTag.Execute(*GameplayTag.Get());
		});

		return SNew(SGameplayTagWidget, EditableContainers)
			.Filter(FilterString)
			.ReadOnly(false)
			.MultiSelect(false)
			.OnTagChanged(OnChanged);
	}

	static bool WriteCustomReport(FString FileName, TArray<FString>& FileLines)
	{
		// Has a report been generated
		bool ReportGenerated = false;

		// Ensure we have a log to write
		if (FileLines.Num())
		{
			// Create the file name		
			FString FileLocation = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() + TEXT("Reports/"));
			FString FullPath = FString::Printf(TEXT("%s%s"), *FileLocation, *FileName);

			// save file
			FArchive* LogFile = IFileManager::Get().CreateFileWriter(*FullPath);

			if (LogFile != NULL)
			{
				for (int32 Index = 0; Index < FileLines.Num(); ++Index)
				{
					FString LogEntry = FString::Printf(TEXT("%s"), *FileLines[Index]) + LINE_TERMINATOR;
					LogFile->Serialize(TCHAR_TO_ANSI(*LogEntry), LogEntry.Len());
				}

				LogFile->Close();
				delete LogFile;

				// A report has been generated
				ReportGenerated = true;
			}
		}

		return ReportGenerated;
	}

	static void DumpTagList()
	{
		UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

		TArray<FString> ReportLines;

		ReportLines.Add(TEXT("Tag,Reference Count,Source,Comment"));

		FGameplayTagContainer AllTags;
		Manager.RequestAllGameplayTags(AllTags, true);

		TArray<FGameplayTag> ExplicitList;
		AllTags.GetGameplayTagArray(ExplicitList);

		ExplicitList.Sort();

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		
		for (const FGameplayTag& Tag : ExplicitList)
		{
			TArray<FAssetIdentifier> Referencers;
			FAssetIdentifier TagId = FAssetIdentifier(FGameplayTag::StaticStruct(), Tag.GetTagName());
			AssetRegistryModule.Get().GetReferencers(TagId, Referencers, UE::AssetRegistry::EDependencyCategory::SearchableName);

			FString Comment;
			FName TagSource;
			bool bExplicit, bRestricted, bAllowNonRestrictedChildren;

			Manager.GetTagEditorData(Tag.GetTagName(), Comment, TagSource, bExplicit, bRestricted, bAllowNonRestrictedChildren);

			ReportLines.Add(FString::Printf(TEXT("%s,%d,%s,%s"), *Tag.ToString(), Referencers.Num(), *TagSource.ToString(), *Comment));
		}

		WriteCustomReport(TEXT("TagList.csv"), ReportLines);
	}

	FDelegateHandle AssetImportHandle;
	FDelegateHandle SettingsChangedHandle;

	FName GameplayTagPackageName;
	FName GameplayTagStructName;
};

static FAutoConsoleCommand CVarDumpTagList(
	TEXT("GameplayTags.DumpTagList"),
	TEXT("Writes out a csv with all tags to Reports/TagList.csv"),
	FConsoleCommandDelegate::CreateStatic(FGameplayTagsEditorModule::DumpTagList),
	ECVF_Cheat);

IMPLEMENT_MODULE(FGameplayTagsEditorModule, GameplayTagsEditor)

#undef LOCTEXT_NAMESPACE

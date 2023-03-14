// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#include "GameFeatureData.h"
#include "Features/IPluginsEditorFeature.h"
#include "Features/EditorFeatures.h"

#include "Interfaces/IPluginManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "GameFeatureDataDetailsCustomization.h"
#include "GameFeaturesEditorSettings.h"
#include "GameFeaturesSubsystemSettings.h"
#include "GameFeaturePluginMetadataCustomization.h"
#include "Logging/MessageLog.h"
#include "SSettingsEditorCheckoutNotice.h"
#include "Engine/AssetManagerSettings.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Engine/AssetManager.h"
#include "HAL/FileManager.h"
#include "Editor.h"
#include "Dom/JsonValue.h"

#define LOCTEXT_NAMESPACE "GameFeatures"

//////////////////////////////////////////////////////////////////////

struct FGameFeaturePluginTemplateDescription : public FPluginTemplateDescription
{
	FGameFeaturePluginTemplateDescription(FText InName, FText InDescription, FString InOnDiskPath
		, TSubclassOf<UGameFeatureData> GameFeatureDataClassOverride, FString GameFeatureDataNameOverride, EPluginEnabledByDefault InEnabledByDefault)
		: FPluginTemplateDescription(InName, InDescription, InOnDiskPath, /*bCanContainContent=*/ true, EHostType::Runtime)
	{
		SortPriority = 10;
		bCanBePlacedInEngine = false;
		GameFeatureDataName = !GameFeatureDataNameOverride.IsEmpty() ? GameFeatureDataNameOverride : FString();
		GameFeatureDataClass = GameFeatureDataClassOverride != nullptr ? GameFeatureDataClassOverride : TSubclassOf<UGameFeatureData>(UGameFeatureData::StaticClass());
		PluginEnabledByDefault = InEnabledByDefault;
	}

	virtual bool ValidatePathForPlugin(const FString& ProposedAbsolutePluginPath, FText& OutErrorMessage) override
	{
		if (!IsRootedInGameFeaturesRoot(ProposedAbsolutePluginPath))
		{
			OutErrorMessage = LOCTEXT("InvalidPathForGameFeaturePlugin", "Game features must be inside the Plugins/GameFeatures folder");
			return false;
		}

		OutErrorMessage = FText::GetEmpty();
		return true;
	}

	virtual void UpdatePathWhenTemplateSelected(FString& InOutPath) override
	{
		if (!IsRootedInGameFeaturesRoot(InOutPath))
		{
			InOutPath = GetGameFeatureRoot();
		}
	}

	virtual void UpdatePathWhenTemplateUnselected(FString& InOutPath) override
	{
		InOutPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*FPaths::ProjectPluginsDir());
		FPaths::MakePlatformFilename(InOutPath);
	}

	virtual void CustomizeDescriptorBeforeCreation(FPluginDescriptor& Descriptor) override
	{
		Descriptor.bExplicitlyLoaded = true;
		Descriptor.AdditionalFieldsToWrite.FindOrAdd(TEXT("BuiltInInitialFeatureState")) = MakeShared<FJsonValueString>(TEXT("Active"));
		Descriptor.Category = TEXT("Game Features");

		// Game features should not be enabled by default if the game wants to strictly manage default settings in the target settings
		Descriptor.EnabledByDefault = PluginEnabledByDefault;

		if (Descriptor.Modules.Num() > 0)
		{
			Descriptor.Modules[0].Name = FName(*(Descriptor.Modules[0].Name.ToString() + TEXT("Runtime")));
		}
	}

	virtual void OnPluginCreated(TSharedPtr<IPlugin> NewPlugin) override
	{
		// If the template includes an existing game feature data, do not create a new one.
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		TArray<FAssetData> ObjectList;
		FARFilter AssetFilter;
		AssetFilter.ClassPaths.Add(UGameFeatureData::StaticClass()->GetClassPathName());
		AssetFilter.PackagePaths.Add(FName(NewPlugin->GetMountedAssetPath()));
		AssetFilter.bRecursiveClasses = true;
		AssetFilter.bRecursivePaths = true;

		AssetRegistryModule.Get().GetAssets(AssetFilter, ObjectList);

		UObject* GameFeatureDataAsset = nullptr;

		if (ObjectList.Num() <= 0)
		{
			// Create the game feature data asset
			FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
			FString const& AssetName = !GameFeatureDataName.IsEmpty() ? GameFeatureDataName : NewPlugin->GetName();
			GameFeatureDataAsset = AssetToolsModule.Get().CreateAsset(AssetName, NewPlugin->GetMountedAssetPath(), GameFeatureDataClass, /*Factory=*/ nullptr);
		}
		else
		{
			GameFeatureDataAsset = ObjectList[0].GetAsset();
		}
		

		// Activate the new game feature plugin
		auto AdditionalFilter = [](const FString&, const FGameFeaturePluginDetails&, FBuiltInGameFeaturePluginBehaviorOptions&) -> bool { return true; };
		UGameFeaturesSubsystem::Get().LoadBuiltInGameFeaturePlugin(NewPlugin.ToSharedRef(), AdditionalFilter);

		// Edit the new game feature data
		if (GameFeatureDataAsset != nullptr)
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(GameFeatureDataAsset);
		}
	}

	FString GetGameFeatureRoot() const
	{
		FString Result = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*(FPaths::ProjectPluginsDir() / TEXT("GameFeatures/")));
		FPaths::MakePlatformFilename(Result);
		return Result;
	}

	bool IsRootedInGameFeaturesRoot(const FString& InStr)
	{
		const FString ConvertedPath = FPaths::ConvertRelativePathToFull(FPaths::CreateStandardFilename(InStr / TEXT("test.uplugin")));
		return GetDefault<UGameFeaturesSubsystemSettings>()->IsValidGameFeaturePlugin(ConvertedPath);
	}

	TSubclassOf<UGameFeatureData> GameFeatureDataClass;
	FString GameFeatureDataName;
	EPluginEnabledByDefault PluginEnabledByDefault = EPluginEnabledByDefault::Disabled;
};

//////////////////////////////////////////////////////////////////////

class FGameFeaturesEditorModule : public FDefaultModuleImpl
{
	virtual void StartupModule() override
	{
		// Register the details customizations
		{
			FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertyModule.RegisterCustomClassLayout(UGameFeatureData::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FGameFeatureDataDetailsCustomization::MakeInstance));

			PropertyModule.NotifyCustomizationModuleChanged();
		}

		// Register to get a warning on startup if settings aren't configured correctly
		UAssetManager::CallOrRegister_OnAssetManagerCreated(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FGameFeaturesEditorModule::OnAssetManagerCreated));

		// Add templates to the new plugin wizard
		{
			GameFeaturesEditorSettingsWatcher = MakeShared<FGameFeaturesEditorSettingsWatcher>(this);
			GameFeaturesEditorSettingsWatcher->Init();

			CachePluginTemplates();

			IModularFeatures& ModularFeatures = IModularFeatures::Get();
			ModularFeatures.OnModularFeatureRegistered().AddRaw(this, &FGameFeaturesEditorModule::OnModularFeatureRegistered);
			ModularFeatures.OnModularFeatureUnregistered().AddRaw(this, &FGameFeaturesEditorModule::OnModularFeatureUnregistered);

			if (ModularFeatures.IsModularFeatureAvailable(EditorFeatures::PluginsEditor))
			{
				OnModularFeatureRegistered(EditorFeatures::PluginsEditor, &ModularFeatures.GetModularFeature<IPluginsEditorFeature>(EditorFeatures::PluginsEditor));
			}
		}
	}

	virtual void ShutdownModule() override
	{
		// Remove the customization
		if (UObjectInitialized() && FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
		{
			FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertyModule.UnregisterCustomClassLayout(UGameFeatureData::StaticClass()->GetFName());

			PropertyModule.NotifyCustomizationModuleChanged();
		}

		// Remove the plugin wizard override
		if (UObjectInitialized())
		{
			GameFeaturesEditorSettingsWatcher = nullptr;

			IModularFeatures& ModularFeatures = IModularFeatures::Get();
 			ModularFeatures.OnModularFeatureRegistered().RemoveAll(this);
 			ModularFeatures.OnModularFeatureUnregistered().RemoveAll(this);

			if (ModularFeatures.IsModularFeatureAvailable(EditorFeatures::PluginsEditor))
			{
				OnModularFeatureUnregistered(EditorFeatures::PluginsEditor, &ModularFeatures.GetModularFeature<IPluginsEditorFeature>(EditorFeatures::PluginsEditor));
			}
			UnregisterFunctionTemplates();
			PluginTemplates.Empty();
		}
	}

	void OnSettingsChanged(UObject* Settings, FPropertyChangedEvent& PropertyChangedEvent)
	{
		const FName PropertyName = PropertyChangedEvent.GetPropertyName();
		const FName MemberPropertyName = (PropertyChangedEvent.MemberProperty != nullptr) ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;
		const FName PluginTemplatePropertyName = GET_MEMBER_NAME_CHECKED(UGameFeaturesEditorSettings, PluginTemplates);
		if (PropertyName == PluginTemplatePropertyName
			|| MemberPropertyName == PluginTemplatePropertyName)
		{
			ResetPluginTemplates();
		}
	}

	void CachePluginTemplates()
	{
		PluginTemplates.Reset();
		if (const UGameFeaturesEditorSettings* GameFeatureEditorSettings = GetDefault<UGameFeaturesEditorSettings>())
		{
			for (const FPluginTemplateData& PluginTemplate : GameFeatureEditorSettings->PluginTemplates)
			{
				PluginTemplates.Add(MakeShareable(new FGameFeaturePluginTemplateDescription(
					PluginTemplate.Label,
					PluginTemplate.Description,
					PluginTemplate.Path.Path,
					PluginTemplate.DefaultGameFeatureDataClass,
					PluginTemplate.DefaultGameFeatureDataName,
					PluginTemplate.bIsEnabledByDefault ? EPluginEnabledByDefault::Enabled : EPluginEnabledByDefault::Disabled)));
			}
		}
	}

	void ResetPluginTemplates()
	{
		UnregisterFunctionTemplates();
		CachePluginTemplates();
		RegisterPluginTemplates();
	}

	void RegisterPluginTemplates()
	{
		if (IModularFeatures::Get().IsModularFeatureAvailable(EditorFeatures::PluginsEditor))
		{
			IPluginsEditorFeature& PluginEditor = IModularFeatures::Get().GetModularFeature<IPluginsEditorFeature>(EditorFeatures::PluginsEditor);
			for (const TSharedPtr<FGameFeaturePluginTemplateDescription, ESPMode::ThreadSafe>& TemplateDescription : PluginTemplates)
			{
				PluginEditor.RegisterPluginTemplate(TemplateDescription.ToSharedRef());
			}
			PluginEditorExtensionDelegate = PluginEditor.RegisterPluginEditorExtension(FOnPluginBeingEdited::CreateRaw(this, &FGameFeaturesEditorModule::CustomizePluginEditing));
		}
	}

	void UnregisterFunctionTemplates()
	{
		if (IModularFeatures::Get().IsModularFeatureAvailable(EditorFeatures::PluginsEditor))
		{
			IPluginsEditorFeature& PluginEditor = IModularFeatures::Get().GetModularFeature<IPluginsEditorFeature>(EditorFeatures::PluginsEditor);
			for (const TSharedPtr<FGameFeaturePluginTemplateDescription, ESPMode::ThreadSafe>& TemplateDescription : PluginTemplates)
			{
				PluginEditor.UnregisterPluginTemplate(TemplateDescription.ToSharedRef());
			}
			PluginEditor.UnregisterPluginEditorExtension(PluginEditorExtensionDelegate);
		}
		
	}

	void OnModularFeatureRegistered(const FName& Type, class IModularFeature* ModularFeature)
	{
		if (Type == EditorFeatures::PluginsEditor)
		{
			ResetPluginTemplates();
		}
	}

	void OnModularFeatureUnregistered(const FName& Type, class IModularFeature* ModularFeature)
	{
		if (Type == EditorFeatures::PluginsEditor)
		{
			UnregisterFunctionTemplates();
		}
	}

	void AddDefaultGameDataRule()
	{
		// Check out the ini or make it writable
		UAssetManagerSettings* Settings = GetMutableDefault<UAssetManagerSettings>();

		const FString& ConfigFileName = Settings->GetDefaultConfigFilename();

		bool bSuccess = false;

		FText NotificationOpText;
 		if (!SettingsHelpers::IsCheckedOut(ConfigFileName, true))
 		{
			FText ErrorMessage;
			bSuccess = SettingsHelpers::CheckOutOrAddFile(ConfigFileName, true, !IsRunningCommandlet(), &ErrorMessage);
			if (bSuccess)
			{
				NotificationOpText = LOCTEXT("CheckedOutAssetManagerIni", "Checked out {0}");
			}
			else
			{
				UE_LOG(LogGameFeatures, Error, TEXT("%s"), *ErrorMessage.ToString());
				bSuccess = SettingsHelpers::MakeWritable(ConfigFileName);

				if (bSuccess)
				{
					NotificationOpText = LOCTEXT("MadeWritableAssetManagerIni", "Made {0} writable (you may need to manually add to source control)");
				}
				else
				{
					NotificationOpText = LOCTEXT("FailedToTouchAssetManagerIni", "Failed to check out {0} or make it writable, so no rule was added");
				}
			}
		}
		else
		{
			NotificationOpText = LOCTEXT("UpdatedAssetManagerIni", "Updated {0}");
			bSuccess = true;
		}

		// Add the rule to project settings
		if (bSuccess)
		{
			FDirectoryPath DummyPath;
			DummyPath.Path = TEXT("/Game/Unused");

			FPrimaryAssetTypeInfo NewTypeInfo(
				UGameFeatureData::StaticClass()->GetFName(),
				UGameFeatureData::StaticClass(),
				false,
				false,
				{ DummyPath },
				{});
			NewTypeInfo.Rules.CookRule = EPrimaryAssetCookRule::AlwaysCook;

			Settings->Modify(true);

			Settings->PrimaryAssetTypesToScan.Add(NewTypeInfo);

 			Settings->PostEditChange();
			Settings->TryUpdateDefaultConfigFile();

			UAssetManager::Get().ReinitializeFromConfig();
		}

		// Show a message that the file was checked out/updated and must be submitted
		FNotificationInfo Info(FText::Format(NotificationOpText, FText::FromString(FPaths::GetCleanFilename(ConfigFileName))));
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}

	void OnAssetManagerCreated()
	{
		// Make sure the game has the appropriate asset manager configuration or we won't be able to load game feature data assets
		FPrimaryAssetId DummyGameFeatureDataAssetId(UGameFeatureData::StaticClass()->GetFName(), NAME_None);
		FPrimaryAssetRules GameDataRules = UAssetManager::Get().GetPrimaryAssetRules(DummyGameFeatureDataAssetId);
		if (GameDataRules.IsDefault())
		{
			FMessageLog("LoadErrors").Error()
				->AddToken(FTextToken::Create(FText::Format(NSLOCTEXT("GameFeatures", "MissingRuleForGameFeatureData", "Asset Manager settings do not include an entry for assets of type {0}, which is required for game feature plugins to function."), FText::FromName(UGameFeatureData::StaticClass()->GetFName()))))
				->AddToken(FActionToken::Create(NSLOCTEXT("GameFeatures", "AddRuleForGameFeatureData", "Add entry to PrimaryAssetTypesToScan?"), FText(),
					FOnActionTokenExecuted::CreateRaw(this, &FGameFeaturesEditorModule::AddDefaultGameDataRule), true));
		}
	}

	TSharedPtr<FPluginEditorExtension> CustomizePluginEditing(FPluginEditingContext& InPluginContext, IDetailLayoutBuilder& DetailBuilder)
	{
		const bool bIsGameFeaturePlugin = InPluginContext.PluginBeingEdited->GetDescriptorFileName().Contains(TEXT("/GameFeatures/"));
		if (bIsGameFeaturePlugin)
		{
			TSharedPtr<FGameFeaturePluginMetadataCustomization> Result = MakeShareable(new FGameFeaturePluginMetadataCustomization);
			Result->CustomizeDetails(InPluginContext, DetailBuilder);
			return Result;
		}

		return nullptr;
	}
private:

	struct FGameFeaturesEditorSettingsWatcher : public TSharedFromThis<FGameFeaturesEditorSettingsWatcher>
	{
		FGameFeaturesEditorSettingsWatcher(FGameFeaturesEditorModule* InParentModule)
			: ParentModule(InParentModule)
		{

		}

		void Init()
		{
			GetMutableDefault<UGameFeaturesEditorSettings>()->OnSettingChanged().AddSP(this, &FGameFeaturesEditorSettingsWatcher::OnSettingsChanged);
		}

		void OnSettingsChanged(UObject* Settings, FPropertyChangedEvent& PropertyChangedEvent)
		{
			if (ParentModule != nullptr)
			{
				ParentModule->OnSettingsChanged(Settings, PropertyChangedEvent);
			}
		}
	private:

		FGameFeaturesEditorModule* ParentModule;
	};

	TSharedPtr<FGameFeaturesEditorSettingsWatcher> GameFeaturesEditorSettingsWatcher;

	// Array of Plugin templates populated from GameFeatureDeveloperSettings. Allows projects to
	//	specify reusable plugin templates for the plugin creation wizard.
	TArray<TSharedPtr<FGameFeaturePluginTemplateDescription>> PluginTemplates;
	FPluginEditorExtensionHandle PluginEditorExtensionDelegate;
};

IMPLEMENT_MODULE(FGameFeaturesEditorModule, GameFeaturesEditor)

#undef LOCTEXT_NAMESPACE

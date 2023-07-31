// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceEditorModule.h"
#include "Modules/ModuleManager.h"
#include "UObject/Class.h"
#include "LevelSequence.h"
#include "Factories/Factory.h"
#include "AssetRegistry/AssetData.h"
#include "AssetToolsModule.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "IAssetTypeActions.h"
#include "AssetTools/LevelSequenceActions.h"
#include "LevelSequenceEditorCommands.h"
#include "Misc/LevelSequenceEditorSettings.h"
#include "Misc/LevelSequenceEditorHelpers.h"
#include "Styles/LevelSequenceEditorStyle.h"
#include "CineCameraActor.h"
#include "CameraRig_Crane.h"
#include "CameraRig_Rail.h"
#include "IPlacementModeModule.h"
#include "ISettingsModule.h"
#include "ViewportTypeDefinition.h"
#include "LevelEditor.h"
#include "LevelSequencePlayer.h"
#include "LevelSequenceActor.h"
#include "PropertyEditorModule.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "CinematicViewport/CinematicViewportLayoutEntity.h"
#include "ISequencerModule.h"
#include "Misc/LevelSequenceEditorActorBinding.h"
#include "ILevelSequenceModule.h"
#include "Misc/LevelSequenceEditorActorSpawner.h"
#include "SequencerSettings.h"
#include "Misc/MovieSceneSequenceEditor_LevelSequence.h"
#include "BlueprintAssetHandler.h"
#include "Subsystems/AssetEditorSubsystem.h"

#define LOCTEXT_NAMESPACE "LevelSequenceEditor"


TSharedPtr<FLevelSequenceEditorStyle> FLevelSequenceEditorStyle::Singleton;


/**
 * Implements the LevelSequenceEditor module.
 */
class FLevelSequenceEditorModule
	: public ILevelSequenceEditorModule, public FGCObject
{
public:

	FLevelSequenceEditorModule()
		: Settings(nullptr)
	{
	}

	// IModuleInterface interface

	virtual void StartupModule() override
	{
		FLevelSequenceEditorStyle::Get();

		RegisterEditorObjectBindings();
		RegisterEditorActorSpawner();
		RegisterAssetTools();
		RegisterMenuExtensions();
		RegisterLevelEditorExtensions();
		RegisterPlacementModeExtensions();
		RegisterSettings();
		RegisterSequenceEditor();

		class FLevelSequenceAssetBlueprintHandler : public IBlueprintAssetHandler
		{
			virtual UBlueprint* RetrieveBlueprint(UObject* InObject) const override
			{
				return CastChecked<ULevelSequence>(InObject)->GetDirectorBlueprint();
			}

			virtual bool AssetContainsBlueprint(const FAssetData& InAssetData) const
			{
				// Only have a blueprint if it contains the BlueprintPathWithinPackage tag
				return InAssetData.TagsAndValues.Contains(FBlueprintTags::BlueprintPathWithinPackage);
			}
		};

		FBlueprintAssetHandler::Get().RegisterHandler<FLevelSequenceAssetBlueprintHandler>(ULevelSequence::StaticClass()->GetClassPathName());
	}
	
	virtual void ShutdownModule() override
	{
		UnregisterEditorObjectBindings();
		UnregisterEditorActorSpawner();
		UnregisterAssetTools();
		UnregisterMenuExtensions();
		UnregisterLevelEditorExtensions();
		UnregisterPlacementModeExtensions();
		UnregisterSettings();
		UnregisterSequenceEditor();
	}

protected:

	/** Register sequencer editor object bindings */
	void RegisterEditorObjectBindings()
	{
		ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
		ActorBindingDelegateHandle = SequencerModule.RegisterEditorObjectBinding(FOnCreateEditorObjectBinding::CreateStatic(&FLevelSequenceEditorModule::OnCreateActorBinding));
	}

	/** Register level sequence object spawner */
	void RegisterEditorActorSpawner()
	{
		ILevelSequenceModule& LevelSequenceModule = FModuleManager::LoadModuleChecked<ILevelSequenceModule>("LevelSequence");
		EditorActorSpawnerDelegateHandle = LevelSequenceModule.RegisterObjectSpawner(FOnCreateMovieSceneObjectSpawner::CreateStatic(&FLevelSequenceEditorActorSpawner::CreateObjectSpawner));
	}

	/** Registers asset tool actions. */
	void RegisterAssetTools()
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		EAssetTypeCategories::Type CinematicAssetCategoryBit = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("Cinematics")), LOCTEXT("CinematicsAssetCategory", "Cinematics"));
		LevelSequenceTypeActions = MakeShared<FLevelSequenceActions>(FLevelSequenceEditorStyle::Get(), CinematicAssetCategoryBit);
		AssetTools.RegisterAssetTypeActions(LevelSequenceTypeActions.ToSharedRef());
	}

	/** Registers level editor extensions. */
	void RegisterLevelEditorExtensions()
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

		FViewportTypeDefinition CinematicViewportType = FViewportTypeDefinition::FromType<FCinematicViewportLayoutEntity>(FLevelSequenceEditorCommands::Get().ToggleCinematicViewportCommand);
		LevelEditorModule.RegisterViewportType("Cinematic", CinematicViewportType);
	}

	/** Register menu extensions for the level editor toolbar. */
	void RegisterMenuExtensions()
	{
		FLevelSequenceEditorCommands::Register();

		CommandList = MakeShareable(new FUICommandList);
		CommandList->MapAction(FLevelSequenceEditorCommands::Get().CreateNewLevelSequenceInLevel,
			FExecuteAction::CreateStatic(&FLevelSequenceEditorModule::OnCreateActorInLevel)
		);
		CommandList->MapAction(FLevelSequenceEditorCommands::Get().CreateNewMasterSequenceInLevel,
			FExecuteAction::CreateStatic(&FLevelSequenceEditorModule::OnCreateMasterSequenceInLevel)
		);

		// Create and register the level editor toolbar menu extension
		CinematicsMenuExtender = MakeShareable(new FExtender);
		CinematicsMenuExtender->AddMenuExtension("LevelEditorNewCinematics", EExtensionHook::First, CommandList, FMenuExtensionDelegate::CreateStatic([](FMenuBuilder& MenuBuilder) {
			MenuBuilder.AddMenuEntry(FLevelSequenceEditorCommands::Get().CreateNewLevelSequenceInLevel);
		}));
		CinematicsMenuExtender->AddMenuExtension("LevelEditorNewCinematics", EExtensionHook::First, CommandList, FMenuExtensionDelegate::CreateStatic([](FMenuBuilder& MenuBuilder) {
			MenuBuilder.AddMenuEntry(FLevelSequenceEditorCommands::Get().CreateNewMasterSequenceInLevel);
		}));

		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditorModule.GetAllLevelEditorToolbarCinematicsMenuExtenders().Add(CinematicsMenuExtender);
	}

	/** Registers placement mode extensions. */
	void RegisterPlacementModeExtensions()
	{
		FPlacementCategoryInfo Info(
			LOCTEXT("CinematicCategoryName", "Cinematic"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "PlacementBrowser.Icons.Cinematics"),
			"Cinematic",
			TEXT("PMCinematic"),
			25
		);

		IPlacementModeModule::Get().RegisterPlacementCategory(Info);
		IPlacementModeModule::Get().RegisterPlaceableItem(Info.UniqueHandle, MakeShareable( new FPlaceableItem(nullptr, FAssetData(ACineCameraActor::StaticClass())) ));
		IPlacementModeModule::Get().RegisterPlaceableItem(Info.UniqueHandle, MakeShareable( new FPlaceableItem(nullptr, FAssetData(ACameraRig_Crane::StaticClass())) ));
		IPlacementModeModule::Get().RegisterPlaceableItem(Info.UniqueHandle, MakeShareable( new FPlaceableItem(nullptr, FAssetData(ACameraRig_Rail::StaticClass())) ));
		IPlacementModeModule::Get().RegisterPlaceableItem(Info.UniqueHandle, MakeShareable( new FPlaceableItem(nullptr, FAssetData(ALevelSequenceActor::StaticClass())) ));
	}

	/** Register settings objects. */
	void RegisterSettings()
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->RegisterSettings("Project", "Plugins", "LevelSequencer",
				LOCTEXT("LevelSequencerSettingsName", "Level Sequencer"),
				LOCTEXT("LevelSequencerSettingsDescription", "Configure the Level Sequence Editor."),
				GetMutableDefault<ULevelSequenceEditorSettings>()
			);

			Settings = USequencerSettingsContainer::GetOrCreate<USequencerSettings>(TEXT("LevelSequenceEditor"));

			SettingsModule->RegisterSettings("Editor", "ContentEditors", "LevelSequenceEditor",
				LOCTEXT("LevelSequenceEditorSettingsName", "Level Sequence Editor"),
				LOCTEXT("LevelSequenceEditorSettingsDescription", "Configure the look and feel of the Level Sequence Editor."),
				Settings);	
		}
	}

	void RegisterSequenceEditor()
	{
		ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
		SequenceEditorHandle = SequencerModule.RegisterSequenceEditor(ULevelSequence::StaticClass(), MakeUnique<FMovieSceneSequenceEditor_LevelSequence>());
	}

protected:

	/** Unregisters sequencer editor object bindings */
	void UnregisterEditorActorSpawner()
	{
		ILevelSequenceModule* LevelSequenceModule = FModuleManager::GetModulePtr<ILevelSequenceModule>("LevelSequence");
		if (LevelSequenceModule)
		{
			LevelSequenceModule->UnregisterObjectSpawner(EditorActorSpawnerDelegateHandle);
		}
	}

	/** Unregisters sequencer editor object bindings */
	void UnregisterEditorObjectBindings()
	{
		ISequencerModule* SequencerModule = FModuleManager::GetModulePtr<ISequencerModule>("Sequencer");
		if (SequencerModule)
		{
			SequencerModule->UnRegisterEditorObjectBinding(ActorBindingDelegateHandle);
		}
	}

	/** Unregisters asset tool actions. */
	void UnregisterAssetTools()
	{
		FAssetToolsModule* AssetToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>("AssetTools");

		if (AssetToolsModule != nullptr)
		{
			IAssetTools& AssetTools = AssetToolsModule->Get();
			AssetTools.UnregisterAssetTypeActions(LevelSequenceTypeActions.ToSharedRef());
		}
	}

	/** Unregisters level editor extensions. */
	void UnregisterLevelEditorExtensions()
	{
		FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor");
		if (LevelEditorModule)
		{
			LevelEditorModule->UnregisterViewportType("Cinematic");
		}
	}

	/** Unregisters menu extensions for the level editor toolbar. */
	void UnregisterMenuExtensions()
	{
		if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor"))
		{
			LevelEditorModule->GetAllLevelEditorToolbarCinematicsMenuExtenders().Remove(CinematicsMenuExtender);
	}

		CinematicsMenuExtender = nullptr;
		CommandList = nullptr;

		FLevelSequenceEditorCommands::Unregister();
	}

	/** Unregisters placement mode extensions. */
	void UnregisterPlacementModeExtensions()
	{
		if (IPlacementModeModule::IsAvailable())
		{
			IPlacementModeModule::Get().UnregisterPlacementCategory("Cinematic");
		}
	}

	/** Unregister settings objects. */
	void UnregisterSettings()
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Plugins", "LevelSequencer");

			SettingsModule->UnregisterSettings("Editor", "ContentEditors", "LevelSequenceEditor");
		}
	}

	void UnregisterSequenceEditor()
	{
		ISequencerModule* SequencerModule = FModuleManager::GetModulePtr<ISequencerModule>("Sequencer");
		if (SequencerModule)
		{
			SequencerModule->UnregisterSequenceEditor(SequenceEditorHandle);
		}
	}

protected:

	/** Callback for creating a new level sequence asset in the level. */
	static void OnCreateActorInLevel()
	{
		// Create a new level sequence
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

		UObject* NewAsset = nullptr;

		// Attempt to create a new asset
		for (TObjectIterator<UClass> It ; It ; ++It)
		{
			UClass* CurrentClass = *It;
			if (CurrentClass->IsChildOf(UFactory::StaticClass()) && !(CurrentClass->HasAnyClassFlags(CLASS_Abstract)))
			{
				UFactory* Factory = Cast<UFactory>(CurrentClass->GetDefaultObject());
				if (Factory->CanCreateNew() && Factory->ImportPriority >= 0 && Factory->SupportedClass == ULevelSequence::StaticClass())
				{
					NewAsset = AssetTools.CreateAssetWithDialog(ULevelSequence::StaticClass(), Factory);
					break;
				}
			}
		}

		if (!NewAsset)
		{
			return;
		}

		// Spawn an actor at the origin, and either move infront of the camera or focus camera on it (depending on the viewport) and open for edit
		UActorFactory* ActorFactory = GEditor->FindActorFactoryForActorClass(ALevelSequenceActor::StaticClass());
		if (!ensure(ActorFactory))
		{
			return;
		}

		AActor* Actor = GEditor->UseActorFactory(ActorFactory, FAssetData(NewAsset), &FTransform::Identity);
		if (Actor == nullptr)
		{
			return;
		}
		ALevelSequenceActor* NewActor = CastChecked<ALevelSequenceActor>(Actor);
		if (GCurrentLevelEditingViewportClient != nullptr && GCurrentLevelEditingViewportClient->IsPerspective())
		{
			GEditor->MoveActorInFrontOfCamera(*NewActor, GCurrentLevelEditingViewportClient->GetViewLocation(), GCurrentLevelEditingViewportClient->GetViewRotation().Vector());
		}
		else
		{
			GEditor->MoveViewportCamerasToActor(*NewActor, false);
		}

		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(NewAsset);
	}

	/** Callback for creating a new level sequence asset in the level. */
	static void OnCreateMasterSequenceInLevel()
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		LevelSequenceEditorHelpers::OpenMasterSequenceDialog(LevelEditorModule.GetLevelEditorTabManager().ToSharedRef());
	}

	FOnMasterSequenceCreated& OnMasterSequenceCreated() override
	{
		return OnMasterSequenceCreatedEvent;
	}

	FAllowPlaybackContext& OnComputePlaybackContext() override
	{
		return OnComputePlaybackContextDelegate;
	}

	static TSharedRef<ISequencerEditorObjectBinding> OnCreateActorBinding(TSharedRef<ISequencer> InSequencer)
	{
		return MakeShareable(new FLevelSequenceEditorActorBinding(InSequencer));
	}

	/** FGCObject interface */
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override
	{
		if (Settings)
		{
			Collector.AddReferencedObject(Settings);
		}
	}

	virtual FString GetReferencerName() const override
	{
		return "FLevelSequenceEditorModule";
	}

private:

	/** The type actions for interacting with level sequences. */
	TSharedPtr<FLevelSequenceActions> LevelSequenceTypeActions;

	/** Extender for the cinematics menu */
	TSharedPtr<FExtender> CinematicsMenuExtender;

	TSharedPtr<FUICommandList> CommandList;

	FOnMasterSequenceCreated OnMasterSequenceCreatedEvent;

	FAllowPlaybackContext OnComputePlaybackContextDelegate;

	FDelegateHandle ActorBindingDelegateHandle;

	FDelegateHandle EditorActorSpawnerDelegateHandle;

	USequencerSettings* Settings;

	FDelegateHandle SequenceEditorHandle;
};


IMPLEMENT_MODULE(FLevelSequenceEditorModule, LevelSequenceEditor);

#undef LOCTEXT_NAMESPACE

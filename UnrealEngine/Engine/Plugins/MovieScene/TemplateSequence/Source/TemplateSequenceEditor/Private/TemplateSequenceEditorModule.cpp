// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTools/CameraAnimationSequenceActions.h"
#include "AssetTools/TemplateSequenceActions.h"
#include "AssetToolsModule.h"
#include "CameraAnimationSequence.h"
#include "Customizations/CameraAnimationSequenceCustomization.h"
#include "Customizations/TemplateSequenceCustomization.h"
#include "ISequencerModule.h"
#include "ISettingsModule.h"
#include "Misc/MovieSceneSequenceEditor_TemplateSequence.h"
#include "Misc/TemplateSequenceEditorSettings.h"
#include "Modules/ModuleManager.h"
#include "SequencerSettings.h"
#include "Styles/TemplateSequenceEditorStyle.h"
#include "TemplateSequence.h"
#include "TrackEditors/TemplateSequenceTrackEditor.h"
#include "UObject/Class.h"
#include "UObject/GCObject.h"

#define LOCTEXT_NAMESPACE "TemplateSequenceEditor"

/**
 * Implements the FTemplateSequenceEditor module.
 */
class FTemplateSequenceEditorModule : public IModuleInterface, public FGCObject
{
public:
	FTemplateSequenceEditorModule()
		: Settings(nullptr)
	{
	}

	virtual void StartupModule() override
	{
		RegisterSettings();
		RegisterAssetTools();
		RegisterSequenceEditor();
		RegisterTrackEditors();
		RegisterSequenceCustomizations();
	}

	virtual void ShutdownModule() override
	{
		UnregisterSequenceCustomizations();
		UnregisterTrackEditors();
		UnregisterSequenceEditor();
		UnregisterAssetTools();
		UnregisterSettings();
	}

	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		if (Settings != nullptr)
		{
			Collector.AddReferencedObject(Settings);
		}
	}
	virtual FString GetReferencerName() const override
	{
		return TEXT("FTemplateSequenceEditorModule");
	}

private:
	void RegisterSettings()
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->RegisterSettings("Project", "Plugins", "TemplateSequencer",
				LOCTEXT("TemplateSequencerSettingsName", "Template Sequencer"),
				LOCTEXT("TemplateSequencerSettingsDescription", "Configure the Template Sequence Editor."),
				GetMutableDefault<UTemplateSequenceEditorSettings>()
			);

			Settings = USequencerSettingsContainer::GetOrCreate<USequencerSettings>(TEXT("TemplateSequenceEditor"));

			SettingsModule->RegisterSettings("Editor", "ContentEditors", "TemplateSequenceEditor",
				LOCTEXT("TemplateSequenceEditorSettingsName", "Template Sequence Editor"),
				LOCTEXT("TemplateSequenceEditorSettingsDescription", "Configure the look and feel of the Template Sequence Editor."),
				Settings);
		}
	}

	void UnregisterSettings()
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Plugins", "TemplateSequencer");
			SettingsModule->UnregisterSettings("Editor", "ContentEditors", "TemplateSequenceEditor");
		}
	}

	void RegisterAssetTools()
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		EAssetTypeCategories::Type CinematicAssetCategoryBit = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("Cinematics")), LOCTEXT("CinematicsAssetCategory", "Cinematics"));

		TemplateSequenceTypeActions = MakeShared<FTemplateSequenceActions>(FTemplateSequenceEditorStyle::Get(), CinematicAssetCategoryBit);
		AssetTools.RegisterAssetTypeActions(TemplateSequenceTypeActions.ToSharedRef());

		CameraAnimationSequenceActions = MakeShared<FCameraAnimationSequenceActions>(FTemplateSequenceEditorStyle::Get(), CinematicAssetCategoryBit);
		AssetTools.RegisterAssetTypeActions(CameraAnimationSequenceActions.ToSharedRef());
	}

	void UnregisterAssetTools()
	{
		FAssetToolsModule* AssetToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>("AssetTools");

		if (AssetToolsModule != nullptr)
		{
			IAssetTools& AssetTools = AssetToolsModule->Get();
			AssetTools.UnregisterAssetTypeActions(TemplateSequenceTypeActions.ToSharedRef());
			AssetTools.UnregisterAssetTypeActions(CameraAnimationSequenceActions.ToSharedRef());
		}
	}

	void RegisterSequenceEditor()
	{
		ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
		SequenceEditorHandle = SequencerModule.RegisterSequenceEditor(UTemplateSequence::StaticClass(), MakeUnique<FMovieSceneSequenceEditor_TemplateSequence>());
	}

	void UnregisterSequenceEditor()
	{
		ISequencerModule* SequencerModule = FModuleManager::GetModulePtr<ISequencerModule>("Sequencer");
		if (SequencerModule)
		{
			SequencerModule->UnregisterSequenceEditor(SequenceEditorHandle);
		}
	}

	void RegisterTrackEditors()
	{
		ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
		TemplateSequenceTrackCreateEditorHandle = SequencerModule.RegisterTrackEditor(FOnCreateTrackEditor::CreateStatic(&FTemplateSequenceTrackEditor::CreateTrackEditor));
	}

	void UnregisterTrackEditors()
	{
		ISequencerModule* SequencerModule = FModuleManager::GetModulePtr<ISequencerModule>("Sequencer");
		if (SequencerModule)
		{
			SequencerModule->UnRegisterTrackEditor(TemplateSequenceTrackCreateEditorHandle);
		}
	}

	void RegisterSequenceCustomizations()
	{
		ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
		SequencerModule.GetSequencerCustomizationManager()->RegisterInstancedSequencerCustomization(UTemplateSequence::StaticClass(),
				FOnGetSequencerCustomizationInstance::CreateLambda([]()
				{
					return new FTemplateSequenceCustomization();
				}));
		SequencerModule.GetSequencerCustomizationManager()->RegisterInstancedSequencerCustomization(UCameraAnimationSequence::StaticClass(),
				FOnGetSequencerCustomizationInstance::CreateLambda([]()
				{
					return new FCameraAnimationSequenceCustomization();
				}));
	}

	void UnregisterSequenceCustomizations()
	{
		ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
		SequencerModule.GetSequencerCustomizationManager()->UnregisterInstancedSequencerCustomization(UCameraAnimationSequence::StaticClass());
		SequencerModule.GetSequencerCustomizationManager()->UnregisterInstancedSequencerCustomization(UTemplateSequence::StaticClass());
	}

private:

	TSharedPtr<FTemplateSequenceActions> TemplateSequenceTypeActions;
	TSharedPtr<FCameraAnimationSequenceActions> CameraAnimationSequenceActions;

	FDelegateHandle SequenceEditorHandle;
	FDelegateHandle TemplateSequenceTrackCreateEditorHandle;

	USequencerSettings* Settings;
};

IMPLEMENT_MODULE(FTemplateSequenceEditorModule, TemplateSequenceEditor);

#undef LOCTEXT_NAMESPACE

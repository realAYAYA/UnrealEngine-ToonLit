// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Delegates/IDelegateInstance.h"
#include "UObject/GCObject.h"
#include "ITakeRecorderModule.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/Input/SCheckBox.h"

class FExtender;
class FTakePresetActions;
class UTakePreset;
class FSerializedRecorder;
class UTakeRecorderSources;
class USequencerSettings;

class FTakeRecorderModule : public ITakeRecorderModule, public FGCObject
{
public:
	FTakeRecorderModule();

	void PopulateSourcesMenu(TSharedRef<FExtender> InExtender, UTakeRecorderSources* InSources);
	virtual UTakePreset* GetPendingTake() const override;

	//~ ITakeRecorderModule API
	FOnGenerateWidgetExtensions& GetToolbarExtensionGenerators() override { return ToolbarExtensionGenerators; }
	FOnGenerateWidgetExtensions& GetRecordButtonExtensionGenerators() override { return ButtonExtensionGenerators; }
	FOnExternalObjectAddRemoveEvent& GetExternalObjectAddRemoveEventDelegate() override { return ExternalObjectAddRemoveEvent; }
	FOnRecordErrorCheck&  GetRecordErrorCheckGenerator() override { return RecordErrorCheck; }
	TArray<TWeakObjectPtr<>>& GetExternalObjects() override { return ExternalObjects; }
	FLastRecordedLevelSequenceProvider& GetLastLevelSequenceProvider() override { return LastLevelSequenceProvider; }
	FCanReviewLastRecordedLevelSequence& GetCanReviewLastRecordedLevelSequenceDelegate() override { return CanReviewLastRecordedSequence; };

	void RegisterExternalObject(UObject* InExternalObject) override;
	void UnregisterExternalObject(UObject* InExternalObject) override;

	FOnForceSaveAsPreset& OnForceSaveAsPreset() override
	{
		return ForceSaveAsPresetEvent;
	}
private:

	void StartupModule() override;
	void ShutdownModule() override;

	FDelegateHandle RegisterSourcesMenuExtension(const FOnExtendSourcesMenu& InExtension) override;
	void UnregisterSourcesMenuExtension(FDelegateHandle Handle) override;
	void RegisterSettingsObject(UObject* InSettingsObject) override;

	/** FGCObject interface */
	void AddReferencedObjects(FReferenceCollector& Collector) override;
	FString GetReferencerName() const override
	{
		return "FTakeRecorderModule";
	}

private:

	void RegisterDetailCustomizations();

	void UnregisterDetailCustomizations();

	void RegisterLevelEditorExtensions();

	void UnregisterLevelEditorExtensions();

	void RegisterAssetTools();

	void UnregisterAssetTools();

	void RegisterSettings();

	void UnregisterSettings();

	void RegisterSerializedRecorder();

	void UnregisterSerializedRecorder();

	void RegisterMenus();

	void OnEditorClose();

private:

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnExtendSourcesMenuEvent, TSharedRef<FExtender>, UTakeRecorderSources*);

	FOnExtendSourcesMenuEvent SourcesMenuExtenderEvent;
	FOnGenerateWidgetExtensions ToolbarExtensionGenerators;
	FOnGenerateWidgetExtensions ButtonExtensionGenerators;
	FLastRecordedLevelSequenceProvider LastLevelSequenceProvider;
	FCanReviewLastRecordedLevelSequence CanReviewLastRecordedSequence;

	FOnExternalObjectAddRemoveEvent ExternalObjectAddRemoveEvent;
	FOnForceSaveAsPreset ForceSaveAsPresetEvent;
	FOnRecordErrorCheck RecordErrorCheck;

	FDelegateHandle LevelEditorLayoutExtensionHandle;
	FDelegateHandle LevelEditorTabManagerChangedHandle;
	FDelegateHandle ModulesChangedHandle;

	/** Cached name of the project settings fo de-registration of details customizations on shutdown (after UObject destruction) */
	FName ProjectSettingsName;

	TSharedPtr<FTakePresetActions> TakePresetActions;
	TSharedPtr<FSerializedRecorder> SerializedRecorder;

	TArray<TWeakObjectPtr<>> ExternalObjects;

	USequencerSettings* SequencerSettings;
};

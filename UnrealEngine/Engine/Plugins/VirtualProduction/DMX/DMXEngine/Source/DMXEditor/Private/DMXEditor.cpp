// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXEditor.h"
#include "DMXEditorLog.h"
#include "DMXEditorModule.h"
#include "DMXEditorSettings.h"
#include "DMXEditorTabNames.h"
#include "DMXEditorUtils.h"
#include "DMXFixtureTypeSharedData.h"
#include "DMXFixturePatchSharedData.h"
#include "DMXRuntimeLog.h"
#include "DMXRuntimeUtils.h"
#include "Exporters/DMXMVRExporter.h"
#include "Library/DMXLibrary.h"
#include "Library/DMXEntityFixtureType.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityReference.h"
#include "Modes/DMXEditorApplicationMode.h"
#include "Toolbars/DMXEditorToolbar.h"
#include "Commands/DMXEditorCommands.h"

#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Docking/SDockTab.h"

#include "Widgets/SDMXEntityEditor.h"
#include "Widgets/FixturePatch/SDMXFixturePatchEditor.h"
#include "Widgets/FixtureType/SDMXFixtureTypeEditor.h"
#include "Widgets/LibrarySettings/SDMXLibraryEditorTab.h"
#include "Widgets/OutputConsole/SDMXOutputConsole.h"

#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "ScopedTransaction.h"
#include "Utils.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "Widgets/Notifications/SNotificationList.h"


#define LOCTEXT_NAMESPACE "FDMXEditor"

const FName FDMXEditor::ToolkitFName(TEXT("DMXEditor"));

FName FDMXEditor::GetToolkitFName() const
{
	return ToolkitFName;
}

FText FDMXEditor::GetBaseToolkitName() const
{
	return LOCTEXT("DMXEditor", "DMX Editor");
}

FString FDMXEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix_LevelScript", "Script ").ToString();
}

FLinearColor FDMXEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.0f, 0.0f, 0.3f, 0.5f);
}

void FDMXEditor::InitEditor(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost>& InitToolkitHost, UDMXLibrary* DMXLibrary)
{
	if (!Toolbar.IsValid())
	{
		Toolbar = MakeShared<FDMXEditorToolbar>(SharedThis(this));
	}

	if (!FixtureTypeSharedData.IsValid())
	{
		FixtureTypeSharedData = MakeShared<FDMXFixtureTypeSharedData>(SharedThis(this));
	}
	
	if (!FixturePatchSharedData.IsValid())
	{
		FixturePatchSharedData = MakeShared<FDMXFixturePatchSharedData>(SharedThis(this));
	}

	// Initialize the asset editor and spawn nothing (dummy layout)
	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	const TSharedRef<FTabManager::FLayout> DummyLayout = FTabManager::NewLayout("NullLayout")->AddArea(FTabManager::NewPrimaryArea());
	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, FDMXEditorModule::DMXEditorAppIdentifier, DummyLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, (UObject*)DMXLibrary);

	CommonInitialization(DMXLibrary);

	InitalizeExtenders();

	RegenerateMenusAndToolbars();

	const bool bShouldOpenInDefaultsMode = true;
	bool bNewlyCreated = true;
	RegisterApplicationModes(DMXLibrary, bShouldOpenInDefaultsMode, bNewlyCreated);
}

void FDMXEditor::CommonInitialization(UDMXLibrary* DMXLibrary)
{
	CreateDefaultCommands();
	CreateDefaultTabContents(DMXLibrary);
}

void FDMXEditor::InitalizeExtenders()
{
	FDMXEditorModule* DMXEditorModule = &FDMXEditorModule::Get();
	TSharedPtr<FExtender> MenuExtender = DMXEditorModule->GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects());
	AddMenuExtender(MenuExtender);

	TSharedPtr<FExtender> ToolbarExtender = DMXEditorModule->GetToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects());
	AddToolbarExtender(ToolbarExtender);
}

void FDMXEditor::RegisterApplicationModes(UDMXLibrary* DMXLibrary, bool bShouldOpenInDefaultsMode, bool bNewlyCreated)
{
	// Only one for now
	FWorkflowCentricApplication::AddApplicationMode(
		FDMXEditorApplicationMode::DefaultsMode,
		MakeShared<FDMXEditorDefaultApplicationMode>(SharedThis(this)));
	FWorkflowCentricApplication::SetCurrentMode(FDMXEditorApplicationMode::DefaultsMode);
}

UDMXLibrary* FDMXEditor::GetDMXLibrary() const
{
	return Cast<UDMXLibrary>(GetEditingObject());
}

void FDMXEditor::ImportDMXLibrary() const
{
	UDMXLibrary* DMXLibrary = GetDMXLibrary();
	if (!DMXLibrary)
	{
		return;
	}

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return;
	}

	UDMXEditorSettings* DMXEditorSettings = GetMutableDefault<UDMXEditorSettings>();
	if (!DMXEditorSettings)
	{
		return;
	}

	const FString LastMVRImportPath = DMXEditorSettings->LastMVRImportPath;
	const FString DefaultPath = FPaths::DirectoryExists(LastMVRImportPath) ? LastMVRImportPath : FPaths::ProjectSavedDir();
	
	if (!DMXLibrary->GetEntities().IsEmpty())
	{
		const FText MessageText = LOCTEXT("MVRImportDialog", "DMX Library already contains data. Importing the MVR will clear existing data. Do you want to proceed?");
		if (FMessageDialog::Open(EAppMsgType::YesNo, MessageText) == EAppReturnType::No)
		{
			return;
		}
	}

	TArray<FString> OpenFilenames;
	DesktopPlatform->OpenFileDialog(
		FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
		LOCTEXT("ImportMVR", "Import MVR").ToString(),
		DefaultPath,
		TEXT(""),
		TEXT("My Virtual Rig (*.mvr)|*.mvr"),
		EFileDialogFlags::None,
		OpenFilenames);

	if (OpenFilenames.IsEmpty())
	{
		return;
	}

	if (ImportObject<UDMXLibrary>(DMXLibrary->GetOuter(), DMXLibrary->GetFName(), RF_Public | RF_Standalone, *OpenFilenames[0], nullptr))
	{
		DMXEditorSettings->LastMVRImportPath = FPaths::GetPath(OpenFilenames[0]);
		DMXEditorSettings->SaveConfig();
	}
}

void FDMXEditor::ExportDMXLibrary() const
{
	UDMXLibrary* DMXLibrary = GetDMXLibrary();
	if (!DMXLibrary)
	{
		return;
	}

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		UDMXEditorSettings* DMXEditorSettings = GetMutableDefault<UDMXEditorSettings>();
		check(DMXEditorSettings);

		const FString LastMVRExportPath = DMXEditorSettings->LastMVRExportPath;
		const FString DefaultPath = FPaths::DirectoryExists(LastMVRExportPath) ? LastMVRExportPath : FPaths::ProjectSavedDir();

		TArray<FString> SaveFilenames;
		const bool bSaveFile = DesktopPlatform->SaveFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			LOCTEXT("ExportMVR", "Export MVR").ToString(),
			DefaultPath,
			DMXLibrary->GetName() + TEXT(".mvr"),
			TEXT("My Virtual Rig (*.mvr)|*.mvr"),
			EFileDialogFlags::None,
			SaveFilenames);

		if (!bSaveFile || SaveFilenames.IsEmpty())
		{
			return;
		}

		FText ErrorReason;
		FDMXMVRExporter::Export(DMXLibrary, SaveFilenames[0], ErrorReason);
		if (ErrorReason.IsEmpty())
		{
			DMXEditorSettings->LastMVRExportPath = FPaths::GetPath(SaveFilenames[0]);
			DMXEditorSettings->SaveConfig();

			FNotificationInfo NotificationInfo(FText::Format(LOCTEXT("ExportDMXLibraryAsMVRSuccessNotification", "Successfully exported MVR to {0}."), FText::FromString(SaveFilenames[0])));
			NotificationInfo.ExpireDuration = 5.f;

			FSlateNotificationManager::Get().AddNotification(NotificationInfo);
		}
		else
		{
			FNotificationInfo NotificationInfo(ErrorReason);
			NotificationInfo.ExpireDuration = 10.f;
			NotificationInfo.Image = FAppStyle::GetBrush("Icons.Warning");

			FSlateNotificationManager::Get().AddNotification(NotificationInfo);
		}
	}
}

void FDMXEditor::RegisterToolbarTab(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
}

void FDMXEditor::CreateDefaultTabContents(UDMXLibrary* DMXLibrary)
{
	DMXLibraryEditorTab = CreateDMXLibraryEditorTab();
	FixtureTypeEditor = CreateFixtureTypeEditor();
	FixturePatchEditor = CreateFixturePatchEditor();
}

void FDMXEditor::CreateDefaultCommands()
{
	FDMXEditorCommands::Register();

	ToolkitCommands->MapAction(
		FDMXEditorCommands::Get().ImportDMXLibrary,
		FExecuteAction::CreateSP(this, &FDMXEditor::ImportDMXLibrary)
	);
	ToolkitCommands->MapAction(
		FDMXEditorCommands::Get().ExportDMXLibrary,
		FExecuteAction::CreateSP(this, &FDMXEditor::ExportDMXLibrary)
	);
	ToolkitCommands->MapAction(
		FDMXEditorCommands::Get().AddNewEntityFixtureType,
		FExecuteAction::CreateLambda([this]() { OnAddNewEntity(UDMXEntityFixtureType::StaticClass()); })
	);
	ToolkitCommands->MapAction(
		FDMXEditorCommands::Get().AddNewEntityFixturePatch,
		FExecuteAction::CreateLambda([this]() { OnAddNewEntity(UDMXEntityFixturePatch::StaticClass()); })
	);
}

void FDMXEditor::OnAddNewEntity(TSubclassOf<UDMXEntity> InEntityClass)
{
	UDMXLibrary* DMXLibrary = GetDMXLibrary();
	check(DMXLibrary);

	if (InEntityClass == UDMXEntityFixtureType::StaticClass())
	{
		FDMXEntityFixtureTypeConstructionParams FixtureTypeConstructionParams;
		FixtureTypeConstructionParams.ParentDMXLibrary = DMXLibrary;

		UDMXEntityFixtureType* FixtureType = UDMXEntityFixtureType::CreateFixtureTypeInLibrary(FixtureTypeConstructionParams);
		FixtureTypeSharedData->SelectFixtureTypes(TArray<TWeakObjectPtr<UDMXEntityFixtureType>>({ FixtureType }));
	}
	else if (InEntityClass == UDMXEntityFixturePatch::StaticClass())
	{
		if (UDMXEntity* LastAddedEntity = DMXLibrary->GetLastAddedEntity().Get())
		{
			UDMXEntityFixtureType* LastAddedFixtureType = [LastAddedEntity]() -> UDMXEntityFixtureType*
			{
				if (UDMXEntityFixtureType* EntityAsFixtureType = Cast<UDMXEntityFixtureType>(LastAddedEntity))
				{
					return EntityAsFixtureType;
				}
				else if (UDMXEntityFixturePatch* EntityAsFixturePatch = Cast<UDMXEntityFixturePatch>(LastAddedEntity))
				{
					return EntityAsFixturePatch->GetFixtureType();
				}
				return nullptr;
			}();

			if (LastAddedFixtureType)
			{
				FDMXEntityFixturePatchConstructionParams FixturePatchConstructionParams;
				FixturePatchConstructionParams.FixtureTypeRef = FDMXEntityFixtureTypeRef(LastAddedFixtureType);

				UDMXEntityFixturePatch* NewFixturePatch = UDMXEntityFixturePatch::CreateFixturePatchInLibrary(FixturePatchConstructionParams);
				FixturePatchSharedData->SelectFixturePatch(NewFixturePatch);

				return;
			}
		}
		else
		{
			const TArray<UDMXEntityFixtureType*> FixtureTypesInLibrary = DMXLibrary->GetEntitiesTypeCast<UDMXEntityFixtureType>();
			if (FixtureTypesInLibrary.Num() > 0)
			{
				FDMXEntityFixturePatchConstructionParams FixturePatchConstructionParams;
				FixturePatchConstructionParams.FixtureTypeRef = FDMXEntityFixtureTypeRef(FixtureTypesInLibrary[0]);

				UDMXEntityFixturePatch* NewFixturePatch = UDMXEntityFixturePatch::CreateFixturePatchInLibrary(FixturePatchConstructionParams);
				FixturePatchSharedData->SelectFixturePatch(NewFixturePatch);
			}
			else
			{
				UE_LOG(LogDMXRuntime, Warning, TEXT("Cannot create a fixture patch in Library %s when the Library doesn't define any Fixture Types."), *DMXLibrary->GetName());
			}
		}
	}
}

bool FDMXEditor::InvokeEditorTabFromEntityType(TSubclassOf<UDMXEntity> InEntityClass)
{
	// Make sure we're in the right tab for the current type
	FName TargetTabId = NAME_None;
	if (InEntityClass->IsChildOf(UDMXEntityFixtureType::StaticClass()))
	{
		TargetTabId = FDMXEditorTabNames::DMXFixtureTypesEditor;
	}
	else if (InEntityClass->IsChildOf(UDMXEntityFixturePatch::StaticClass()))
	{
		TargetTabId = FDMXEditorTabNames::DMXFixturePatchEditor;
	}
	else
	{
		UE_LOG_DMXEDITOR(Error, TEXT("%S: Unimplemented Entity type. Can't set currect Tab."), __FUNCTION__);
	}

	if (!TargetTabId.IsNone())
	{
		FName CurrentTab = FGlobalTabmanager::Get()->GetActiveTab()->GetLayoutIdentifier().TabType;
		if (!CurrentTab.IsEqual(TargetTabId))
		{
			TabManager->TryInvokeTab(MoveTemp(TargetTabId));
		}
		
		return true;
	}

	return false;
}

bool FDMXEditor::NewEntity_IsVisibleForType(TSubclassOf<UDMXEntity> InEntityClass) const
{
	return true;
}

void FDMXEditor::RenameNewlyAddedEntity(UDMXEntity* InEntity, TSubclassOf<UDMXEntity> InEntityClass)
{
	TSharedPtr<SDMXEntityEditor> EntityEditor = GetEditorWidgetForEntityType(InEntityClass);
	if (!EntityEditor.IsValid())
	{
		return;
	}

	// if this check ever fails, something is really wrong!
	// How can an Entity be created without the button in the editor?!
	check(EntityEditor.IsValid());
	
	EntityEditor->RequestRenameOnNewEntity(InEntity, ESelectInfo::OnMouseClick);
}

TSharedPtr<SDMXEntityEditor> FDMXEditor::GetEditorWidgetForEntityType(TSubclassOf<UDMXEntity> InEntityClass) const
{
	TSharedPtr<SDMXEntityEditor> EntityEditor = nullptr;

	if (InEntityClass->IsChildOf(UDMXEntityFixtureType::StaticClass()))
	{
		return FixtureTypeEditor;
	}
	else if (InEntityClass->IsChildOf(UDMXEntityFixturePatch::StaticClass()))
	{
		return FixturePatchEditor;
	}
	else
	{
		UE_LOG_DMXEDITOR(Error, TEXT("%S not implemented for %s"), __FUNCTION__, *InEntityClass->GetFName().ToString());
	}

	return FixtureTypeEditor;
}

void FDMXEditor::SelectEntityInItsTypeTab(UDMXEntity* InEntity, ESelectInfo::Type InSelectionType /*= ESelectInfo::Type::Direct*/)
{
	check(InEntity != nullptr);

	if (!InvokeEditorTabFromEntityType(InEntity->GetClass()))
	{
		return;
	}

	if (TSharedPtr<SDMXEntityEditor> EntityEditor = GetEditorWidgetForEntityType(InEntity->GetClass()))
	{
		EntityEditor->SelectEntity(InEntity, InSelectionType);
	}
}

void FDMXEditor::SelectEntitiesInTypeTab(const TArray<UDMXEntity*>& InEntities, ESelectInfo::Type InSelectionType /*= ESelectInfo::Type::Direct*/)
{
	if (InEntities.Num() == 0 || InEntities[0] == nullptr)
	{
		return; 
	}

	if (!InvokeEditorTabFromEntityType(InEntities[0]->GetClass()))
	{
		return;
	}

	if (TSharedPtr<SDMXEntityEditor> EntityEditor = GetEditorWidgetForEntityType(InEntities[0]->GetClass()))
	{
		EntityEditor->SelectEntities(InEntities, InSelectionType);
	}
}

TArray<UDMXEntity*> FDMXEditor::GetSelectedEntitiesFromTypeTab(TSubclassOf<UDMXEntity> InEntityClass) const
{
	if (TSharedPtr<SDMXEntityEditor> EntityEditor = GetEditorWidgetForEntityType(InEntityClass))
	{
		return EntityEditor->GetSelectedEntities();
	}

	return TArray<UDMXEntity*>();
}

TSharedRef<SDMXLibraryEditorTab> FDMXEditor::CreateDMXLibraryEditorTab()
{
	UDMXLibrary* DMXLibrary = GetDMXLibrary();
	check(DMXLibrary);

	return SNew(SDMXLibraryEditorTab)
		.DMXLibrary(DMXLibrary)
		.DMXEditor(SharedThis(this));
}

TSharedRef<SDMXFixtureTypeEditor> FDMXEditor::CreateFixtureTypeEditor()
{
	return SNew(SDMXFixtureTypeEditor, SharedThis(this));
}

TSharedRef<SDMXFixturePatchEditor> FDMXEditor::CreateFixturePatchEditor()
{
	return SNew(SDMXFixturePatchEditor)
		.DMXEditor(SharedThis(this));
}

TSharedPtr<FDMXFixtureTypeSharedData> FDMXEditor::GetFixtureTypeSharedData() const
{
	return FixtureTypeSharedData;
}

const TSharedPtr<FDMXFixturePatchSharedData>& FDMXEditor::GetFixturePatchSharedData() const
{
	return FixturePatchSharedData;
}

#undef LOCTEXT_NAMESPACE

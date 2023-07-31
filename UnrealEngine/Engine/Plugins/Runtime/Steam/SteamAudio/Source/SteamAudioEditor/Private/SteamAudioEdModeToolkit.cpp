//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "SteamAudioEdModeToolkit.h"
#include "SteamAudioEdMode.h"
#include "SteamAudioEditorModule.h"
#include "TickableNotification.h"
#include "PhononScene.h"
#include "PhononCommon.h"

#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailPropertyRow.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "Editor.h"
#include "LevelEditorViewport.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "EditorModeManager.h"
#include "Async/Async.h"

namespace SteamAudio
{
	static TSharedPtr<FTickableNotification> GEdModeTickable = MakeShareable(new FTickableNotification());

	FSteamAudioEdModeToolkit::FSteamAudioEdModeToolkit()
	{
		FSteamAudioEditorModule* Module = &FModuleManager::GetModuleChecked<FSteamAudioEditorModule>("SteamAudioEditor");
		if (Module != nullptr)
		{
			UWorld* World = GEditor->GetLevelViewportClients()[0]->GetWorld();
			FPhononSceneInfo PhononSceneInfo;
			LoadSceneInfoFromDisk(World, PhononSceneInfo);
			Module->SetCurrentPhononSceneInfo(PhononSceneInfo);
		}
	}

	FSteamAudioEdModeToolkit::~FSteamAudioEdModeToolkit()
	{
	}

	void FSteamAudioEdModeToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost)
	{
		SAssignNew(ToolkitWidget, SScrollBox)
		+ SScrollBox::Slot()
		.Padding(5)
		.HAlign(HAlign_Left)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			[
				SNew(SButton)
				.ContentPadding(2)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.IsEnabled(this, &FSteamAudioEdModeToolkit::IsAddAllEnabled)
				.ToolTipText(NSLOCTEXT("SteamAudio", "AddAllTooltip", "Adds Phonon Geometry components (with default settings) to all Static Mesh actors that do not already have one."))
				.OnClicked(this, &FSteamAudioEdModeToolkit::OnAddAll)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SteamAudio", "AddAll", "Add All"))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
			+ SVerticalBox::Slot()
			[
				SNew(SButton)
				.ContentPadding(2)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.IsEnabled(this, &FSteamAudioEdModeToolkit::IsRemoveAllEnabled)
				.ToolTipText(NSLOCTEXT("SteamAudio", "RemoveAllTooltip", "Removes all Phonon Geometry components from Static Mesh actors."))
				.OnClicked(this, &FSteamAudioEdModeToolkit::OnRemoveAll)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SteamAudio", "RemoveAll", "Remove All"))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
			+ SVerticalBox::Slot()
			[
				SNew(SButton)
				.ContentPadding(2)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.IsEnabled(this, &FSteamAudioEdModeToolkit::IsExportObjEnabled)
				.ToolTipText(NSLOCTEXT("SteamAudio", "ExportObjTooltip", "Exports all Phonon Geometry to an OBJ file in /Content. Used to debug scene export."))
				.OnClicked(this, &FSteamAudioEdModeToolkit::OnExportObj)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SteamAudio", "ExportOBJ", "Export OBJ"))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
			+ SVerticalBox::Slot()
			[
				SNew(SButton)
				.ContentPadding(2)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.IsEnabled(this, &FSteamAudioEdModeToolkit::IsExportSceneEnabled)
				.ToolTipText(NSLOCTEXT("SteamAudio", "ExportSceneTooltip", "Exports all Phonon Geometry for use by the Steam Audio system."))
				.OnClicked(this, &FSteamAudioEdModeToolkit::OnExportScene)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SteamAudio", "ExportScene", "Export Scene"))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
			+ SVerticalBox::Slot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SteamAudio", "SceneTriangles", "Scene Triangles"))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4)
				[
					SNew(STextBlock)
					.Text(this, &FSteamAudioEdModeToolkit::GetNumSceneTrianglesText)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
			+ SVerticalBox::Slot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SteamAudio", "DynamicSceneTriangles", "Dynamic Scene Triangles"))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4)
				[
					SNew(STextBlock)
					.Text(this, &FSteamAudioEdModeToolkit::GetNumDynSceneTrianglesText)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
			+ SVerticalBox::Slot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SteamAudio", "SceneDataSize", "Scene Data Size"))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4)
				[
					SNew(STextBlock)
					.Text(this, &FSteamAudioEdModeToolkit::GetSceneDataSizeText)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
			+ SVerticalBox::Slot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SteamAudio", "DynamicSceneDataSize", "Dynamic Scene Data Size"))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4)
				[
					SNew(STextBlock)
					.Text(this, &FSteamAudioEdModeToolkit::GetDynSceneDataSizeText)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
		];

		FModeToolkit::Init(InitToolkitHost);
	}

	FName FSteamAudioEdModeToolkit::GetToolkitFName() const
	{
		return FName("SteamAudioEdMode");
	}

	FText FSteamAudioEdModeToolkit::GetBaseToolkitName() const
	{
		return NSLOCTEXT("SteamAudio", "ToolkitDisplayName", "Steam Audio Editor Mode Tool");
	}

	FEdMode* FSteamAudioEdModeToolkit::GetEditorMode() const
	{
		return GLevelEditorModeTools().GetActiveMode(FSteamAudioEdMode::EM_SteamAudio);
	}

	FReply FSteamAudioEdModeToolkit::OnAddAll()
	{
		// Display editor notification
		GEdModeTickable->SetDisplayText(NSLOCTEXT("SteamAudio", "AddingPhononGeometryToAllStaticMeshActors", "Adding Phonon Geometry to all Static Mesh actors..."));
		GEdModeTickable->CreateNotification();

		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			UWorld* World = GEditor->GetLevelViewportClients()[0]->GetWorld();
			AddGeometryComponentsToStaticMeshes(World);

			// Notify UI that we're done
			GEdModeTickable->QueueWorkItem(FWorkItem([](FText& DisplayText) {
				DisplayText = NSLOCTEXT("SteamAudio", "ComponentsAdded", "Components added.");
			}, SNotificationItem::CS_Success, true));
		});

		return FReply::Handled();
	}

	bool FSteamAudioEdModeToolkit::IsAddAllEnabled() const
	{
		return true;
	}

	FReply FSteamAudioEdModeToolkit::OnRemoveAll()
	{
		// Display editor notification
		GEdModeTickable->SetDisplayText(NSLOCTEXT("SteamAudio", "RemovingPhononGeometryFromAllStaticMeshActors", "Removing Phonon Geometry from all Static Mesh actors..."));
		GEdModeTickable->CreateNotification();

		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			UWorld* World = GEditor->GetLevelViewportClients()[0]->GetWorld();
			RemoveGeometryComponentsFromStaticMeshes(World);

			// Notify UI that we're done
			GEdModeTickable->QueueWorkItem(FWorkItem([](FText& DisplayText) {
				DisplayText = NSLOCTEXT("SteamAudio", "ComponentsRemoved", "Components removed.");
			}, SNotificationItem::CS_Success, true));
		});

		return FReply::Handled();
	}

	bool FSteamAudioEdModeToolkit::IsRemoveAllEnabled() const
	{
		return true;
	}

	static void FinalizeSceneProgressCallback(IPLfloat32 Progress)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("FinalizeSceneProgress"), FText::AsPercent(Progress));
		GEdModeTickable->SetDisplayText(FText::Format(NSLOCTEXT("SteamAudio", "FinalizeSceneProgressFmt", "Finalizing scene ({FinalizeSceneProgress} complete)"), Arguments));
	}

	FReply FSteamAudioEdModeToolkit::OnExportObj()
	{
		// Display editor notification
		GEdModeTickable->SetDisplayText(NSLOCTEXT("SteamAudio", "ExportingSceneOBJ", "Exporting scene OBJ..."));
		GEdModeTickable->CreateNotification();

		Async(EAsyncExecution::Thread, [&]()
		{
			// Export the scene
			UWorld* World = GEditor->GetLevelViewportClients()[0]->GetWorld();
			IPLhandle PhononScene = nullptr;
			IPLhandle PhononStaticMesh = nullptr;
			FPhononSceneInfo PhononSceneInfo;

			if (!CreateScene(World, &PhononScene, &PhononStaticMesh, PhononSceneInfo.NumTriangles, PhononSceneInfo.NumDynTriangles, PhononSceneInfo.DynDataSize, true))
			{
				GEdModeTickable->QueueWorkItem(FWorkItem([](FText& DisplayText) {
					DisplayText = NSLOCTEXT("SteamAudio", "ErrorExportingOBJ", "Error exporting OBJ.");
				}, SNotificationItem::CS_Fail, true));
				return;
			}

			// Write OBJ
			iplSaveSceneAsObj(PhononScene, TCHAR_TO_ANSI(*(EditorOnlyPath + World->GetMapName() + ".obj")));

			// Clean up Phonon structures
			iplDestroyStaticMesh(&PhononStaticMesh);
			iplDestroyScene(&PhononScene);

			// Notify UI that we're done
			GEdModeTickable->QueueWorkItem(FWorkItem([](FText& DisplayText) {
				DisplayText = NSLOCTEXT("SteamAudio", "ExportSceneOBJComplete", "Export scene OBJ complete.");
			}, SNotificationItem::CS_Success, true));
		});

		return FReply::Handled();
	}

	bool FSteamAudioEdModeToolkit::IsExportObjEnabled() const
	{
		return true;
	}

	FReply FSteamAudioEdModeToolkit::OnExportScene()
	{
		// Display editor notification
		GEdModeTickable->SetDisplayText(NSLOCTEXT("SteamAudio", "ExportingScene", "Exporting scene..."));
		GEdModeTickable->CreateNotification();

		Async(EAsyncExecution::Thread, [&]()
		{
			// Create the scene
			UWorld* World = GEditor->GetLevelViewportClients()[0]->GetWorld();
			IPLhandle PhononScene = nullptr;
			IPLhandle PhononStaticMesh = nullptr;
			FPhononSceneInfo PhononSceneInfo;

			if (!CreateScene(World, &PhononScene, &PhononStaticMesh, PhononSceneInfo.NumTriangles, PhononSceneInfo.NumDynTriangles, PhononSceneInfo.DynDataSize))
			{
				GEdModeTickable->QueueWorkItem(FWorkItem([](FText& DisplayText) {
					DisplayText = NSLOCTEXT("SteamAudio", "ErrorExportingScene", "Error exporting scene.");
				}, SNotificationItem::CS_Fail, true));
				return;
			}

			// Finalize and save to disk
			PhononSceneInfo.DataSize = iplSaveScene(PhononScene, nullptr);
			bool SaveSceneSuccessful = SaveFinalizedSceneToDisk(World, PhononScene, PhononSceneInfo);

			// Clean up Phonon structures
			iplDestroyStaticMesh(&PhononStaticMesh);
			iplDestroyScene(&PhononScene);

			if (SaveSceneSuccessful)
			{
				FSteamAudioEditorModule* Module = &FModuleManager::GetModuleChecked<FSteamAudioEditorModule>("SteamAudioEditor");
				if (Module != nullptr)
				{
					Module->SetCurrentPhononSceneInfo(PhononSceneInfo);
				}

				GEdModeTickable->QueueWorkItem(FWorkItem([](FText& DisplayText) {
					DisplayText = NSLOCTEXT("SteamAudio", "ExportSceneComplete", "Export scene complete.");
				}, SNotificationItem::CS_Success, true));
			}
			else
			{
				GEdModeTickable->QueueWorkItem(FWorkItem([](FText& DisplayText) {
					DisplayText = NSLOCTEXT("SteamAudio", "ExportSceneFailed", "Export scene failed.");
				}, SNotificationItem::CS_Fail, true));
			}
		});

		return FReply::Handled();
	}

	bool FSteamAudioEdModeToolkit::IsExportSceneEnabled() const
	{
		return true;
	}

	FText FSteamAudioEdModeToolkit::GetNumSceneTrianglesText() const
	{
		FSteamAudioEditorModule* Module = &FModuleManager::GetModuleChecked<FSteamAudioEditorModule>("SteamAudioEditor");
		if (Module != nullptr)
		{
			auto PhononSceneInfo = Module->GetCurrentPhononSceneInfo();
			return FText::AsNumber(PhononSceneInfo.NumTriangles);
		}

		return FText::FromString("No .phononscene found");
	}

	FText FSteamAudioEdModeToolkit::GetNumDynSceneTrianglesText() const
	{
		FSteamAudioEditorModule* Module = &FModuleManager::GetModuleChecked<FSteamAudioEditorModule>("SteamAudioEditor");
		if (Module != nullptr)
		{
			auto PhononSceneInfo = Module->GetCurrentPhononSceneInfo();
			return FText::AsNumber(PhononSceneInfo.NumDynTriangles);
		}

		return FText::FromString("No .phononscene found");
	}

	FText FSteamAudioEdModeToolkit::GetSceneDataSizeText() const
	{
		FSteamAudioEditorModule* Module = &FModuleManager::GetModuleChecked<FSteamAudioEditorModule>("SteamAudioEditor");
		if (Module != nullptr)
		{
			auto PhononSceneInfo = Module->GetCurrentPhononSceneInfo();
			return FText::AsMemory(PhononSceneInfo.DataSize);
		}

		return FText::FromString("No .phononscene found");
	}

	FText FSteamAudioEdModeToolkit::GetDynSceneDataSizeText() const
	{
		FSteamAudioEditorModule* Module = &FModuleManager::GetModuleChecked<FSteamAudioEditorModule>("SteamAudioEditor");
		if (Module != nullptr)
		{
			auto PhononSceneInfo = Module->GetCurrentPhononSceneInfo();
			return FText::AsMemory(PhononSceneInfo.DynDataSize);
		}

		return FText::FromString("No .phononscene found");
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.


#include "CameraCalibrationEditorModule.h"

#include "ActorFactories/ActorFactoryBlueprint.h"
#include "AssetEditor/CameraCalibrationCommands.h"
#include "AssetEditor/Curves/LensDataCurveModel.h"
#include "AssetEditor/SCameraCalibrationCurveEditorView.h"
#include "AssetToolsModule.h"
#include "AssetTypeActions/AssetTypeActions_LensFile.h"
#include "CameraCalibrationEditorLog.h"
#include "CameraCalibrationSubsystem.h"
#include "CameraCalibrationTypes.h"
#include "IAssetTools.h"
#include "IAssetTypeActions.h"
#include "ICurveEditorModule.h"
#include "IPlacementModeModule.h"
#include "LensFile.h"
#include "LevelEditor.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "UI/CameraCalibrationEditorStyle.h"
#include "UI/CameraCalibrationMenuEntry.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"


#define LOCTEXT_NAMESPACE "CameraCalibrationEditor"

DEFINE_LOG_CATEGORY(LogCameraCalibrationEditor);

void FCameraCalibrationEditorModule::StartupModule()
{
	FCameraCalibrationCommands::Register();
	FCameraCalibrationEditorStyle::Get();

	// Register AssetTypeActions
	auto RegisterAssetTypeAction = [this](const TSharedRef<IAssetTypeActions>& InAssetTypeAction)
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		RegisteredAssetTypeActions.Add(InAssetTypeAction);
		AssetTools.RegisterAssetTypeActions(InAssetTypeAction);
	};

	// register asset type actions
	RegisterAssetTypeAction(MakeShared<FAssetTypeActions_LensFile>());

	{
		const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();
		TSharedRef<FWorkspaceItem> BrowserGroup = MenuStructure.GetDeveloperToolsMiscCategory()->GetParent()->AddGroup(
			LOCTEXT("WorkspaceMenu_VirtualProduction", "Virtual Production"),
			FSlateIcon(),
			true);
	}

	FCameraCalibrationMenuEntry::Register();

	RegisterPlacementModeItems();

	RegisterOverlayMaterials();

	ICurveEditorModule& CurveEditorModule = FModuleManager::LoadModuleChecked<ICurveEditorModule>("CurveEditor");
	FLensDataCurveModel::ViewId = CurveEditorModule.RegisterView(FOnCreateCurveEditorView::CreateStatic(
		[](TWeakPtr<FCurveEditor> WeakCurveEditor) -> TSharedRef<SCurveEditorView>
		{
			return SNew(SCameraCalibrationCurveEditorView, WeakCurveEditor);
		}
	));
}

const FPlacementCategoryInfo* FCameraCalibrationEditorModule::GetVirtualProductionCategoryRegisteredInfo() const
{
	IPlacementModeModule& PlacementModeModule = IPlacementModeModule::Get();

	static const FName VirtualProductionName = TEXT("VirtualProduction");

	if (const FPlacementCategoryInfo* RegisteredInfo = PlacementModeModule.GetRegisteredPlacementCategory(VirtualProductionName))
	{
		return RegisteredInfo;
	}
	else
	{
		FPlacementCategoryInfo Info(
			LOCTEXT("VirtualProductionCategoryName", "Virtual Production"),
			FSlateIcon(FCameraCalibrationEditorStyle::Get().GetStyleSetName(), "PlacementBrowser.Icons.VirtualProduction"),
			VirtualProductionName,
			TEXT("PMVirtualProduction"),
			25
		);

		IPlacementModeModule::Get().RegisterPlacementCategory(Info);

		return PlacementModeModule.GetRegisteredPlacementCategory(VirtualProductionName);
	}
}

void FCameraCalibrationEditorModule::RegisterPlacementModeItems()
{
	auto RegisterPlaceActors = [&]() -> void
	{
		if (!GEditor)
		{
			return;
		}

		const FPlacementCategoryInfo* Info = GetVirtualProductionCategoryRegisteredInfo();

		if (!Info)
		{
			UE_LOG(LogCameraCalibrationEditor, Warning, TEXT("Could not find or create VirtualProduction Place Actor Category"));
			return;
		}

		// Register the Trackers, Version 2 and 3
		{
			const FAssetData TrackerAssetDataV2(
				TEXT("/CameraCalibration/Devices/Tracker/BP_UE_Tracker"),
				TEXT("/CameraCalibration/Devices/Tracker"),
				TEXT("BP_UE_Tracker"),
				FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Blueprint"))
			);

			PlaceActors.Add(IPlacementModeModule::Get().RegisterPlaceableItem(Info->UniqueHandle, MakeShared<FPlaceableItem>(
				*UActorFactoryBlueprint::StaticClass(),
				TrackerAssetDataV2,
				NAME_None,
				NAME_None,
				TOptional<FLinearColor>(),
				TOptional<int32>(),
				NSLOCTEXT("PlacementMode", "TrackerV2", "TrackerV2")
			)));

			const FAssetData TrackerAssetDataV3(
				TEXT("/CameraCalibration/Devices/Tracker/BP_UE_Tracker3"),
				TEXT("/CameraCalibration/Devices/Tracker"),
				TEXT("BP_UE_Tracker3"),
				FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Blueprint"))
			);

			PlaceActors.Add(IPlacementModeModule::Get().RegisterPlaceableItem(Info->UniqueHandle, MakeShared<FPlaceableItem>(
				*UActorFactoryBlueprint::StaticClass(),
				TrackerAssetDataV3,
				NAME_None,
				NAME_None,
				TOptional<FLinearColor>(),
				TOptional<int32>(),
				NSLOCTEXT("PlacementMode", "TrackerV3", "TrackerV3")
				)));
		}
	};

	if (FApp::CanEverRender())
	{
		if (GEngine && GEngine->IsInitialized())
		{
			RegisterPlaceActors();
		}
		else
		{
			PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddLambda(RegisterPlaceActors);
		}
	}
}

void FCameraCalibrationEditorModule::RegisterOverlayMaterials()
{
	auto RegisterOverlays = [this]()
	{
		// Register all overlay materials defined in this module
		UCameraCalibrationSubsystem* SubSystem = GEngine->GetEngineSubsystem<UCameraCalibrationSubsystem>();
		SubSystem->RegisterOverlayMaterial(TEXT("Crosshair"), FSoftObjectPath(TEXT("/CameraCalibration/Materials/M_Crosshair.M_Crosshair")));
	};

	if (FApp::CanEverRender())
	{
		if (GEngine && GEngine->IsInitialized())
		{
			RegisterOverlays();
		}
		else
		{
			PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddLambda(RegisterOverlays);
		}
	}
}

void FCameraCalibrationEditorModule::UnregisterOverlayMaterials()
{
	if (GEngine)
	{
		// Unregister all overlay materials defined in this module
		if (UCameraCalibrationSubsystem* SubSystem = GEngine->GetEngineSubsystem<UCameraCalibrationSubsystem>())
		{
			SubSystem->UnregisterOverlayMaterial(TEXT("Crosshair"));
		}
	}
}

void FCameraCalibrationEditorModule::ShutdownModule()
{
	if (!IsEngineExitRequested() && GEditor && UObjectInitialized())
	{
		FCameraCalibrationMenuEntry::Unregister();

		FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor");

		// Unregister AssetTypeActions
		FAssetToolsModule* AssetToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>("AssetTools");

		if (AssetToolsModule != nullptr)
		{
			IAssetTools& AssetTools = AssetToolsModule->Get();

			for (TSharedRef<IAssetTypeActions> Action : RegisteredAssetTypeActions)
			{
				AssetTools.UnregisterAssetTypeActions(Action);
			}
		}

		FCameraCalibrationCommands::Unregister();

		UnregisterPlacementModeItems();

		UnregisterOverlayMaterials();
	}

	if (PostEngineInitHandle.IsValid())
	{
		FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
	}

	if (ICurveEditorModule* CurveEditorModule = FModuleManager::GetModulePtr<ICurveEditorModule>("CurveEditor"))
	{
		CurveEditorModule->UnregisterView(FLensDataCurveModel::ViewId);
	}
}


void FCameraCalibrationEditorModule::UnregisterPlacementModeItems()
{
	IPlacementModeModule& PlacementModeModule = IPlacementModeModule::Get();

	for (TOptional<FPlacementModeID>& PlaceActor : PlaceActors)
	{
		if (PlaceActor.IsSet())
		{
			PlacementModeModule.UnregisterPlaceableItem(*PlaceActor);
		}
	}

	PlaceActors.Empty();
}

IMPLEMENT_MODULE(FCameraCalibrationEditorModule, CameraCalibrationEditor);

#undef LOCTEXT_NAMESPACE

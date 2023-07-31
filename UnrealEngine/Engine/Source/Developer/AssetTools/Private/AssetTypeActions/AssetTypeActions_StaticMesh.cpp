// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_StaticMesh.h"

#include "AssetTools.h"
#include "Editor.h"
#include "EditorSupportDelegates.h"
#include "StaticMeshEditorModule.h"
#include "EditorFramework/AssetImportData.h"
#include "FbxMeshUtils.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/MessageDialog.h"
#include "Misc/ScopedSlowTask.h"
#include "StaticMeshCompiler.h"
#include "Styling/AppStyle.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "ToolMenus.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

static TAutoConsoleVariable<int32> CVarEnableSaveGeneratedLODsInPackage(
	TEXT("r.StaticMesh.EnableSaveGeneratedLODsInPackage"),
	0,
	TEXT("Enables saving generated LODs in the Package.\n") \
	TEXT("0 - Do not save (and hide this menu option) [default].\n") \
	TEXT("1 - Enable this option and save the LODs in the Package.\n"),
	ECVF_Default);

void FAssetTypeActions_StaticMesh::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	auto Meshes = GetTypedWeakObjectPtrs<UStaticMesh>(InObjects);

	if (CVarEnableSaveGeneratedLODsInPackage.GetValueOnGameThread() != 0)
	{
		Section.AddMenuEntry(
			"ObjectContext_SaveGeneratedLODsInPackage",
			NSLOCTEXT("AssetTypeActions_StaticMesh", "ObjectContext_SaveGeneratedLODsInPackage", "Save Generated LODs"),
			NSLOCTEXT("AssetTypeActions_StaticMesh", "ObjectContext_SaveGeneratedLODsInPackageTooltip", "Run the mesh reduce and save the generated LODs as part of the package."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetTypeActions_StaticMesh::ExecuteSaveGeneratedLODsInPackage, Meshes),
				FCanExecuteAction()
				)
			);
	}

	Section.AddSubMenu(
		"StaticMesh_NaniteMenu",
		NSLOCTEXT("AssetTypeActions_StaticMesh", "StaticMesh_NaniteMenu", "Nanite"),
		NSLOCTEXT("AssetTypeActions_StaticMesh", "StaticMesh_NaniteTooltip", "Nanite Options and Tools"),
		FNewMenuDelegate::CreateSP(this, &FAssetTypeActions_StaticMesh::GetNaniteMenu, Meshes),
		false,
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Adjust")
	);

	Section.AddSubMenu(
		"StaticMesh_LODMenu",
		NSLOCTEXT("AssetTypeActions_StaticMesh", "StaticMesh_LODMenu", "Level Of Detail"),
		NSLOCTEXT("AssetTypeActions_StaticMesh", "StaticMesh_LODTooltip", "LOD Options and Tools"),
		FNewMenuDelegate::CreateSP(this, &FAssetTypeActions_StaticMesh::GetLODMenu, Meshes),
		false,
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.LOD")
	);
	
	Section.AddMenuEntry(
		"ObjectContext_ClearVertexColors",
		NSLOCTEXT("AssetTypeActions_StaticMesh", "ObjectContext_ClearVertexColors", "Remove Vertex Colors"),
		NSLOCTEXT("AssetTypeActions_StaticMesh", "ObjectContext_ClearVertexColorsTooltip", "Removes vertex colors from all LODS in all selected meshes."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions.RemoveVertexColors"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_StaticMesh::ExecuteRemoveVertexColors, Meshes)
		)
	);
}

void FAssetTypeActions_StaticMesh::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Mesh = Cast<UStaticMesh>(*ObjIt);
		if (Mesh != NULL)
		{
			IStaticMeshEditorModule* StaticMeshEditorModule = &FModuleManager::LoadModuleChecked<IStaticMeshEditorModule>( "StaticMeshEditor" );
			StaticMeshEditorModule->CreateStaticMeshEditor(Mode, EditWithinLevelEditor, Mesh);
		}
	}
}

UThumbnailInfo* FAssetTypeActions_StaticMesh::GetThumbnailInfo(UObject* Asset) const
{
	UStaticMesh* StaticMesh = CastChecked<UStaticMesh>(Asset);
	UThumbnailInfo* ThumbnailInfo = StaticMesh->ThumbnailInfo;
	if ( ThumbnailInfo == NULL )
	{
		ThumbnailInfo = NewObject<USceneThumbnailInfo>(StaticMesh, NAME_None, RF_Transactional);
		StaticMesh->ThumbnailInfo = ThumbnailInfo;
	}

	return ThumbnailInfo;
}

void FAssetTypeActions_StaticMesh::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for (auto& Asset : TypeAssets)
	{
		const auto StaticMesh = CastChecked<UStaticMesh>(Asset);
		if (StaticMesh->AssetImportData)
		{
			StaticMesh->AssetImportData->ExtractFilenames(OutSourceFilePaths);
		}
	}
}

void FAssetTypeActions_StaticMesh::GetNaniteMenu(class FMenuBuilder& MenuBuilder, TArray<TWeakObjectPtr<UStaticMesh>> Meshes)
{
	if (Meshes.Num() == 1)
	{
		TWeakObjectPtr<UStaticMesh> First = Meshes[0];
		UStaticMesh* StaticMesh = First.Get();

		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("AssetTypeActions_StaticMesh", "StaticMesh_NaniteToggle", "Nanite"),
			NSLOCTEXT("AssetTypeActions_StaticMesh", "StaticMesh_NaniteToggleTooltip", "Toggle Nanite support on the selected mesh."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this, Meshes, StaticMesh]()
					{
						if (StaticMesh->NaniteSettings.bEnabled)
						{
							ExecuteNaniteDisable(Meshes);
						}
						else
						{
							ExecuteNaniteEnable(Meshes);
						}
					}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([StaticMesh]()
					{
						return static_cast<bool>(StaticMesh->NaniteSettings.bEnabled);
					})
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);
	}
	else
	{
		// Dummy action that cannot be executed to indicate when multiple meshes are selected
		MenuBuilder.AddMenuEntry(
			NSLOCTEXT("AssetTypeActions_StaticMesh", "StaticMesh_NaniteMultipleSelected", "Multiple Meshes Selected"),
			TAttribute<FText>(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction(),
				FCanExecuteAction::CreateLambda([]() { return false; })
			)
		);
	}

	MenuBuilder.AddMenuSeparator();

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("AssetTypeActions_StaticMesh", "StaticMesh_NaniteEnableAll", "Enable Nanite for Selected"),
		NSLOCTEXT("AssetTypeActions_StaticMesh", "StaticMesh_NaniteEnableAllTooltip", "Enables support for Nanite on the selected meshes."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_StaticMesh::ExecuteNaniteEnable, Meshes),
			FCanExecuteAction::CreateLambda([Meshes]()
				{
					return Meshes.Num() > 1;
				})
		)
	);

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("AssetTypeActions_StaticMesh", "StaticMesh_NaniteDisableAll", "Disable Nanite for Selected"),
		NSLOCTEXT("AssetTypeActions_StaticMesh", "StaticMesh_NaniteDisableAllTooltip", "Disables support for Nanite on the selected meshes."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_StaticMesh::ExecuteNaniteDisable, Meshes),
			FCanExecuteAction::CreateLambda([Meshes]()
				{
					return Meshes.Num() > 1;
				})
			)
	);
}

void FAssetTypeActions_StaticMesh::GetImportLODMenu(class FMenuBuilder& MenuBuilder,TArray<TWeakObjectPtr<UStaticMesh>> Objects, bool bReimportWithNewFile)
{
	check(Objects.Num() > 0);
	TWeakObjectPtr<UStaticMesh> First = Objects[0];
	UStaticMesh* StaticMesh = First.Get();
	//Add 1 so we can Add LOD when we are not in "Reimport With New File" submenu
	const int32 LodCount = First->GetNumLODs() + (bReimportWithNewFile ? 0 : 1);

	for(int32 LOD = 1; LOD < LodCount; ++LOD)
	{
		FText LODText = FText::AsNumber( LOD );
		FText Description = FText::Format( NSLOCTEXT("AssetTypeActions_StaticMesh", "Reimport LOD (number)", "Reimport LOD {0}"), LODText );
		FText ToolTip = NSLOCTEXT("AssetTypeActions_StaticMesh", "ReimportTip", "Reimport over existing LOD");
		if(LOD == First->GetNumLODs())
		{
			Description = FText::Format( NSLOCTEXT("AssetTypeActions_StaticMesh", "LOD (number)", "Add LOD {0}"), LODText );
			ToolTip = NSLOCTEXT("AssetTypeActions_StaticMesh", "NewImportTip", "Import new LOD");
		}

		MenuBuilder.AddMenuEntry( Description, ToolTip, FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic( &FAssetTypeActions_StaticMesh::ExecuteImportMeshLOD, static_cast<UObject*>(StaticMesh), LOD, bReimportWithNewFile) ));
	}
}

void FAssetTypeActions_StaticMesh::GetLODMenu(class FMenuBuilder& MenuBuilder, TArray<TWeakObjectPtr<UStaticMesh>> Meshes)
{
	constexpr bool bReimportWithNewFile = true;
	MenuBuilder.AddSubMenu(
		NSLOCTEXT("AssetTypeActions_StaticMesh", "StaticMesh_ImportLOD", "Import LOD"),
		NSLOCTEXT("AssetTypeActions_StaticMesh", "StaticMesh_ImportLODtooltip", "Imports meshes into the LODs"),
		FNewMenuDelegate::CreateSP(this, &FAssetTypeActions_StaticMesh::GetImportLODMenu, Meshes, !bReimportWithNewFile)
		);

	MenuBuilder.AddSubMenu(
		NSLOCTEXT("AssetTypeActions_StaticMesh", "StaticMesh_ImportLODWithNewFile", "Reimport LOD With New File"),
		NSLOCTEXT("AssetTypeActions_StaticMesh", "StaticMesh_ImportLODWithNewFiletooltip", "Reimports meshes LODs with new file."),
		FNewMenuDelegate::CreateSP(this, &FAssetTypeActions_StaticMesh::GetImportLODMenu, Meshes, bReimportWithNewFile)
	);

	MenuBuilder.AddMenuSeparator();

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("AssetTypeActions_StaticMesh", "StaticMesh_CopyLOD", "Copy LOD"),
		NSLOCTEXT("AssetTypeActions_StaticMesh", "StaticMesh_CopyLODTooltip", "Copies the LOD settings from the selected mesh."),
		FSlateIcon(),
		FUIAction(
		FExecuteAction::CreateSP(this, &FAssetTypeActions_StaticMesh::ExecuteCopyLODSettings, Meshes),
		FCanExecuteAction::CreateSP(this, &FAssetTypeActions_StaticMesh::CanCopyLODSettings, Meshes)
		)
		);

	FText PasteLabel = FText(LOCTEXT("StaticMesh_PasteLOD", "Paste LOD"));
	if (LODCopyMesh.IsValid())
	{
		PasteLabel = FText::Format(LOCTEXT("StaticMesh_PasteLODWithName", "Paste LOD from {0}"), FText::FromString(LODCopyMesh->GetName()));
	}
	
	MenuBuilder.AddMenuEntry(
		PasteLabel,
		NSLOCTEXT("AssetTypeActions_StaticMesh", "StaticMesh_PasteLODToltip", "Pastes LOD settings to the selected mesh(es)."),
		FSlateIcon(),
		FUIAction(
		FExecuteAction::CreateSP(this, &FAssetTypeActions_StaticMesh::ExecutePasteLODSettings, Meshes),
		FCanExecuteAction::CreateSP(this, &FAssetTypeActions_StaticMesh::CanPasteLODSettings, Meshes)
		)
	);

	MenuBuilder.AddMenuSeparator();

	MenuBuilder.AddSubMenu(
		NSLOCTEXT("AssetTypeActions_StaticMesh", "StaticMesh_HiRes", "High Res"),
		NSLOCTEXT("AssetTypeActions_StaticMesh", "StaticMesh_HiRestooltip", "High resolution version of the mesh. Used instead of LOD0 by systems that can benefit from more detailed geometry (Nanite, modeling, etc.)"),
		FNewMenuDelegate::CreateSP(this, &FAssetTypeActions_StaticMesh::GetImportHiResMenu, Meshes)
	);
}

void FAssetTypeActions_StaticMesh::ExecuteImportMeshLOD(UObject* Mesh, int32 LOD, bool bReimportWithNewFile)
{
	FbxMeshUtils::ImportMeshLODDialog(Mesh, LOD, true, bReimportWithNewFile);
}

void FAssetTypeActions_StaticMesh::ExecuteImportHiResMesh(UStaticMesh* StaticMesh)
{
	if (FbxMeshUtils::ImportStaticMeshHiResSourceModelDialog(StaticMesh))
	{
		FEditorDelegates::RefreshEditor.Broadcast();
		FEditorSupportDelegates::RedrawAllViewports.Broadcast();
	}
}

void FAssetTypeActions_StaticMesh::ExecuteReimportHiResMeshWithNewFile(UStaticMesh* StaticMesh)
{
	if (StaticMesh)
	{
		StaticMesh->GetHiResSourceModel().SourceImportFilename = FString();
		if (FbxMeshUtils::ImportStaticMeshHiResSourceModelDialog(StaticMesh))
		{
			FEditorDelegates::RefreshEditor.Broadcast();
			FEditorSupportDelegates::RedrawAllViewports.Broadcast();
		}
	}
}

void FAssetTypeActions_StaticMesh::ExecuteRemoveHiResMesh(UStaticMesh* StaticMesh)
{
	if (FbxMeshUtils::RemoveStaticMeshHiRes(StaticMesh))
	{
		FEditorDelegates::RefreshEditor.Broadcast();
		FEditorSupportDelegates::RedrawAllViewports.Broadcast();
	}
}

void FAssetTypeActions_StaticMesh::GetImportHiResMenu(class FMenuBuilder& MenuBuilder, TArray<TWeakObjectPtr<UStaticMesh>> Objects)
{
	check(Objects.Num() > 0);
	TWeakObjectPtr<UStaticMesh> First = Objects[0];
	UStaticMesh* StaticMesh = First.Get();

	if (!StaticMesh)
	{
		return;
	}

	// Import / Reimport
	{
		if (!StaticMesh->IsHiResMeshDescriptionValid())
		{
			FText Description = NSLOCTEXT("AssetTypeActions_StaticMesh", "Import_HighRes", "Import High Res...");
			FText ToolTip = NSLOCTEXT("AssetTypeActions_StaticMesh", "Import_HighResTip", "Adds a high resolution version of the mesh. Used instead of LOD0 by systems that can benefit from more detailed geometry (Nanite, modeling, etc.).");
			MenuBuilder.AddMenuEntry(Description, ToolTip, FSlateIcon(),
				FUIAction(FExecuteAction::CreateStatic(&FAssetTypeActions_StaticMesh::ExecuteImportHiResMesh, StaticMesh)));
		}
		else
		{
			FText Description = NSLOCTEXT("AssetTypeActions_StaticMesh", "Reimport_HighRes", "Reimport High Res");
			FText ToolTip = NSLOCTEXT("AssetTypeActions_StaticMesh", "Reimport_HighRes_Tip", "Reimport over existing the existing high res mesh.");
			MenuBuilder.AddMenuEntry(Description, ToolTip, FSlateIcon(),
				FUIAction(FExecuteAction::CreateStatic(&FAssetTypeActions_StaticMesh::ExecuteImportHiResMesh, StaticMesh)));

			Description = NSLOCTEXT("AssetTypeActions_StaticMesh", "Reimport_HighRes_WithNewFile", "Reimport High Res with new file...");
			ToolTip = NSLOCTEXT("AssetTypeActions_StaticMesh", "Reimport_HighRes_WithNewFileTip", "Reimport the high resolution version of the mesh with a new file.");
			MenuBuilder.AddMenuEntry(Description, ToolTip, FSlateIcon(),
				FUIAction(FExecuteAction::CreateStatic(&FAssetTypeActions_StaticMesh::ExecuteReimportHiResMeshWithNewFile, StaticMesh)));
		}
	}

	// Remove
	{
		FText Description = NSLOCTEXT("AssetTypeActions_StaticMesh", "Remove_HighRes", "Remove High Res");
		FText ToolTip = NSLOCTEXT("AssetTypeActions_StaticMesh", "Remove_HighRes_Tip", "Remove the high res version of the mesh");

		MenuBuilder.AddMenuEntry(Description, ToolTip, FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateStatic(&FAssetTypeActions_StaticMesh::ExecuteRemoveHiResMesh, StaticMesh),
				FCanExecuteAction::CreateLambda(
					[StaticMesh]()
					{
						if (StaticMesh)
						{
							return StaticMesh->IsHiResMeshDescriptionValid();
						}
						else
						{
							return false;
						}
					}
				)
			)
		);
	}
}

void FAssetTypeActions_StaticMesh::ExecuteCopyLODSettings(TArray<TWeakObjectPtr<UStaticMesh>> Objects)
{
	LODCopyMesh = Objects[0];
}

bool FAssetTypeActions_StaticMesh::CanCopyLODSettings(TArray<TWeakObjectPtr<UStaticMesh>> Objects) const
{
	return Objects.Num() == 1;
}

void FAssetTypeActions_StaticMesh::ExecutePasteLODSettings(TArray<TWeakObjectPtr<UStaticMesh>> Objects)
{
	if (!LODCopyMesh.IsValid())
	{
		return;
	}

	// Retrieve LOD settings from source mesh
	struct FLODSettings
	{
		FMeshReductionSettings		ReductionSettings;
		float						ScreenSize;
	};

	TArray<FLODSettings> LODSettings;
	LODSettings.AddZeroed(LODCopyMesh->GetNumSourceModels());
	for (int32 i = 0; i < LODCopyMesh->GetNumSourceModels(); i++)
	{
		LODSettings[i].ReductionSettings = LODCopyMesh->GetSourceModel(i).ReductionSettings;
		LODSettings[i].ScreenSize = LODCopyMesh->GetSourceModel(i).ScreenSize.Default;
	}

	const bool bAutoComputeLODScreenSize = LODCopyMesh->bAutoComputeLODScreenSize;

	// Copy LOD settings over to selected objects in content browser (meshes)
	for (TWeakObjectPtr<UStaticMesh> MeshPtr : Objects)
	{
		if (MeshPtr.IsValid())
		{
			UStaticMesh* Mesh = MeshPtr.Get();

			const int32 LODCount = LODSettings.Num();
			Mesh->SetNumSourceModels(LODCount);

			for (int32 i = 0; i < LODCount; i++)
			{
				FStaticMeshSourceModel& SrcModel = Mesh->GetSourceModel(i);
				SrcModel.ReductionSettings = LODSettings[i].ReductionSettings;
				SrcModel.ScreenSize.Default = LODSettings[i].ScreenSize;
			}

			Mesh->bAutoComputeLODScreenSize = bAutoComputeLODScreenSize;

			Mesh->PostEditChange();
			Mesh->MarkPackageDirty();
		}
	}

}

bool FAssetTypeActions_StaticMesh::CanPasteLODSettings(TArray<TWeakObjectPtr<UStaticMesh>> Objects) const
{
	return LODCopyMesh.IsValid();
}

void FAssetTypeActions_StaticMesh::ExecuteSaveGeneratedLODsInPackage(TArray<TWeakObjectPtr<UStaticMesh>> Objects)
{
	for (auto StaticMeshIt = Objects.CreateConstIterator(); StaticMeshIt; ++StaticMeshIt)
	{
		auto StaticMesh = (*StaticMeshIt).Get();
		if (StaticMesh)
		{
			StaticMesh->GenerateLodsInPackage();
		}
	}
}

void FAssetTypeActions_StaticMesh::ExecuteRemoveVertexColors(TArray<TWeakObjectPtr<UStaticMesh>> Objects)
{
	FText WarningMessage = LOCTEXT("Warning_RemoveVertexColors", "Are you sure you want to remove vertex colors from all selected meshes?  There is no undo available.");
	if (FMessageDialog::Open(EAppMsgType::YesNo, WarningMessage) == EAppReturnType::Yes)
	{
		FScopedSlowTask SlowTask(1.0f, LOCTEXT("RemovingVertexColors", "Removing Vertex Colors"));
		for (auto StaticMeshPtr : Objects)
		{
			bool bRemovedVertexColors = false;
			UStaticMesh* Mesh = StaticMeshPtr.Get();
			if (Mesh)
			{
				Mesh->RemoveVertexColors();
			}
		}
	}
}

void FAssetTypeActions_StaticMesh::ModifyNaniteEnable(TArray<TWeakObjectPtr<UStaticMesh>> Objects, bool bNaniteEnable)
{
	TArray<UStaticMesh*> Meshes;
	Meshes.Reserve(Objects.Num());

	for (TWeakObjectPtr<UStaticMesh>& StaticMeshPtr : Objects)
	{
		UStaticMesh* Mesh = StaticMeshPtr.Get();
		if (Mesh && Mesh->NaniteSettings.bEnabled != bNaniteEnable)
		{
			Meshes.Add(Mesh);
		}
	}

	FStaticMeshCompilingManager::Get().FinishCompilation(Meshes);

	for (UStaticMesh* Mesh : Meshes)
	{
		Mesh->NaniteSettings.bEnabled = bNaniteEnable;
	}

	UStaticMesh::BatchBuild(Meshes);

	for (UStaticMesh* Mesh : Meshes)
	{
		Mesh->MarkPackageDirty();
		Mesh->OnMeshChanged.Broadcast();
	}
}

void FAssetTypeActions_StaticMesh::ExecuteNaniteEnable(TArray<TWeakObjectPtr<UStaticMesh>> Objects)
{
	ModifyNaniteEnable(Objects, true);
}

void FAssetTypeActions_StaticMesh::ExecuteNaniteDisable(TArray<TWeakObjectPtr<UStaticMesh>> Objects)
{
	ModifyNaniteEnable(Objects, false);
}

#undef LOCTEXT_NAMESPACE

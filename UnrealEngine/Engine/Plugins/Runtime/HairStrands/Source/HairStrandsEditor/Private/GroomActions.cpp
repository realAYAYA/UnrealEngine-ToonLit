// Copyright Epic Games, Inc. All Rights Reserved.

#include "GroomActions.h"
#include "GroomAsset.h"

#include "EditorFramework/AssetImportData.h"
#include "HairStrandsCore.h"
#include "HairStrandsRendering.h"
#include "GeometryCache.h"
#include "GroomAssetImportData.h"
#include "GroomBuilder.h"
#include "GroomDeformerBuilder.h"
#include "GroomImportOptions.h"
#include "GroomImportOptionsWindow.h"
#include "GroomCustomAssetEditorToolkit.h"
#include "GroomCreateBindingOptions.h"
#include "GroomCreateBindingOptionsWindow.h"
#include "GroomCreateFollicleMaskOptions.h"
#include "GroomCreateFollicleMaskOptionsWindow.h"
#include "GroomCreateStrandsTexturesOptions.h"
#include "GroomCreateStrandsTexturesOptionsWindow.h"
#include "HairStrandsImporter.h"
#include "HairStrandsTranslator.h"
#include "ToolMenuSection.h"
#include "Misc/ScopedSlowTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "GroomBindingBuilder.h"
#include "GroomTextureBuilder.h"
#include "GroomBindingAsset.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ObjectTools.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

///////////////////////////////////////////////////////////////////////////////////////////////////
// Actions

FGroomActions::FGroomActions()
{}

bool FGroomActions::CanFilter()
{
	return true;
}

void FGroomActions::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	TArray<TWeakObjectPtr<UGroomAsset>> GroomAssets = GetTypedWeakObjectPtrs<UGroomAsset>(InObjects);

	Section.AddMenuEntry(
		"RebuildGroom",
		LOCTEXT("RebuildGroom", "Rebuild"),
		LOCTEXT("RebuildGroomTooltip", "Rebuild the groom with new build settings"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FGroomActions::ExecuteRebuild, GroomAssets),
			FCanExecuteAction::CreateSP(this, &FGroomActions::CanRebuild, GroomAssets)
		)
	);

	Section.AddMenuEntry(
		"CreateBindingAsset",
		LOCTEXT("CreateBindingAsset", "Create Binding"),
		LOCTEXT("CreateBindingAssetTooltip", "Create a binding asset between a skeletal mesh and a groom asset"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FGroomActions::ExecuteCreateBindingAsset, GroomAssets),
			FCanExecuteAction::CreateSP(this, &FGroomActions::CanCreateBindingAsset, GroomAssets)
		)
	);

	Section.AddMenuEntry(
		"CreateFollicleTexture",
		LOCTEXT("CreateFollicleTexture", "Create Follicle Texture"),
		LOCTEXT("CreateFollicleTextureTooltip", "Create a follicle texture for the selected groom assets"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FGroomActions::ExecuteCreateFollicleTexture, GroomAssets),
			FCanExecuteAction::CreateSP(this, &FGroomActions::CanCreateFollicleTexture, GroomAssets)
			)
		);

	Section.AddMenuEntry(
		"CreateStrandsTextures",
		LOCTEXT("CreateStrandsTextures", "Create Strands Textures"),
		LOCTEXT("CreateStrandsTexturesTooltip", "Create projected strands textures onto meshes"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FGroomActions::ExecuteCreateStrandsTextures, GroomAssets),
			FCanExecuteAction::CreateSP(this, &FGroomActions::CanCreateStrandsTextures, GroomAssets)
		)
	);
}

uint32 FGroomActions::GetCategories()
{
	return EAssetTypeCategories::Misc;
}

FText FGroomActions::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_Groom", "Groom");
}

void FGroomActions::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for (UObject* Asset : TypeAssets)
	{
		const UGroomAsset* GroomAsset = CastChecked<UGroomAsset>(Asset);
		if (GroomAsset && GroomAsset->AssetImportData)
		{
			GroomAsset->AssetImportData->ExtractFilenames(OutSourceFilePaths);
		}
	}
}

UClass* FGroomActions::GetSupportedClass() const
{
	return UGroomAsset::StaticClass();
}

FColor FGroomActions::GetTypeColor() const
{
	return FColor::White;
}

void FGroomActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	// #ueent_todo: Will need a custom editor at some point, for now just use the Properties editor
	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto GroomAsset = Cast<UGroomAsset>(*ObjIt);
		if (GroomAsset != nullptr)
		{
			// Make sure the groom asset has a document 
			//FHairLabDataIO::GetDocumentForAsset(GroomAsset);

			TSharedRef<FGroomCustomAssetEditorToolkit> NewCustomAssetEditor(new FGroomCustomAssetEditorToolkit());
			NewCustomAssetEditor->InitCustomAssetEditor(EToolkitMode::Standalone, EditWithinLevelEditor, GroomAsset);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Groom build/rebuild

bool FGroomActions::CanRebuild(TArray<TWeakObjectPtr<UGroomAsset>> Objects) const
{
	for (TWeakObjectPtr<UGroomAsset> GroomAsset : Objects)
	{
		if (GroomAsset.IsValid() && GroomAsset->CanRebuildFromDescription())
		{
			return true;
		}
	}
	return false;
}

void FGroomActions::ExecuteRebuild(TArray<TWeakObjectPtr<UGroomAsset>> Objects) const
{
	for (TWeakObjectPtr<UGroomAsset> GroomAsset : Objects)
	{
		if (GroomAsset.IsValid() && GroomAsset->CanRebuildFromDescription() && GroomAsset->AssetImportData)
		{
			UGroomAssetImportData* GroomAssetImportData = Cast<UGroomAssetImportData>(GroomAsset->AssetImportData);
			if (GroomAssetImportData && GroomAssetImportData->ImportOptions)
			{
				FString Filename(GroomAssetImportData->GetFirstFilename());

				// Duplicate the options to prevent dirtying the asset when they are modified but the rebuild is cancelled
				UGroomImportOptions* CurrentOptions = DuplicateObject<UGroomImportOptions>(GroomAssetImportData->ImportOptions, nullptr);
			
				const uint32 GroupCount = GroomAsset->GetNumHairGroups();
				UGroomHairGroupsPreview* GroupsPreview = NewObject<UGroomHairGroupsPreview>();
				{					
					for (uint32 GroupIndex = 0; GroupIndex < GroupCount; GroupIndex++)
					{
						FGroomHairGroupPreview& OutGroup = GroupsPreview->Groups.AddDefaulted_GetRef();
						OutGroup.GroupID    = GroupIndex;
						OutGroup.GroupName	= GroomAsset->HairGroupsInfo[GroupIndex].GroupName;
						OutGroup.CurveCount = GroomAsset->HairGroupsData[GroupIndex].Strands.BulkData.GetNumCurves();
						OutGroup.GuideCount = GroomAsset->HairGroupsData[GroupIndex].Guides.BulkData.GetNumCurves();
						OutGroup.bHasRootUV = false;
						OutGroup.bHasColorAttributes = false;
						OutGroup.bHasRoughnessAttributes = false;
						OutGroup.bHasPrecomputedWeights = false;
						OutGroup.InterpolationSettings = GroomAsset->HairGroupsInterpolation[GroupIndex];
						OutGroup.InterpolationSettings.RiggingSettings.bCanEditRigging = true;
					}
				}
				TSharedPtr<SGroomImportOptionsWindow> GroomOptionWindow = SGroomImportOptionsWindow::DisplayRebuildOptions(CurrentOptions, GroupsPreview, Filename);

				if (!GroomOptionWindow->ShouldImport())
				{
					continue;
				}

				// Apply new interpolation settings to the groom, prior to rebuilding the groom
				bool bEnableRigging = false;
				for (uint32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
				{
					GroomAsset->HairGroupsInterpolation[GroupIndex] = GroupsPreview->Groups[GroupIndex].InterpolationSettings;
					GroomAsset->HairGroupsInterpolation[GroupIndex].RiggingSettings.bCanEditRigging = false;
					bEnableRigging |= GroomAsset->HairGroupsInterpolation[GroupIndex].RiggingSettings.bEnableRigging &&
						GroomAsset->HairGroupsInterpolation[GroupIndex].InterpolationSettings.bOverrideGuides;
				}

				bool bSucceeded = GroomAsset->CacheDerivedDatas();
				if (bSucceeded)
				{
					if(bEnableRigging)
					{
						GroomAsset->RiggedSkeletalMesh = FGroomDeformerBuilder::CreateSkeletalMesh(GroomAsset.Get());
					}
					// Move the transient ImportOptions to the asset package and set it on the GroomAssetImportData for serialization
					CurrentOptions->Rename(nullptr, GroomAssetImportData);
					for (const FGroomHairGroupPreview& GroupPreview : GroupsPreview->Groups)
					{
						CurrentOptions->InterpolationSettings[GroupPreview.GroupID] = GroupPreview.InterpolationSettings;
					}
					GroomAssetImportData->ImportOptions = CurrentOptions;
					GroomAsset->MarkPackageDirty();
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Binding

bool FGroomActions::CanCreateBindingAsset(TArray<TWeakObjectPtr<UGroomAsset>> Objects) const
{
	for (TWeakObjectPtr<UGroomAsset> GroomAsset : Objects)
	{
		if (GroomAsset.IsValid())
		{
			return true;
		}
	}
	return false;
}

void FGroomActions::ExecuteCreateBindingAsset(TArray<TWeakObjectPtr<UGroomAsset>> Objects) const
{
	for (TWeakObjectPtr<UGroomAsset> GroomAsset : Objects)
	{
		if (GroomAsset.IsValid())
		{
			// Duplicate the options to prevent dirtying the asset when they are modified but the rebuild is cancelled
			UGroomCreateBindingOptions* CurrentOptions = NewObject<UGroomCreateBindingOptions>();
			TSharedPtr<SGroomCreateBindingOptionsWindow> GroomOptionWindow = SGroomCreateBindingOptionsWindow::DisplayCreateBindingOptions(CurrentOptions);

			if (!GroomOptionWindow->ShouldCreate())
			{
				continue;
			}
			else if (GroomAsset.Get() && CurrentOptions && 
				    ((CurrentOptions->GroomBindingType == EGroomBindingMeshType::SkeletalMesh && CurrentOptions->TargetSkeletalMesh) ||
					(CurrentOptions->GroomBindingType == EGroomBindingMeshType::GeometryCache && CurrentOptions->TargetGeometryCache)))
			{
				GroomAsset->ConditionalPostLoad();

				UGroomBindingAsset* BindingAsset = nullptr;
				if (CurrentOptions->GroomBindingType == EGroomBindingMeshType::SkeletalMesh)
				{
					CurrentOptions->TargetSkeletalMesh->ConditionalPostLoad();
					if (CurrentOptions->SourceSkeletalMesh)
					{
						CurrentOptions->SourceSkeletalMesh->ConditionalPostLoad();
					}
					BindingAsset = FHairStrandsCore::CreateGroomBindingAsset(CurrentOptions->GroomBindingType, GroomAsset.Get(), CurrentOptions->SourceSkeletalMesh, CurrentOptions->TargetSkeletalMesh, CurrentOptions->NumInterpolationPoints, CurrentOptions->MatchingSection);
				}
				else
				{
					CurrentOptions->TargetGeometryCache->ConditionalPostLoad();
					if (CurrentOptions->SourceGeometryCache)
					{
						CurrentOptions->SourceGeometryCache->ConditionalPostLoad();
					}
					BindingAsset = FHairStrandsCore::CreateGroomBindingAsset(CurrentOptions->GroomBindingType, GroomAsset.Get(), CurrentOptions->SourceGeometryCache, CurrentOptions->TargetGeometryCache, CurrentOptions->NumInterpolationPoints, CurrentOptions->MatchingSection);
				}

				if (BindingAsset)
				{
					BindingAsset->Build();
					if (BindingAsset->bIsValid)
					{
						TArray<UObject*> CreatedObjects;
						CreatedObjects.Add(BindingAsset);

						FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
						ContentBrowserModule.Get().SyncBrowserToAssets(CreatedObjects);
#if WITH_EDITOR
						GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAssets(CreatedObjects);
#endif
					}
					else
					{
						FNotificationInfo Info(LOCTEXT("FailedToCreateBinding", "Failed to create groom binding. See Output Log for details"));
						Info.ExpireDuration = 5.0f;
						FSlateNotificationManager::Get().AddNotification(Info);

						if (ObjectTools::DeleteSingleObject(BindingAsset))
						{
							CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
						}
					}
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Follicle

bool FGroomActions::CanCreateFollicleTexture(TArray<TWeakObjectPtr<UGroomAsset>> Objects) const
{
	for (TWeakObjectPtr<UGroomAsset> GroomAsset : Objects)
	{
		if (GroomAsset.IsValid())
		{
			return true;
		}
	}
	return false;
}

void FGroomActions::ExecuteCreateFollicleTexture(TArray<TWeakObjectPtr<UGroomAsset>> Objects) const
{
	if (Objects.Num() == 0)
	{
		return;
	}

	// Duplicate the options to prevent dirtying the asset when they are modified but the rebuild is cancelled
	UGroomCreateFollicleMaskOptions* CurrentOptions = NewObject<UGroomCreateFollicleMaskOptions>();
	if (!CurrentOptions)
	{
		return;
	}

	for (TWeakObjectPtr<UGroomAsset>& GroomAsset : Objects)
	{
		if (GroomAsset.IsValid())
		{
			FFollicleMaskOptions& Items = CurrentOptions->Grooms.AddDefaulted_GetRef();;
			Items.Groom   = GroomAsset.Get();
			Items.Channel = EFollicleMaskChannel::R;
		}
	}

	if (CurrentOptions->Grooms.Num() == 0)
	{
		return;
	}
	
	TSharedPtr<SGroomCreateFollicleMaskOptionsWindow> GroomOptionWindow = SGroomCreateFollicleMaskOptionsWindow::DisplayCreateFollicleMaskOptions(CurrentOptions);

	if (!GroomOptionWindow->ShouldCreate())
	{
		return;
	}
	else 
	{
		TArray<FFollicleInfo> Infos;
		for (FFollicleMaskOptions& Option : CurrentOptions->Grooms)
		{
			if (Option.Groom)
			{
				Option.Groom->ConditionalPostLoad();

				FFollicleInfo& Info = Infos.AddDefaulted_GetRef();
				Info.GroomAsset			= Option.Groom;
				Info.Channel			= FFollicleInfo::EChannel(uint8(Option.Channel));
				Info.KernelSizeInPixels = FMath::Max(2, CurrentOptions->RootRadius);
			}
		}

		const uint32 Resolution = FMath::RoundUpToPowerOfTwo(CurrentOptions->Resolution);
		UTexture2D* FollicleTexture = FGroomTextureBuilder::CreateGroomFollicleMaskTexture(CurrentOptions->Grooms[0].Groom, Resolution);
		if (FollicleTexture)
		{
			FGroomTextureBuilder::BuildFollicleTexture(Infos, FollicleTexture, false);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Strands Textures

bool FGroomActions::CanCreateStrandsTextures(TArray<TWeakObjectPtr<UGroomAsset>> Objects) const
{
	for (TWeakObjectPtr<UGroomAsset> GroomAsset : Objects)
	{
		if (GroomAsset.IsValid())
		{
			return true;
		}
	}
	return false;
}

void FGroomActions::ExecuteCreateStrandsTextures(TArray<TWeakObjectPtr<UGroomAsset>> Objects) const
{
	if (Objects.Num() == 0)
	{
		return;
	}

	// Duplicate the options to prevent dirtying the asset when they are modified but the rebuild is cancelled
	UGroomCreateStrandsTexturesOptions* CurrentOptions = NewObject<UGroomCreateStrandsTexturesOptions>();
	for (TWeakObjectPtr<UGroomAsset>& GroomAsset : Objects)
	{
		if (GroomAsset.IsValid() && CurrentOptions)
		{
			TSharedPtr<SGroomCreateStrandsTexturesOptionsWindow> GroomOptionWindow = SGroomCreateStrandsTexturesOptionsWindow::DisplayCreateStrandsTexturesOptions(CurrentOptions);
			if (!GroomOptionWindow->ShouldCreate())
			{
				return;
			}
			else
			{
				GroomAsset->ConditionalPostLoad();

				// Create debug data for the groom asset for tracing hair geometry when redering strands texture.
				if (!GroomAsset->HasDebugData())
				{
					GroomAsset->CreateDebugData();
				}

				float SignDirection = 1;
				float MaxDistance = CurrentOptions->TraceDistance;
				switch (CurrentOptions->TraceType)
				{
				case EStrandsTexturesTraceType::TraceOuside:		SignDirection =  1; break;
				case EStrandsTexturesTraceType::TraceInside:		SignDirection = -1; break;
				case EStrandsTexturesTraceType::TraceBidirectional: SignDirection = 0;  MaxDistance *= 2; break;
				}

				UStaticMesh* StaticMesh = nullptr;
				USkeletalMesh* SkeletalMesh = nullptr;
				switch (CurrentOptions->MeshType)
				{
				case EStrandsTexturesMeshType::Static: StaticMesh = CurrentOptions->StaticMesh; break;
				case EStrandsTexturesMeshType::Skeletal: SkeletalMesh = CurrentOptions->SkeletalMesh; break;
				}
				if (SkeletalMesh == nullptr && StaticMesh == nullptr)
				{
					return;
				}
				
				FStrandsTexturesInfo Info;
				Info.GroomAsset   = GroomAsset.Get();
				Info.TracingDirection = SignDirection;
				Info.MaxTracingDistance = MaxDistance;
				Info.Resolution = FMath::RoundUpToPowerOfTwo(FMath::Max(256, CurrentOptions->Resolution));
				Info.LODIndex = FMath::Max(0, CurrentOptions->LODIndex);
				Info.SectionIndex = FMath::Max(0, CurrentOptions->SectionIndex);
				Info.UVChannelIndex= FMath::Max(0, CurrentOptions->UVChannelIndex);
				Info.SkeletalMesh = SkeletalMesh;
				Info.StaticMesh = StaticMesh;
				if (CurrentOptions->GroupIndex.Num())
				{
					Info.GroupIndices = CurrentOptions->GroupIndex;
				}
				else
				{
					for (int32 GroupIndex = 0; GroupIndex < GroomAsset->GetNumHairGroups(); ++GroupIndex)
					{
						Info.GroupIndices.Add(GroupIndex);
					}
				}
				FStrandsTexturesOutput Output = FGroomTextureBuilder::CreateGroomStrandsTexturesTexture(GroomAsset.Get(), Info.Resolution);
				if (Output.IsValid())
				{
					FGroomTextureBuilder::BuildStrandsTextures(Info, Output);
				}
			}
		}
	}

}


#undef LOCTEXT_NAMESPACE

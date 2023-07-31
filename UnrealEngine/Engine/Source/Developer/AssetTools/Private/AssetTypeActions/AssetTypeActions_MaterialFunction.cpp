// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_MaterialFunction.h"
#include "Factories/MaterialFunctionInstanceFactory.h"
#include "ToolMenus.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Styling/AppStyle.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunctionInstance.h"
#include "ThumbnailRendering/SceneThumbnailInfoWithPrimitive.h"
#include "AssetTools.h"
#include "MaterialEditorModule.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "Misc/PackageName.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

const FName MaterialFunctionClass = FName(TEXT("MaterialFunction"));
const FName MaterialFunctionInstanceClass = FName(TEXT("MaterialFunctionInstance"));
const FName MaterialFunctionUsageTag = FName(TEXT("MaterialFunctionUsage"));
const FString LayerCompareString = (TEXT("MaterialLayer"));
const FString BlendCompareString = (TEXT("MaterialLayerBlend"));


void FAssetTypeActions_MaterialFunction::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	const TArray<TWeakObjectPtr<UMaterialFunctionInterface>> Functions = GetTypedWeakObjectPtrs<UMaterialFunctionInterface>(InObjects);

	Section.AddMenuEntry(
		"MaterialFunction_NewMFI",
		GetInstanceText(),
		LOCTEXT("Material_NewMFITooltip", "Creates a parameterized function using this function as a base."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.MaterialInstanceActor"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_MaterialFunction::ExecuteNewMFI, Functions)
		)
	);

	if (FEditorDelegates::OnOpenReferenceViewer.IsBound())
	{
		Section.AddMenuEntry(
			"MaterialFunction_FindMaterials",
			LOCTEXT("MaterialFunction_FindMaterials", "Find Materials Using This"),
			LOCTEXT("MaterialFunction_FindMaterialsTooltip", "Finds the materials that reference this material function and visually displays them with the light version of the Reference Viewer."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions.GenericFind"),
			FUIAction(
				FExecuteAction::CreateSP( this, &FAssetTypeActions_MaterialFunction::ExecuteFindMaterials, Functions ),
				FCanExecuteAction()
				)
			);
	}
}

void FAssetTypeActions_MaterialFunction::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Function = Cast<UMaterialFunction>(*ObjIt);
		if (Function != NULL)
		{
			IMaterialEditorModule* MaterialEditorModule = &FModuleManager::LoadModuleChecked<IMaterialEditorModule>( "MaterialEditor" );
			MaterialEditorModule->CreateMaterialEditor(Mode, EditWithinLevelEditor, Function);
		}
	}
}

void FAssetTypeActions_MaterialFunction::ExecuteNewMFI(TArray<TWeakObjectPtr<UMaterialFunctionInterface>> Objects)
{
	const FString DefaultSuffix = TEXT("_Inst");

	if ( Objects.Num() == 1 )
	{
		auto Object = Objects[0].Get();

		if ( Object )
		{
			// Create an appropriate and unique name 
			FString Name;
			FString PackageName;
			CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

			UMaterialFunctionInstanceFactory* Factory = NewObject<UMaterialFunctionInstanceFactory>();
			Factory->InitialParent = Object;
			
			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackageName), UMaterialFunctionInstance::StaticClass(), Factory);
		}
	}
	else
	{
		TArray<UObject*> ObjectsToSync;
		for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			auto Object = (*ObjIt).Get();
			if ( Object )
			{
				// Determine an appropriate name
				FString Name;
				FString PackageName;
				CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

				// Create the factory used to generate the asset
				UMaterialFunctionInstanceFactory* Factory = NewObject<UMaterialFunctionInstanceFactory>();
				Factory->InitialParent = Object;

				FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
				UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), UMaterialFunctionInstance::StaticClass(), Factory);

				if ( NewAsset )
				{
					ObjectsToSync.Add(NewAsset);
				}
			}
		}

		if ( ObjectsToSync.Num() > 0 )
		{
			FAssetTools::Get().SyncBrowserToAssets(ObjectsToSync);
		}
	}
}

void FAssetTypeActions_MaterialFunction::ExecuteFindMaterials(TArray<TWeakObjectPtr<UMaterialFunctionInterface>> Objects)
{
#if WITH_EDITOR
	if (FEditorDelegates::OnOpenReferenceViewer.IsBound())
	{
		// TArray that will be send to the ReferenceViewer for display
		TArray<FAssetIdentifier> AssetIdentifiers;
		// Iterate over all selected UMaterialFunctionInterface instances
		for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			const UMaterialFunctionInterface* const Object = (*ObjIt).Get();
			// If valid pointer, add to AssetIdentifiers for display on the ReferenceViewer
			if (Object)
			{
				// Construct FAssetIdentifier and add to TArray
				const FName AssetName(*FPackageName::ObjectPathToPackageName(GetPathNameSafe(Object)));
				AssetIdentifiers.Add(FAssetIdentifier(AssetName));
			}
		}
		// Call ReferenceViewer
		FReferenceViewerParams ReferenceViewerParams;
		ReferenceViewerParams.bShowDependencies = false;
		ReferenceViewerParams.FixAndHideSearchDepthLimit = 1;
		ReferenceViewerParams.bShowShowReferencesOptions = false;
		ReferenceViewerParams.bShowShowSearchableNames = false;
		FEditorDelegates::OnOpenReferenceViewer.Broadcast(AssetIdentifiers, ReferenceViewerParams);
	}
#endif // WITH_EDITOR
}

UThumbnailInfo* FAssetTypeActions_MaterialFunction::GetThumbnailInfo(UObject* Asset) const
{
	UMaterialFunctionInterface* MaterialFunc = CastChecked<UMaterialFunctionInterface>(Asset);
	UThumbnailInfo* ThumbnailInfo = MaterialFunc->ThumbnailInfo;
	if (ThumbnailInfo == NULL)
	{
		ThumbnailInfo = NewObject<USceneThumbnailInfoWithPrimitive>(MaterialFunc, NAME_None, RF_Transactional);
		MaterialFunc->ThumbnailInfo = ThumbnailInfo;
	}

	return ThumbnailInfo;
}

UClass* FAssetTypeActions_MaterialFunctionLayer::GetSupportedClass() const
{
	IMaterialEditorModule::Get();	// force load Material Module here
	UClass* SupportedClass = UMaterialFunctionMaterialLayer::StaticClass();
	return SupportedClass;
}

bool FAssetTypeActions_MaterialFunctionLayer::CanFilter()
{
	return true;
}




void FAssetTypeActions_MaterialFunctionLayer::ExecuteNewMFI(TArray<TWeakObjectPtr<UMaterialFunctionInterface>> Objects)
{
	const FString DefaultSuffix = TEXT("_Inst");

	if (Objects.Num() == 1)
	{
		auto Object = Objects[0].Get();

		if (Object)
		{
			// Create an appropriate and unique name 
			FString Name;
			FString PackageName;
			CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

			UMaterialFunctionMaterialLayerInstanceFactory* Factory = NewObject<UMaterialFunctionMaterialLayerInstanceFactory>();
			Factory->InitialParent = Object;

			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackageName), UMaterialFunctionMaterialLayerInstance::StaticClass(), Factory);
		}
	}
	else
	{
		TArray<UObject*> ObjectsToSync;
		for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			auto Object = (*ObjIt).Get();
			if (Object)
			{
				// Determine an appropriate name
				FString Name;
				FString PackageName;
				CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

				// Create the factory used to generate the asset
				UMaterialFunctionMaterialLayerInstanceFactory* Factory = NewObject<UMaterialFunctionMaterialLayerInstanceFactory>();
				Factory->InitialParent = Object;

				FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
				UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), UMaterialFunctionMaterialLayerInstance::StaticClass(), Factory);

				if (NewAsset)
				{
					ObjectsToSync.Add(NewAsset);
				}
			}
		}

		if (ObjectsToSync.Num() > 0)
		{
			FAssetTools::Get().SyncBrowserToAssets(ObjectsToSync);
		}
	}
}

UClass* FAssetTypeActions_MaterialFunctionLayerBlend::GetSupportedClass() const
{
	UClass* SupportedClass = UMaterialFunctionMaterialLayerBlend::StaticClass();
	return SupportedClass;
}

bool FAssetTypeActions_MaterialFunctionLayerBlend::CanFilter()
{
	return true;
}

void FAssetTypeActions_MaterialFunctionLayerBlend::ExecuteNewMFI(TArray<TWeakObjectPtr<UMaterialFunctionInterface>> Objects)
{
	const FString DefaultSuffix = TEXT("_Inst");

	if (Objects.Num() == 1)
	{
		auto Object = Objects[0].Get();

		if (Object)
		{
			// Create an appropriate and unique name 
			FString Name;
			FString PackageName;
			CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

			UMaterialFunctionMaterialLayerBlendInstanceFactory* Factory = NewObject<UMaterialFunctionMaterialLayerBlendInstanceFactory>();
			Factory->InitialParent = Object;

			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackageName), UMaterialFunctionMaterialLayerBlendInstance::StaticClass(), Factory);
		}
	}
	else
	{
		TArray<UObject*> ObjectsToSync;
		for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			auto Object = (*ObjIt).Get();
			if (Object)
			{
				// Determine an appropriate name
				FString Name;
				FString PackageName;
				CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

				// Create the factory used to generate the asset
				UMaterialFunctionMaterialLayerBlendInstanceFactory* Factory = NewObject<UMaterialFunctionMaterialLayerBlendInstanceFactory>();
				Factory->InitialParent = Object;

				FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
				UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), UMaterialFunctionMaterialLayerBlendInstance::StaticClass(), Factory);

				if (NewAsset)
				{
					ObjectsToSync.Add(NewAsset);
				}
			}
		}

		if (ObjectsToSync.Num() > 0)
		{
			FAssetTools::Get().SyncBrowserToAssets(ObjectsToSync);
		}
	}
}

#undef LOCTEXT_NAMESPACE

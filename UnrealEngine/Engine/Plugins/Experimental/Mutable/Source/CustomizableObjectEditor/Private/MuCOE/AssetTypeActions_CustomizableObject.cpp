// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/AssetTypeActions_CustomizableObject.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "ContentBrowserModule.h"
#include "Delegates/Delegate.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformCrt.h"
#include "HAL/PlatformMisc.h"
#include "IContentBrowserSingleton.h"
#include "Misc/AssertionMacros.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuCOE/CustomizableObjectEditorModule.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Templates/Casts.h"
#include "Textures/SlateIcon.h"
#include "Toolkits/IToolkit.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/Package.h"
#include "UObject/TopLevelAssetPath.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtr.h"

class IToolkitHost;


#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"

void FAssetTypeActions_CustomizableObject::GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder )
{
	TArray<TWeakObjectPtr<UCustomizableObject>> Objects = GetTypedWeakObjectPtrs<UCustomizableObject>(InObjects);

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("AssetTypeActions_CustomizableObject","CustomizableObject_Edit","Edit"),
		NSLOCTEXT("AssetTypeActions_CustomizableObject", "CustomizableObject_EditTooltip", "Opens the selected object in the customizable object editor."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetTypeActions_CustomizableObject::ExecuteEdit, Objects ),
			FCanExecuteAction()
			)
		);

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("AssetTypeActions_CustomizableObject", "CustomizableObject_NewInstance", "Create New Instance"),
		NSLOCTEXT("AssetTypeActions_CustomizableObject", "CustomizableObject_NewInstanceTooltip", "Creates a new instance of this customizable object."),
		FSlateIcon(),
		FUIAction(
		FExecuteAction::CreateSP( this, &FAssetTypeActions_CustomizableObject::ExecuteNewInstance, Objects ),
		FCanExecuteAction()
		)
		);

	// Recompile sub-menu
	MenuBuilder.AddSubMenu(
		NSLOCTEXT("AssetTypeActions_CustomizableObject", "CustomizableObject_RecompileObjects", "Recompile"),
		NSLOCTEXT("AssetTypeActions_CustomizableObject", "CustomizableObject_RecompileAllObjectsTooltip", "Recompile Customizable Objects actions"),
		FNewMenuDelegate::CreateSP(this, &FAssetTypeActions_CustomizableObject::MakeRecompileSubMenu, Objects));



	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("AssetTypeActions_CustomizableObject", "CustomizableObject_Debug", "Debug"),
		NSLOCTEXT("AssetTypeActions_CustomizableObject", "CustomizableObject_DebugTooltip", "Open the debugger for the customizable object."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_CustomizableObject::ExecuteDebug, Objects),
			FCanExecuteAction()
		)
	);

}


void FAssetTypeActions_CustomizableObject::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		UCustomizableObject* Object = Cast<UCustomizableObject>(*ObjIt);
		if (Object != NULL)
		{
			ICustomizableObjectEditorModule* CustomizableObjectEditorModule = &FModuleManager::LoadModuleChecked<ICustomizableObjectEditorModule>( "CustomizableObjectEditor" );
			CustomizableObjectEditorModule->CreateCustomizableObjectEditor(Mode, EditWithinLevelEditor, Object);
		}
	}
}


uint32 FAssetTypeActions_CustomizableObject::GetCategories()
{
	const ICustomizableObjectEditorModule* CustomizableObjectEditorModule = &FModuleManager::LoadModuleChecked<ICustomizableObjectEditorModule>("CustomizableObjectEditor");
	return CustomizableObjectEditorModule->GetAssetCategory();
}



void FAssetTypeActions_CustomizableObject::ExecuteNewInstance(TArray<TWeakObjectPtr<UCustomizableObject>> Objects)
{
	const FString DefaultSuffix = TEXT("_Inst");

	TArray<UObject*> ObjectsToSync;

	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		UCustomizableObject* Object = (*ObjIt).Get();
		if ( Object )
		{
			// Determine an appropriate name
			FString Name;
			FString PackageName;
			CreateUniqueAssetName( Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name );
			UPackage* Pkg = CreatePackage(*PackageName);

			UCustomizableObject* CustomizableObject = Cast<UCustomizableObject>(Object);
			UCustomizableObjectInstance* Instance = NewObject<UCustomizableObjectInstance>(Pkg, FName(*Name), RF_Public|RF_Standalone);
			
			if (Instance)
			{
				// Mark the package dirty...
				Pkg->MarkPackageDirty();

				Instance->SetObject(CustomizableObject);

				ObjectsToSync.Add(Instance);
			}
		}
	}

	if ( ObjectsToSync.Num() > 0 )
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowserModule.Get().SyncBrowserToAssets( ObjectsToSync, /*bAllowLockedBrowsers=*/true );

		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(ObjectsToSync[0]);
	}

}


void FAssetTypeActions_CustomizableObject::ExecuteEdit(TArray<TWeakObjectPtr<UCustomizableObject>> Objects)
{
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		UCustomizableObject* Object = (*ObjIt).Get();
		if (Object)
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Object);
		}
	}
}


void FAssetTypeActions_CustomizableObject::ExecuteDebug(TArray<TWeakObjectPtr<UCustomizableObject>> Objects)
{
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		UCustomizableObject* Object = (*ObjIt).Get();
		if (!Object) continue;

		ICustomizableObjectEditorModule* CustomizableObjectEditorModule = &FModuleManager::LoadModuleChecked<ICustomizableObjectEditorModule>("CustomizableObjectEditor");
		if (!CustomizableObjectEditorModule) continue;

		CustomizableObjectEditorModule->CreateCustomizableObjectDebugger(EToolkitMode::Standalone, nullptr , Object);
	}
}


void FAssetTypeActions_CustomizableObject::MakeRecompileSubMenu(FMenuBuilder& MenuBuilder, TArray<TWeakObjectPtr<UCustomizableObject>> InObjects)
{
	// Recompile Selected
	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("AssetTypeActions_CustomizableObject", "CustomizableObject_RecompileSelected", "Selected"),
		NSLOCTEXT("AssetTypeActions_CustomizableObject", "CustomizableObject_RecompileSelectedTooltip", "Recompile selected Customizable Object."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_CustomizableObject::RecompileObjects, ERecompileCO::RCO_Selected, InObjects),
			FCanExecuteAction()
		)
	);

	// Recompile All
	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("AssetTypeActions_CustomizableObject", "CustomizableObject_RecompileAll", "All"),
		NSLOCTEXT("AssetTypeActions_CustomizableObject", "CustomizableObject_RecompileAllToolTip", "Recompile all Customizable Object assets in the project. Can take a really long time and freeze the editor while it is compiling."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_CustomizableObject::RecompileObjects, ERecompileCO::RCO_All, InObjects),
			FCanExecuteAction()
		)
	);

	// Recompile All Root Objects
	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("AssetTypeActions_CustomizableObject", "CustomizableObject_RecompileRootObjects", "All root objects"),
		NSLOCTEXT("AssetTypeActions_CustomizableObject", "CustomizableObject_RecompileRootObjectsTooltip", "Recompile all Root Customizable Objects in the project. Can take a long time and freeze the editor while it is compiling."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_CustomizableObject::RecompileObjects, ERecompileCO::RCO_AllRootObjects, InObjects),
			FCanExecuteAction()
		)
	);

	// Recompile all objects in memory, either loaded by the level, and editor or a during a compilation (child objects)
	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("AssetTypeActions_CustomizableObject", "CustomizableObject_RecompileObjectsInMemory", "All loaded objects"),
		NSLOCTEXT("AssetTypeActions_CustomizableObject", "CustomizableObject_RecompileObjectsInMemoryTooltip", "Recompile all Customizable Objects in memory. This action will compile objects referenced by the current level, CO Editors, etc. Can take a long time and freeze the editor while it is compiling."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_CustomizableObject::RecompileObjects, ERecompileCO::RCO_InMemory, InObjects),
			FCanExecuteAction()
		)
	);

}


void FAssetTypeActions_CustomizableObject::RecompileObjects(ERecompileCO RecompileType, TArray<TWeakObjectPtr<UCustomizableObject>> InObjects)
{
	if (!UCustomizableObjectSystem::GetInstance()) return;

	TArray<FAssetData> ObjectsToRecompile;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	if (RecompileType == ERecompileCO::RCO_Selected)
	{
		for (const TWeakObjectPtr<UCustomizableObject>& Object : InObjects)
		{
			FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(Object.Get()));
			if (AssetData.IsValid())
			{
				ObjectsToRecompile.Add(AssetData);
			}
		}
	}
	else
	{
		FARFilter AssetRegistryFilter;
		AssetRegistryFilter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/CustomizableObject"), TEXT("CustomizableObject")));

		FText Msg;
		switch (RecompileType)
		{
		case ERecompileCO::RCO_All:
			Msg = LOCTEXT("AssetTypeActions_CustomizableObject_RecompileAll_Dialog", "Are you sure you want to recompile all Customizable Objects in this project?\n\nThis action might take a long time and leave the editor unresponsive until it finishes.");
			break;
		case ERecompileCO::RCO_AllRootObjects:
			Msg = LOCTEXT("AssetTypeActions_CustomizableObject_RecompileAllRoot_Dialog", "Are you sure you want to recompile all Root Customizable Objects in this project?\n\nThis action might take a long time and leave the editor unresponsive until it finishes.");
			AssetRegistryFilter.TagsAndValues.Add(FName("IsRoot"), FString::FromInt(1));
			break;
		case ERecompileCO::RCO_InMemory:
			Msg = LOCTEXT("AssetTypeActions_CustomizableObject_RecompileObjectsInMemory_Dialog", "Do you want to recompile all Customizable Objects in memory?\n\nThis action will compile objects referenced by the current level and other loaded objects.");
			break;
		default: check(false);
			break;
		}

		if (FMessageDialog::Open(EAppMsgType::OkCancel, Msg) == EAppReturnType::Ok)
		{
			if (RecompileType == ERecompileCO::RCO_InMemory)
			{
				TArray<FAssetData> OutAssets;
				AssetRegistryModule.Get().GetAssets(AssetRegistryFilter, OutAssets);

				for (const FAssetData& Asset : OutAssets)
				{
					if (Asset.IsAssetLoaded())
					{
						ObjectsToRecompile.Add(Asset);
					}
				}
			}
			else
			{
				AssetRegistryModule.Get().GetAssets(AssetRegistryFilter, ObjectsToRecompile);
			}
		}
	}

	UCustomizableObjectSystem::GetInstance()->RecompileCustomizableObjects(ObjectsToRecompile);
}


bool FAssetTypeActions_CustomizableObject::AssetsActivatedOverride( const TArray<UObject*>& InObjects, EAssetTypeActivationMethod::Type ActivationType )
{
	if ( ActivationType == EAssetTypeActivationMethod::DoubleClicked || ActivationType == EAssetTypeActivationMethod::Opened )
	{
		if ( InObjects.Num() == 1 )
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(InObjects[0]);
			return true;
		}
		else if ( InObjects.Num() > 1 )
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAssets(InObjects);
			return true;
		}
	}

	return false;
}


#undef LOCTEXT_NAMESPACE


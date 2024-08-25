// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetContextMenu.h"

#include "AssetDefinition.h"
#include "AssetRegistry/AssetData.h"
#include "AssetToolsModule.h"
#include "AssetViewUtils.h"
#include "CollectionAssetManagement.h"
#include "CollectionManagerModule.h"
#include "CollectionManagerTypes.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "ContentBrowserCommands.h"
#include "ContentBrowserDataSource.h"
#include "ContentBrowserDelegates.h"
#include "ContentBrowserItemData.h"
#include "ContentBrowserMenuContexts.h"
#include "ContentBrowserModule.h"
#include "ContentBrowserSingleton.h"
#include "ContentBrowserUtils.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/SlateDelegates.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformCrt.h"
#include "HAL/PlatformStackWalk.h"
#include "IAssetTools.h"
#include "IAssetTypeActions.h"
#include "ICollectionManager.h"
#include "Internationalization/Internationalization.h"
#include "Logging/MessageLog.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "Misc/NamePermissionList.h"
#include "Misc/PathViews.h"
#include "Misc/Paths.h"
#include "Misc/StringBuilder.h"
#include "Modules/ModuleManager.h"
#include "SAssetView.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateTypes.h"
#include "Templates/Casts.h"
#include "Templates/Tuple.h"
#include "Templates/UnrealTemplate.h"
#include "Textures/SlateIcon.h"
#include "ToolMenu.h"
#include "ToolMenuContext.h"
#include "ToolMenuDelegates.h"
#include "ToolMenuSection.h"
#include "ToolMenus.h"
#include "Toolkits/GlobalEditorCommonCommands.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealNames.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "AssetDefinitionRegistry.h"
#include "TelemetryRouter.h"
#include "AssetRegistry/AssetRegistryHelpers.h"
#include "Engine/AssetManager.h"
#include "Misc/WarnIfAssetsLoadedInScope.h"

class FMenuBuilder;
class SWidget;

#define LOCTEXT_NAMESPACE "ContentBrowser"

FAssetContextMenu::FAssetContextMenu(const TWeakPtr<SAssetView>& InAssetView)
	: AssetView(InAssetView)
{
}

void FAssetContextMenu::BindCommands(TSharedPtr< FUICommandList >& Commands)
{
	Commands->MapAction(FGenericCommands::Get().Duplicate, FUIAction(
		FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecuteDuplicate),
		FCanExecuteAction::CreateSP(this, &FAssetContextMenu::CanExecuteDuplicate)
		));

	Commands->MapAction(FGlobalEditorCommonCommands::Get().FindInContentBrowser, FUIAction(
		FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecuteSyncToAssetTree),
		FCanExecuteAction::CreateSP(this, &FAssetContextMenu::CanExecuteSyncToAssetTree)
		));
}

TSharedRef<SWidget> FAssetContextMenu::MakeContextMenu(TArrayView<const FContentBrowserItem> InSelectedItems, const FSourcesData& InSourcesData, TSharedPtr< FUICommandList > InCommandList)
{
	FWarnIfAssetsLoadedInScope WarnIfAssetsLoaded;
	
	SetSelectedItems(InSelectedItems);
	SourcesData = InSourcesData;

	// Cache any vars that are used in determining if you can execute any actions.
	// Useful for actions whose "CanExecute" will not change or is expensive to calculate.
	CacheCanExecuteVars();

	// Get all menu extenders for this context menu from the content browser module
	FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>( TEXT("ContentBrowser") );
	TArray<FContentBrowserMenuExtender_SelectedAssets> MenuExtenderDelegates = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();

	TSharedPtr<FExtender> MenuExtender;
	{
		TArray<FAssetData> SelectedAssets;
		for (const FContentBrowserItem& SelectedFile : SelectedFiles)
		{
			FAssetData ItemAssetData;
			if (SelectedFile.Legacy_TryGetAssetData(ItemAssetData))
			{
				SelectedAssets.Add(ItemAssetData);
			}
		}

		if (SelectedAssets.Num() > 0)
		{
			TArray<TSharedPtr<FExtender>> Extenders;
			for (int32 i = 0; i < MenuExtenderDelegates.Num(); ++i)
			{
				if (MenuExtenderDelegates[i].IsBound())
				{
					Extenders.Add(MenuExtenderDelegates[i].Execute(SelectedAssets));
				}
			}
			MenuExtender = FExtender::Combine(Extenders);
		}
	}

	UContentBrowserAssetContextMenuContext* ContextObject = NewObject<UContentBrowserAssetContextMenuContext>();
	ContextObject->AssetContextMenu = SharedThis(this);

	UToolMenus* ToolMenus = UToolMenus::Get();

	static const FName BaseMenuName("ContentBrowser.AssetContextMenu");
	static const FName ItemContextMenuName("ContentBrowser.ItemContextMenu");
	RegisterContextMenu(BaseMenuName);
	
	// Create menu hierarchy based on class hierarchy
	FName MenuName = BaseMenuName;
	{
		// TODO: Ideally all of this asset specific stuff would happen in the asset data source, however we 
		// need to keep it here for now to build the correct menu name and register the correct extenders

		// Objects must be loaded for this operation... for now
		UContentBrowserDataSource* CommonDataSource = nullptr;
		{
			TArray<FAssetData> SelectedAssets;
			bool bKeepCheckingCommonDataSource = true;
			for (const FContentBrowserItem& SelectedItem : SelectedItems)
			{
				if (bKeepCheckingCommonDataSource)
				{
					if (const FContentBrowserItemData* PrimaryInternalItem = SelectedItem.GetPrimaryInternalItem())
					{
						if (UContentBrowserDataSource* OwnerDataSource = PrimaryInternalItem->GetOwnerDataSource())
						{
							if (CommonDataSource == nullptr)
							{
								CommonDataSource = OwnerDataSource;
							}
							else if (CommonDataSource != OwnerDataSource)
							{
								CommonDataSource = nullptr;
								bKeepCheckingCommonDataSource = false;
							}
						}
					}
				}

				FAssetData ItemAssetData;
				if (SelectedItem.Legacy_TryGetAssetData(ItemAssetData))
				{
					SelectedAssets.Add(MoveTemp(ItemAssetData));
				}
			}

			ContextObject->bCanBeModified = SelectedAssets.Num() == 0;
			ContextObject->SelectedAssets = MoveTemp(SelectedAssets);
		}
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
		const TSharedRef<FPathPermissionList>& WritableFolderPermission = AssetToolsModule.Get().GetWritableFolderPermissionList();
		
		ContextObject->bContainsUnsupportedAssets = false;
		for (const FContentBrowserItem& SelectedItem : SelectedItems)
		{
			if(!SelectedItem.IsSupported())
			{
				ContextObject->bContainsUnsupportedAssets = true;
				break;
			}
		}

		const TArray<FAssetData>& SelectedAssets = ContextObject->SelectedAssets;

		if (SelectedAssets.Num() > 0 && SelectedAssets.Num() == SelectedItems.Num())
		{

			// Find common class for selected objects
			UClass* CommonClass = nullptr;
			for (int32 ObjIdx = 0; ObjIdx < SelectedAssets.Num(); ++ObjIdx)
			{
				if (CommonClass == nullptr)
				{
					CommonClass = UAssetRegistryHelpers::FindAssetNativeClass(SelectedAssets[ObjIdx]);
					continue;
				}

				// Update the CommonClass until we find a common shared class, ignore anything that's not native.
				UClass* Class = UAssetRegistryHelpers::FindAssetNativeClass(SelectedAssets[ObjIdx]);
				while (Class && !Class->IsChildOf(CommonClass))
				{
					CommonClass = CommonClass->GetSuperClass();
				}
			}
			ContextObject->CommonClass = CommonClass;

			ContextObject->bCanBeModified = true;
			ContextObject->bHasCookedPackages = false;
			for (const FAssetData& SelectedAsset : SelectedAssets)
			{
				if (SelectedAsset.HasAnyPackageFlags(PKG_Cooked | PKG_FilterEditorOnly))
				{
					ContextObject->bCanBeModified = false;
					ContextObject->bHasCookedPackages = true;
					break;
				}

				if (WritableFolderPermission->HasFiltering() && !WritableFolderPermission->PassesStartsWithFilter(SelectedAsset.PackageName))
				{
					ContextObject->bCanBeModified = false;
					break;
				}

				if (const UClass* AssetClass = SelectedAsset.GetClass())
				{
					if (AssetClass->IsChildOf<UClass>())
					{
						ContextObject->bCanBeModified = false;
						break;
					}
				}
			}

			// We can have a null common class if an asset is from unloaded plugin or an missing class.
			if (CommonClass)
			{
				MenuName = UToolMenus::JoinMenuPaths(BaseMenuName, CommonClass->GetFName());

				RegisterMenuHierarchy(CommonClass);

				// Find asset actions for common class
				ContextObject->CommonAssetDefinition = UAssetDefinitionRegistry::Get()->GetAssetDefinitionForClass(ContextObject->CommonClass);
				PRAGMA_DISABLE_DEPRECATION_WARNINGS
				ContextObject->CommonAssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(ContextObject->CommonClass).Pin();
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
			}
		}
		else
		{
			if (CommonDataSource)
			{
				ContextObject->bCanBeModified = true;
				ContextObject->bHasCookedPackages = false;

				if (WritableFolderPermission->HasFiltering())
				{
					for (const FContentBrowserItem& SelectedItem : SelectedItems)
					{
						if (!WritableFolderPermission->PassesStartsWithFilter(SelectedItem.GetInternalPath()))
						{
							ContextObject->bCanBeModified = false;
							break;
						}
					}
				}

				for (const FAssetData& SelectedAsset : SelectedAssets)
				{
					if (SelectedAsset.HasAnyPackageFlags(PKG_Cooked | PKG_FilterEditorOnly))
					{
						ContextObject->bCanBeModified = false;
						ContextObject->bHasCookedPackages = true;
						break;
					}

					if (const UClass* AssetClass = SelectedAsset.GetClass())
					{
						if (AssetClass->IsChildOf<UClass>())
						{
							ContextObject->bCanBeModified = false;
							break;
						}
					}
				}

				MenuName = UToolMenus::JoinMenuPaths(ItemContextMenuName, CommonDataSource->GetFName());

				if (!ToolMenus->IsMenuRegistered(MenuName))
				{
					ToolMenus->RegisterMenu(MenuName, BaseMenuName);
				}
			}
		}
	}

	FToolMenuContext MenuContext(InCommandList, MenuExtender, ContextObject);

	{
		UContentBrowserDataMenuContext_FileMenu* DataContextObject = NewObject<UContentBrowserDataMenuContext_FileMenu>();
		DataContextObject->SelectedItems = SelectedItems;
		DataContextObject->SelectedCollections = SourcesData.Collections;
		DataContextObject->bCanBeModified = ContextObject->bCanBeModified;
		DataContextObject->bHasCookedPackages = ContextObject->bHasCookedPackages;
		DataContextObject->bContainsUnsupportedAssets = ContextObject->bContainsUnsupportedAssets;
		DataContextObject->ParentWidget = AssetView;
		DataContextObject->OnShowInPathsView = OnShowInPathsViewRequested;
		DataContextObject->OnRefreshView = OnAssetViewRefreshRequested;
		MenuContext.AddObject(DataContextObject);
	}

	return ToolMenus->GenerateWidget(MenuName, MenuContext);
}

void FAssetContextMenu::RegisterMenuHierarchy(UClass* InClass)
{
	static const FName BaseMenuName("ContentBrowser.AssetContextMenu");

	UToolMenus* ToolMenus = UToolMenus::Get();

	for (UClass* CurrentClass = InClass; CurrentClass; CurrentClass = CurrentClass->GetSuperClass())
	{
		FName CurrentMenuName = UToolMenus::JoinMenuPaths(BaseMenuName, CurrentClass->GetFName());
		if (!ToolMenus->IsMenuRegistered(CurrentMenuName))
		{
			FName ParentMenuName;
			UClass* ParentClass = CurrentClass->GetSuperClass();
			if (ParentClass == UObject::StaticClass() || ParentClass == nullptr)
			{
				ParentMenuName = BaseMenuName;
			}
			else
			{
				ParentMenuName = UToolMenus::JoinMenuPaths(BaseMenuName, ParentClass->GetFName());
			}

			ToolMenus->RegisterMenu(CurrentMenuName, ParentMenuName);

			if (ParentMenuName == BaseMenuName)
			{
				break;
			}
		}
	}
}

void FAssetContextMenu::RegisterContextMenu(const FName MenuName)
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	if (!ToolMenus->IsMenuRegistered(MenuName))
	{
		UToolMenu* Menu = ToolMenus->RegisterMenu(MenuName);
		FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");

		// TODO Remove when IAssetTypeActions is dead or fully deprecated.
		{
			// Note: Do  not use "GetActions" again when copying this code, otherwise "GetActions" menu entry will be overwritten
			Section.AddDynamicEntry("GetActions", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
			{
				UContentBrowserAssetContextMenuContext* Context = InSection.FindContext<UContentBrowserAssetContextMenuContext>();
				PRAGMA_DISABLE_DEPRECATION_WARNINGS
					if (Context && Context->CommonAssetTypeActions.IsValid() && Context->CommonAssetTypeActions.Pin()->ShouldCallGetActions())
					{
						TArray<UObject*> SelectedObjects = Context->LoadSelectedObjectsIfNeeded();						
						//  It's possible for an unloaded object to be selected if the content browser is out of date, in that case it is unnecessary to call `GetActions`
						if (SelectedObjects.Num() > 0)
						{
							Context->CommonAssetTypeActions.Pin()->GetActions(SelectedObjects, InSection);
						}
					}
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
			}));

			Section.AddDynamicEntry("GetActionsLegacy", FNewToolMenuDelegateLegacy::CreateLambda([](FMenuBuilder& MenuBuilder, UToolMenu* InMenu)
			{
				UContentBrowserAssetContextMenuContext* Context = InMenu->FindContext<UContentBrowserAssetContextMenuContext>();
				PRAGMA_DISABLE_DEPRECATION_WARNINGS
					if (Context && Context->CommonAssetTypeActions.IsValid() && Context->CommonAssetTypeActions.Pin()->ShouldCallGetActions())
					{
						TArray<UObject*> SelectedObjects = Context->LoadSelectedObjectsIfNeeded();
						if (SelectedObjects.Num() > 0)
						{
							Context->CommonAssetTypeActions.Pin()->GetActions(SelectedObjects, MenuBuilder);
						}
					}
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
			}));
		}		

		Menu->AddDynamicSection("AddMenuOptions", FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
		{
			const UContentBrowserAssetContextMenuContext* Context = InMenu->FindContext<UContentBrowserAssetContextMenuContext>();
			if (Context && Context->AssetContextMenu.IsValid())
			{
				Context->AssetContextMenu.Pin()->AddMenuOptions(InMenu);
			}
		}));
	}
}

void FAssetContextMenu::AddMenuOptions(UToolMenu* InMenu)
{
	UContentBrowserDataMenuContext_FileMenu* Context = InMenu->FindContext<UContentBrowserDataMenuContext_FileMenu>();
	const bool bCanBeModified = !Context || Context->bCanBeModified;

	// Add any type-specific context menu options
	AddAssetTypeMenuOptions(InMenu);

	// Add quick access to common commands.
	AddCommonMenuOptions(InMenu);

	// Add quick access to view commands
	AddExploreMenuOptions(InMenu);

	static const IConsoleVariable* EnablePublicAssetFeatureCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("AssetTools.EnablePublicAssetFeature"));
	if (EnablePublicAssetFeatureCVar && EnablePublicAssetFeatureCVar->GetBool())
	{
		AddPublicStateMenuOptions(InMenu);
	}

	// Add reference options
	AddReferenceMenuOptions(InMenu);

	// Add collection options
	if (bCanBeModified)
	{
		AddCollectionMenuOptions(InMenu);
	}
}

void FAssetContextMenu::SetSelectedItems(TArrayView<const FContentBrowserItem> InSelectedItems)
{
	SelectedItems.Reset();
	SelectedItems.Append(InSelectedItems.GetData(), InSelectedItems.Num());

	SelectedFiles.Reset();
	SelectedFolders.Reset();
	for (const FContentBrowserItem& SelectedItem : SelectedItems)
	{
		if (SelectedItem.IsFile())
		{
			SelectedFiles.Add(SelectedItem);
		}

		if (SelectedItem.IsFolder())
		{
			SelectedFolders.Add(SelectedItem);
		}
	}
}

void FAssetContextMenu::SetOnShowInPathsViewRequested(const FOnShowInPathsViewRequested& InOnShowInPathsViewRequested)
{
	OnShowInPathsViewRequested = InOnShowInPathsViewRequested;
}

void FAssetContextMenu::SetOnRenameRequested(const FOnRenameRequested& InOnRenameRequested)
{
	OnRenameRequested = InOnRenameRequested;
}

void FAssetContextMenu::SetOnDuplicateRequested(const FOnDuplicateRequested& InOnDuplicateRequested)
{
	OnDuplicateRequested = InOnDuplicateRequested;
}

void FAssetContextMenu::SetOnEditRequested(const FOnEditRequested& InOnEditRequested)
{
	OnEditRequested = InOnEditRequested;
}

void FAssetContextMenu::SetOnAssetViewRefreshRequested(const FOnAssetViewRefreshRequested& InOnAssetViewRefreshRequested)
{
	OnAssetViewRefreshRequested = InOnAssetViewRefreshRequested;
}

bool FAssetContextMenu::AddCommonMenuOptions(UToolMenu* Menu)
{
	UContentBrowserDataMenuContext_FileMenu* Context = Menu->FindContext<UContentBrowserDataMenuContext_FileMenu>();
	const bool bCanBeModified = !Context || Context->bCanBeModified;

	{
		FToolMenuSection& Section = Menu->AddSection("CommonAssetActions", LOCTEXT("CommonAssetActionsMenuHeading", "Common"));

		
		if (bCanBeModified)
		{
			// Edit
			Section.AddMenuEntry(
				"EditAsset",
				LOCTEXT("EditAsset", "Edit..."),
				LOCTEXT("EditAssetTooltip", "Opens the selected item(s) for edit."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Edit"),
				FUIAction(
					FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecuteEditItems),
					FCanExecuteAction::CreateSP(this, &FAssetContextMenu::CanExecuteEditItems)
				)
			);

			// Rename
			Section.AddMenuEntry(FGenericCommands::Get().Rename,
				LOCTEXT("Rename", "Rename"),
				LOCTEXT("RenameTooltip", "Rename the selected item.")
			);

			// Duplicate
			Section.AddMenuEntry(FGenericCommands::Get().Duplicate,
				LOCTEXT("Duplicate", "Duplicate"),
				LOCTEXT("DuplicateTooltip", "Create a copy of the selected item(s)."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Duplicate")
			);

			// Save
			Section.AddMenuEntry(FContentBrowserCommands::Get().SaveSelectedAsset,
				LOCTEXT("SaveAsset", "Save"),
				LOCTEXT("SaveAssetTooltip", "Saves the item to file."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Save")
			);

			// Delete
			Section.AddMenuEntry(FGenericCommands::Get().Delete,
				LOCTEXT("Delete", "Delete"),
				TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &FAssetContextMenu::GetDeleteToolTip))
			);
		}
	}

	return true;
}

void FAssetContextMenu::AddExploreMenuOptions(UToolMenu* Menu)
{
	UContentBrowserDataMenuContext_FileMenu* Context = Menu->FindContext<UContentBrowserDataMenuContext_FileMenu>();

	FToolMenuSection& Section = Menu->AddSection("AssetContextExploreMenuOptions", LOCTEXT("AssetContextExploreMenuOptionsHeading", "Explore"));
	{
		// Find in Content Browser
		Section.AddMenuEntry(
			FGlobalEditorCommonCommands::Get().FindInContentBrowser, 
			LOCTEXT("ShowInFolderView", "Show in Folder View"),
			LOCTEXT("ShowInFolderViewTooltip", "Selects the folder that contains this asset in the Content Browser Sources Panel.")
			);

		if (!Context->bHasCookedPackages)
		{
			// Find in Explorer
			Section.AddMenuEntry(
				"FindInExplorer",
				ContentBrowserUtils::GetExploreFolderText(),
				LOCTEXT("FindInExplorerTooltip", "Finds this asset on disk"),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.ShowInExplorer"),
				FUIAction(
					FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecuteFindInExplorer),
					FCanExecuteAction::CreateSP(this, &FAssetContextMenu::CanExecuteFindInExplorer)
				)
			);
		}
	}
}

bool FAssetContextMenu::AddPublicStateMenuOptions(UToolMenu* Menu)
{
	if (!bCanExecutePublicAssetToggle && !bCanExecuteBulkSetPublicAsset)
	{
		return false;
	}

	UContentBrowserDataMenuContext_FileMenu* Context = Menu->FindContext<UContentBrowserDataMenuContext_FileMenu>();
	{
		FToolMenuSection& Section = Menu->AddSection("AssetPublicState", LOCTEXT("PublicStateHandling", "Asset State"));

		if (SelectedFiles.Num() == 1 && bCanExecutePublicAssetToggle)
		{
			Section.AddMenuEntry(
				"PublicAsset",
				LOCTEXT("PublicAssetToggle", "Public Asset"),
				LOCTEXT("PublicAssetToggleTooltip", "Sets the asset to be referencable by other Plugins"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecutePublicAssetToggle),
					FCanExecuteAction::CreateSP(this, &FAssetContextMenu::CanExecutePublicAssetToggle),
					FIsActionChecked::CreateSP(this, &FAssetContextMenu::IsSelectedAssetPublic)
				),
				EUserInterfaceActionType::RadioButton
			);

			Section.AddMenuEntry(
				"PrivateAsset",
				LOCTEXT("SetPrivateAsset", "Private Asset"),
				LOCTEXT("SetAssetPrivateTooltip", "Sets the asset so it can't be referenced by other Plugins"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecutePublicAssetToggle),
					FCanExecuteAction::CreateSP(this, &FAssetContextMenu::CanExecutePublicAssetToggle),
					FIsActionChecked::CreateSP(this, &FAssetContextMenu::IsSelectedAssetPrivate)
				),
				EUserInterfaceActionType::RadioButton
			);
		}
		else if (SelectedFiles.Num() > 1 && bCanExecuteBulkSetPublicAsset)
		{
			Section.AddMenuEntry(
				"MarkSelectedAsPublic",
				LOCTEXT("MarkSelectedAsPublic", "Mark Selected As Public"),
				LOCTEXT("MarkSelectedAsPublicTooltip", "Sets all selected assets to be publicly available for reference by other plugins"),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions.PublicAssetToggle"),
				FUIAction(
					FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecuteBulkSetPublicAsset),
					FCanExecuteAction::CreateSP(this, &FAssetContextMenu::CanExecuteBulkSetPublicAsset)
				)
			);

			Section.AddMenuEntry(
				"MarkSelectedAsPrivate",
				LOCTEXT("MarkSelectedAsPrivate", "Mark Selected As Private"),
				LOCTEXT("MarkSelectedAsPrivateTooltip", "Sets all selected assets to be private and unavailable for reference by other plugins"),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions.PublicAssetToggle"),
				FUIAction(
					FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecuteBulkUnsetPublicAsset),
					FCanExecuteAction::CreateSP(this, &FAssetContextMenu::CanExecuteBulkSetPublicAsset)
				)
			);
		}
	}

	return true;
}

bool FAssetContextMenu::AddReferenceMenuOptions(UToolMenu* Menu)
{
	UContentBrowserDataMenuContext_FileMenu* Context = Menu->FindContext<UContentBrowserDataMenuContext_FileMenu>();
	{
		FToolMenuSection& Section = Menu->AddSection("AssetContextReferences", LOCTEXT("ReferencesMenuHeading", "References"));

		Section.AddMenuEntry(
			"CopyReference",
			LOCTEXT("CopyReference", "Copy Reference"),
			LOCTEXT("CopyReferenceTooltip", "Copies reference paths for the selected assets to the clipboard."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Copy"),
			FUIAction( FExecuteAction::CreateSP( this, &FAssetContextMenu::ExecuteCopyReference ) )
			);
	
		if (!Context->bHasCookedPackages)
		{
			Section.AddMenuEntry(
				"CopyFilePath",
				LOCTEXT("CopyFilePath", "Copy File Path"),
				LOCTEXT("CopyFilePathTooltip", "Copies the file paths on disk for the selected assets to the clipboard."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Copy"),
				FUIAction(FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecuteCopyFilePath))
			);
		}
	}

	return true;
}

bool FAssetContextMenu::AddAssetTypeMenuOptions(UToolMenu* Menu)
{
	bool bAnyTypeOptions = false;

	UContentBrowserAssetContextMenuContext* Context = Menu->FindContext<UContentBrowserAssetContextMenuContext>();
	if (Context && Context->SelectedAssets.Num() > 0)
	{
		// Label "GetAssetActions" section
		FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
		if (Context->CommonAssetDefinition)
		{
			Section.Label = FText::Format(NSLOCTEXT("AssetTools", "AssetSpecificOptionsMenuHeading", "{0} Actions"), Context->CommonAssetDefinition->GetAssetDisplayName());
		}
		else if (Context->CommonClass)
		{
			Section.Label = FText::Format(NSLOCTEXT("AssetTools", "AssetSpecificOptionsMenuHeading", "{0} Actions"), FText::FromName(Context->CommonClass->GetFName()));
		}
		else
		{
			Section.Label = FText::Format(NSLOCTEXT("AssetTools", "AssetSpecificOptionsMenuHeading", "{0} Actions"), FText::FromString(TEXT("Asset")));
		}

		bAnyTypeOptions = true;
	}

	return bAnyTypeOptions;
}

bool FAssetContextMenu::AddCollectionMenuOptions(UToolMenu* Menu)
{
	class FManageCollectionsContextMenu
	{
	public:
		static void CreateManageCollectionsSubMenu(UToolMenu* SubMenu, TSharedRef<FCollectionAssetManagement> QuickAssetManagement)
		{
			FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

			TArray<FCollectionNameType> AvailableCollections;
			CollectionManagerModule.Get().GetRootCollections(AvailableCollections);

			CreateManageCollectionsSubMenu(SubMenu, QuickAssetManagement, MoveTemp(AvailableCollections));
		}

		static void CreateManageCollectionsSubMenu(UToolMenu* SubMenu, TSharedRef<FCollectionAssetManagement> QuickAssetManagement, TArray<FCollectionNameType> AvailableCollections)
		{
			FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

			AvailableCollections.Sort([](const FCollectionNameType& One, const FCollectionNameType& Two) -> bool
			{
				return One.Name.LexicalLess(Two.Name);
			});

			FToolMenuSection& Section = SubMenu->AddSection("Section");
			for (const FCollectionNameType& AvailableCollection : AvailableCollections)
			{
				// Never display system collections
				if (AvailableCollection.Type == ECollectionShareType::CST_System)
				{
					continue;
				}

				// Can only manage assets for static collections
				ECollectionStorageMode::Type StorageMode = ECollectionStorageMode::Static;
				CollectionManagerModule.Get().GetCollectionStorageMode(AvailableCollection.Name, AvailableCollection.Type, StorageMode);
				if (StorageMode != ECollectionStorageMode::Static)
				{
					continue;
				}

				TArray<FCollectionNameType> AvailableChildCollections;
				CollectionManagerModule.Get().GetChildCollections(AvailableCollection.Name, AvailableCollection.Type, AvailableChildCollections);

				if (AvailableChildCollections.Num() > 0)
				{
					Section.AddSubMenu(
						NAME_None,
						FText::FromName(AvailableCollection.Name), 
						FText::GetEmpty(), 
						FNewToolMenuDelegate::CreateStatic(&FManageCollectionsContextMenu::CreateManageCollectionsSubMenu, QuickAssetManagement, AvailableChildCollections),
						FUIAction(
							FExecuteAction::CreateStatic(&FManageCollectionsContextMenu::OnCollectionClicked, QuickAssetManagement, AvailableCollection),
							FCanExecuteAction::CreateStatic(&FManageCollectionsContextMenu::IsCollectionEnabled, QuickAssetManagement, AvailableCollection),
							FGetActionCheckState::CreateStatic(&FManageCollectionsContextMenu::GetCollectionCheckState, QuickAssetManagement, AvailableCollection)
							), 
						EUserInterfaceActionType::ToggleButton,
						false,
						FSlateIcon(FAppStyle::GetAppStyleSetName(), ECollectionShareType::GetIconStyleName(AvailableCollection.Type))
						);
				}
				else
				{
					Section.AddMenuEntry(
						NAME_None,
						FText::FromName(AvailableCollection.Name), 
						FText::GetEmpty(), 
						FSlateIcon(FAppStyle::GetAppStyleSetName(), ECollectionShareType::GetIconStyleName(AvailableCollection.Type)),
						FUIAction(
							FExecuteAction::CreateStatic(&FManageCollectionsContextMenu::OnCollectionClicked, QuickAssetManagement, AvailableCollection),
							FCanExecuteAction::CreateStatic(&FManageCollectionsContextMenu::IsCollectionEnabled, QuickAssetManagement, AvailableCollection),
							FGetActionCheckState::CreateStatic(&FManageCollectionsContextMenu::GetCollectionCheckState, QuickAssetManagement, AvailableCollection)
							), 
						EUserInterfaceActionType::ToggleButton
						);
				}
			}
		}

	private:
		static bool IsCollectionEnabled(TSharedRef<FCollectionAssetManagement> QuickAssetManagement, FCollectionNameType InCollectionKey)
		{
			return QuickAssetManagement->IsCollectionEnabled(InCollectionKey);
		}

		static ECheckBoxState GetCollectionCheckState(TSharedRef<FCollectionAssetManagement> QuickAssetManagement, FCollectionNameType InCollectionKey)
		{
			return QuickAssetManagement->GetCollectionCheckState(InCollectionKey);
		}

		static void OnCollectionClicked(TSharedRef<FCollectionAssetManagement> QuickAssetManagement, FCollectionNameType InCollectionKey)
		{
			const double BeginTimeSec = FPlatformTime::Seconds();
			const int32 ObjectCount = QuickAssetManagement->GetCurrentAssetCount();
			
			// The UI actions don't give you the new check state, so we need to emulate the behavior of SCheckBox
			// Basically, checked will transition to unchecked (removing items), and anything else will transition to checked (adding items)
			const bool RemoveFromCollection = GetCollectionCheckState(QuickAssetManagement, InCollectionKey) == ECheckBoxState::Checked;
			if (RemoveFromCollection)
			{
				QuickAssetManagement->RemoveCurrentAssetsFromCollection(InCollectionKey);
			}
			else
			{
				QuickAssetManagement->AddCurrentAssetsToCollection(InCollectionKey);
			}

			const double DurationSec = FPlatformTime::Seconds() - BeginTimeSec;
			
			{
				if (RemoveFromCollection)
				{
					FAssetRemovedFromCollectionTelemetryEvent AssetRemoved;
					AssetRemoved.DurationSec = DurationSec;
					AssetRemoved.NumRemoved = ObjectCount;
					AssetRemoved.CollectionShareType = InCollectionKey.Type;
					AssetRemoved.Workflow = ECollectionTelemetryAssetRemovedWorkflow::ContextMenu;
					FTelemetryRouter::Get().ProvideTelemetry(AssetRemoved);
				}
				else
				{
					FAssetAddedToCollectionTelemetryEvent AssetAdded;
					AssetAdded.DurationSec = DurationSec;
					AssetAdded.NumAdded = ObjectCount;
					AssetAdded.CollectionShareType = InCollectionKey.Type;
					AssetAdded.Workflow = ECollectionTelemetryAssetAddedWorkflow::ContextMenu;
					FTelemetryRouter::Get().ProvideTelemetry(AssetAdded);
				}
			}
		}
	};

	bool bHasAddedItems = false;

	FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

	FToolMenuSection& Section = Menu->AddSection("AssetContextCollections", LOCTEXT("AssetCollectionOptionsMenuHeading", "Collections"));

	// Show a sub-menu that allows you to quickly add or remove the current asset selection from the available collections
	if (CollectionManagerModule.Get().HasCollections())
	{
		TSharedRef<FCollectionAssetManagement> QuickAssetManagement = MakeShared<FCollectionAssetManagement>();

		TArray<FSoftObjectPath> SelectedItemCollectionIds;
		for (const FContentBrowserItem& SelectedItem : SelectedFiles)
		{
			FSoftObjectPath ItemCollectionId;
			if (SelectedItem.TryGetCollectionId(ItemCollectionId))
			{
				SelectedItemCollectionIds.Add(ItemCollectionId);
			}
		}
		QuickAssetManagement->SetCurrentAssetPaths(SelectedItemCollectionIds);

		Section.AddSubMenu(
			"ManageCollections",
			LOCTEXT("ManageCollections", "Manage Collections"),
			FText::Format(LOCTEXT("ManageCollections_ToolTip", "Manage the collections that the selected {0}|plural(one=item belongs, other=items belong) to."), SelectedFiles.Num()),
			FNewToolMenuDelegate::CreateStatic(&FManageCollectionsContextMenu::CreateManageCollectionsSubMenu, QuickAssetManagement),
			false, // default value
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.Collections")
			);

		bHasAddedItems = true;
	}

	// "Remove from collection" (only display option if exactly one collection is selected)
	if ( SourcesData.Collections.Num() == 1 && !SourcesData.IsDynamicCollection() )
	{
		Section.AddMenuEntry(
			"RemoveFromCollection",
			FText::Format(LOCTEXT("RemoveFromCollectionFmt", "Remove From {0}"), FText::FromName(SourcesData.Collections[0].Name)),
			LOCTEXT("RemoveFromCollection_ToolTip", "Removes the selected item from the current collection."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &FAssetContextMenu::ExecuteRemoveFromCollection ),
				FCanExecuteAction::CreateSP( this, &FAssetContextMenu::CanExecuteRemoveFromCollection )
				)
			);

		bHasAddedItems = true;
	}


	return bHasAddedItems;
}

void FAssetContextMenu::ExecuteSyncToAssetTree()
{
	// Copy this as the sync may adjust our selected assets array
	const TArray<FContentBrowserItem> SelectedFilesCopy = SelectedFiles;
	OnShowInPathsViewRequested.ExecuteIfBound(SelectedFilesCopy);
}

void FAssetContextMenu::ExecuteFindInExplorer()
{
	ContentBrowserUtils::ExploreFolders(SelectedFiles, AssetView.Pin().ToSharedRef());
}

bool FAssetContextMenu::CanExecuteEditItems() const
{
	bool bCanEdit = false;
	for (const FContentBrowserItem& SelectedItem : SelectedFiles)
	{
		bCanEdit |= SelectedItem.CanEdit();
	}
	return bCanEdit;
}

void FAssetContextMenu::ExecuteEditItems()
{
	if (SelectedFiles.Num() > 0)
	{
		OnEditRequested.ExecuteIfBound(SelectedFiles);
	}
}

void FAssetContextMenu::ExecuteSaveAsset()
{
	const EContentBrowserItemSaveFlags SaveFlags = EContentBrowserItemSaveFlags::None;

	// Batch these by their data sources
	TMap<UContentBrowserDataSource*, TArray<FContentBrowserItemData>> SourcesAndItems;
	for (const FContentBrowserItem& SelectedItem : SelectedFiles)
	{
		FContentBrowserItem::FItemDataArrayView ItemDataArray = SelectedItem.GetInternalItems();
		for (const FContentBrowserItemData& ItemData : ItemDataArray)
		{
			if (UContentBrowserDataSource* ItemDataSource = ItemData.GetOwnerDataSource())
			{
				FText SaveErrorMsg;
				if (ItemDataSource->CanSaveItem(ItemData, SaveFlags, &SaveErrorMsg))
				{
					TArray<FContentBrowserItemData>& ItemsForSource = SourcesAndItems.FindOrAdd(ItemDataSource);
					ItemsForSource.Add(ItemData);
				}
				else
				{
					AssetViewUtils::ShowErrorNotifcation(SaveErrorMsg);
				}
			}
		}
	}

	// Execute the operation now
	for (const auto& SourceAndItemsPair : SourcesAndItems)
	{
		SourceAndItemsPair.Key->BulkSaveItems(SourceAndItemsPair.Value, SaveFlags);
	}
}

void FAssetContextMenu::ExecuteDuplicate() 
{
	if (SelectedFiles.Num() > 0)
	{
		OnDuplicateRequested.ExecuteIfBound(SelectedFiles);
	}
}

void FAssetContextMenu::ExecuteRename(EContentBrowserViewContext ViewContext)
{
	if (SelectedItems.Num() == 1)
	{
		OnRenameRequested.ExecuteIfBound(SelectedItems[0], ViewContext);
	}
}

void FAssetContextMenu::ExecuteDelete()
{
	// Batch these by their data sources
	TMap<UContentBrowserDataSource*, TArray<FContentBrowserItemData>> SourcesAndItems;
	for (const FContentBrowserItem& SelectedItem : SelectedFiles)
	{
		FContentBrowserItem::FItemDataArrayView ItemDataArray = SelectedItem.GetInternalItems();
		for (const FContentBrowserItemData& ItemData : ItemDataArray)
		{
			if (UContentBrowserDataSource* ItemDataSource = ItemData.GetOwnerDataSource())
			{
				FText DeleteErrorMsg;
				if (ItemDataSource->CanDeleteItem(ItemData, &DeleteErrorMsg))
				{
					TArray<FContentBrowserItemData>& ItemsForSource = SourcesAndItems.FindOrAdd(ItemDataSource);
					ItemsForSource.Add(ItemData);
				}
				else
				{
					AssetViewUtils::ShowErrorNotifcation(DeleteErrorMsg);
				}
			}
		}
	}

	// Execute the operation now
	for (const auto& SourceAndItemsPair : SourcesAndItems)
	{
		SourceAndItemsPair.Key->BulkDeleteItems(SourceAndItemsPair.Value);
	}

	// If we had any folders selected, ask the user whether they want to delete them 
	// as it can be slow to build the deletion dialog on an accidental click
	if (SelectedFolders.Num() > 0)
	{
		FText Prompt;
		if (SelectedFolders.Num() == 1)
		{
			Prompt = FText::Format(LOCTEXT("FolderDeleteConfirm_Single", "Delete folder '{0}'?"), SelectedFolders[0].GetDisplayName());
		}
		else
		{
			Prompt = FText::Format(LOCTEXT("FolderDeleteConfirm_Multiple", "Delete {0} folders?"), SelectedFolders.Num());
		}

		// Spawn a confirmation dialog since this is potentially a highly destructive operation
		ContentBrowserUtils::DisplayConfirmationPopup(
			Prompt,
			LOCTEXT("FolderDeleteConfirm_Yes", "Delete"),
			LOCTEXT("FolderDeleteConfirm_No", "Cancel"),
			AssetView.Pin().ToSharedRef(),
			FOnClicked::CreateSP(this, &FAssetContextMenu::ExecuteDeleteFolderConfirmed)
		);
	}
}

FReply FAssetContextMenu::ExecuteDeleteFolderConfirmed()
{
	// Batch these by their data sources
	TMap<UContentBrowserDataSource*, TArray<FContentBrowserItemData>> SourcesAndItems;
	for (const FContentBrowserItem& SelectedItem : SelectedFolders)
	{
		FContentBrowserItem::FItemDataArrayView ItemDataArray = SelectedItem.GetInternalItems();
		for (const FContentBrowserItemData& ItemData : ItemDataArray)
		{
			if (UContentBrowserDataSource* ItemDataSource = ItemData.GetOwnerDataSource())
			{
				FText DeleteErrorMsg;
				if (ItemDataSource->CanDeleteItem(ItemData, &DeleteErrorMsg))
				{
					TArray<FContentBrowserItemData>& ItemsForSource = SourcesAndItems.FindOrAdd(ItemDataSource);
					ItemsForSource.Add(ItemData);
				}
				else
				{
					AssetViewUtils::ShowErrorNotifcation(DeleteErrorMsg);
				}
			}
		}
	}

	// Execute the operation now
	for (const auto& SourceAndItemsPair : SourcesAndItems)
	{
		SourceAndItemsPair.Key->BulkDeleteItems(SourceAndItemsPair.Value);
	}

	return FReply::Handled();
}

void FAssetContextMenu::ExecutePublicAssetToggle()
{
	// Toggle selected files public asset flag
	if (ensure(SelectedFiles.Num() == 1))
	{
		FAssetData ItemAssetData;
		if (SelectedFiles[0].Legacy_TryGetAssetData(ItemAssetData))
		{
			UPackage* ItemAssetPackage = ItemAssetData.GetPackage();

			if (!ItemAssetPackage)
			{
				return;
			}

			if (ItemAssetPackage->IsExternallyReferenceable())
			{
				ExecuteBulkUnsetPublicAsset();
				return;
			}
			else
			{
				ItemAssetPackage->SetIsExternallyReferenceable(true);
			}

			ItemAssetPackage->Modify();

			OnAssetViewRefreshRequested.ExecuteIfBound();
		}
	}
}

bool FAssetContextMenu::CanExecutePublicAssetToggle()
{
	return bCanExecutePublicAssetToggle;
}

void FAssetContextMenu::ExecuteBulkSetPublicAsset()
{
	for (const FContentBrowserItem& SelectedItem : SelectedFiles)
	{
		FAssetData ItemAssetData;
		if (SelectedItem.Legacy_TryGetAssetData(ItemAssetData))
		{
			UPackage* ItemAssetPackage = ItemAssetData.GetPackage();

			if (!ItemAssetPackage)
			{
				continue;
			}

			if (!ItemAssetPackage->IsExternallyReferenceable())
			{
				ItemAssetPackage->SetIsExternallyReferenceable(true);

				ItemAssetPackage->Modify();
			}
		}
	}

	OnAssetViewRefreshRequested.ExecuteIfBound();
}

void FAssetContextMenu::ExecuteBulkUnsetPublicAsset()
{
	// Batch these by their data sources
	TMap<UContentBrowserDataSource*, TArray<FContentBrowserItemData>> SourcesAndItems;
	for (const FContentBrowserItem& SelectedItem : SelectedFiles)
	{
		FContentBrowserItem::FItemDataArrayView ItemDataArray = SelectedItem.GetInternalItems();
		for (const FContentBrowserItemData& ItemData : ItemDataArray)
		{
			if (UContentBrowserDataSource* ItemDataSource = ItemData.GetOwnerDataSource())
			{
				FText PrivateErrorMsg;
				if (ItemDataSource->CanPrivatizeItem(ItemData, &PrivateErrorMsg))
				{
					TArray<FContentBrowserItemData>& ItemsForSource = SourcesAndItems.FindOrAdd(ItemDataSource);
					ItemsForSource.Add(ItemData);
				}
				else
				{
					AssetViewUtils::ShowErrorNotifcation(PrivateErrorMsg);
				}
			}
		}
	}

	for (const auto& SourceAndItemsPair : SourcesAndItems)
	{
		SourceAndItemsPair.Key->BulkPrivatizeItems(SourceAndItemsPair.Value);
	}

	OnAssetViewRefreshRequested.ExecuteIfBound();
}

bool FAssetContextMenu::CanExecuteBulkSetPublicAsset()
{
	return bCanExecuteBulkSetPublicAsset;
}

bool FAssetContextMenu::IsSelectedAssetPublic()
{
	if (ensure(SelectedFiles.Num() == 1))
	{
		FAssetData ItemAssetData;
		if (SelectedFiles[0].Legacy_TryGetAssetData(ItemAssetData))
		{
			return !(ItemAssetData.PackageFlags & PKG_NotExternallyReferenceable);
		}
	}
	return true;
}

bool FAssetContextMenu::IsSelectedAssetPrivate()
{
	return !IsSelectedAssetPublic();
}

void FAssetContextMenu::ExecuteCopyReference()
{
	if (SelectedFiles.Num() > 0)
	{
		ContentBrowserUtils::CopyItemReferencesToClipboard(SelectedFiles);
	}
}

void FAssetContextMenu::ExecuteCopyFilePath()
{
	if (SelectedFiles.Num() > 0)
	{
		ContentBrowserUtils::CopyFilePathsToClipboard(SelectedFiles);
	}
}

void FAssetContextMenu::ExecuteRemoveFromCollection()
{
	if ( ensure(SourcesData.Collections.Num() == 1) )
	{
		TArray<FSoftObjectPath> SelectedItemCollectionIds;
		for (const FContentBrowserItem& SelectedItem : SelectedFiles)
		{
			FSoftObjectPath ItemCollectionId;
			if (SelectedItem.TryGetCollectionId(ItemCollectionId))
			{
				SelectedItemCollectionIds.Add(ItemCollectionId);
			}
		}

		if ( SelectedItemCollectionIds.Num() > 0 )
		{
			FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

			const FCollectionNameType& Collection = SourcesData.Collections[0];
			const double BeginTimeSec = FPlatformTime::Seconds();
			CollectionManagerModule.Get().RemoveFromCollection(Collection.Name, Collection.Type, SelectedItemCollectionIds);
			const double DurationSec = FPlatformTime::Seconds() - BeginTimeSec;
			OnAssetViewRefreshRequested.ExecuteIfBound();

			FAssetRemovedFromCollectionTelemetryEvent AssetRemoved;
			AssetRemoved.DurationSec = DurationSec;
			AssetRemoved.NumRemoved = SelectedItemCollectionIds.Num();
			AssetRemoved.CollectionShareType = Collection.Type;
			AssetRemoved.Workflow = ECollectionTelemetryAssetRemovedWorkflow::ContextMenu;
			FTelemetryRouter::Get().ProvideTelemetry(AssetRemoved);
		}
	}
}

bool FAssetContextMenu::CanExecuteSyncToAssetTree() const
{
	return SelectedFiles.Num() > 0;
}

bool FAssetContextMenu::CanExecuteFindInExplorer() const
{
	return bCanExecuteFindInExplorer;
}

bool FAssetContextMenu::CanExecuteRemoveFromCollection() const 
{
	return SourcesData.Collections.Num() == 1 && !SourcesData.IsDynamicCollection();
}

bool FAssetContextMenu::CanExecuteDuplicate() const
{
	bool bCanDuplicate = false;
	for (const FContentBrowserItem& SelectedItem : SelectedFiles)
	{
		bCanDuplicate |= SelectedItem.CanDuplicate();
	}
	return bCanDuplicate;
}

bool FAssetContextMenu::CanExecuteRename() const
{
	return ContentBrowserUtils::CanRenameFromAssetView(AssetView);
}

bool FAssetContextMenu::CanExecuteDelete() const
{
	return ContentBrowserUtils::CanDeleteFromAssetView(AssetView);
}

FText FAssetContextMenu::GetDeleteToolTip() const
{
	FText ErrorMessage;
	if (!ContentBrowserUtils::CanDeleteFromAssetView(AssetView, &ErrorMessage) && !ErrorMessage.IsEmpty())
	{
		return ErrorMessage;
	}

	return LOCTEXT("DeleteTooltip", "Delete the selected items.");
}

bool FAssetContextMenu::CanExecuteSaveAsset() const
{
	bool bCanSave = false;
	for (const FContentBrowserItem& SelectedItem : SelectedFiles)
	{
		bCanSave |= SelectedItem.CanSave(EContentBrowserItemSaveFlags::None);
	}
	return bCanSave;
}

void FAssetContextMenu::CacheCanExecuteVars()
{
	bCanExecuteFindInExplorer = false;
	bCanExecutePublicAssetToggle = false;
	bCanExecuteBulkSetPublicAsset = true;

	// Selection must contain at least one file that has exists on disk
	for (const FContentBrowserItem& SelectedItem : SelectedFiles)
	{
		FString ItemFilename;
		if (!bCanExecuteFindInExplorer && SelectedItem.GetItemPhysicalPath(ItemFilename) && FPaths::FileExists(ItemFilename))
		{
			bCanExecuteFindInExplorer = true;
		}
		
		bool bCanChangeAssetPublicState = SelectedItem.CanEdit();
		if (bCanChangeAssetPublicState)
		{
			FNameBuilder ItemVirtualPath(SelectedItem.GetVirtualPath());
			bCanChangeAssetPublicState = FContentBrowserSingleton::Get().IsShowingPrivateContent(FPathViews::GetPath(ItemVirtualPath));
		}
		if (bCanChangeAssetPublicState)
		{
			const FNameBuilder ItemInternalPath(SelectedItem.GetInternalPath());
			const FStringView AssetPath(ItemInternalPath);

			if (!IAssetTools::Get().CanAssetBePublic(AssetPath))
			{
				const FAssetData AssetData = IAssetRegistry::GetChecked().GetAssetByObjectPath(FSoftObjectPath(ItemInternalPath));
				if (!AssetData.IsValid() || AssetData.HasAnyPackageFlags(PKG_NotExternallyReferenceable))
				{
					bCanChangeAssetPublicState = false;
				}
			}
		}

		if (bCanChangeAssetPublicState)
		{
			bCanExecutePublicAssetToggle = true;
		}
		else
		{
			bCanExecuteBulkSetPublicAsset = false;
		}
	}
}

#undef LOCTEXT_NAMESPACE

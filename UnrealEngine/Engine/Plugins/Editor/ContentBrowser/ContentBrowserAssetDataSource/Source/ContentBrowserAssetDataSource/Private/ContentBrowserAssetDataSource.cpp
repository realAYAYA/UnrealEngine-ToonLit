// Copyright Epic Games, Inc. All Rights Reserved.

#include "ContentBrowserAssetDataSource.h"
#include "ContentBrowserAssetDataCore.h"
#include "ContentBrowserDataLegacyBridge.h"
#include "Modules/ModuleManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "CollectionManagerModule.h"
#include "ICollectionManager.h"
#include "AssetViewUtils.h"
#include "ObjectTools.h"
#include "UObject/GCObjectScopeGuard.h"
#include "Factories/Factory.h"
#include "Misc/ScopeExit.h"
#include "Misc/StringBuilder.h"
#include "Misc/ScopedSlowTask.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "FileHelpers.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ToolMenus.h"
#include "NewAssetContextMenu.h"
#include "AssetFolderContextMenu.h"
#include "AssetFileContextMenu.h"
#include "ContentBrowserDataSubsystem.h"
#include "ContentBrowserDataUtils.h"
#include "IContentBrowserDataModule.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "EditorDirectories.h"
#include "ContentBrowserMenuContexts.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ContentBrowserAssetDataSource)

#define LOCTEXT_NAMESPACE "ContentBrowserAssetDataSource"

void UContentBrowserAssetDataSource::Initialize(const bool InAutoRegister)
{
	Super::Initialize(InAutoRegister);

	AssetRegistry = &FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName).Get();
	AssetRegistry->OnFileLoadProgressUpdated().AddUObject(this, &UContentBrowserAssetDataSource::OnAssetRegistryFileLoadProgress);

	{
		static const FName NAME_AssetTools = "AssetTools";
		AssetTools = &FModuleManager::GetModuleChecked<FAssetToolsModule>(NAME_AssetTools).Get();
	}

	CollectionManager = &FCollectionManagerModule::GetModule().Get();

	// Listen for asset registry updates
	AssetRegistry->OnAssetAdded().AddUObject(this, &UContentBrowserAssetDataSource::OnAssetAdded);
	AssetRegistry->OnAssetRemoved().AddUObject(this, &UContentBrowserAssetDataSource::OnAssetRemoved);
	AssetRegistry->OnAssetRenamed().AddUObject(this, &UContentBrowserAssetDataSource::OnAssetRenamed);
	AssetRegistry->OnAssetUpdated().AddUObject(this, &UContentBrowserAssetDataSource::OnAssetUpdated);
	AssetRegistry->OnPathAdded().AddUObject(this, &UContentBrowserAssetDataSource::OnPathAdded);
	AssetRegistry->OnPathRemoved().AddUObject(this, &UContentBrowserAssetDataSource::OnPathRemoved);
	AssetRegistry->OnFilesLoaded().AddUObject(this, &UContentBrowserAssetDataSource::OnScanCompleted);

	// Listen for when assets are loaded or changed
	FCoreUObjectDelegates::OnObjectPropertyChanged.AddUObject(this, &UContentBrowserAssetDataSource::OnObjectPropertyChanged);

	// Listen for new mount roots
	FPackageName::OnContentPathMounted().AddUObject(this, &UContentBrowserAssetDataSource::OnContentPathMounted);
	FPackageName::OnContentPathDismounted().AddUObject(this, &UContentBrowserAssetDataSource::OnContentPathDismounted);

	// Listen for paths being forced visible
	AssetViewUtils::OnAlwaysShowPath().AddUObject(this, &UContentBrowserAssetDataSource::OnAlwaysShowPath);

	// Register our ability to create assets via the legacy Content Browser API
	ContentBrowserDataLegacyBridge::OnCreateNewAsset().BindUObject(this, &UContentBrowserAssetDataSource::OnBeginCreateAsset);

	// Create the asset menu instances
	AssetFolderContextMenu = MakeShared<FAssetFolderContextMenu>();
	AssetFileContextMenu = MakeShared<FAssetFileContextMenu>();

	// Bind the asset specific menu extensions
	{
		if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AddNewContextMenu"))
		{
			Menu->AddDynamicSection(*FString::Printf(TEXT("DynamicSection_DataSource_%s"), *GetName()), FNewToolMenuDelegate::CreateLambda([WeakThis = TWeakObjectPtr<UContentBrowserAssetDataSource>(this)](UToolMenu* InMenu)
			{
				if (UContentBrowserAssetDataSource* This = WeakThis.Get())
				{
					This->PopulateAddNewContextMenu(InMenu);
				}
			}));
		}


		if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.ToolBar"))
		{
			Menu->AddDynamicSection(*FString::Printf(TEXT("DynamicSection_DataSource_%s"), *GetName()), FNewToolMenuDelegate::CreateLambda([WeakThis = TWeakObjectPtr<UContentBrowserAssetDataSource>(this)](UToolMenu* InMenu)
			{
				if (UContentBrowserAssetDataSource* This = WeakThis.Get())
				{
					This->PopulateContentBrowserToolBar(InMenu);
				}
			}));
		}


		if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.FolderContextMenu"))
		{
			Menu->AddDynamicSection(*FString::Printf(TEXT("DynamicSection_DataSource_%s"), *GetName()), FNewToolMenuDelegate::CreateLambda([WeakThis = TWeakObjectPtr<UContentBrowserAssetDataSource>(this)](UToolMenu* InMenu)
			{
				if (UContentBrowserAssetDataSource* This = WeakThis.Get())
				{
					This->PopulateAssetFolderContextMenu(InMenu);
				}
			}));
		}

		if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu"))
		{
			Menu->AddDynamicSection(*FString::Printf(TEXT("DynamicSection_DataSource_%s"), *GetName()), FNewToolMenuDelegate::CreateLambda([WeakThis = TWeakObjectPtr<UContentBrowserAssetDataSource>(this)](UToolMenu* InMenu)
			{
				if (UContentBrowserAssetDataSource* This = WeakThis.Get())
				{
					This->PopulateAssetFileContextMenu(InMenu);
				}
			}));
		}

		if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.DragDropContextMenu"))
		{
			Menu->AddDynamicSection(*FString::Printf(TEXT("DynamicSection_DataSource_%s"), *GetName()), FNewToolMenuDelegate::CreateLambda([WeakThis = TWeakObjectPtr<UContentBrowserAssetDataSource>(this)](UToolMenu* InMenu)
			{
				if (UContentBrowserAssetDataSource* This = WeakThis.Get())
				{
					This->PopulateDragDropContextMenu(InMenu);
				}
			}));
		}
	}

	DiscoveryStatusText = LOCTEXT("InitializingAssetDiscovery", "Initializing Asset Discovery...");

	// Populate the initial set of hidden empty folders
	// This will be updated as the scan finds more content
	AssetRegistry->EnumerateAllCachedPaths([this](FName InPath)
	{
		if (!AssetRegistry->HasAssets(InPath, /*bRecursive*/true))
		{
			EmptyAssetFolders.Add(InPath.ToString());
		}
		return true;
	});

	FPackageName::QueryRootContentPaths(RootContentPaths);

	BuildRootPathVirtualTree();

	// Mount roots are always visible
	for (const FString& RootContentPath : RootContentPaths)
	{
		OnAlwaysShowPath(RootContentPath);
	}
}

void UContentBrowserAssetDataSource::Shutdown()
{
	CollectionManager = nullptr;

	AssetTools = nullptr;

	if (!FModuleManager::Get().IsModuleLoaded(AssetRegistryConstants::ModuleName))
	{
		AssetRegistry = nullptr;
	}

	if (AssetRegistry)
	{
		AssetRegistry->OnFileLoadProgressUpdated().RemoveAll(this);

		AssetRegistry->OnAssetAdded().RemoveAll(this);
		AssetRegistry->OnAssetRemoved().RemoveAll(this);
		AssetRegistry->OnAssetRenamed().RemoveAll(this);
		AssetRegistry->OnAssetUpdated().RemoveAll(this);
		AssetRegistry->OnPathAdded().RemoveAll(this);
		AssetRegistry->OnPathRemoved().RemoveAll(this);
	}

	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);

	AssetViewUtils::OnAlwaysShowPath().RemoveAll(this);

	ContentBrowserDataLegacyBridge::OnCreateNewAsset().Unbind();

	Super::Shutdown();
}

bool UContentBrowserAssetDataSource::PopulateAssetFilterInputParams(FAssetFilterInputParams& Params, UContentBrowserDataSource* DataSource, IAssetRegistry* InAssetRegistry, const FContentBrowserDataFilter& InFilter, FContentBrowserDataCompiledFilter& OutCompiledFilter, ICollectionManager* CollectionManager)
{
	Params.CollectionManager = CollectionManager;

	Params.bIncludeFolders = EnumHasAnyFlags(InFilter.ItemTypeFilter, EContentBrowserItemTypeFilter::IncludeFolders);
	Params.bIncludeFiles = EnumHasAnyFlags(InFilter.ItemTypeFilter, EContentBrowserItemTypeFilter::IncludeFiles);
	Params.bIncludeAssets = EnumHasAnyFlags(InFilter.ItemCategoryFilter, EContentBrowserItemCategoryFilter::IncludeAssets);

	// If we aren't including anything, then we can just bail now
	if (!Params.bIncludeAssets || (!Params.bIncludeFolders && !Params.bIncludeFiles))
	{
		return false;
	}

	Params.CollectionFilter = InFilter.ExtraFilters.FindFilter<FContentBrowserDataCollectionFilter>();
	// If no CollectionManager is given, but there is a collection filter, there will be no results
	if (!CollectionManager && Params.CollectionFilter)
	{
		return false;
	}

	Params.ObjectFilter = InFilter.ExtraFilters.FindFilter<FContentBrowserDataObjectFilter>();
	Params.PackageFilter = InFilter.ExtraFilters.FindFilter<FContentBrowserDataPackageFilter>();
	Params.ClassFilter = InFilter.ExtraFilters.FindFilter<FContentBrowserDataClassFilter>();

	Params.PathPermissionList = Params.PackageFilter && Params.PackageFilter->PathPermissionList && Params.PackageFilter->PathPermissionList->HasFiltering() ? Params.PackageFilter->PathPermissionList.Get() : nullptr;
	Params.ClassPermissionList = Params.ClassFilter && Params.ClassFilter->ClassPermissionList && Params.ClassFilter->ClassPermissionList->HasFiltering() ? Params.ClassFilter->ClassPermissionList.Get() : nullptr;

	// If we are filtering all paths, then we can bail now as we won't return any content
	if (Params.PathPermissionList && Params.PathPermissionList->IsDenyListAll())
	{
		return false;
	}

	Params.DataSource = DataSource;
	Params.AssetRegistry = InAssetRegistry;
	Params.FilterList = &OutCompiledFilter.CompiledFilters.FindOrAdd(DataSource);
	Params.AssetDataFilter = &Params.FilterList->FindOrAddFilter<FContentBrowserCompiledAssetDataFilter>();
	Params.AssetDataFilter->bFilterExcludesAllAssets = true;
	Params.AssetDataFilter->ItemAttributeFilter = InFilter.ItemAttributeFilter;
	Params.InternalPaths.Reset();

	return true;
}

bool UContentBrowserAssetDataSource::CreatePathFilter(FAssetFilterInputParams& Params, const FName InPath, const FContentBrowserDataFilter& InFilter, FContentBrowserDataCompiledFilter& OutCompiledFilter, FSubPathEnumerationFunc SubPathEnumeration)
{
	Params.AssetDataFilter->bFilterExcludesAllAssets = true;
	Params.AssetDataFilter->ItemAttributeFilter = InFilter.ItemAttributeFilter;

	FName ConvertedPath;
	const EContentBrowserPathType ConvertedPathType = Params.DataSource->TryConvertVirtualPath(InPath, ConvertedPath);

	if (ConvertedPathType == EContentBrowserPathType::Internal)
	{
		Params.InternalPaths.Add(ConvertedPath);
	}
	else if (ConvertedPathType != EContentBrowserPathType::Virtual)
	{
		return false;
	}

	if (Params.bIncludeFolders)
	{
		// If we're including folders, but not doing a recursive search then we need to handle that here as the asset code below can't deal with that correctly
		// We also go through this path if we're not including files, as then we don't run the asset code below
		if (!InFilter.bRecursivePaths || !Params.bIncludeFiles)
		{
			// Build the basic paths permissions from the given data
			if (Params.PackageFilter)
			{
				Params.AssetDataFilter->bRecursivePackagePathsToInclude = Params.PackageFilter->bRecursivePackagePathsToInclude;
				for (const FName PackagePathToInclude : Params.PackageFilter->PackagePathsToInclude)
				{
					Params.AssetDataFilter->PackagePathsToInclude.AddAllowListItem(NAME_None, PackagePathToInclude);
				}

				Params.AssetDataFilter->bRecursivePackagePathsToExclude = Params.PackageFilter->bRecursivePackagePathsToExclude;
				for (const FName PackagePathToExclude : Params.PackageFilter->PackagePathsToExclude)
				{
					Params.AssetDataFilter->PackagePathsToExclude.AddDenyListItem(NAME_None, PackagePathToExclude);
				}
			}
			if (Params.PathPermissionList)
			{
				Params.AssetDataFilter->PathPermissionList = *Params.PathPermissionList;
			}
		}

		// Recursive caching of folders is at least as slow as running the query on-demand
		// and significantly slower when only querying the status of a few updated items
		// To this end, we only attempt to pre-cache non-recursive queries
		if (InFilter.bRecursivePaths)
		{
			Params.AssetDataFilter->bRunFolderQueryOnDemand = true;
			Params.AssetDataFilter->VirtualPathToScanOnDemand = InPath.ToString();
		}
		else
		{
			if (ConvertedPathType == EContentBrowserPathType::Internal)
			{
				SubPathEnumeration(ConvertedPath, [&Params](FName SubPath)
				{
					if (UContentBrowserAssetDataSource::PathPassesCompiledDataFilter(*Params.AssetDataFilter, SubPath))
					{
						Params.AssetDataFilter->CachedSubPaths.Add(SubPath);
					}

					return true;
				}, false);
			}
			else if (ConvertedPathType == EContentBrowserPathType::Virtual)
			{
				FContentBrowserCompiledVirtualFolderFilter* VirtualFolderFilter = nullptr;
				Params.DataSource->GetRootPathVirtualTree().EnumerateSubPaths(InPath, [&Params, &VirtualFolderFilter](FName VirtualSubPath, FName InternalSubPath)
				{
					if (!InternalSubPath.IsNone())
					{
						if (UContentBrowserAssetDataSource::PathPassesCompiledDataFilter(*Params.AssetDataFilter, InternalSubPath))
						{
							Params.AssetDataFilter->CachedSubPaths.Add(InternalSubPath);
						}
					}
					else
					{
						// Determine if any internal path under VirtualSubPath passes
						bool bPassesFilter = false;
						Params.DataSource->GetRootPathVirtualTree().EnumerateSubPaths(VirtualSubPath, [&Params, &VirtualFolderFilter, &bPassesFilter](FName RecursiveVirtualSubPath, FName RecursiveInternalSubPath)
						{
							bPassesFilter = bPassesFilter || (!RecursiveInternalSubPath.IsNone() && UContentBrowserAssetDataSource::PathPassesCompiledDataFilter(*Params.AssetDataFilter, RecursiveInternalSubPath));
							return bPassesFilter == false;
						}, true);

						if (bPassesFilter)
						{
							if (!VirtualFolderFilter)
							{
								VirtualFolderFilter = &Params.FilterList->FindOrAddFilter<FContentBrowserCompiledVirtualFolderFilter>();
							}

							if (!VirtualFolderFilter->CachedSubPaths.Contains(VirtualSubPath))
							{
								VirtualFolderFilter->CachedSubPaths.Add(VirtualSubPath, Params.DataSource->CreateVirtualFolderItem(VirtualSubPath));
							}
						}
					}

					return true;
				}, false);
			}
		}
	}
	else if (Params.bIncludeFiles)
	{
		if (InFilter.bRecursivePaths)
		{
			if (ConvertedPathType == EContentBrowserPathType::Internal)
			{
				// Nothing more to do, Params.InternalPaths already contains ConvertedPath
			}
			else if (ConvertedPathType == EContentBrowserPathType::Virtual)
			{
				// Include all internal mounts that pass recursively
				Params.DataSource->GetRootPathVirtualTree().EnumerateSubPaths(InPath, [&Params](FName VirtualSubPath, FName InternalSubPath)
				{
					if (!InternalSubPath.IsNone() && UContentBrowserAssetDataSource::PathPassesCompiledDataFilter(*Params.AssetDataFilter, InternalSubPath))
					{
						Params.InternalPaths.Add(InternalSubPath);
					}
					return true;
				}, true);

				if (Params.InternalPaths.Num() == 0)
				{
					// No internal folders found in the hierarchy of virtual path that passed, there will be no files
					return false;
				}
			}
		}
		else
		{
			if (ConvertedPathType == EContentBrowserPathType::Internal)
			{
				// Nothing more to do, Params.InternalPaths already contains ConvertedPath
			}
			else if (ConvertedPathType == EContentBrowserPathType::Virtual)
			{
				// There are no files directly contained by a dynamically generated fully virtual folder
				return false;
			}
		}
	}

	return true;
}

bool UContentBrowserAssetDataSource::CreateAssetFilter(FAssetFilterInputParams& Params, FName InPath, const FContentBrowserDataFilter& InFilter, FContentBrowserDataCompiledFilter& OutCompiledFilter, FCompileARFilterFunc CreateCompiledFilter)
{
	// If we're not including files, then we can bail now as the rest of this function deals with assets
	if (!Params.bIncludeFiles)
	{
		return false;
	}

	// If we are filtering all classes, then we can bail now as we won't return any content
	if (Params.ClassPermissionList && Params.ClassPermissionList->IsDenyListAll())
	{
		return false;
	}

	// If we are filtering out this path, then we can bail now as it won't return any content
	if (Params.PathPermissionList && !InFilter.bRecursivePaths)
	{
		for (auto It = Params.InternalPaths.CreateIterator(); It; ++It)
		{
			if (!Params.PathPermissionList->PassesStartsWithFilter(*It))
			{
				It.RemoveCurrent();
			}
		}

		if (Params.InternalPaths.Num() == 0)
		{
			return false;
		}
	}

	// Build inclusive asset filter
	FARCompiledFilter CompiledInclusiveFilter;
	{
		// Build the basic inclusive filter from the given data
		{
			FARFilter InclusiveFilter;
			if (Params.ObjectFilter)
			{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
				InclusiveFilter.ObjectPaths.Append(Params.ObjectFilter->ObjectNamesToInclude);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
				InclusiveFilter.TagsAndValues.Append(Params.ObjectFilter->TagsAndValuesToInclude);
				InclusiveFilter.bIncludeOnlyOnDiskAssets |= Params.ObjectFilter->bOnDiskObjectsOnly;
			}
			if (Params.PackageFilter)
			{
				InclusiveFilter.PackageNames.Append(Params.PackageFilter->PackageNamesToInclude);
				InclusiveFilter.PackagePaths.Append(Params.PackageFilter->PackagePathsToInclude);
				InclusiveFilter.bRecursivePaths |= Params.PackageFilter->bRecursivePackagePathsToInclude;
			}
			if (Params.ClassFilter)
			{
				InclusiveFilter.ClassPaths.Append(Params.ClassFilter->ClassNamesToInclude);
				InclusiveFilter.bRecursiveClasses |= Params.ClassFilter->bRecursiveClassNamesToInclude;
			}
			if (Params.CollectionFilter)
			{
				TArray<FSoftObjectPath> ObjectPathsForCollections;
				if (GetObjectPathsForCollections(Params.CollectionManager, Params.CollectionFilter->SelectedCollections, Params.CollectionFilter->bIncludeChildCollections, ObjectPathsForCollections) && ObjectPathsForCollections.Num() == 0)
				{
					// If we had collections but they contained no objects then we can bail as nothing will pass the filter
					return false;
				}
				InclusiveFilter.SoftObjectPaths.Append(MoveTemp(ObjectPathsForCollections));
			}

#if DO_ENSURE
			// Ensure paths do not have trailing slash	
			static const FName RootPath = "/";

			for (const FName ItPath : Params.InternalPaths)
			{
				ensure(ItPath == RootPath || !FStringView(FNameBuilder(ItPath)).EndsWith(TEXT('/')));
			}

			for (const FName ItPath : InclusiveFilter.PackagePaths)
			{
				ensure(ItPath == RootPath || !FStringView(FNameBuilder(ItPath)).EndsWith(TEXT('/')));
			}
#endif // DO_ENSURE

			CreateCompiledFilter(InclusiveFilter, CompiledInclusiveFilter);
		}

		// Remove any inclusive paths that aren't under the set of internal paths that we want to enumerate
		{
			FARCompiledFilter CompiledInternalPathFilter;
			{
				FARFilter InternalPathFilter;
				for (const FName InternalPath : Params.InternalPaths)
				{
					InternalPathFilter.PackagePaths.Add(InternalPath);
				}
				InternalPathFilter.bRecursivePaths = InFilter.bRecursivePaths;
				CreateCompiledFilter(InternalPathFilter, CompiledInternalPathFilter);

				// Remove paths that do not pass item attribute filter (Engine, Plugins, Developer, Localized, __ExternalActors__ etc..)
				for (auto It = CompiledInternalPathFilter.PackagePaths.CreateIterator(); It; ++It)
				{
					FNameBuilder PathStr(*It);
					FStringView Path(PathStr);
					if (!ContentBrowserDataUtils::PathPassesAttributeFilter(Path, 0, InFilter.ItemAttributeFilter))
					{
						It.RemoveCurrent();
					}
				}
			}

			if (CompiledInclusiveFilter.PackagePaths.Num() > 0)
			{
				// Explicit paths given - remove anything not in the internal paths set
				// If the paths resolve as empty then the combined filter will return nothing and can be skipped
				CompiledInclusiveFilter.PackagePaths = CompiledInclusiveFilter.PackagePaths.Intersect(CompiledInternalPathFilter.PackagePaths);
				if (CompiledInclusiveFilter.PackagePaths.Num() == 0)
				{
					return false;
				}
			}
			else
			{
				// No explicit paths given - just use the internal paths set
				CompiledInclusiveFilter.PackagePaths = MoveTemp(CompiledInternalPathFilter.PackagePaths);
			}
		}

		// Remove any inclusive paths that aren't in the explicit AllowList set
		if (Params.PathPermissionList && Params.PathPermissionList->GetAllowList().Num() > 0)
		{
			FARCompiledFilter CompiledPathFilterAllowList;
			{
				FARFilter AllowListPathFilter;
				for (const auto& AllowListPair : Params.PathPermissionList->GetAllowList())
				{
					AllowListPathFilter.PackagePaths.Add(*AllowListPair.Key);
				}
				AllowListPathFilter.bRecursivePaths = true;
				CreateCompiledFilter(AllowListPathFilter, CompiledPathFilterAllowList);
			}

			if (CompiledInclusiveFilter.PackagePaths.Num() > 0)
			{
				// Explicit paths given - remove anything not in the allow list paths set
				// If the paths resolve as empty then the combined filter will return nothing and can be skipped
				CompiledInclusiveFilter.PackagePaths = CompiledInclusiveFilter.PackagePaths.Intersect(CompiledPathFilterAllowList.PackagePaths);
				if (CompiledInclusiveFilter.PackagePaths.Num() == 0)
				{
					return false;
				}
			}
			else
			{
				// No explicit paths given - just use the allow list paths set
				CompiledInclusiveFilter.PackagePaths = MoveTemp(CompiledPathFilterAllowList.PackagePaths);
			}
		}

		// Remove any inclusive classes that aren't in the explicit allow list set
		if (Params.ClassPermissionList && Params.ClassPermissionList->GetAllowList().Num() > 0)
		{
			FARCompiledFilter CompiledClassFilterAllowList;
			{
				FARFilter AllowListClassFilter;
				for (const auto& AllowListPair : Params.ClassPermissionList->GetAllowList())
				{
					AllowListClassFilter.ClassPaths.Add(FTopLevelAssetPath(AllowListPair.Key));
				}
				AllowListClassFilter.bRecursiveClasses = true;
				Params.AssetRegistry->CompileFilter(AllowListClassFilter, CompiledClassFilterAllowList);
			}

			if (CompiledInclusiveFilter.ClassPaths.Num() > 0)
			{
				// Explicit classes given - remove anything not in the allow list class set
				// If the classes resolve as empty then the combined filter will return nothing and can be skipped
				CompiledInclusiveFilter.ClassPaths = CompiledInclusiveFilter.ClassPaths.Intersect(CompiledClassFilterAllowList.ClassPaths);
				if (CompiledInclusiveFilter.ClassPaths.Num() == 0)
				{
					return false;
				}
			}
			else
			{
				// No explicit classes given - just use the allow list class set
				CompiledInclusiveFilter.ClassPaths = MoveTemp(CompiledClassFilterAllowList.ClassPaths);
			}
		}
	}

	// Build exclusive asset filter
	FARCompiledFilter CompiledExclusiveFilter;
	{
		// Build the basic exclusive filter from the given data
		{
			FARFilter ExclusiveFilter;
			if (Params.ObjectFilter)
			{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
				ExclusiveFilter.ObjectPaths.Append(Params.ObjectFilter->ObjectNamesToExclude);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
				ExclusiveFilter.TagsAndValues.Append(Params.ObjectFilter->TagsAndValuesToExclude);
				ExclusiveFilter.bIncludeOnlyOnDiskAssets |= Params.ObjectFilter->bOnDiskObjectsOnly;
			}
			if (Params.PackageFilter)
			{
				ExclusiveFilter.PackageNames.Append(Params.PackageFilter->PackageNamesToExclude);
				ExclusiveFilter.PackagePaths.Append(Params.PackageFilter->PackagePathsToExclude);
				ExclusiveFilter.bRecursivePaths |= Params.PackageFilter->bRecursivePackagePathsToExclude;
			}
			if (Params.ClassFilter)
			{
				ExclusiveFilter.ClassPaths.Append(Params.ClassFilter->ClassNamesToExclude);
				ExclusiveFilter.bRecursiveClasses |= Params.ClassFilter->bRecursiveClassNamesToExclude;
			}
			CreateCompiledFilter(ExclusiveFilter, CompiledExclusiveFilter);
		}

		// Add any exclusive paths that are in the explicit DenyList set
		if (Params.PathPermissionList && Params.PathPermissionList->GetDenyList().Num() > 0)
		{
			FARCompiledFilter CompiledClassFilter;
			{
				FARFilter ClassFilter;
				for (const auto& FilterPair : Params.PathPermissionList->GetDenyList())
				{
					ClassFilter.PackagePaths.Add(*FilterPair.Key);
				}
				ClassFilter.bRecursivePaths = true;
				CreateCompiledFilter(ClassFilter, CompiledClassFilter);
			}

			CompiledExclusiveFilter.PackagePaths.Append(CompiledClassFilter.PackagePaths);
		}

		// Add any exclusive classes that are in the explicit DenyList set
		if (Params.ClassPermissionList && Params.ClassPermissionList->GetDenyList().Num() > 0)
		{
			FARCompiledFilter CompiledClassFilter;
			{
				FARFilter ClassFilter;
				for (const auto& FilterPair : Params.ClassPermissionList->GetDenyList())
				{
					ClassFilter.ClassPaths.Add(FTopLevelAssetPath(FilterPair.Key));
				}
				ClassFilter.bRecursiveClasses = true;
				Params.AssetRegistry->CompileFilter(ClassFilter, CompiledClassFilter);
			}

			CompiledExclusiveFilter.ClassPaths.Append(CompiledClassFilter.ClassPaths);
		}
	}

	// Apply our exclusive filter to the inclusive one to resolve cases where the exclusive filter cancels out the inclusive filter
	// If any filter components resolve as empty then the combined filter will return nothing and can be skipped
	{
		if (CompiledInclusiveFilter.PackageNames.Num() > 0 && CompiledExclusiveFilter.PackageNames.Num() > 0)
		{
			CompiledInclusiveFilter.PackageNames = CompiledInclusiveFilter.PackageNames.Difference(CompiledExclusiveFilter.PackageNames);
			if (CompiledInclusiveFilter.PackageNames.Num() == 0)
			{
				return false;
			}
			CompiledExclusiveFilter.PackageNames.Reset();
		}
		if (CompiledInclusiveFilter.PackagePaths.Num() > 0 && CompiledExclusiveFilter.PackagePaths.Num() > 0)
		{
			CompiledInclusiveFilter.PackagePaths = CompiledInclusiveFilter.PackagePaths.Difference(CompiledExclusiveFilter.PackagePaths);
			if (CompiledInclusiveFilter.PackagePaths.Num() == 0)
			{
				return false;
			}
			CompiledExclusiveFilter.PackagePaths.Reset();
		}
		if (CompiledInclusiveFilter.SoftObjectPaths.Num() > 0 && CompiledExclusiveFilter.SoftObjectPaths.Num() > 0)
		{
			CompiledInclusiveFilter.SoftObjectPaths = CompiledInclusiveFilter.SoftObjectPaths.Difference(CompiledExclusiveFilter.SoftObjectPaths);
			if (CompiledInclusiveFilter.SoftObjectPaths.Num() == 0)
			{
				return false;
			}
			CompiledExclusiveFilter.SoftObjectPaths.Reset();
		}
		if (CompiledInclusiveFilter.ClassPaths.Num() > 0 && CompiledExclusiveFilter.ClassPaths.Num() > 0)
		{
			CompiledInclusiveFilter.ClassPaths = CompiledInclusiveFilter.ClassPaths.Difference(CompiledExclusiveFilter.ClassPaths);
			if (CompiledInclusiveFilter.ClassPaths.Num() == 0)
			{
				return false;
			}
			CompiledExclusiveFilter.ClassPaths.Reset();
		}
	}

	// When InPath is a fully virtual folder such as /All, having no package paths is expected
	if (CompiledInclusiveFilter.PackagePaths.Num() == 0)
	{
		// Leave bFilterExcludesAllAssets set to true
		// Otherwise PackagePaths.Num() == 0 is interpreted as everything passes
		return false;
	}

	// If we are enumerating recursively then the inclusive path list will already be fully filtered so just use that
	if (Params.bIncludeFolders && InFilter.bRecursivePaths)
	{
		Params.AssetDataFilter->CachedSubPaths = CompiledInclusiveFilter.PackagePaths;
		for (const FName InternalPath : Params.InternalPaths)
		{
			Params.AssetDataFilter->CachedSubPaths.Remove(InternalPath); // Remove the root as it's not a sub-path
		}
		Params.AssetDataFilter->CachedSubPaths.Sort(FNameLexicalLess()); // Sort as we enumerate these in parent->child order
	}

	// If we got this far then we have something in the filters and need to run the query
	Params.AssetDataFilter->bFilterExcludesAllAssets = false;
	Params.AssetDataFilter->InclusiveFilter = MoveTemp(CompiledInclusiveFilter);
	Params.AssetDataFilter->ExclusiveFilter = MoveTemp(CompiledExclusiveFilter);

	return true;
}

void UContentBrowserAssetDataSource::CompileFilter(const FName InPath, const FContentBrowserDataFilter& InFilter, FContentBrowserDataCompiledFilter& OutCompiledFilter)
{
	FAssetFilterInputParams Params;
	if (PopulateAssetFilterInputParams(Params, this, AssetRegistry, InFilter, OutCompiledFilter, CollectionManager))
	{
		const bool bCreatedPathFilter = CreatePathFilter(Params, InPath, InFilter, OutCompiledFilter, [this](FName Path, TFunctionRef<bool(FName)> Callback, bool bRecursive)
		{
			AssetRegistry->EnumerateSubPaths(Path, Callback, bRecursive);
		});

		if (bCreatedPathFilter)
		{
			const bool bWasTemporaryCachingModeEnabled = AssetRegistry->GetTemporaryCachingMode();
			AssetRegistry->SetTemporaryCachingMode(true);
			ON_SCOPE_EXIT
			{
				AssetRegistry->SetTemporaryCachingMode(bWasTemporaryCachingModeEnabled);
			};

			const bool bCreatedAssetFilter = CreateAssetFilter(Params, InPath, InFilter, OutCompiledFilter, [this](FARFilter& InputFilter, FARCompiledFilter& OutputFilter)
			{
				AssetRegistry->CompileFilter(InputFilter, OutputFilter);
			});

			if (bCreatedAssetFilter)
			{
				// Resolve any custom assets
				if (const FContentBrowserDataLegacyFilter* LegacyFilter = InFilter.ExtraFilters.FindFilter<FContentBrowserDataLegacyFilter>())
				{
					if (LegacyFilter->OnGetCustomSourceAssets.IsBound())
					{
						FARFilter CustomSourceAssetsFilter;
						CustomSourceAssetsFilter.PackageNames = Params.AssetDataFilter->InclusiveFilter.PackageNames.Array();
						CustomSourceAssetsFilter.PackagePaths = Params.AssetDataFilter->InclusiveFilter.PackagePaths.Array();
PRAGMA_DISABLE_DEPRECATION_WARNINGS
						CustomSourceAssetsFilter.ObjectPaths = Params.AssetDataFilter->InclusiveFilter.ObjectPaths.Array();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
						CustomSourceAssetsFilter.ClassPaths = Params.AssetDataFilter->InclusiveFilter.ClassPaths.Array();
						CustomSourceAssetsFilter.TagsAndValues = Params.AssetDataFilter->InclusiveFilter.TagsAndValues;
						CustomSourceAssetsFilter.bIncludeOnlyOnDiskAssets = Params.AssetDataFilter->InclusiveFilter.bIncludeOnlyOnDiskAssets;

						LegacyFilter->OnGetCustomSourceAssets.Execute(CustomSourceAssetsFilter, Params.AssetDataFilter->CustomSourceAssets);
					}
				}
			}
		}
	}
}

void UContentBrowserAssetDataSource::EnumerateFoldersMatchingFilter(UContentBrowserDataSource* DataSource, const FContentBrowserCompiledAssetDataFilter* AssetDataFilter, TFunctionRef<bool(FContentBrowserItemData&&)> InCallback, FSubPathEnumerationFunc SubPathEnumeration, FCreateFolderItemFunc CreateFolderItem)
{
	if (AssetDataFilter->bRunFolderQueryOnDemand)
	{
		auto HandleInternalPath = [&DataSource, &InCallback, &AssetDataFilter, &SubPathEnumeration, &CreateFolderItem](const FName InInternalPath)
		{
			TArray<FName, TInlineAllocator<16>> PathsToScan;
			PathsToScan.Add(InInternalPath);
			while (PathsToScan.Num() > 0)
			{
				const FName PathToScan = PathsToScan.Pop(/*bAllowShrinking*/false);
				SubPathEnumeration(PathToScan, [&DataSource, &InCallback, &AssetDataFilter, &PathsToScan, &CreateFolderItem](FName SubPath)
				{
					if (UContentBrowserAssetDataSource::PathPassesCompiledDataFilter(*AssetDataFilter, SubPath))
					{
						if (!InCallback(CreateFolderItem(SubPath)))
						{
							return false;
						}

						PathsToScan.Add(SubPath);
					}
					return true;
				}, false);
			}
		};

		const FName StartingVirtualPath = *AssetDataFilter->VirtualPathToScanOnDemand;
		bool bStartingPathIsFullyVirtual = false;
		DataSource->GetRootPathVirtualTree().PathExists(StartingVirtualPath, bStartingPathIsFullyVirtual);

		if (bStartingPathIsFullyVirtual)
		{
			// Virtual paths not supported by PathPassesCompiledDataFilter, enumerate internal paths in hierarchy and propagate results to virtual parents
			TSet<FName> VirtualPathsPassedFilter;
			VirtualPathsPassedFilter.Reserve(DataSource->GetRootPathVirtualTree().NumPaths());
			DataSource->GetRootPathVirtualTree().EnumerateSubPaths(StartingVirtualPath, [&DataSource, &AssetDataFilter, &VirtualPathsPassedFilter](FName VirtualSubPath, FName InternalPath)
			{
				if (!InternalPath.IsNone() && UContentBrowserAssetDataSource::PathPassesCompiledDataFilter(*AssetDataFilter, InternalPath))
				{
					// Propagate result to parents
					for (FName It = VirtualSubPath; !It.IsNone(); It = DataSource->GetRootPathVirtualTree().GetParentPath(It))
					{
						bool bIsAlreadySet = false;
						VirtualPathsPassedFilter.Add(It, &bIsAlreadySet);
						if (bIsAlreadySet)
						{
							break;
						}
					}
				}
				return true;
			}, true);

			// Enumerate virtual path hierarchy again
			TArray<FName, TInlineAllocator<16>> PathsToScan;
			PathsToScan.Add(StartingVirtualPath);
			while (PathsToScan.Num() > 0)
			{
				const FName PathToScan = PathsToScan.Pop(/*bAllowShrinking*/false);
				DataSource->GetRootPathVirtualTree().EnumerateSubPaths(PathToScan, [&DataSource, &InCallback, &AssetDataFilter, &VirtualPathsPassedFilter, &PathsToScan, &HandleInternalPath, &CreateFolderItem](FName VirtualSubPath, FName InternalPath)
				{
					if (VirtualPathsPassedFilter.Contains(VirtualSubPath))
					{
						if (!InternalPath.IsNone())
						{
							if (!InCallback(CreateFolderItem(InternalPath)))
							{
								return false;
							}

							HandleInternalPath(InternalPath);
						}
						else
						{
							if (!InCallback(DataSource->CreateVirtualFolderItem(VirtualSubPath)))
							{
								return false;
							}
						}

						PathsToScan.Add(VirtualSubPath);
					}
					return true;
				}, false);
			}
		}
		else
		{
			FName InternalPath;
			if (DataSource->TryConvertVirtualPathToInternal(StartingVirtualPath, InternalPath))
			{
				HandleInternalPath(InternalPath);
			}
		}
	}
	else
	{
		for (const FName& SubPath : AssetDataFilter->CachedSubPaths)
		{
			if (!InCallback(CreateFolderItem(SubPath)))
			{
				return;
			}
		}
	}
}

void UContentBrowserAssetDataSource::EnumerateItemsMatchingFilter(const FContentBrowserDataCompiledFilter& InFilter, TFunctionRef<bool(FContentBrowserItemData&&)> InCallback)
{
	const FContentBrowserDataFilterList* FilterList = InFilter.CompiledFilters.Find(this);
	if (!FilterList)
	{
		return;
	}
	
	const FContentBrowserCompiledAssetDataFilter* AssetDataFilter = FilterList->FindFilter<FContentBrowserCompiledAssetDataFilter>();
	if (!AssetDataFilter)
	{
		return;
	}

	if (EnumHasAnyFlags(InFilter.ItemTypeFilter, EContentBrowserItemTypeFilter::IncludeFolders))
	{
		auto EnumerateSubPaths = [this](FName Path, TFunctionRef<bool(FName)> Callback, bool bRecursive)
		{
			AssetRegistry->EnumerateSubPaths(Path, Callback, bRecursive);
		};
		auto CreateFolderItem = [this](FName Path) -> FContentBrowserItemData
		{
			return CreateAssetFolderItem(Path);
		};
		EnumerateFoldersMatchingFilter(this, AssetDataFilter, InCallback, EnumerateSubPaths, CreateFolderItem);
	}

	if (EnumHasAnyFlags(InFilter.ItemTypeFilter, EContentBrowserItemTypeFilter::IncludeFiles) && !AssetDataFilter->bFilterExcludesAllAssets)
	{
		for (const FAssetData& CustomSourceAsset : AssetDataFilter->CustomSourceAssets)
		{
			if (!InCallback(CreateAssetFileItem(CustomSourceAsset)))
			{
				return;
			}
		}

		AssetRegistry->EnumerateAssets(AssetDataFilter->InclusiveFilter, [this, &InCallback, &AssetDataFilter](const FAssetData& AssetData)
		{
			if (ContentBrowserAssetData::IsPrimaryAsset(AssetData))
			{
				const bool bPassesExclusiveFilter = AssetDataFilter->ExclusiveFilter.IsEmpty() || !AssetRegistry->IsAssetIncludedByFilter(AssetData, AssetDataFilter->ExclusiveFilter);
				if (bPassesExclusiveFilter)
				{
					return InCallback(CreateAssetFileItem(AssetData));
				}
			}
			return true;
		});
	}
}

void UContentBrowserAssetDataSource::EnumerateItemsAtPath(const FName InPath, const EContentBrowserItemTypeFilter InItemTypeFilter, TFunctionRef<bool(FContentBrowserItemData&&)> InCallback)
{
	FName InternalPath;
	if (!TryConvertVirtualPathToInternal(InPath, InternalPath))
	{
		return;
	}
	
	if (EnumHasAnyFlags(InItemTypeFilter, EContentBrowserItemTypeFilter::IncludeFolders))
	{
		if (AssetRegistry->PathExists(InternalPath))
		{
			InCallback(CreateAssetFolderItem(InternalPath));
		}
	}

	if (EnumHasAnyFlags(InItemTypeFilter, EContentBrowserItemTypeFilter::IncludeFiles))
	{
		FARFilter ARFilter;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		ARFilter.ObjectPaths.Add(InternalPath);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		AssetRegistry->EnumerateAssets(ARFilter, [this, &InCallback](const FAssetData& AssetData)
		{
			if (ContentBrowserAssetData::IsPrimaryAsset(AssetData))
			{
				return InCallback(CreateAssetFileItem(AssetData));
			}
			return true;
		});
	}
}

bool UContentBrowserAssetDataSource::EnumerateItemsAtPaths(const TArrayView<FContentBrowserItemPath> InPaths, const EContentBrowserItemTypeFilter InItemTypeFilter, TFunctionRef<bool(FContentBrowserItemData&&)> InCallback)
{
	if (EnumHasAnyFlags(InItemTypeFilter, EContentBrowserItemTypeFilter::IncludeFolders))
	{
		for (const FContentBrowserItemPath& InPath : InPaths)
		{
			if (InPath.HasInternalPath())
			{
				if (AssetRegistry->PathExists(InPath.GetInternalPathName()))
				{
					if (!InCallback(CreateAssetFolderItem(InPath.GetInternalPathName())))
					{
						return false;
					}
				}
			}
		}
	}

	if (EnumHasAnyFlags(InItemTypeFilter, EContentBrowserItemTypeFilter::IncludeFiles) && !InPaths.IsEmpty())
	{
		FARFilter ARFilter;
		
		// TODO: EnumerateAssets for in memory assets needs optimization, currently enumerates every UObject in memory instead of calling find
		ARFilter.bIncludeOnlyOnDiskAssets = true;

		for (const FContentBrowserItemPath& InPath : InPaths)
		{
			if (InPath.HasInternalPath())
			{
				ARFilter.PackageNames.Add(InPath.GetInternalPathName());
			}
		}

		auto FileFoundCallback = [this, &InCallback](const FAssetData& AssetData)
		{
			if (ContentBrowserAssetData::IsPrimaryAsset(AssetData))
			{
				return InCallback(CreateAssetFileItem(AssetData));
			}
			return true;
		};

		if (!AssetRegistry->EnumerateAssets(ARFilter, FileFoundCallback))
		{
			return false;
		}
	}

	return true;
}

bool UContentBrowserAssetDataSource::IsDiscoveringItems(FText* OutStatus)
{
	if (AssetRegistry->IsLoadingAssets())
	{
		ContentBrowserAssetData::SetOptionalErrorMessage(OutStatus, DiscoveryStatusText);
		return true;
	}

	return false;
}

bool UContentBrowserAssetDataSource::PrioritizeSearchPath(const FName InPath)
{
	FName InternalPath;
	if (!TryConvertVirtualPathToInternal(InPath, InternalPath))
	{
		return false;
	}

	AssetRegistry->PrioritizeSearchPath(InternalPath.ToString() / FString());
	return true;
}

bool UContentBrowserAssetDataSource::IsFolderVisibleIfHidingEmpty(const FName InPath)
{
	FName ConvertedPath;
	const EContentBrowserPathType ConvertedPathType = TryConvertVirtualPath(InPath, ConvertedPath);
	if (ConvertedPathType == EContentBrowserPathType::Internal)
	{
		if (!IsKnownContentPath(ConvertedPath))
		{
			return false;
		}
	}
	else if (ConvertedPathType == EContentBrowserPathType::Virtual)
	{
		return true;
	}
	else
	{
		return false;
	}

	FNameBuilder InternalPathStr(ConvertedPath);

	const FStringView InternalPathStrView = InternalPathStr;
	const uint32 InternalPathHash = GetTypeHash(InternalPathStrView);

	return AlwaysVisibleAssetFolders.ContainsByHash(InternalPathHash, InternalPathStrView) 
		|| !EmptyAssetFolders.ContainsByHash(InternalPathHash, InternalPathStrView);
}

bool UContentBrowserAssetDataSource::CanCreateFolder(const FName InPath, FText* OutErrorMsg)
{
	FName InternalPath;
	if (!TryConvertVirtualPathToInternal(InPath, InternalPath))
	{
		return false;
	}

	if (!IsKnownContentPath(InternalPath))
	{
		return false;
	}

	return ContentBrowserAssetData::CanModifyPath(AssetTools, InternalPath, OutErrorMsg);
}

bool UContentBrowserAssetDataSource::CreateFolder(const FName InPath, FContentBrowserItemDataTemporaryContext& OutPendingItem)
{
	const FString ParentPath = FPackageName::GetLongPackagePath(InPath.ToString());
	FName InternalParentPath;
	if (!TryConvertVirtualPathToInternal(*ParentPath, InternalParentPath))
	{
		return false;
	}

	const FString FolderItemName = FPackageName::GetShortName(InPath);
	FString InternalPathString = InternalParentPath.ToString() + TEXT("/") + FolderItemName;

	FContentBrowserItemData NewItemData(
		this,
		EContentBrowserItemFlags::Type_Folder | EContentBrowserItemFlags::Category_Asset | EContentBrowserItemFlags::Temporary_Creation,
		InPath,
		*FolderItemName,
		FText::AsCultureInvariant(FolderItemName),
		MakeShared<FContentBrowserAssetFolderItemDataPayload>(*InternalPathString)
		);

	OutPendingItem = FContentBrowserItemDataTemporaryContext(
		MoveTemp(NewItemData),
		FContentBrowserItemDataTemporaryContext::FOnValidateItem::CreateUObject(this, &UContentBrowserAssetDataSource::OnValidateItemName),
		FContentBrowserItemDataTemporaryContext::FOnFinalizeItem::CreateUObject(this, &UContentBrowserAssetDataSource::OnFinalizeCreateFolder)
		);

	return true;
}

bool UContentBrowserAssetDataSource::DoesItemPassFolderFilter(UContentBrowserDataSource* DataSource, const FContentBrowserItemData& InItem, const FContentBrowserCompiledAssetDataFilter& Filter)
{
	if (TSharedPtr<const FContentBrowserAssetFolderItemDataPayload> FolderPayload = ContentBrowserAssetData::GetAssetFolderItemPayload(DataSource, InItem))
	{
		if (Filter.bRunFolderQueryOnDemand)
		{
			bool bIsUnderSearchPath = false;
			const FString& PathToScan = Filter.VirtualPathToScanOnDemand;
			if (PathToScan == TEXT("/"))
			{
				bIsUnderSearchPath = true;
			}
			else 
			{
				const FName VirtualPath = InItem.GetVirtualPath();
				FNameBuilder VirtualPathBuilder(VirtualPath);
				const FStringView VirtualPathView(VirtualPathBuilder);
				if (VirtualPathView.StartsWith(PathToScan))
				{
					if ((VirtualPathView.Len() <= PathToScan.Len()) || (VirtualPathView[PathToScan.Len()] == TEXT('/')))
					{
						bIsUnderSearchPath = true;
					}
				}
			}

			const bool bPassesCompiledFilter = bIsUnderSearchPath && UContentBrowserAssetDataSource::PathPassesCompiledDataFilter(Filter, FolderPayload->GetInternalPath());

			return bIsUnderSearchPath && bPassesCompiledFilter;
		}
		else
		{
			return Filter.CachedSubPaths.Contains(FolderPayload->GetInternalPath());
		}
	}
	else
	{
		bool bPasses = false;
		DataSource->GetRootPathVirtualTree().EnumerateSubPaths(InItem.GetVirtualPath(), [&bPasses, &Filter](FName VirtualSubPath, FName InternalPath)
		{
			if (!InternalPath.IsNone())
			{
				if (UContentBrowserAssetDataSource::PathPassesCompiledDataFilter(Filter, InternalPath))
				{
					bPasses = true;

					// Stop enumerate
					return false;
				}
			}
			return true;
		}, true);

		return bPasses;
	}
}

bool UContentBrowserAssetDataSource::DoesItemPassFilter(const FContentBrowserItemData& InItem, const FContentBrowserDataCompiledFilter& InFilter)
{
	const FContentBrowserDataFilterList* FilterList = InFilter.CompiledFilters.Find(this);
	if (!FilterList)
	{
		return false;
	}

	const FContentBrowserCompiledAssetDataFilter* AssetDataFilter = FilterList->FindFilter<FContentBrowserCompiledAssetDataFilter>();
	if (!AssetDataFilter)
	{
		return false;
	}

	switch (InItem.GetItemType())
	{
	case EContentBrowserItemFlags::Type_Folder:
		if (EnumHasAnyFlags(InFilter.ItemTypeFilter, EContentBrowserItemTypeFilter::IncludeFolders))
		{
			return DoesItemPassFolderFilter(this, InItem, *AssetDataFilter);
		}
		break;

	case EContentBrowserItemFlags::Type_File:
		if (EnumHasAnyFlags(InFilter.ItemTypeFilter, EContentBrowserItemTypeFilter::IncludeFiles) && !AssetDataFilter->bFilterExcludesAllAssets)
		{
			if (TSharedPtr<const FContentBrowserAssetFileItemDataPayload> AssetPayload = GetAssetFileItemPayload(InItem))
			{
				// Must pass Inclusive AND !Exclusive, or be a CustomAsset
				return ((AssetDataFilter->InclusiveFilter.IsEmpty() || AssetRegistry->IsAssetIncludedByFilter(AssetPayload->GetAssetData(), AssetDataFilter->InclusiveFilter)) // InclusiveFilter
					&& (AssetDataFilter->ExclusiveFilter.IsEmpty() || !AssetRegistry->IsAssetIncludedByFilter(AssetPayload->GetAssetData(), AssetDataFilter->ExclusiveFilter))) // ExclusiveFilter
					|| AssetDataFilter->CustomSourceAssets.Contains(AssetPayload->GetAssetData()); // CustomAsset
			}
		}
		break;

	default:
		break;
	}
	
	return false;
}

bool UContentBrowserAssetDataSource::GetItemAttribute(const FContentBrowserItemData& InItem, const bool InIncludeMetaData, const FName InAttributeKey, FContentBrowserItemDataAttributeValue& OutAttributeValue)
{
	return ContentBrowserAssetData::GetItemAttribute(this, InItem, InIncludeMetaData, InAttributeKey, OutAttributeValue);
}

bool UContentBrowserAssetDataSource::GetItemAttributes(const FContentBrowserItemData& InItem, const bool InIncludeMetaData, FContentBrowserItemDataAttributeValues& OutAttributeValues)
{
	return ContentBrowserAssetData::GetItemAttributes(this, InItem, InIncludeMetaData, OutAttributeValues);
}

bool UContentBrowserAssetDataSource::GetItemPhysicalPath(const FContentBrowserItemData& InItem, FString& OutDiskPath)
{
	return ContentBrowserAssetData::GetItemPhysicalPath(this, InItem, OutDiskPath);
}

bool UContentBrowserAssetDataSource::IsItemDirty(const FContentBrowserItemData& InItem)
{
	return ContentBrowserAssetData::IsItemDirty(this, InItem);
}

bool UContentBrowserAssetDataSource::CanEditItem(const FContentBrowserItemData& InItem, FText* OutErrorMsg)
{
	return ContentBrowserAssetData::CanEditItem(AssetTools, this, InItem, OutErrorMsg);
}

bool UContentBrowserAssetDataSource::EditItem(const FContentBrowserItemData& InItem)
{
	return ContentBrowserAssetData::EditItems(AssetTools, this, MakeArrayView(&InItem, 1));
}

bool UContentBrowserAssetDataSource::BulkEditItems(TArrayView<const FContentBrowserItemData> InItems)
{
	return ContentBrowserAssetData::EditItems(AssetTools, this, InItems);
}

bool UContentBrowserAssetDataSource::CanPreviewItem(const FContentBrowserItemData& InItem, FText* OutErrorMsg)
{
	return ContentBrowserAssetData::CanPreviewItem(AssetTools, this, InItem, OutErrorMsg);
}

bool UContentBrowserAssetDataSource::PreviewItem(const FContentBrowserItemData& InItem)
{
	return ContentBrowserAssetData::PreviewItems(AssetTools, this, MakeArrayView(&InItem, 1));
}

bool UContentBrowserAssetDataSource::BulkPreviewItems(TArrayView<const FContentBrowserItemData> InItems)
{
	return ContentBrowserAssetData::PreviewItems(AssetTools, this, InItems);
}

bool UContentBrowserAssetDataSource::CanDuplicateItem(const FContentBrowserItemData& InItem, FText* OutErrorMsg)
{
	return ContentBrowserAssetData::CanDuplicateItem(AssetTools, this, InItem, OutErrorMsg);
}

bool UContentBrowserAssetDataSource::DuplicateItem(const FContentBrowserItemData& InItem, FContentBrowserItemDataTemporaryContext& OutPendingItem)
{
	UObject* SourceAsset = nullptr;
	FAssetData NewAssetData;
	if (ContentBrowserAssetData::DuplicateItem(AssetTools, this, InItem, SourceAsset, NewAssetData))
	{
		FName VirtualizedPath;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		TryConvertInternalPathToVirtual(NewAssetData.ObjectPath, VirtualizedPath);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

		FContentBrowserItemData NewItemData(
			this,
			EContentBrowserItemFlags::Type_File | EContentBrowserItemFlags::Category_Asset | EContentBrowserItemFlags::Temporary_Duplication,
			VirtualizedPath,
			NewAssetData.AssetName,
			FText::AsCultureInvariant(NewAssetData.AssetName.ToString()),
			MakeShared<FContentBrowserAssetFileItemDataPayload_Duplication>(MoveTemp(NewAssetData), SourceAsset)
			);

		OutPendingItem = FContentBrowserItemDataTemporaryContext(
			MoveTemp(NewItemData),
			FContentBrowserItemDataTemporaryContext::FOnValidateItem::CreateUObject(this, &UContentBrowserAssetDataSource::OnValidateItemName),
			FContentBrowserItemDataTemporaryContext::FOnFinalizeItem::CreateUObject(this, &UContentBrowserAssetDataSource::OnFinalizeDuplicateAsset)
			);

		return true;
	}

	return false;
}

bool UContentBrowserAssetDataSource::BulkDuplicateItems(TArrayView<const FContentBrowserItemData> InItems, TArray<FContentBrowserItemData>& OutNewItems)
{
	TArray<FAssetData> NewAssets;
	if (ContentBrowserAssetData::DuplicateItems(AssetTools, this, InItems, NewAssets))
	{
		for (const FAssetData& NewAsset : NewAssets)
		{
			OutNewItems.Emplace(CreateAssetFileItem(NewAsset));
		}

		return true;
	}

	return false;
}

bool UContentBrowserAssetDataSource::CanSaveItem(const FContentBrowserItemData& InItem, const EContentBrowserItemSaveFlags InSaveFlags, FText* OutErrorMsg)
{
	return ContentBrowserAssetData::CanSaveItem(AssetTools, this, InItem, InSaveFlags, OutErrorMsg);
}

bool UContentBrowserAssetDataSource::SaveItem(const FContentBrowserItemData& InItem, const EContentBrowserItemSaveFlags InSaveFlags)
{
	return ContentBrowserAssetData::SaveItems(AssetTools, this, MakeArrayView(&InItem, 1), InSaveFlags);
}

bool UContentBrowserAssetDataSource::BulkSaveItems(TArrayView<const FContentBrowserItemData> InItems, const EContentBrowserItemSaveFlags InSaveFlags)
{
	return ContentBrowserAssetData::SaveItems(AssetTools, this, InItems, InSaveFlags);
}

bool UContentBrowserAssetDataSource::CanDeleteItem(const FContentBrowserItemData& InItem, FText* OutErrorMsg)
{
	return ContentBrowserAssetData::CanDeleteItem(AssetTools, AssetRegistry, this, InItem, OutErrorMsg);
}

bool UContentBrowserAssetDataSource::DeleteItem(const FContentBrowserItemData& InItem)
{
	return ContentBrowserAssetData::DeleteItems(AssetTools, AssetRegistry, this, MakeArrayView(&InItem, 1));
}

bool UContentBrowserAssetDataSource::BulkDeleteItems(TArrayView<const FContentBrowserItemData> InItems)
{
	return ContentBrowserAssetData::DeleteItems(AssetTools, AssetRegistry, this, InItems);
}

bool UContentBrowserAssetDataSource::CanPrivatizeItem(const FContentBrowserItemData& InItem, FText* OutErrorMsg)
{
	return ContentBrowserAssetData::CanPrivatizeItem(AssetTools, AssetRegistry, this, InItem, OutErrorMsg);
}

bool UContentBrowserAssetDataSource::PrivatizeItem(const FContentBrowserItemData& InItem)
{
	return ContentBrowserAssetData::PrivatizeItems(AssetTools, AssetRegistry, this, MakeArrayView(&InItem, 1));
}

bool UContentBrowserAssetDataSource::BulkPrivatizeItems(TArrayView<const FContentBrowserItemData> InItems)
{
	return ContentBrowserAssetData::PrivatizeItems(AssetTools, AssetRegistry, this, InItems);
}

bool UContentBrowserAssetDataSource::CanRenameItem(const FContentBrowserItemData& InItem, const FString* InNewName, FText* OutErrorMsg)
{
	return ContentBrowserAssetData::CanRenameItem(AssetTools, this, InItem, InNewName, OutErrorMsg);
}

bool UContentBrowserAssetDataSource::RenameItem(const FContentBrowserItemData& InItem, const FString& InNewName, FContentBrowserItemData& OutNewItem)
{
	if (ContentBrowserAssetData::RenameItem(AssetTools, AssetRegistry, this, InItem, InNewName))
	{
		switch (InItem.GetItemType())
		{
		case EContentBrowserItemFlags::Type_Folder:
			if (TSharedPtr<const FContentBrowserAssetFolderItemDataPayload> FolderPayload = GetAssetFolderItemPayload(InItem))
			{
				const FName NewFolderPath = *(FPaths::GetPath(FolderPayload->GetInternalPath().ToString()) / InNewName);
				OutNewItem = CreateAssetFolderItem(NewFolderPath);
			}
			break;

		case EContentBrowserItemFlags::Type_File:
			if (TSharedPtr<const FContentBrowserAssetFileItemDataPayload> AssetPayload = GetAssetFileItemPayload(InItem))
			{
				// The asset should already be loaded from preforming the rename
				// We can use the renamed object instance to create the new asset data for the renamed item
				if (UObject* Asset = AssetPayload->GetAsset())
				{
					OutNewItem = CreateAssetFileItem(FAssetData(Asset));
				}
			}
			break;

		default:
			break;
		}

		return true;
	}

	return false;
}

bool UContentBrowserAssetDataSource::CanCopyItem(const FContentBrowserItemData& InItem, const FName InDestPath, FText* OutErrorMsg)
{
	// Cannot copy an item outside the paths known to this data source
	FName InternalDestPath;
	if (!TryConvertVirtualPathToInternal(InDestPath, InternalDestPath))
	{
		ContentBrowserAssetData::SetOptionalErrorMessage(OutErrorMsg, FText::Format(LOCTEXT("Error_FolderIsUnknown", "Folder '{0}' is outside the mount roots of this data source"), FText::FromName(InDestPath)));
		return false;
	}

	// The destination path must be a content folder
	if (!IsKnownContentPath(InternalDestPath))
	{
		ContentBrowserAssetData::SetOptionalErrorMessage(OutErrorMsg, FText::Format(LOCTEXT("Error_FolderIsNotContent", "Folder '{0}' is not a known content path"), FText::FromName(InDestPath)));
		return false;
	}

	// The destination path must be writable
	if (!ContentBrowserAssetData::CanModifyPath(AssetTools, InternalDestPath, OutErrorMsg))
	{
		return false;
	}

	return true;
}

bool UContentBrowserAssetDataSource::CopyItem(const FContentBrowserItemData& InItem, const FName InDestPath)
{
	FName InternalDestPath;
	if (!TryConvertVirtualPathToInternal(InDestPath, InternalDestPath))
	{
		return false;
	}

	if (!IsKnownContentPath(InternalDestPath))
	{
		return false;
	}

	return ContentBrowserAssetData::CopyItems(AssetTools, this, MakeArrayView(&InItem, 1), InternalDestPath);
}

bool UContentBrowserAssetDataSource::BulkCopyItems(TArrayView<const FContentBrowserItemData> InItems, const FName InDestPath)
{
	FName InternalDestPath;
	if (!TryConvertVirtualPathToInternal(InDestPath, InternalDestPath))
	{
		return false;
	}

	if (!IsKnownContentPath(InternalDestPath))
	{
		return false;
	}

	return ContentBrowserAssetData::CopyItems(AssetTools, this, InItems, InternalDestPath);
}

bool UContentBrowserAssetDataSource::CanMoveItem(const FContentBrowserItemData& InItem, const FName InDestPath, FText* OutErrorMsg)
{
	// Cannot move an item outside the paths known to this data source
	FName InternalDestPath;
	if (!TryConvertVirtualPathToInternal(InDestPath, InternalDestPath))
	{
		ContentBrowserAssetData::SetOptionalErrorMessage(OutErrorMsg, FText::Format(LOCTEXT("Error_FolderIsUnknown", "Folder '{0}' is outside the mount roots of this data source"), FText::FromName(InDestPath)));
		return false;
	}

	// The destination path must be a content folder
	if (!IsKnownContentPath(InternalDestPath))
	{
		ContentBrowserAssetData::SetOptionalErrorMessage(OutErrorMsg, FText::Format(LOCTEXT("Error_FolderIsNotContent", "Folder '{0}' is not a known content path"), FText::FromName(InDestPath)));
		return false;
	}

	// The destination path must be writable
	if (!ContentBrowserAssetData::CanModifyPath(AssetTools, InternalDestPath, OutErrorMsg))
	{
		return false;
	}

	// Moving has to be able to delete the original item
	if (!ContentBrowserAssetData::CanModifyItem(AssetTools, this, InItem, OutErrorMsg))
	{
		return false;
	}

	return true;
}

bool UContentBrowserAssetDataSource::MoveItem(const FContentBrowserItemData& InItem, const FName InDestPath)
{
	FName InternalDestPath;
	if (!TryConvertVirtualPathToInternal(InDestPath, InternalDestPath))
	{
		return false;
	}

	if (!IsKnownContentPath(InternalDestPath))
	{
		return false;
	}

	return ContentBrowserAssetData::MoveItems(AssetTools, this, MakeArrayView(&InItem, 1), InternalDestPath);
}

bool UContentBrowserAssetDataSource::BulkMoveItems(TArrayView<const FContentBrowserItemData> InItems, const FName InDestPath)
{
	FName InternalDestPath;
	if (!TryConvertVirtualPathToInternal(InDestPath, InternalDestPath))
	{
		return false;
	}

	if (!IsKnownContentPath(InternalDestPath))
	{
		return false;
	}

	return ContentBrowserAssetData::MoveItems(AssetTools, this, InItems, InternalDestPath);
}

bool UContentBrowserAssetDataSource::AppendItemReference(const FContentBrowserItemData& InItem, FString& InOutStr)
{
	return ContentBrowserAssetData::AppendItemReference(AssetRegistry, this, InItem, InOutStr);
}

bool UContentBrowserAssetDataSource::UpdateThumbnail(const FContentBrowserItemData& InItem, FAssetThumbnail& InThumbnail)
{
	return ContentBrowserAssetData::UpdateItemThumbnail(this, InItem, InThumbnail);
}

bool UContentBrowserAssetDataSource::CanHandleDragDropEvent(const FContentBrowserItemData& InItem, const FDragDropEvent& InDragDropEvent) const
{
	if (TSharedPtr<const FContentBrowserAssetFolderItemDataPayload> FolderPayload = GetAssetFolderItemPayload(InItem))
	{
		if (TSharedPtr<FExternalDragOperation> ExternalDragDropOp = InDragDropEvent.GetOperationAs<FExternalDragOperation>())
		{
			TOptional<EMouseCursor::Type> NewDragCursor;
			if (!ExternalDragDropOp->HasFiles() || !ContentBrowserAssetData::CanModifyPath(AssetTools, FolderPayload->GetInternalPath(), nullptr))
			{
				NewDragCursor = EMouseCursor::SlashedCircle;
			}
			ExternalDragDropOp->SetCursorOverride(NewDragCursor);

			return true; // We will handle this drop, even if the result is invalid (eg, read-only folder)
		}
	}

	return false;
}

bool UContentBrowserAssetDataSource::HandleDragEnterItem(const FContentBrowserItemData& InItem, const FDragDropEvent& InDragDropEvent)
{
	return CanHandleDragDropEvent(InItem, InDragDropEvent);
}

bool UContentBrowserAssetDataSource::HandleDragOverItem(const FContentBrowserItemData& InItem, const FDragDropEvent& InDragDropEvent)
{
	return CanHandleDragDropEvent(InItem, InDragDropEvent);
}

bool UContentBrowserAssetDataSource::HandleDragLeaveItem(const FContentBrowserItemData& InItem, const FDragDropEvent& InDragDropEvent)
{
	return CanHandleDragDropEvent(InItem, InDragDropEvent);
}

bool UContentBrowserAssetDataSource::HandleDragDropOnItem(const FContentBrowserItemData& InItem, const FDragDropEvent& InDragDropEvent)
{
	if (TSharedPtr<const FContentBrowserAssetFolderItemDataPayload> FolderPayload = GetAssetFolderItemPayload(InItem))
	{
		if (TSharedPtr<FExternalDragOperation> ExternalDragDropOp = InDragDropEvent.GetOperationAs<FExternalDragOperation>())
		{
			FText ErrorMsg;
			if (ExternalDragDropOp->HasFiles() && ContentBrowserAssetData::CanModifyPath(AssetTools, FolderPayload->GetInternalPath(), &ErrorMsg))
			{
				// Delay import until next tick to avoid blocking the process that files were dragged from
				GEditor->GetEditorSubsystem<UImportSubsystem>()->ImportNextTick(ExternalDragDropOp->GetFiles(), FolderPayload->GetInternalPath().ToString());
			}

			if (!ErrorMsg.IsEmpty())
			{
				AssetViewUtils::ShowErrorNotifcation(ErrorMsg);
			}

			return true; // We handled this drop, even if the result was invalid (eg, read-only folder)
		}
	}

	return false;
}

bool UContentBrowserAssetDataSource::TryGetCollectionId(const FContentBrowserItemData& InItem, FSoftObjectPath& OutCollectionId)
{
	if (TSharedPtr<const FContentBrowserAssetFileItemDataPayload> AssetPayload = GetAssetFileItemPayload(InItem))
	{
		OutCollectionId = AssetPayload->GetAssetData().GetSoftObjectPath();
		return true;
	}
	return false;
}

bool UContentBrowserAssetDataSource::Legacy_TryGetPackagePath(const FContentBrowserItemData& InItem, FName& OutPackagePath)
{
	if (TSharedPtr<const FContentBrowserAssetFolderItemDataPayload> FolderPayload = GetAssetFolderItemPayload(InItem))
	{
		OutPackagePath = FolderPayload->GetInternalPath();
		return true;
	}
	return false;
}

bool UContentBrowserAssetDataSource::Legacy_TryGetAssetData(const FContentBrowserItemData& InItem, FAssetData& OutAssetData)
{
	if (TSharedPtr<const FContentBrowserAssetFileItemDataPayload> AssetPayload = GetAssetFileItemPayload(InItem))
	{
		OutAssetData = AssetPayload->GetAssetData();
		return true;
	}
	return false;
}

bool UContentBrowserAssetDataSource::Legacy_TryConvertPackagePathToVirtualPath(const FName InPackagePath, FName& OutPath)
{
	return IsKnownContentPath(InPackagePath) // Ignore unknown content paths
		&& TryConvertInternalPathToVirtual(InPackagePath, OutPath);
}

bool UContentBrowserAssetDataSource::Legacy_TryConvertAssetDataToVirtualPath(const FAssetData& InAssetData, const bool InUseFolderPaths, FName& OutPath)
{
	return InAssetData.AssetClassPath != FTopLevelAssetPath(TEXT("/Script/CoreUObject"), TEXT("Class")) // Ignore legacy class items
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		&& TryConvertInternalPathToVirtual(InUseFolderPaths ? InAssetData.PackagePath : InAssetData.ObjectPath, OutPath);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

bool UContentBrowserAssetDataSource::IsKnownContentPath(const FName InPackagePath) const
{
	FNameBuilder PackagePathStr(InPackagePath);
	const FStringView PackagePathStrView = PackagePathStr;
	for (const FString& RootContentPath : RootContentPaths)
	{
		const FStringView RootContentPathNoSlash = FStringView(RootContentPath).LeftChop(1);
		if (PackagePathStrView.StartsWith(RootContentPath, ESearchCase::IgnoreCase) || PackagePathStrView.Equals(RootContentPathNoSlash, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

bool UContentBrowserAssetDataSource::IsRootContentPath(const FName InPackagePath) const
{
	FNameBuilder PackagePathStr(InPackagePath);
	PackagePathStr << TEXT('/'); // RootContentPaths have a trailing slash

	const FStringView PackagePathStrView = PackagePathStr;
	return RootContentPaths.ContainsByPredicate([&PackagePathStrView](const FString& InRootContentPath)
	{
		return PackagePathStrView == InRootContentPath;
	});
}

bool UContentBrowserAssetDataSource::GetObjectPathsForCollections(ICollectionManager* CollectionManager, TArrayView<const FCollectionNameType> InCollections, const bool bIncludeChildCollections, TArray<FSoftObjectPath>& OutObjectPaths)
{
	if (InCollections.Num() > 0)
	{
		const ECollectionRecursionFlags::Flags CollectionRecursionMode = bIncludeChildCollections ? ECollectionRecursionFlags::SelfAndChildren : ECollectionRecursionFlags::Self;
		
		for (const FCollectionNameType& CollectionNameType : InCollections)
		{
			CollectionManager->GetObjectsInCollection(CollectionNameType.Name, CollectionNameType.Type, OutObjectPaths, CollectionRecursionMode);
		}

		return true;
	}

	return false;
}

FContentBrowserItemData UContentBrowserAssetDataSource::CreateAssetFolderItem(const FName InFolderPath)
{
	FName VirtualizedPath;
	TryConvertInternalPathToVirtual(InFolderPath, VirtualizedPath);

	return ContentBrowserAssetData::CreateAssetFolderItem(this, VirtualizedPath, InFolderPath);
}

FContentBrowserItemData UContentBrowserAssetDataSource::CreateAssetFileItem(const FAssetData& InAssetData)
{
	FName VirtualizedPath;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	TryConvertInternalPathToVirtual(InAssetData.ObjectPath, VirtualizedPath);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	return ContentBrowserAssetData::CreateAssetFileItem(this, VirtualizedPath, InAssetData);
}

TSharedPtr<const FContentBrowserAssetFolderItemDataPayload> UContentBrowserAssetDataSource::GetAssetFolderItemPayload(const FContentBrowserItemData& InItem) const
{
	return ContentBrowserAssetData::GetAssetFolderItemPayload(this, InItem);
}

TSharedPtr<const FContentBrowserAssetFileItemDataPayload> UContentBrowserAssetDataSource::GetAssetFileItemPayload(const FContentBrowserItemData& InItem) const
{
	return ContentBrowserAssetData::GetAssetFileItemPayload(this, InItem);
}

void UContentBrowserAssetDataSource::OnAssetRegistryFileLoadProgress(const IAssetRegistry::FFileLoadProgressUpdateData& InProgressUpdateData)
{
	if (InProgressUpdateData.bIsDiscoveringAssetFiles)
	{
		DiscoveryStatusText = FText::Format(LOCTEXT("DiscoveringAssetFiles", "Discovering Asset Files: {0} files found."), InProgressUpdateData.NumTotalAssets);
	}
	else
	{
		float ProgressFraction = 0.0f;
		if (InProgressUpdateData.NumTotalAssets > 0)
		{
			ProgressFraction = InProgressUpdateData.NumAssetsProcessedByAssetRegistry / (float)InProgressUpdateData.NumTotalAssets;
		}

		if (InProgressUpdateData.NumAssetsPendingDataLoad > 0)
		{
			DiscoveryStatusText = FText::Format(LOCTEXT("DiscoveringAssetData", "Discovering Asset Data ({0}): {1} assets remaining."), FText::AsPercent(ProgressFraction), InProgressUpdateData.NumAssetsPendingDataLoad);
		}
		else
		{
			const int32 NumAssetsLeftToProcess = InProgressUpdateData.NumTotalAssets - InProgressUpdateData.NumAssetsProcessedByAssetRegistry;
			if (NumAssetsLeftToProcess == 0)
			{
				DiscoveryStatusText = FText();
			}
			else
			{
				DiscoveryStatusText = FText::Format(LOCTEXT("ProcessingAssetData", "Processing Asset Data ({0}): {1} assets remaining."), FText::AsPercent(ProgressFraction), NumAssetsLeftToProcess);
			}
		}
	}
}

void UContentBrowserAssetDataSource::OnAssetAdded(const FAssetData& InAssetData)
{
	if (ContentBrowserAssetData::IsPrimaryAsset(InAssetData))
	{
		// The owner folder of this asset is no longer considered empty
		OnPathPopulated(InAssetData.PackagePath);

		QueueItemDataUpdate(FContentBrowserItemDataUpdate::MakeItemAddedUpdate(CreateAssetFileItem(InAssetData)));
	}
}

void UContentBrowserAssetDataSource::OnAssetRemoved(const FAssetData& InAssetData)
{
	if (ContentBrowserAssetData::IsPrimaryAsset(InAssetData))
	{
		QueueItemDataUpdate(FContentBrowserItemDataUpdate::MakeItemRemovedUpdate(CreateAssetFileItem(InAssetData)));
	}
}

void UContentBrowserAssetDataSource::OnAssetRenamed(const FAssetData& InAssetData, const FString& InOldObjectPath)
{
	if (ContentBrowserAssetData::IsPrimaryAsset(InAssetData))
	{
		// The owner folder of this asset is no longer considered empty
		OnPathPopulated(InAssetData.PackagePath);

		FName VirtualizedPath;
		TryConvertInternalPathToVirtual(*InOldObjectPath, VirtualizedPath);

		QueueItemDataUpdate(FContentBrowserItemDataUpdate::MakeItemMovedUpdate(CreateAssetFileItem(InAssetData), VirtualizedPath));
	}
}

void UContentBrowserAssetDataSource::OnAssetUpdated(const FAssetData& InAssetData)
{
	if (ContentBrowserAssetData::IsPrimaryAsset(InAssetData))
	{
		QueueItemDataUpdate(FContentBrowserItemDataUpdate::MakeItemModifiedUpdate(CreateAssetFileItem(InAssetData)));
	}
}

void UContentBrowserAssetDataSource::OnObjectPropertyChanged(UObject* InObject, FPropertyChangedEvent& InPropertyChangedEvent)
{
	if (InObject && InObject->IsAsset() && ContentBrowserAssetData::IsPrimaryAsset(InObject))
	{
		FAssetData AssetData(InObject);
		QueueItemDataUpdate(FContentBrowserItemDataUpdate::MakeItemModifiedUpdate(CreateAssetFileItem(AssetData)));
	}
}

void UContentBrowserAssetDataSource::OnPathAdded(const FString& InPath)
{
	// New paths are considered empty until assets are added inside them
	EmptyAssetFolders.Add(InPath);

	QueueItemDataUpdate(FContentBrowserItemDataUpdate::MakeItemAddedUpdate(CreateAssetFolderItem(*InPath)));
}

void UContentBrowserAssetDataSource::OnPathRemoved(const FString& InPath)
{
	// Deleted paths are no longer relevant for tracking
	AlwaysVisibleAssetFolders.Remove(InPath);
	EmptyAssetFolders.Remove(InPath);

	QueueItemDataUpdate(FContentBrowserItemDataUpdate::MakeItemRemovedUpdate(CreateAssetFolderItem(*InPath)));
}

void UContentBrowserAssetDataSource::OnPathPopulated(const FName InPath)
{
	OnPathPopulated(FNameBuilder(InPath));
}

void UContentBrowserAssetDataSource::OnPathPopulated(const FStringView InPath)
{
	// Recursively un-hide this path, emitting update events for any paths that change state so that the view updates
	if (InPath.Len() > 1)
	{
		// Trim any trailing slash
		FStringView Path = InPath;
		if (Path[Path.Len() - 1] == TEXT('/'))
		{
			Path = Path.Left(Path.Len() - 1);
		}

		// Recurse first as we want parents to be updated before their children
		{
			int32 LastSlashIndex = INDEX_NONE;
			if (Path.FindLastChar(TEXT('/'), LastSlashIndex) && LastSlashIndex > 0)
			{
				OnPathPopulated(Path.Left(LastSlashIndex));
			}
		}

		// Unhide this folder and emit a notification if required
		const uint32 PathHash = GetTypeHash(Path);
		if (EmptyAssetFolders.RemoveByHash(PathHash, Path) > 0)
		{
			// Queue an update event for this path as it may have become visible in the view
			QueueItemDataUpdate(FContentBrowserItemDataUpdate::MakeItemModifiedUpdate(CreateAssetFolderItem(FName(Path))));
		}
	}
}

void UContentBrowserAssetDataSource::OnAlwaysShowPath(const FString& InPath)
{
	// Recursively force show this path, emitting update events for any paths that change state so that the view updates
	if (InPath.Len() > 1)
	{
		// Trim any trailing slash
		FString Path = InPath;
		if (Path[Path.Len() - 1] == TEXT('/'))
		{
			Path.LeftInline(Path.Len() - 1);
		}

		// Recurse first as we want parents to be updated before their children
		{
			int32 LastSlashIndex = INDEX_NONE;
			if (Path.FindLastChar(TEXT('/'), LastSlashIndex) && LastSlashIndex > 0)
			{
				OnAlwaysShowPath(Path.Left(LastSlashIndex));
			}
		}

		// Force show this folder and emit a notification if required
		if (!AlwaysVisibleAssetFolders.Contains(Path))
		{
			AlwaysVisibleAssetFolders.Add(Path);

			// Queue an update event for this path as it may have become visible in the view
			QueueItemDataUpdate(FContentBrowserItemDataUpdate::MakeItemModifiedUpdate(CreateAssetFolderItem(FName(*Path))));
		}
	}
}

void UContentBrowserAssetDataSource::OnScanCompleted()
{
	// Done finding content - compact this set as items would have been removed as assets were found
	EmptyAssetFolders.CompactStable();
}

void UContentBrowserAssetDataSource::BuildRootPathVirtualTree() 
{
	Super::BuildRootPathVirtualTree();

	for (const FString& RootContentPath : RootContentPaths)
	{
		RootPathAdded(RootContentPath);
	}
}

void UContentBrowserAssetDataSource::OnContentPathMounted(const FString& InAssetPath, const FString& InFileSystemPath)
{
	RootContentPaths.AddUnique(InAssetPath);

	RootPathAdded(InAssetPath);

	// Mount roots are always visible
	OnAlwaysShowPath(InAssetPath);
}

void UContentBrowserAssetDataSource::OnContentPathDismounted(const FString& InAssetPath, const FString& InFileSystemPath)
{
	RootPathRemoved(InAssetPath);

	RootContentPaths.Remove(InAssetPath);
}

void UContentBrowserAssetDataSource::PopulateAddNewContextMenu(UToolMenu* InMenu)
{
	const UContentBrowserDataMenuContext_AddNewMenu* ContextObject = InMenu->FindContext<UContentBrowserDataMenuContext_AddNewMenu>();
	checkf(ContextObject, TEXT("Required context UContentBrowserDataMenuContext_AddNewMenu was missing!"));

	// Extract the internal asset paths that belong to this data source from the full list of selected paths given in the context
	TArray<FName> SelectedAssetPaths;
	for (const FName& SelectedPath : ContextObject->SelectedPaths)
	{
		FName InternalPath;
		if (TryConvertVirtualPathToInternal(SelectedPath, InternalPath) && IsKnownContentPath(InternalPath))
		{
			SelectedAssetPaths.Add(InternalPath);
		}
	}

	// Only add the asset items if we have an asset path selected
	FNewAssetContextMenu::FOnNewAssetRequested OnNewAssetRequested;
	FNewAssetContextMenu::FOnImportAssetRequested OnImportAssetRequested;
	if (SelectedAssetPaths.Num() > 0)
	{
		OnImportAssetRequested = FNewAssetContextMenu::FOnImportAssetRequested::CreateUObject(this, &UContentBrowserAssetDataSource::OnImportAsset);
		if (ContextObject->OnBeginItemCreation.IsBound())
		{
			OnNewAssetRequested = FNewAssetContextMenu::FOnNewAssetRequested::CreateUObject(this, &UContentBrowserAssetDataSource::OnNewAssetRequested, ContextObject->OnBeginItemCreation);
		}
	}

	FNewAssetContextMenu::MakeContextMenu(
		InMenu,
		SelectedAssetPaths,
		OnImportAssetRequested,
		OnNewAssetRequested
		);
}

void UContentBrowserAssetDataSource::PopulateContentBrowserToolBar(UToolMenu* InMenu)
{
	const UContentBrowserToolbarMenuContext* ContextObject = InMenu->FindContext<UContentBrowserToolbarMenuContext>();
	checkf(ContextObject, TEXT("Required context UContentBrowserToolbarMenuContext was missing!"));

	TSharedRef<SWidget> ImportButton = 
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.ToolTipText(LOCTEXT("ImportTooltip", "Import assets from files to the currently selected folder"))
		.ContentPadding(2)
		.OnClicked_UObject(this, &UContentBrowserAssetDataSource::OnImportClicked, ContextObject)
		.IsEnabled_UObject(this, &UContentBrowserAssetDataSource::IsImportEnabled, ContextObject)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("Icons.Import"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
			+ SHorizontalBox::Slot()
			.Padding(FMargin(3, 0, 0, 0))
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.TextStyle(FAppStyle::Get(), "NormalText")
				.Text(LOCTEXT("Import", "Import"))
			]
		];

	FToolMenuSection& Section = InMenu->FindOrAddSection("New");

	Section.AddSeparator(NAME_None);

	Section.AddEntry(
		FToolMenuEntry::InitWidget(
			"Import",
			ImportButton,
			FText::GetEmpty(),
			true,
			false
		));
	
}

void UContentBrowserAssetDataSource::PopulateAssetFolderContextMenu(UToolMenu* InMenu)
{
	return ContentBrowserAssetData::PopulateAssetFolderContextMenu(this, InMenu, *AssetFolderContextMenu);
}

void UContentBrowserAssetDataSource::PopulateAssetFileContextMenu(UToolMenu* InMenu)
{
	return ContentBrowserAssetData::PopulateAssetFileContextMenu(this, InMenu, *AssetFileContextMenu);
}

void UContentBrowserAssetDataSource::PopulateDragDropContextMenu(UToolMenu* InMenu)
{
	const UContentBrowserDataMenuContext_DragDropMenu* ContextObject = InMenu->FindContext<UContentBrowserDataMenuContext_DragDropMenu>();
	checkf(ContextObject, TEXT("Required context UContentBrowserDataMenuContext_DragDropMenu was missing!"));

	FToolMenuSection& Section = InMenu->FindOrAddSection("MoveCopy");
	if (ContextObject->bCanCopy)
	{
		// Get the internal drop path
		FName DropAssetPath;
		{
			for (const FContentBrowserItemData& DropTargetItemData : ContextObject->DropTargetItem.GetInternalItems())
			{
				if (TSharedPtr<const FContentBrowserAssetFolderItemDataPayload> FolderPayload = GetAssetFolderItemPayload(DropTargetItemData))
				{
					DropAssetPath = FolderPayload->GetInternalPath();
					break;
				}
			}
		}

		// Extract the internal package paths that belong to this data source from the full list of selected items given in the context
		TArray<FName> AdvancedCopyInputs;
		for (const FContentBrowserItem& DraggedItem : ContextObject->DraggedItems)
		{
			for (const FContentBrowserItemData& DraggedItemData : DraggedItem.GetInternalItems())
			{
				if (TSharedPtr<const FContentBrowserAssetFileItemDataPayload> AssetPayload = GetAssetFileItemPayload(DraggedItemData))
				{
					AdvancedCopyInputs.Add(AssetPayload->GetAssetData().PackageName);
				}

				if (TSharedPtr<const FContentBrowserAssetFolderItemDataPayload> FolderPayload = GetAssetFolderItemPayload(DraggedItemData))
				{
					AdvancedCopyInputs.Add(FolderPayload->GetInternalPath());
				}
			}
		}

		if (!DropAssetPath.IsNone() && AdvancedCopyInputs.Num() > 0)
		{
			Section.AddMenuEntry(
				"DragDropAdvancedCopy",
				LOCTEXT("DragDropAdvancedCopy", "Advanced Copy Here"),
				LOCTEXT("DragDropAdvancedCopyTooltip", "Copy the dragged items and any specified dependencies to this folder, afterwards fixing up any dependencies on copied files to the new files."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([this, AdvancedCopyInputs, DestinationPath = DropAssetPath.ToString()]() { OnAdvancedCopyRequested(AdvancedCopyInputs, DestinationPath); }))
				);
		}
	}
}

void UContentBrowserAssetDataSource::OnAdvancedCopyRequested(const TArray<FName>& InAdvancedCopyInputs, const FString& InDestinationPath)
{
	AssetTools->BeginAdvancedCopyPackages(InAdvancedCopyInputs, InDestinationPath / FString());
}

void UContentBrowserAssetDataSource::OnImportAsset(const FName InPath)
{
	if (ensure(!InPath.IsNone()))
	{
		AssetTools->ImportAssetsWithDialogAsync(InPath.ToString());
	}
}

void UContentBrowserAssetDataSource::OnNewAssetRequested(const FName InPath, TWeakObjectPtr<UClass> InFactoryClass, UContentBrowserDataMenuContext_AddNewMenu::FOnBeginItemCreation InOnBeginItemCreation)
{
	UClass* FactoryClass = InFactoryClass.Get();
	if (ensure(!InPath.IsNone()) && ensure(FactoryClass) && ensure(InOnBeginItemCreation.IsBound()))
	{
		UFactory* NewFactory = NewObject<UFactory>(GetTransientPackage(), FactoryClass);

		// This factory may get gc'd as a side effect of various delegates potentially calling CollectGarbage so protect against it from being gc'd out from under us
		FGCObjectScopeGuard FactoryGCGuard(NewFactory);

		FEditorDelegates::OnConfigureNewAssetProperties.Broadcast(NewFactory);
		if (NewFactory->ConfigureProperties())
		{
			FEditorDelegates::OnNewAssetCreated.Broadcast(NewFactory);

			FString DefaultAssetName;
			FString PackageNameToUse;
			AssetTools->CreateUniqueAssetName(InPath.ToString() / NewFactory->GetDefaultNewAssetName(), FString(), PackageNameToUse, DefaultAssetName);

			OnBeginCreateAsset(*DefaultAssetName, InPath, NewFactory->GetSupportedClass(), NewFactory, InOnBeginItemCreation);
		}
	}
}

void UContentBrowserAssetDataSource::OnBeginCreateAsset(const FName InDefaultAssetName, const FName InPackagePath, UClass* InAssetClass, UFactory* InFactory, UContentBrowserDataMenuContext_AddNewMenu::FOnBeginItemCreation InOnBeginItemCreation)
{
	if (!ensure(InOnBeginItemCreation.IsBound()))
	{
		return;
	}

	if (!ensure(InAssetClass || InFactory))
	{
		return;
	}

	if (InAssetClass && InFactory && !ensure(InAssetClass->IsChildOf(InFactory->GetSupportedClass())))
	{
		return;
	}

	UClass* ClassToUse = InAssetClass ? InAssetClass : (InFactory ? InFactory->GetSupportedClass() : nullptr);
	if (!ensure(ClassToUse))
	{
		return;
	}

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().GetModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	FContentBrowserItemPath AssetPathToUse = ContentBrowserModule.Get().GetInitialPathToSaveAsset(FContentBrowserItemPath(InPackagePath, EContentBrowserPathType::Internal));

	const bool bShowDialogToPickPath = !AssetPathToUse.HasInternalPath() || (AssetPathToUse.GetInternalPathName() != InPackagePath);
	if (bShowDialogToPickPath)
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
		const FString InitialInternalPath = AssetPathToUse.HasInternalPath() ? AssetPathToUse.GetInternalPathString() : TEXT("/Game");
		AssetToolsModule.Get().CreateAssetWithDialog(InDefaultAssetName.ToString(), InitialInternalPath, ClassToUse, InFactory, NAME_None, /*bCallConfigureProperties*/ false);
	}
	else
	{
		FAssetData NewAssetData(*(InPackagePath.ToString() / InDefaultAssetName.ToString()), InPackagePath, InDefaultAssetName, ClassToUse->GetClassPathName());

		FName VirtualizedPath;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		TryConvertInternalPathToVirtual(NewAssetData.ObjectPath, VirtualizedPath);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

		FContentBrowserItemData NewItemData(
			this, 
			EContentBrowserItemFlags::Type_File | EContentBrowserItemFlags::Category_Asset | EContentBrowserItemFlags::Temporary_Creation,
			VirtualizedPath, 
			NewAssetData.AssetName,
			FText::AsCultureInvariant(NewAssetData.AssetName.ToString()),
			MakeShared<FContentBrowserAssetFileItemDataPayload_Creation>(MoveTemp(NewAssetData), InAssetClass, InFactory)
			);

		InOnBeginItemCreation.Execute(FContentBrowserItemDataTemporaryContext(
			MoveTemp(NewItemData), 
			FContentBrowserItemDataTemporaryContext::FOnValidateItem::CreateUObject(this, &UContentBrowserAssetDataSource::OnValidateItemName),
			FContentBrowserItemDataTemporaryContext::FOnFinalizeItem::CreateUObject(this, &UContentBrowserAssetDataSource::OnFinalizeCreateAsset)
			));
	}
}

bool UContentBrowserAssetDataSource::OnValidateItemName(const FContentBrowserItemData& InItem, const FString& InProposedName, FText* OutErrorMsg)
{
	return CanRenameItem(InItem, &InProposedName, OutErrorMsg);
}

FReply UContentBrowserAssetDataSource::OnImportClicked(const UContentBrowserToolbarMenuContext* ContextObject)
{
	// Extract the internal asset paths that belong to this data source from the full list of selected paths given in the context
	FName InternalPath;
	if (TryConvertVirtualPathToInternal(ContextObject->GetCurrentPath(), InternalPath) && IsKnownContentPath(InternalPath))
	{
		OnImportAsset(InternalPath);
	}

	return FReply::Handled();
}

bool UContentBrowserAssetDataSource::IsImportEnabled(const UContentBrowserToolbarMenuContext* ContextObject) const
{
	return ContextObject->CanWriteToCurrentPath();
}

FContentBrowserItemData UContentBrowserAssetDataSource::OnFinalizeCreateFolder(const FContentBrowserItemData& InItemData, const FString& InProposedName, FText* OutErrorMsg)
{
	checkf(InItemData.GetOwnerDataSource() == this, TEXT("OnFinalizeCreateFolder was bound to an instance from the wrong data source!"));
	checkf(EnumHasAllFlags(InItemData.GetItemFlags(), EContentBrowserItemFlags::Type_Folder | EContentBrowserItemFlags::Temporary_Creation), TEXT("OnFinalizeCreateFolder called for an instance with the incorrect type flags!"));

	// Committed creation
	if (TSharedPtr<const FContentBrowserAssetFolderItemDataPayload> FolderPayload = GetAssetFolderItemPayload(InItemData))
	{
		const FString FolderPath = FPaths::GetPath(FolderPayload->GetInternalPath().ToString()) / InProposedName;

		FString NewPathOnDisk;
		if (FPackageName::TryConvertLongPackageNameToFilename(FolderPath, NewPathOnDisk) && IFileManager::Get().MakeDirectory(*NewPathOnDisk, true))
		{
			AssetRegistry->AddPath(FolderPath);
			AssetViewUtils::OnAlwaysShowPath().Broadcast(FolderPath);
			return CreateAssetFolderItem(*FolderPath);
		}
	}

	ContentBrowserAssetData::SetOptionalErrorMessage(OutErrorMsg, LOCTEXT("Error_FailedToCreateFolder", "Failed to create folder"));
	return FContentBrowserItemData();
}

FContentBrowserItemData UContentBrowserAssetDataSource::OnFinalizeCreateAsset(const FContentBrowserItemData& InItemData, const FString& InProposedName, FText* OutErrorMsg)
{
	checkf(InItemData.GetOwnerDataSource() == this, TEXT("OnFinalizeCreateAsset was bound to an instance from the wrong data source!"));
	checkf(EnumHasAllFlags(InItemData.GetItemFlags(), EContentBrowserItemFlags::Type_File | EContentBrowserItemFlags::Temporary_Creation), TEXT("OnFinalizeCreateAsset called for an instance with the incorrect type flags!"));

	// Committed creation
	UObject* Asset = nullptr;
	{
		TSharedPtr<const FContentBrowserAssetFileItemDataPayload_Creation> CreationContext = StaticCastSharedPtr<const FContentBrowserAssetFileItemDataPayload_Creation>(InItemData.GetPayload());
		
		UClass* AssetClass = CreationContext->GetAssetClass();
		UFactory* Factory = CreationContext->GetFactory();
		
		if (AssetClass || Factory)
		{
			Asset = AssetTools->CreateAsset(InProposedName, CreationContext->GetAssetData().PackagePath.ToString(), AssetClass, Factory, FName("ContentBrowserNewAsset"));
		}
	}

	if (!Asset)
	{
		ContentBrowserAssetData::SetOptionalErrorMessage(OutErrorMsg, LOCTEXT("Error_FailedToCreateAsset", "Failed to create asset"));
		return FContentBrowserItemData();
	}

	return CreateAssetFileItem(FAssetData(Asset));
}

FContentBrowserItemData UContentBrowserAssetDataSource::OnFinalizeDuplicateAsset(const FContentBrowserItemData& InItemData, const FString& InProposedName, FText* OutErrorMsg)
{
	checkf(InItemData.GetOwnerDataSource() == this, TEXT("OnFinalizeDuplicateAsset was bound to an instance from the wrong data source!"));
	checkf(EnumHasAllFlags(InItemData.GetItemFlags(), EContentBrowserItemFlags::Type_File | EContentBrowserItemFlags::Temporary_Duplication), TEXT("OnFinalizeDuplicateAsset called for an instance with the incorrect type flags!"));

	// Committed duplication
	UObject* Asset = nullptr;
	{
		TSharedPtr<const FContentBrowserAssetFileItemDataPayload_Duplication> DuplicationContext = StaticCastSharedPtr<const FContentBrowserAssetFileItemDataPayload_Duplication>(InItemData.GetPayload());

		if (UObject* SourceObject = DuplicationContext->GetSourceObject())
		{
			Asset = AssetTools->DuplicateAsset(InProposedName, DuplicationContext->GetAssetData().PackagePath.ToString(), SourceObject);
		}
	}

	if (!Asset)
	{
		ContentBrowserAssetData::SetOptionalErrorMessage(OutErrorMsg, LOCTEXT("Error_FailedToCreateAsset", "Failed to create asset"));
		return FContentBrowserItemData();
	}

	return CreateAssetFileItem(FAssetData(Asset));
}

bool UContentBrowserAssetDataSource::PathPassesCompiledDataFilter(const FContentBrowserCompiledAssetDataFilter& InFilter, const FName InPath)
{
	FNameBuilder PathStr(InPath);
	FStringView Path(PathStr);

	auto PathPassesFilter = [Path](const FPathPermissionList& InPathFilter, const bool InRecursive)
	{
		return !InPathFilter.HasFiltering() || (InRecursive ? InPathFilter.PassesStartsWithFilter(Path, /*bAllowParentPaths*/true) : InPathFilter.PassesFilter(Path));
	};

	return PathPassesFilter(InFilter.PackagePathsToInclude, InFilter.bRecursivePackagePathsToInclude)
		&& PathPassesFilter(InFilter.PackagePathsToExclude, InFilter.bRecursivePackagePathsToExclude)
		&& PathPassesFilter(InFilter.PathPermissionList, /*bRecursive*/true) // PassesPathFilter
		&& !InFilter.ExcludedPackagePaths.Contains(InPath) // PassesExcludedPathsFilter
		&& ContentBrowserDataUtils::PathPassesAttributeFilter(Path, 0, InFilter.ItemAttributeFilter); // PassesAttributeFilter
}

#undef LOCTEXT_NAMESPACE


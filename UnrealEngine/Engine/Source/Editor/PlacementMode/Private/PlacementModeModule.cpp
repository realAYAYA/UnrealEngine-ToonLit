// Copyright Epic Games, Inc. All Rights Reserved.
#include "PlacementModeModule.h"

#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"
#include "Misc/NamePermissionList.h"
#include "UObject/Class.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Textures/SlateIcon.h"
#include "Styling/AppStyle.h"
#include "GameFramework/Actor.h"
#include "ActorFactories/ActorFactory.h"
#include "ActorFactories/ActorFactoryBoxReflectionCapture.h"
#include "ActorFactories/ActorFactoryBoxVolume.h"
#include "ActorFactories/ActorFactoryCharacter.h"
#include "ActorFactories/ActorFactoryDeferredDecal.h"
#include "ActorFactories/ActorFactoryDirectionalLight.h"
#include "ActorFactories/ActorFactoryEmptyActor.h"
#include "ActorFactories/ActorFactoryPawn.h"
#include "ActorFactories/ActorFactoryExponentialHeightFog.h"
#include "ActorFactories/ActorFactorySkyAtmosphere.h"
#include "ActorFactories/ActorFactoryVolumetricCloud.h"
#include "ActorFactories/ActorFactoryPlayerStart.h"
#include "ActorFactories/ActorFactoryPointLight.h"
#include "ActorFactories/ActorFactorySpotLight.h"
#include "ActorFactories/ActorFactoryRectLight.h"
#include "ActorFactories/ActorFactorySkyLight.h"
#include "ActorFactories/ActorFactorySphereReflectionCapture.h"
#include "ActorFactories/ActorFactoryBasicShape.h"
#include "ActorFactories/ActorFactoryTriggerBox.h"
#include "ActorFactories/ActorFactoryTriggerSphere.h"
#include "Engine/BrushBuilder.h"
#include "Engine/Brush.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Volume.h"
#include "Engine/PostProcessVolume.h"
#include "LevelEditorActions.h"
#include "AssetRegistry/AssetData.h"
#include "EditorModeRegistry.h"
#include "EditorModes.h"
#include "IAssetTools.h"
#include "IAssetTypeActions.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ActorPlacementInfo.h"
#include "IPlacementModeModule.h"
#include "ToolMenus.h"
#include "AssetToolsModule.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ActorFactories/ActorFactoryPlanarReflection.h"
#include "SPlacementModeTools.h"
#include "AssetSelection.h"


TOptional<FLinearColor> GetBasicShapeColorOverride()
{
	// Get color for basic shapes.  It should appear like all the other basic types
	static TOptional<FLinearColor> BasicShapeColorOverride;

	if (!BasicShapeColorOverride.IsSet())
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		TSharedPtr<IAssetTypeActions> AssetTypeActions;
		AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(UClass::StaticClass()).Pin();
		if (AssetTypeActions.IsValid())
		{
			BasicShapeColorOverride = TOptional<FLinearColor>(AssetTypeActions->GetTypeColor());
		}
	}
	return BasicShapeColorOverride;
}

FPlacementModeModule::FPlacementModeModule()
	: CategoryPermissionList(MakeShareable(new FNamePermissionList()))
{
	CategoryPermissionList->OnFilterChanged().AddRaw(this, &FPlacementModeModule::OnCategoryPermissionListChanged);
}

void FPlacementModeModule::StartupModule()
{
	TArray< FString > RecentlyPlacedAsStrings;
	GConfig->GetArray(TEXT("PlacementMode"), TEXT("RecentlyPlaced"), RecentlyPlacedAsStrings, GEditorPerProjectIni);

	for (int Index = 0; Index < RecentlyPlacedAsStrings.Num(); Index++)
	{
		RecentlyPlaced.Add(FActorPlacementInfo(RecentlyPlacedAsStrings[Index]));
	}

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetRegistry.OnAssetRemoved().AddRaw(this, &FPlacementModeModule::OnAssetRemoved);
	AssetRegistry.OnAssetRenamed().AddRaw(this, &FPlacementModeModule::OnAssetRenamed);
	if (AssetRegistry.IsLoadingAssets())
	{
		AssetRegistry.OnFilesLoaded().AddRaw(this, &FPlacementModeModule::OnInitialAssetsScanComplete);
	}
	else
	{
		AssetRegistry.OnAssetAdded().AddRaw(this, &FPlacementModeModule::OnAssetAdded);
	}

	TOptional<FLinearColor> BasicShapeColorOverride = GetBasicShapeColorOverride();


	RegisterPlacementCategory(
		FPlacementCategoryInfo(
			NSLOCTEXT("PlacementMode", "RecentlyPlaced", "Recently Placed"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "PlacementBrowser.Icons.Recent"),
			FBuiltInPlacementCategories::RecentlyPlaced(),
			TEXT("PMRecentlyPlaced"),
			TNumericLimits<int32>::Lowest(),
			false
		)
	);

	{
		int32 SortOrder = 0;
		FName CategoryName = FBuiltInPlacementCategories::Basic();
		RegisterPlacementCategory(
			FPlacementCategoryInfo(
				NSLOCTEXT("PlacementMode", "Basic", "Basic"),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "PlacementBrowser.Icons.Basic"),
				CategoryName,
				TEXT("PMBasic"),
				10
			)
		);

		FPlacementCategory* Category = Categories.Find(CategoryName);
		Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactoryEmptyActor::StaticClass(), SortOrder += 10)));
		Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactoryCharacter::StaticClass(), SortOrder += 10)));
		Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactoryPawn::StaticClass(), SortOrder += 10)));
		Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactoryPointLight::StaticClass(), SortOrder += 10)));
		Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactoryPlayerStart::StaticClass(), SortOrder += 10)));
	
		Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactoryTriggerBox::StaticClass(), SortOrder += 10)));
		Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactoryTriggerSphere::StaticClass(), SortOrder += 10)));
	}

	{
		int32 SortOrder = 0;
		FName CategoryName = FBuiltInPlacementCategories::Lights();
		RegisterPlacementCategory(
			FPlacementCategoryInfo(
				NSLOCTEXT("PlacementMode", "Lights", "Lights"),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "PlacementBrowser.Icons.Lights"),
				CategoryName,
				TEXT("PMLights"),
				20
			)
		);

		FPlacementCategory* Category = Categories.Find(CategoryName);
		Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactoryDirectionalLight::StaticClass(), SortOrder += 10)));
		Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactoryPointLight::StaticClass(), SortOrder += 10)));
		Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactorySpotLight::StaticClass(), SortOrder += 10)));
		Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactoryRectLight::StaticClass(), SortOrder += 10)));
		Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactorySkyLight::StaticClass(), SortOrder += 10)));
	}

	{

		int32 SortOrder = 0;
		FName CategoryName = FBuiltInPlacementCategories::Shapes();
		RegisterPlacementCategory(
			FPlacementCategoryInfo(
				NSLOCTEXT("PlacementMode", "Shapes", "Shapes"),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Cube"),
				CategoryName,
				TEXT("PMShapes"),
				25	
				)
			);

		FPlacementCategory* Category = Categories.Find(CategoryName);
		// Cube
		Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactoryBasicShape::StaticClass(), FAssetData(LoadObject<UStaticMesh>(nullptr, *UActorFactoryBasicShape::BasicCube.ToString())), FName("ClassThumbnail.Cube"), FName("ClassIcon.Cube"), BasicShapeColorOverride, SortOrder += 10, NSLOCTEXT("PlacementMode", "Cube", "Cube"))));
		// Sphere
		Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactoryBasicShape::StaticClass(), FAssetData(LoadObject<UStaticMesh>(nullptr, *UActorFactoryBasicShape::BasicSphere.ToString())), FName("ClassThumbnail.Sphere"), FName("ClassIcon.Sphere"), BasicShapeColorOverride, SortOrder += 10, NSLOCTEXT("PlacementMode", "Sphere", "Sphere"))));
		// Cylinder
		Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactoryBasicShape::StaticClass(), FAssetData(LoadObject<UStaticMesh>(nullptr, *UActorFactoryBasicShape::BasicCylinder.ToString())), FName("ClassThumbnail.Cylinder"), FName("ClassIcon.Cylinder"), BasicShapeColorOverride, SortOrder += 10, NSLOCTEXT("PlacementMode", "Cylinder", "Cylinder"))));
		// Cone
		Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactoryBasicShape::StaticClass(), FAssetData(LoadObject<UStaticMesh>(nullptr, *UActorFactoryBasicShape::BasicCone.ToString())), FName("ClassThumbnail.Cone"), FName("ClassIcon.Cone"), BasicShapeColorOverride, SortOrder += 10, NSLOCTEXT("PlacementMode", "Cone", "Cone"))));
		// Plane
		Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactoryBasicShape::StaticClass(), FAssetData(LoadObject<UStaticMesh>(nullptr, *UActorFactoryBasicShape::BasicPlane.ToString())), FName("ClassThumbnail.Plane"), FName("ClassIcon.Plane"), BasicShapeColorOverride, SortOrder += 10, NSLOCTEXT("PlacementMode", "Plane", "Plane"))));
	}

	{
		int32 SortOrder = 0;
		FName CategoryName = FBuiltInPlacementCategories::Visual();
		RegisterPlacementCategory(
			FPlacementCategoryInfo(
				NSLOCTEXT("PlacementMode", "VisualEffects", "Visual Effects"),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "PlacementBrowser.Icons.VisualEffects"),
				CategoryName,
				TEXT("PMVisual"),
				30
			)
		);

		UActorFactory* PPFactory = GEditor->FindActorFactoryByClassForActorClass(UActorFactoryBoxVolume::StaticClass(), APostProcessVolume::StaticClass());

		FPlacementCategory* Category = Categories.Find(CategoryName);
		Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(PPFactory, FAssetData(APostProcessVolume::StaticClass()), SortOrder += 10)));
		Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactorySkyAtmosphere::StaticClass(), SortOrder += 10)));
		Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactoryVolumetricCloud::StaticClass(), SortOrder += 10)));
		Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactoryExponentialHeightFog::StaticClass(), SortOrder += 10)));
		Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactorySphereReflectionCapture::StaticClass(), SortOrder += 10)));
		Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactoryBoxReflectionCapture::StaticClass(), SortOrder += 10)));
		Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactoryPlanarReflection::StaticClass(), SortOrder += 10)));
		Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactoryDeferredDecal::StaticClass(), SortOrder += 10)));
	}

	RegisterPlacementCategory(
		FPlacementCategoryInfo(
			NSLOCTEXT("PlacementMode", "Volumes", "Volumes"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "PlacementBrowser.Icons.Volumes"),
			FBuiltInPlacementCategories::Volumes(),
			TEXT("PMVolumes"),
			40
		)
	);

	RegisterPlacementCategory(
		FPlacementCategoryInfo(
			NSLOCTEXT("PlacementMode", "AllClasses", "All Classes"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "PlacementBrowser.Icons.All"),
			FBuiltInPlacementCategories::AllClasses(),
			TEXT("PMAllClasses"),
			50
		)
	);


	BasicShapeThumbnails.Add(UActorFactoryBasicShape::BasicCube.ToString(), TEXT("ClassThumbnail.Cube"));
	BasicShapeThumbnails.Add(UActorFactoryBasicShape::BasicSphere.ToString(), TEXT("ClassThumbnail.Sphere"));
	BasicShapeThumbnails.Add(UActorFactoryBasicShape::BasicCylinder.ToString(), TEXT("ClassThumbnail.Cylinder"));
	BasicShapeThumbnails.Add(UActorFactoryBasicShape::BasicCone.ToString(), TEXT("ClassThumbnail.Cone"));
	BasicShapeThumbnails.Add(UActorFactoryBasicShape::BasicPlane.ToString(), TEXT("ClassThumbnail.Plane"));


	if (FSlateApplication::IsInitialized())
	{
		// Given a Category Name, this will add a section and all the placeable items in that section directly into the menu
		auto GenerateQuickCreateSection = [this](const FName& SectionName, UToolMenu* InMenu, int MaxItems = 20)
		{
			FPlacementCategory* Category = Categories.Find(SectionName);
			FToolMenuSection& Section = InMenu->AddSection(SectionName, Category->DisplayName);

			int count = 1;
			for (auto& Pair : Category->Items)
			{
				TSharedPtr<const FPlaceableItem> Item = Pair.Value;
				FToolMenuEntry& NewEntry = Section.AddEntry(
					FToolMenuEntry::InitWidget(Item->AssetData.AssetName, SNew(SPlacementAssetMenuEntry, Item), FText(), true, true)
				);

				if (++count > MaxItems)
					break;
			}
		};

		auto GenerateCategorySubMenu = [this](UToolMenu* InMenu, const FName& InSectionName, const FText& InSectionDisplayName, const FName& PlacementCategory)
		{
			
			RegenerateItemsForCategory(PlacementCategory);
			FPlacementCategory* Category = Categories.Find(PlacementCategory);
			if (!Category->Items.IsEmpty())
			{
				FToolMenuSection* InSection = InMenu->FindSection(InSectionName);	
				if (InSection == nullptr)
				{
					InSection = &(InMenu->AddSection(InSectionName, InSectionDisplayName));
				}

				FToolMenuEntry& AllSubMenu = InSection->AddSubMenu(PlacementCategory,
					Category->DisplayName,
					FText::GetEmpty(),
					FNewToolMenuDelegate::CreateLambda([this, PlacementCategory](UToolMenu* InMenu)
					{
						FToolMenuSection& Section = InMenu->AddSection(PlacementCategory);
						RegenerateItemsForCategory(PlacementCategory);
						FPlacementCategory* Category = Categories.Find(PlacementCategory);
						for (auto& Pair : Category->Items)
						{
							TSharedPtr<const FPlaceableItem> Item = Pair.Value;
							FToolMenuEntry& NewEntry = Section.AddEntry(
								FToolMenuEntry::InitWidget(Item->AssetData.AssetName, SNew(SPlacementAssetMenuEntry, Item), FText(), true, true)
							);
						}
					})
				);
				AllSubMenu.Icon = Category->DisplayIcon;
			}
		};

		
		UToolMenu* ContentMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.AddQuickMenu");
		FName CreateSectionName = TEXT("PMQCreateMenu");
		FText CreateSectionDisplayName = NSLOCTEXT("PlacementMode", "PMQCreateMenu", "Place Actors");

		GenerateCategorySubMenu(ContentMenu, CreateSectionName, CreateSectionDisplayName, FBuiltInPlacementCategories::Basic());

		// All Subcategories as submenus
		FName CategoriesSectionName = TEXT("CreateAllCategories");
		ContentMenu->AddDynamicSection(CategoriesSectionName,
			FNewToolMenuDelegate::CreateLambda([this, CreateSectionName, CreateSectionDisplayName, GenerateCategorySubMenu](UToolMenu* InMenu)
			{
				TArray<FPlacementCategoryInfo> SortedCategories;
				GetSortedCategories(SortedCategories);
				for (auto CategoryInfo : SortedCategories)
				{
					// Skip Basic and Recent since we add those later
					if (CategoryInfo.UniqueHandle == FBuiltInPlacementCategories::Basic() ||
						CategoryInfo.UniqueHandle == FBuiltInPlacementCategories::RecentlyPlaced())
						continue;

					GenerateCategorySubMenu(InMenu, CreateSectionName, CreateSectionDisplayName, CategoryInfo.UniqueHandle);
				}
			}
		), FToolMenuInsert(NAME_None, EToolMenuInsertType::First));


		// Recents Section, limit to 5 items
		const FName& RecentName = FBuiltInPlacementCategories::RecentlyPlaced();

		ContentMenu->AddDynamicSection(RecentName,
			FNewToolMenuDelegate::CreateLambda([this, GenerateQuickCreateSection, GenerateCategorySubMenu, CreateSectionName, CreateSectionDisplayName](UToolMenu* InMenu)
			{
				const FName& RecentName = FBuiltInPlacementCategories::RecentlyPlaced();
				FPlacementCategory* RecentCategory = Categories.Find(RecentName);
				RefreshRecentlyPlaced();
				GenerateQuickCreateSection(RecentName, InMenu, 5);
			}
		));

		// Open Placement Browser Panel
		FToolMenuSection& BrowserSection = ContentMenu->AddSection(TEXT("PlacementBrowserMenuSection"), FText::GetEmpty(), FToolMenuInsert(RecentName, EToolMenuInsertType::Before));

		BrowserSection.AddMenuEntry(FLevelEditorCommands::Get().OpenPlaceActors);

	} // end FSlateApplication::IsInitialized()

}

void FPlacementModeModule::PreUnloadCallback()
{
	FAssetRegistryModule* AssetRegistryModule = FModuleManager::GetModulePtr<FAssetRegistryModule>("AssetRegistry");
	if (AssetRegistryModule)
	{
		IAssetRegistry* AssetRegistry = AssetRegistryModule->TryGet();
		if (AssetRegistry)
		{
			AssetRegistry->OnAssetRemoved().RemoveAll(this);
			AssetRegistry->OnAssetRenamed().RemoveAll(this);
			AssetRegistry->OnAssetAdded().RemoveAll(this);
			AssetRegistry->OnFilesLoaded().RemoveAll(this);
		}
	}
}

void FPlacementModeModule::AddToRecentlyPlaced(const TArray<UObject *>& PlacedObjects, UActorFactory* FactoryUsed /* = NULL */)
{
	FString FactoryPath;
	if (FactoryUsed != NULL)
	{
		FactoryPath = FactoryUsed->GetPathName();
	}

	TArray< UObject* > FilteredPlacedObjects;
	for (UObject* PlacedObject : PlacedObjects)
	{
		// Don't include null placed objects that just have factories.
		if (PlacedObject == NULL)
		{
			continue;
		}

		// Don't add brush builders to the recently placed.
		if (PlacedObject->IsA(UBrushBuilder::StaticClass()))
		{
			continue;
		}

		FilteredPlacedObjects.Add(PlacedObject);
	}

	// Don't change the recently placed if nothing passed the filter.
	if (FilteredPlacedObjects.Num() == 0)
	{
		return;
	}

	bool Changed = false;
	for (int Index = 0; Index < FilteredPlacedObjects.Num(); Index++)
	{
		Changed |= RecentlyPlaced.Remove(FActorPlacementInfo(FilteredPlacedObjects[Index]->GetPathName(), FactoryPath)) > 0;
	}

	for (int Index = 0; Index < FilteredPlacedObjects.Num(); Index++)
	{
		if (FilteredPlacedObjects[Index] != NULL)
		{
			RecentlyPlaced.Insert(FActorPlacementInfo(FilteredPlacedObjects[Index]->GetPathName(), FactoryPath), 0);
			Changed = true;
		}
	}

	for (int Index = RecentlyPlaced.Num() - 1; Index >= 20; Index--)
	{
		RecentlyPlaced.RemoveAt(Index);
		Changed = true;
	}

	if (Changed)
	{
		TArray< FString > RecentlyPlacedAsStrings;
		for (int Index = 0; Index < RecentlyPlaced.Num(); Index++)
		{
			RecentlyPlacedAsStrings.Add(RecentlyPlaced[Index].ToString());
		}

		GConfig->SetArray(TEXT("PlacementMode"), TEXT("RecentlyPlaced"), RecentlyPlacedAsStrings, GEditorPerProjectIni);
		RecentlyPlacedChanged.Broadcast(RecentlyPlaced);
	}
}

void FPlacementModeModule::OnAssetRemoved(const FAssetData&)
{
	RecentlyPlacedChanged.Broadcast(RecentlyPlaced);
	AllPlaceableAssetsChanged.Broadcast();
}

void FPlacementModeModule::OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath)
{
	for (auto& RecentlyPlacedItem : RecentlyPlaced)
	{
		if (RecentlyPlacedItem.ObjectPath == OldObjectPath)
		{
			RecentlyPlacedItem.ObjectPath = AssetData.GetObjectPathString();
			break;
		}
	}

	RecentlyPlacedChanged.Broadcast(RecentlyPlaced);
	AllPlaceableAssetsChanged.Broadcast();
}

void FPlacementModeModule::OnAssetAdded(const FAssetData& AssetData)
{
	AllPlaceableAssetsChanged.Broadcast();
}

void FPlacementModeModule::OnInitialAssetsScanComplete()
{
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetRegistry.OnAssetAdded().AddRaw(this, &FPlacementModeModule::OnAssetAdded);
	AssetRegistry.OnFilesLoaded().RemoveAll(this);

	AllPlaceableAssetsChanged.Broadcast();
}

void FPlacementModeModule::AddToRecentlyPlaced(UObject* Asset, UActorFactory* FactoryUsed /* = NULL */)
{
	TArray< UObject* > Assets;
	Assets.Add(Asset);
	AddToRecentlyPlaced(Assets, FactoryUsed);
}

TSharedRef<SWidget> FPlacementModeModule::CreatePlacementModeBrowser(TSharedRef<SDockTab> ParentTab)
{
	return SNew(SPlacementModeTools, ParentTab);
}

bool FPlacementModeModule::RegisterPlacementCategory(const FPlacementCategoryInfo& Info)
{
	if (Categories.Contains(Info.UniqueHandle))
	{
		return false;
	}

	Categories.Add(Info.UniqueHandle, Info);
	PlacementModeCategoryListChanged.Broadcast();
	return true;
}

void FPlacementModeModule::UnregisterPlacementCategory(FName Handle)
{
	if (Categories.Remove(Handle))
	{
		PlacementModeCategoryListChanged.Broadcast();
	}
}

void FPlacementModeModule::GetSortedCategories(TArray<FPlacementCategoryInfo>& OutCategories) const
{
	TArray<FName> SortedNames;
	Categories.GenerateKeyArray(SortedNames);

	SortedNames.Sort([&](const FName& A, const FName& B) {
		return Categories[A].SortOrder < Categories[B].SortOrder;
	});

	OutCategories.Reset(Categories.Num());
	for (const FName& Name : SortedNames)
	{
		if (CategoryPermissionList->PassesFilter(Name))
		{
			OutCategories.Add(Categories[Name]);
		}
	}
}

TOptional<FPlacementModeID> FPlacementModeModule::RegisterPlaceableItem(FName CategoryName, const TSharedRef<FPlaceableItem>& InItem)
{
	FPlacementCategory* Category = Categories.Find(CategoryName);
	if (Category && !Category->CustomGenerator)
	{
		FPlacementModeID ID = CreateID(CategoryName);
		Category->Items.Add(ID.UniqueID, InItem);
		return ID;
	}
	return TOptional<FPlacementModeID>();
}

void FPlacementModeModule::UnregisterPlaceableItem(FPlacementModeID ID)
{
	FPlacementCategory* Category = Categories.Find(ID.Category);
	if (Category)
	{
		Category->Items.Remove(ID.UniqueID);
	}
}

bool FPlacementModeModule::RegisterPlaceableItemFilter(TPlaceableItemPredicate Predicate, FName OwnerName)
{
	TPlaceableItemPredicate& ExistingPredicate = PlaceableItemPredicates.FindOrAdd(OwnerName);
	if (!ExistingPredicate)
	{
		ExistingPredicate = MoveTemp(Predicate);
		PlaceableItemFilteringChanged.Broadcast();
		return true;
	}
	return false;
}

void FPlacementModeModule::UnregisterPlaceableItemFilter(FName OwnerName)
{
	if (PlaceableItemPredicates.Remove(OwnerName))
	{
		PlaceableItemFilteringChanged.Broadcast();
	}
}

void FPlacementModeModule::GetItemsForCategory(FName CategoryName, TArray<TSharedPtr<FPlaceableItem>>& OutItems) const
{
	const FPlacementCategory* Category = Categories.Find(CategoryName);
	if (Category)
	{
		for (auto& Pair : Category->Items)
		{
			if (PassesFilters(Pair.Value))
			{
				OutItems.Add(Pair.Value);
			}
		}
	}
}

void FPlacementModeModule::GetFilteredItemsForCategory(FName CategoryName, TArray<TSharedPtr<FPlaceableItem>>& OutItems, TFunctionRef<bool(const TSharedPtr<FPlaceableItem> &)> Filter) const
{
	const FPlacementCategory* Category = Categories.Find(CategoryName);
	if (Category)
	{
		for (auto& Pair : Category->Items)
		{
			if (PassesFilters(Pair.Value))
			{
				if (Filter(Pair.Value))
				{
					OutItems.Add(Pair.Value);
				}
			}
		}
	}
}

void FPlacementModeModule::RegenerateItemsForCategory(FName Category)
{
	if (Category == FBuiltInPlacementCategories::RecentlyPlaced())
	{
		RefreshRecentlyPlaced();
	}
	else if (Category == FBuiltInPlacementCategories::Volumes())
	{
		RefreshVolumes();
	}
	else if (Category == FBuiltInPlacementCategories::AllClasses())
	{
		RefreshAllPlaceableClasses();
	}

	PlacementModeCategoryRefreshed.Broadcast(Category);
}

void FPlacementModeModule::RefreshRecentlyPlaced()
{
	FPlacementCategory* Category = Categories.Find(FBuiltInPlacementCategories::RecentlyPlaced());
	if (!Category)
	{
		return;
	}

	Category->Items.Reset();


	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	for (const FActorPlacementInfo& RecentlyPlacedItem : RecentlyPlaced)
	{
		UObject* Asset = FindObject<UObject>(nullptr, *RecentlyPlacedItem.ObjectPath);

		// If asset is pending delete, it will not be marked as RF_Standalone, in which case we skip it
		if (Asset != nullptr && Asset->HasAnyFlags(RF_Standalone))
		{
			FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(RecentlyPlacedItem.ObjectPath));

			if (AssetData.IsValid())
			{
				UActorFactory* Factory = FindObject<UActorFactory>(nullptr, *RecentlyPlacedItem.Factory);
				if (Factory)
				{
					TSharedPtr<FPlaceableItem> Ptr = MakeShareable(new FPlaceableItem(Factory, AssetData));
					if (FString* FoundThumbnail = BasicShapeThumbnails.Find(RecentlyPlacedItem.ObjectPath))
					{
						Ptr->ClassThumbnailBrushOverride = FName(**FoundThumbnail);
						Ptr->bAlwaysUseGenericThumbnail = true;
						Ptr->AssetTypeColorOverride = GetBasicShapeColorOverride();
					}
					Category->Items.Add(CreateID(), Ptr);
				}
			}
		}
	}
}

void FPlacementModeModule::RefreshVolumes()
{
	FPlacementCategory* Category = Categories.Find(FBuiltInPlacementCategories::Volumes());
	if (!Category)
	{
		return;
	}

	Category->Items.Reset();

	// Add loaded classes
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		const UClass* Class = *ClassIt;

		if (!Class->HasAllClassFlags(CLASS_NotPlaceable) &&
			!Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) &&
			Class->IsChildOf(AVolume::StaticClass()) &&
			Class->ClassGeneratedBy == nullptr)
		{
			UActorFactory* Factory = GEditor->FindActorFactoryByClassForActorClass(UActorFactoryBoxVolume::StaticClass(), Class);
			Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(Factory, FAssetData(Class))));
		}
	}
}

void FPlacementModeModule::RefreshAllPlaceableClasses()
{
	// Unregister old stuff
	FPlacementCategory* Category = Categories.Find(FBuiltInPlacementCategories::AllClasses());
	if (!Category)
	{
		return;
	}

	Category->Items.Reset();

	// Manually add some special cases that aren't added below
	Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactoryEmptyActor::StaticClass())));
	Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactoryCharacter::StaticClass())));
	Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactoryPawn::StaticClass())));
	Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactoryBasicShape::StaticClass(), FAssetData(LoadObject<UStaticMesh>(nullptr, *UActorFactoryBasicShape::BasicCube.ToString())), FName("ClassThumbnail.Cube"), FName("ClassIcon.Cube"), GetBasicShapeColorOverride(), TOptional<int32>(), NSLOCTEXT("PlacementMode", "Cube", "Cube"))));
	Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactoryBasicShape::StaticClass(), FAssetData(LoadObject<UStaticMesh>(nullptr, *UActorFactoryBasicShape::BasicSphere.ToString())), FName("ClassThumbnail.Sphere"), FName("ClassIcon.Sphere"), GetBasicShapeColorOverride(), TOptional<int32>(), NSLOCTEXT("PlacementMode", "Sphere", "Sphere"))));
	Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactoryBasicShape::StaticClass(), FAssetData(LoadObject<UStaticMesh>(nullptr, *UActorFactoryBasicShape::BasicCylinder.ToString())), FName("ClassThumbnail.Cylinder"), FName("ClassIcon.Cylinder"), GetBasicShapeColorOverride(), TOptional<int32>(), NSLOCTEXT("PlacementMode", "Cylinder", "Cylinder"))));
	Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactoryBasicShape::StaticClass(), FAssetData(LoadObject<UStaticMesh>(nullptr, *UActorFactoryBasicShape::BasicCone.ToString())), FName("ClassThumbnail.Cone"), FName("ClassIcon.Cone"), GetBasicShapeColorOverride(), TOptional<int32>(), NSLOCTEXT("PlacementMode", "Cone", "Cone"))));
	Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(*UActorFactoryBasicShape::StaticClass(), FAssetData(LoadObject<UStaticMesh>(nullptr, *UActorFactoryBasicShape::BasicPlane.ToString())), FName("ClassThumbnail.Plane"), FName("ClassIcon.Plane"), GetBasicShapeColorOverride(), TOptional<int32>(), NSLOCTEXT("PlacementMode", "Plane", "Plane"))));

	// Make a map of UClasses to ActorFactories that support them
	const TArray< UActorFactory *>& ActorFactories = GEditor->ActorFactories;
	TMap<UClass*, UActorFactory*> ActorFactoryMap;
	for (int32 FactoryIdx = 0; FactoryIdx < ActorFactories.Num(); ++FactoryIdx)
	{
		UActorFactory* ActorFactory = ActorFactories[FactoryIdx];

		if (ActorFactory)
		{
			ActorFactoryMap.Add(ActorFactory->GetDefaultActorClass(FAssetData()), ActorFactory);
		}
	}

	FAssetData NoAssetData;
	FText UnusedErrorMessage;

	// Add loaded classes
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		// Don't offer skeleton classes
		bool bIsSkeletonClass = FKismetEditorUtilities::IsClassABlueprintSkeleton(*ClassIt);

		if (!ClassIt->HasAllClassFlags(CLASS_NotPlaceable) &&
			!ClassIt->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) &&
			ClassIt->IsChildOf(AActor::StaticClass()) &&
			(!ClassIt->IsChildOf(ABrush::StaticClass()) || ClassIt->IsChildOf(AVolume::StaticClass())) &&
			!bIsSkeletonClass)
		{
			UActorFactory* ActorFactory = ActorFactoryMap.FindRef(*ClassIt);

			const bool IsVolume = ClassIt->IsChildOf(AVolume::StaticClass());

			if (IsVolume)
			{
				ActorFactory = GEditor->FindActorFactoryByClassForActorClass(UActorFactoryBoxVolume::StaticClass(), *ClassIt);
			}
			else if (ActorFactory && !ActorFactory->CanCreateActorFrom(NoAssetData, UnusedErrorMessage))
			{
				continue;
			}

			Category->Items.Add(CreateID(), MakeShareable(new FPlaceableItem(ActorFactory, FAssetData(*ClassIt))));
		}
	}

	Category->Items.ValueSort([&](const TSharedPtr<FPlaceableItem>& A, const TSharedPtr<FPlaceableItem>& B) {
		return A->DisplayName.CompareTo(B->DisplayName) < 0;
		});
}

FGuid FPlacementModeModule::CreateID()
{
	return FGuid::NewGuid();
}

FPlacementModeID FPlacementModeModule::CreateID(FName InCategory)
{
	FPlacementModeID NewID;
	NewID.UniqueID = CreateID();
	NewID.Category = InCategory;
	return NewID;
}

bool FPlacementModeModule::PassesFilters(const TSharedPtr<FPlaceableItem>& Item) const
{
	if (PlaceableItemPredicates.Num() == 0)
	{
		return true;
	}

	for (auto& PredicatePair : PlaceableItemPredicates)
	{
		if (PredicatePair.Value(Item))
		{
			bool bPlaceable = true;
			UClass* AssetClass = Item->AssetData.GetClass();
			if (AssetClass == UClass::StaticClass())
			{
				UClass* Class = Cast<UClass>(Item->AssetData.GetAsset());

				bPlaceable = AssetSelectionUtils::IsClassPlaceable(Class);
			}
			else if (AssetClass && AssetClass->IsChildOf<UBlueprint>())
			{
				// For blueprints, attempt to determine placeability from its tag information

				FString TagValue;

				if (Item->AssetData.GetTagValue(FBlueprintTags::NativeParentClassPath, TagValue) && !TagValue.IsEmpty())
				{
					// If the native parent class can't be placed, neither can the blueprint
					UClass* NativeParentClass = UClass::TryFindTypeSlow<UClass>(FPackageName::ExportTextPathToObjectPath(TagValue));

					bPlaceable = AssetSelectionUtils::IsChildBlueprintPlaceable(NativeParentClass);
				}

				if (bPlaceable && Item->AssetData.GetTagValue(FBlueprintTags::ClassFlags, TagValue) && !TagValue.IsEmpty())
				{
					// Check to see if this class is placeable from its class flags

					const int32 NotPlaceableFlags = CLASS_NotPlaceable | CLASS_Deprecated | CLASS_Abstract;
					uint32 ClassFlags = FCString::Atoi(*TagValue);

					bPlaceable = (ClassFlags & NotPlaceableFlags) == CLASS_None;
				}
			}
			return bPlaceable;
		}
	}
	return false;
}

void FPlacementModeModule::OnCategoryPermissionListChanged()
{
	PlacementModeCategoryListChanged.Broadcast();
}
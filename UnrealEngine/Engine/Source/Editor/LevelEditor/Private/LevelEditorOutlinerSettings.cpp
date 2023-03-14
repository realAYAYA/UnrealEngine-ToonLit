// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelEditorOutlinerSettings.h"

#include "Filters/FilterBase.h"
#include "Filters/CustomClassFilterData.h"

#include "SceneOutlinerPublicTypes.h"
#include "IPlacementModeModule.h"

#include "Animation/SkeletalMeshActor.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/Brush.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/StaticMeshActor.h"
#include "InstancedFoliageActor.h"
#include "Landscape.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "Sound/AmbientSound.h"

#define LOCTEXT_NAMESPACE "LevelEditorOutlinerSettings"

void FLevelEditorOutlinerSettings::AddCustomFilter(TSharedRef<FFilterBase<SceneOutliner::FilterBarType>> InCustomFilter)
{
	CustomFilters.Add(InCustomFilter);
}

void FLevelEditorOutlinerSettings::AddCustomClassFilter(TSharedRef<FCustomClassFilterData> InCustomClassFilterData)
{
	TSharedRef<FCustomClassFilterData>* FoundFilter = CustomClassFilters.FindByPredicate([&InCustomClassFilterData](const TSharedPtr<FCustomClassFilterData> FilterData)
	{
		return FilterData->GetClassPathName() == InCustomClassFilterData->GetClassPathName();
	});

	// If a filter for the class already exists, just add the new categories to it
	// TODO: Not perfect, what if the user wants to use InCustomClassFilterData after if there is already a dupe?
	if(FoundFilter)
	{
		TArray<TSharedPtr<FFilterCategory>> Categories = InCustomClassFilterData->GetCategories();

		for(const TSharedPtr<FFilterCategory>& Category : Categories)
		{
			(*FoundFilter)->AddCategory(Category);
		}
	}
	else
	{
		CustomClassFilters.Add(InCustomClassFilterData);
	}
}

TSharedPtr<FFilterCategory> FLevelEditorOutlinerSettings::GetFilterCategory(const FName& CategoryName)
{
	if(TSharedPtr<FFilterCategory>* FoundCategory = FilterBarCategories.Find(CategoryName))
	{
		return *FoundCategory;
	}

	return nullptr;
}

void FLevelEditorOutlinerSettings::SetupBuiltInCategories()
{
	// First setup our unique built in categories
	TSharedPtr<FFilterCategory> CommonFiltersCategory = MakeShared<FFilterCategory>(LOCTEXT("CommonFiltersCategory", "Common"), FText::GetEmpty());
	FilterBarCategories.Add(FLevelEditorOutlinerBuiltInCategories::Common(), CommonFiltersCategory);

	TSharedPtr<FFilterCategory> AudioFiltersCategory = MakeShared<FFilterCategory>(LOCTEXT("AudioFiltersCategory", "Audio"), FText::GetEmpty());
	FilterBarCategories.Add(FLevelEditorOutlinerBuiltInCategories::Audio(), AudioFiltersCategory);

	TSharedPtr<FFilterCategory> AnimationFiltersCategory = MakeShared<FFilterCategory>(LOCTEXT("AnimationFiltersCategory", "Animation"), FText::GetEmpty());
	FilterBarCategories.Add(FLevelEditorOutlinerBuiltInCategories::Animation(), AnimationFiltersCategory);
	
	TSharedPtr<FFilterCategory> GeometryFiltersCategory = MakeShared<FFilterCategory>(LOCTEXT("GeometryFiltersCategory", "Geometry"), FText::GetEmpty());
	FilterBarCategories.Add(FLevelEditorOutlinerBuiltInCategories::Geometry(), GeometryFiltersCategory);

	TSharedPtr<FFilterCategory> EnvironmentFiltersCategory = MakeShared<FFilterCategory>(LOCTEXT("EnvironmentFiltersCategory", "Environment"), FText::GetEmpty());
	FilterBarCategories.Add(FLevelEditorOutlinerBuiltInCategories::Environment(), EnvironmentFiltersCategory);
	
	TSharedPtr<FFilterCategory> VPFiltersCategory = MakeShared<FFilterCategory>(LOCTEXT("VPFiltersCategory", "Virtual Production"), FText::GetEmpty());
	FilterBarCategories.Add(FLevelEditorOutlinerBuiltInCategories::VirtualProduction(), VPFiltersCategory);
	
	// Now convert some of the built in placement mode categories we want to filter categories and add them

	PlacementToFilterCategoryMap.Add(FBuiltInPlacementCategories::Basic(), FLevelEditorOutlinerBuiltInCategories::Basic());
	TSharedPtr<FFilterCategory> BasicFilterCategory = MakeShared<FFilterCategory>(LOCTEXT("BasicFilterCategory", "Basic"), FText::GetEmpty());
	FilterBarCategories.Add(FLevelEditorOutlinerBuiltInCategories::Basic(), BasicFilterCategory);

	PlacementToFilterCategoryMap.Add(FBuiltInPlacementCategories::Lights(), FLevelEditorOutlinerBuiltInCategories::Lights());
	TSharedPtr<FFilterCategory> LightsFilterCategory = MakeShared<FFilterCategory>(LOCTEXT("LightsFilterCategory", "Lights"), FText::GetEmpty());
	FilterBarCategories.Add(FLevelEditorOutlinerBuiltInCategories::Lights(), LightsFilterCategory);

	PlacementToFilterCategoryMap.Add(FBuiltInPlacementCategories::Visual(), FLevelEditorOutlinerBuiltInCategories::Visual());
	TSharedPtr<FFilterCategory> VisualFilterCategory = MakeShared<FFilterCategory>(LOCTEXT("VisualFilterCategory", "Visual"), FText::GetEmpty());
	FilterBarCategories.Add(FLevelEditorOutlinerBuiltInCategories::Visual(), VisualFilterCategory);

	PlacementToFilterCategoryMap.Add(FBuiltInPlacementCategories::Volumes(), FLevelEditorOutlinerBuiltInCategories::Volumes());
	TSharedPtr<FFilterCategory> VolumesFilterCategory = MakeShared<FFilterCategory>(LOCTEXT("VolumnesFilterCategory", "Volumes"), FText::GetEmpty());
	FilterBarCategories.Add(FLevelEditorOutlinerBuiltInCategories::Volumes(), VolumesFilterCategory);
}

void FLevelEditorOutlinerSettings::CreateDefaultFilters()
{
	// First we will add all items registered to the place actors panel as filters
	IPlacementModeModule& PlacementModeModule = IPlacementModeModule::Get();
	
	// Get all the Categories
	TArray<FPlacementCategoryInfo> Categories;
	PlacementModeModule.GetSortedCategories(Categories);

	// Remove the Recently Placed, Shapes (don't make sense) and All Classes (too much bloat) categories from the filters
	Categories.RemoveAll([](const FPlacementCategoryInfo& Category)
	{
		return Category.UniqueHandle == FBuiltInPlacementCategories::RecentlyPlaced() || Category.UniqueHandle == FBuiltInPlacementCategories::AllClasses()
			|| Category.UniqueHandle == FBuiltInPlacementCategories::Shapes();
	});

	auto FindOrAddClassFilter = [this](UClass* Class, TSharedPtr<FFilterCategory> FilterCategory)
	{
		// If the underlying class already exists, just add this category to it
		TSharedRef<FCustomClassFilterData>* ExistingClassData = CustomClassFilters.FindByPredicate([Class](const TSharedPtr<FCustomClassFilterData> FilterData)
		{
			return FilterData->GetClassPathName() == Class->GetClassPathName();
		});
		
		if(ExistingClassData)
		{
			(*ExistingClassData)->AddCategory(FilterCategory);
		}
		else
		{
			TSharedRef<FCustomClassFilterData> NewClassData = MakeShared<FCustomClassFilterData>(Class, FilterCategory, FLinearColor::White);
			CustomClassFilters.Add(NewClassData);
		}
	};
	
	for (const FPlacementCategoryInfo& Category : Categories)
	{
		// Make an FFilterCategory using the current PlacementCategory if it doesn't already exist (built in)
		TSharedPtr<FFilterCategory> FilterCategory;
		if(TSharedPtr<FFilterCategory>* FoundCategory = FilterBarCategories.Find(Category.UniqueHandle))
		{
			FilterCategory = *FoundCategory;
		}
		else
		{
			FilterCategory = MakeShared<FFilterCategory>(Category.DisplayName, FText::GetEmpty());
		}

		FName CategoryName = Category.UniqueHandle;

		// If it is a built in placement mode category, convert the name to a built in filter category
		if(FName* MappedCategoryName = PlacementToFilterCategoryMap.Find(CategoryName))
		{
			CategoryName = *MappedCategoryName;
		}

		// Add the category to our list
		FilterBarCategories.Add(Category.UniqueHandle, FilterCategory);

		// Get all the items belonging to the current category
		TArray<TSharedPtr<FPlaceableItem>> Items;
		PlacementModeModule.RegenerateItemsForCategory(Category.UniqueHandle);
		PlacementModeModule.GetItemsForCategory(Category.UniqueHandle, Items);

		// Add each item to as a filter
		for(TSharedPtr<FPlaceableItem>& Item : Items)
		{
			// Get the underlying class from the Actor belonging to this item
			const bool bIsClass = Item->AssetData.GetClass() == UClass::StaticClass();
			const bool bIsActor = bIsClass ? CastChecked<UClass>(Item->AssetData.GetAsset())->IsChildOf(AActor::StaticClass()) : false;
			AActor* DefaultActor = nullptr;
			if (Item->Factory != nullptr)
			{
				DefaultActor = Item->Factory->GetDefaultActor(Item->AssetData);
			}
			else if (bIsActor)
			{
				DefaultActor = CastChecked<AActor>(CastChecked<UClass>(Item->AssetData.GetAsset())->ClassDefaultObject);
			
			}
			if(!DefaultActor)
			{
				continue;
			}
			UClass* Class = DefaultActor->GetClass();

			FindOrAddClassFilter(Class, FilterCategory);
		}
	}

	// Now we add some custom filterable types that are not a part of the place actors panel

	TSharedPtr<FFilterCategory>* CommonFilterCategory = FilterBarCategories.Find(FLevelEditorOutlinerBuiltInCategories::Common());

	if(CommonFilterCategory)
	{
		FindOrAddClassFilter(AStaticMeshActor::StaticClass(), *CommonFilterCategory);
		FindOrAddClassFilter(ALevelInstance::StaticClass(), *CommonFilterCategory);
		FindOrAddClassFilter(ASkeletalMeshActor::StaticClass(), *CommonFilterCategory);
		FindOrAddClassFilter(UBlueprint::StaticClass(), *CommonFilterCategory);
		FindOrAddClassFilter(APostProcessVolume::StaticClass(), *CommonFilterCategory);
	}

	TSharedPtr<FFilterCategory>* GeometryFilterCategory = FilterBarCategories.Find(FLevelEditorOutlinerBuiltInCategories::Geometry());

	if(GeometryFilterCategory)
	{
		FindOrAddClassFilter(AStaticMeshActor::StaticClass(), *GeometryFilterCategory);
		FindOrAddClassFilter(ABrush::StaticClass(), *GeometryFilterCategory);
	}

	TSharedPtr<FFilterCategory>* AnimationFilterCategory = FilterBarCategories.Find(FLevelEditorOutlinerBuiltInCategories::Animation());

	if(AnimationFilterCategory)
	{
		FindOrAddClassFilter(ASkeletalMeshActor::StaticClass(), *AnimationFilterCategory);
	}

	TSharedPtr<FFilterCategory>* EnvironmentFilterCategory = FilterBarCategories.Find(FLevelEditorOutlinerBuiltInCategories::Environment());

	if(EnvironmentFilterCategory)
	{
		FindOrAddClassFilter(AExponentialHeightFog::StaticClass(), *EnvironmentFilterCategory);
		FindOrAddClassFilter(AInstancedFoliageActor::StaticClass(), *EnvironmentFilterCategory);
		FindOrAddClassFilter(ASkyAtmosphere::StaticClass(), *EnvironmentFilterCategory);
		FindOrAddClassFilter(AVolumetricCloud::StaticClass(), *EnvironmentFilterCategory);
	}
	
	TSharedPtr<FFilterCategory>* AudioFilterCategory = FilterBarCategories.Find(FLevelEditorOutlinerBuiltInCategories::Audio());

	if(AudioFilterCategory)
	{
		FindOrAddClassFilter(AAmbientSound::StaticClass(), *AudioFilterCategory);
	}
}

void FLevelEditorOutlinerSettings::GetOutlinerFilters(FSceneOutlinerFilterBarOptions& OutFilterBarOptions)
{
	// Sort the type filters by name
	CustomClassFilters.Sort([](const TSharedRef<FCustomClassFilterData>& ClassA, const TSharedRef<FCustomClassFilterData>& ClassB)
		{
			return ClassA->GetName().CompareTo(ClassB->GetName()) < 0;
		});
	
	OutFilterBarOptions.CustomClassFilters.Append(CustomClassFilters);
	OutFilterBarOptions.CustomFilters.Append(CustomFilters);
}

#undef LOCTEXT_NAMESPACE

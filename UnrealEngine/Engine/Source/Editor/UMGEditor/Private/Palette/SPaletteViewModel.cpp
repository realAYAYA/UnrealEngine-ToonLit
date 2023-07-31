// Copyright Epic Games, Inc. All Rights Reserved.

#include "Palette/SPaletteViewModel.h"
#include "Palette/SPaletteView.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "WidgetBlueprint.h"
#include "Editor.h"

#if WITH_EDITOR
	#include "Styling/AppStyle.h"
#endif // WITH_EDITOR

#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"
#include "EditorClassUtils.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "DragDrop/WidgetTemplateDragDropOp.h"

#include "Templates/WidgetTemplateClass.h"
#include "Templates/WidgetTemplateBlueprintClass.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "WidgetBlueprintEditorUtils.h"
#include "Misc/NamePermissionList.h"

#include "Settings/ContentBrowserSettings.h"
#include "Settings/WidgetDesignerSettings.h"
#include "UMGEditorProjectSettings.h"
#include "WidgetPaletteFavorites.h"

#define LOCTEXT_NAMESPACE "UMG"

namespace UE::Editor::SPaletteViewModel::Private
{
	/** Helper class to perform path based filtering for unloaded BP's */
	class FUnloadedBlueprintData : public IUnloadedBlueprintData
	{
	public:
		FUnloadedBlueprintData(const FAssetData& InAssetData)
			:ClassPath()
			,ClassFlags(CLASS_None)
			,bIsNormalBlueprintType(false)
		{
			ClassName = MakeShared<FString>(InAssetData.AssetName.ToString());

			FString GeneratedClassPath;
			const UClass* AssetClass = InAssetData.GetClass();
			if (AssetClass && AssetClass->IsChildOf(UBlueprintGeneratedClass::StaticClass()))
			{
				ClassPath = InAssetData.ToSoftObjectPath().GetAssetPathString();
			}
			else if (InAssetData.GetTagValue(FBlueprintTags::GeneratedClassPath, GeneratedClassPath))
			{
				ClassPath = FTopLevelAssetPath(*FPackageName::ExportTextPathToObjectPath(GeneratedClassPath));
			}

			FEditorClassUtils::GetImplementedInterfaceClassPathsFromAsset(InAssetData, ImplementedInterfaces);
		}

		virtual ~FUnloadedBlueprintData()
		{
		}

		// Begin IUnloadedBlueprintData interface
		virtual bool HasAnyClassFlags(uint32 InFlagsToCheck) const
		{
			return (ClassFlags & InFlagsToCheck) != 0;
		}

		virtual bool HasAllClassFlags(uint32 InFlagsToCheck) const
		{
			return ((ClassFlags & InFlagsToCheck) == InFlagsToCheck);
		}

		virtual void SetClassFlags(uint32 InFlags)
		{
			ClassFlags = InFlags;
		}

		virtual bool ImplementsInterface(const UClass* InInterface) const
		{
			FString InterfacePath = InInterface->GetPathName();
			for (const FString& ImplementedInterface : ImplementedInterfaces)
			{
				if (ImplementedInterface == InterfacePath)
				{
					return true;
				}
			}

			return false;
		}

		virtual bool IsChildOf(const UClass* InClass) const
		{
			return false;
		}

		virtual bool IsA(const UClass* InClass) const
		{
			// Unloaded blueprint classes should always be a BPGC, so this just checks against the expected type.
			return UBlueprintGeneratedClass::StaticClass()->UObject::IsA(InClass);
		}

		virtual const UClass* GetClassWithin() const
		{
			return nullptr;
		}

		virtual const UClass* GetNativeParent() const
		{
			return nullptr;
		}

		virtual void SetNormalBlueprintType(bool bInNormalBPType)
		{
			bIsNormalBlueprintType = bInNormalBPType;
		}

		virtual bool IsNormalBlueprintType() const
		{
			return bIsNormalBlueprintType;
		}

		virtual TSharedPtr<FString> GetClassName() const
		{
			return ClassName;
		}

		virtual FName GetClassPath() const
		{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
			return ClassPath.ToFName();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}

		virtual FTopLevelAssetPath GetClassPathName() const
		{
			return ClassPath;
		}
		// End IUnloadedBlueprintData interface

	private:
		TSharedPtr<FString> ClassName;
		FTopLevelAssetPath ClassPath;
		uint32 ClassFlags;
		TArray<FString> ImplementedInterfaces;
		bool bIsNormalBlueprintType;
	};
}

FWidgetTemplateViewModel::FWidgetTemplateViewModel()
	: FavortiesViewModel(nullptr),
	bIsFavorite(false)
{
}

FText FWidgetTemplateViewModel::GetName() const
{
	return Template->Name;
}

bool FWidgetTemplateViewModel::IsTemplate() const
{
	return true;
}

void FWidgetTemplateViewModel::GetFilterStrings(TArray<FString>& OutStrings) const
{
	Template->GetFilterStrings(OutStrings);
}

TSharedRef<ITableRow> FWidgetTemplateViewModel::BuildRow(const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FWidgetViewModel>>, OwnerTable)
		.Padding(2.0f)
		.OnDragDetected(this, &FWidgetTemplateViewModel::OnDraggingWidgetTemplateItem)
		[
			SNew(SPaletteViewItem, SharedThis(this))
			.HighlightText(FavortiesViewModel, &FWidgetCatalogViewModel::GetSearchText)
		];
}

FReply FWidgetTemplateViewModel::OnDraggingWidgetTemplateItem(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Handled().BeginDragDrop(FWidgetTemplateDragDropOp::New(Template));
}

void FWidgetTemplateViewModel::AddToFavorites()
{
	bIsFavorite = true;
	FavortiesViewModel->AddToFavorites(this);
}

void FWidgetTemplateViewModel::RemoveFromFavorites()
{
	bIsFavorite = false;
	FavortiesViewModel->RemoveFromFavorites(this);
}

TSharedRef<ITableRow> FWidgetHeaderViewModel::BuildRow(const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FWidgetViewModel>>, OwnerTable)
		.Style(FAppStyle::Get(), "UMGEditor.PaletteHeader")
		.Padding(5.0f)
		.ShowSelection(false)
		[
			SNew(STextBlock)
			.TransformPolicy(ETextTransformPolicy::ToUpper)
			.Text(GroupName)
			.Font(FAppStyle::Get().GetFontStyle("SmallFontBold"))
		];
}

void FWidgetHeaderViewModel::GetChildren(TArray< TSharedPtr<FWidgetViewModel> >& OutChildren)
{
	for (TSharedPtr<FWidgetViewModel>& Child : Children)
	{
		OutChildren.Add(Child);
	}
}

FWidgetCatalogViewModel::FWidgetCatalogViewModel(TSharedPtr<FWidgetBlueprintEditor> InBlueprintEditor)
	: bRebuildRequested(true)
{
	BlueprintEditor = InBlueprintEditor;

	FavoriteHeader = MakeShareable(new FWidgetHeaderViewModel());
	FavoriteHeader->GroupName = LOCTEXT("Favorites", "Favorites");
}

void FWidgetCatalogViewModel::RegisterToEvents()
{
	// Register for events that can trigger a palette rebuild
	GEditor->OnBlueprintReinstanced().AddRaw(this, &FWidgetCatalogViewModel::OnBlueprintReinstanced);
	FEditorDelegates::OnAssetsDeleted.AddSP(this, &FWidgetCatalogViewModel::HandleOnAssetsDeleted);
	FCoreUObjectDelegates::ReloadCompleteDelegate.AddSP(this, &FWidgetCatalogViewModel::OnReloadComplete);

	// register for any objects replaced
	FCoreUObjectDelegates::OnObjectsReplaced.AddRaw(this, &FWidgetCatalogViewModel::OnObjectsReplaced);

	// Register for favorite list update to handle the case where a favorite is added in another window of the UMG Designer
	UWidgetPaletteFavorites* Favorites = GetDefault<UWidgetDesignerSettings>()->Favorites;
	Favorites->OnFavoritesUpdated.AddSP(this, &FWidgetCatalogViewModel::OnFavoritesUpdated);
}

FWidgetCatalogViewModel::~FWidgetCatalogViewModel()
{
	GEditor->OnBlueprintReinstanced().RemoveAll(this);
	FEditorDelegates::OnAssetsDeleted.RemoveAll(this);
	FCoreUObjectDelegates::ReloadCompleteDelegate.RemoveAll(this);
	FCoreUObjectDelegates::OnObjectsReplaced.RemoveAll(this);

	UWidgetPaletteFavorites* Favorites = GetDefault<UWidgetDesignerSettings>()->Favorites;
	Favorites->OnFavoritesUpdated.RemoveAll(this);
}

void FWidgetCatalogViewModel::Update()
{
	if (bRebuildRequested)
	{
		OnUpdating.Broadcast();
		BuildWidgetList();
		bRebuildRequested = false;
		OnUpdated.Broadcast();
	}
}

UWidgetBlueprint* FWidgetCatalogViewModel::GetBlueprint() const
{
	if (BlueprintEditor.IsValid())
	{
		UBlueprint* BP = BlueprintEditor.Pin()->GetBlueprintObj();
		return Cast<UWidgetBlueprint>(BP);
	}

	return NULL;
}

void FWidgetCatalogViewModel::BuildWidgetList()
{
	// Clear the current list of view models and categories
	WidgetViewModels.Reset();
	WidgetTemplateCategories.Reset();

	// Generate a list of templates
	BuildClassWidgetList();

	// Clear the Favorite section
	bool bHasFavorites = FavoriteHeader->Children.Num() != 0;
	FavoriteHeader->Children.Reset();
	
	// Copy of the list of favorites to be able to do some cleanup in the real list
	UWidgetPaletteFavorites* FavoritesPalette = GetDefault<UWidgetDesignerSettings>()->Favorites;
	TArray<FString> FavoritesList = FavoritesPalette->GetFavorites();

	// For each entry in the category create a view model for the widget template
	for ( auto& Entry : WidgetTemplateCategories )
	{
		BuildWidgetTemplateCategory(Entry.Key, Entry.Value, FavoritesList);
	}

	// Remove all Favorites that may be left in the list.Typically happening when the list of favorite contains widget that were deleted since the last opening.
	for (const FString& favoriteName : FavoritesList)
	{
		FavoritesPalette->Remove(favoriteName);
	}

	// Sort the view models by name
	WidgetViewModels.Sort([] (TSharedPtr<FWidgetViewModel> L, TSharedPtr<FWidgetViewModel> R) { return R->GetName().CompareTo(L->GetName()) > 0; });

	// Add the Favorite section at the top
	if (FavoriteHeader->Children.Num() != 0)
	{
		// We force expansion of the favorite header when we add favorites for the first time.
		FavoriteHeader->SetForceExpansion(!bHasFavorites);
		FavoriteHeader->Children.Sort([](TSharedPtr<FWidgetViewModel> L, TSharedPtr<FWidgetViewModel> R) { return R->GetName().CompareTo(L->GetName()) > 0; });
		WidgetViewModels.Insert(FavoriteHeader, 0);
	}
	
	// Take the Advanced Section, and put it at the end.
	TSharedPtr<FWidgetViewModel>* advancedSectionPtr = WidgetViewModels.FindByPredicate([](TSharedPtr<FWidgetViewModel> widget) {return widget->GetName().CompareTo(LOCTEXT("Advanced", "Advanced")) == 0; });
	if (advancedSectionPtr)
	{
		TSharedPtr<FWidgetViewModel> advancedSection = *advancedSectionPtr;
		WidgetViewModels.Remove(advancedSection);
		WidgetViewModels.Push(advancedSection);
	}
}

void FWidgetCatalogViewModel::BuildClassWidgetList()
{
	static const FName DevelopmentStatusKey(TEXT("DevelopmentStatus"));
	static const FString InvalidCategoryName("InvalidCategoryName");

	TMap<FName, TSubclassOf<UUserWidget>> LoadedWidgetBlueprintClassesByName;
	TMap<FName, TSubclassOf<UBlueprintGeneratedClass>> LoadedGeneratedBlueprintClassesByName;

	auto ActiveWidgetBlueprintClass = GetBlueprint()->GeneratedClass;
	FName ActiveWidgetBlueprintClassName = ActiveWidgetBlueprintClass->GetFName();

	UUMGEditorProjectSettings* UMGEditorProjectSettings = GetMutableDefault<UUMGEditorProjectSettings>();
	bool bUseEditorConfigPaletteFiltering = UMGEditorProjectSettings->bUseEditorConfigPaletteFiltering;
	const FNamePermissionList& AllowedPaletteCategories = UMGEditorProjectSettings->GetAllowedPaletteCategories();
	const FPathPermissionList& AllowedPaletteWidgets = UMGEditorProjectSettings->GetAllowedPaletteWidgets();
	const TArray<FSoftClassPath>& WidgetClassesToHide = UMGEditorProjectSettings->WidgetClassesToHide;

	// Since allowing all widgets in a category can be excessive when using permission lists, ensure widgets pass class viewing filters
	FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");
	const TSharedPtr<IClassViewerFilter>& GlobalClassFilter = ClassViewerModule.GetGlobalClassViewerFilter();
	TSharedRef<FClassViewerFilterFuncs> ClassFilterFuncs = ClassViewerModule.CreateFilterFuncs();
	FClassViewerInitializationOptions ClassViewerOptions = {};

	auto WidgetPassesConfigFiltering = [&](const FAssetData& InWidgetAssetData, const FString& InCategoryName, TWeakObjectPtr<UClass> InWidgetClass)
	{
		if (AllowedPaletteWidgets.PassesFilter(InWidgetAssetData.GetObjectPathString()))
		{
			return true;
		}

		if (AllowedPaletteCategories.PassesFilter(*InCategoryName) && GlobalClassFilter.IsValid())
		{
			if (InWidgetClass.IsValid())
			{
				return GlobalClassFilter->IsClassAllowed(ClassViewerOptions, InWidgetClass.Get(), ClassFilterFuncs);
			}
			else if (InWidgetAssetData.IsValid())
			{
				using namespace UE::Editor::SPaletteViewModel::Private;
				TSharedRef<FUnloadedBlueprintData> UnloadedBlueprint = MakeShared<FUnloadedBlueprintData>(InWidgetAssetData);
				return GlobalClassFilter->IsUnloadedClassAllowed(ClassViewerOptions, UnloadedBlueprint, ClassFilterFuncs);
			}
		}

		return false;
	};

	// Locate all UWidget classes from code and loaded widget BPs
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* WidgetClass = *ClassIt;

		if (!FWidgetBlueprintEditorUtils::IsUsableWidgetClass(WidgetClass))
		{
			continue;
		}

		if (WidgetClass->HasAnyClassFlags(CLASS_HideDropDown))
		{
			continue;
		}

		// Initialize AssetData for checking PackagePath
		FAssetData WidgetAssetData = FAssetData(WidgetClass);

		// Excludes engine content if user sets it to false
		if (!GetDefault<UContentBrowserSettings>()->GetDisplayEngineFolder() || !GetDefault<UUMGEditorProjectSettings>()->bShowWidgetsFromEngineContent)
		{
			if (WidgetAssetData.PackagePath.ToString().Find(TEXT("/Engine")) == 0)
			{
				continue;
			}
		}

		// Excludes developer content if user sets it to false
		if (!GetDefault<UContentBrowserSettings>()->GetDisplayDevelopersFolder() || !GetDefault<UUMGEditorProjectSettings>()->bShowWidgetsFromDeveloperContent)
		{
			if (WidgetAssetData.PackagePath.ToString().Find(TEXT("/Game/Developers")) == 0)
			{
				continue;
			}
		}

		if (bUseEditorConfigPaletteFiltering)
		{
			if (!WidgetPassesConfigFiltering(WidgetAssetData, WidgetClass->GetDefaultObject<UWidget>()->GetPaletteCategory().ToString(), WidgetClass))
			{
				continue;
			}
		}
		else
		{
			// Excludes this widget if it is on the hide list
			bool bIsOnList = false;
			for (FSoftClassPath Widget : WidgetClassesToHide)
			{
				if (WidgetAssetData.GetObjectPathString().Find(Widget.ToString()) == 0)
				{
					bIsOnList = true;
					break;
				}
			}
			if (bIsOnList)
			{
				continue;
			}
		}

		const bool bIsSameClass = WidgetClass->GetFName() == ActiveWidgetBlueprintClassName;

		// Check that the asset that generated this class is valid (necessary b/c of a larger issue wherein force delete does not wipe the generated class object)
		if ( bIsSameClass )
		{
			continue;
		}

		if (WidgetClass->IsChildOf(UUserWidget::StaticClass()))
		{
			if ( WidgetClass->ClassGeneratedBy )
			{
				// Track the widget blueprint classes that are already loaded
				LoadedWidgetBlueprintClassesByName.Add(WidgetClass->ClassGeneratedBy->GetFName()) = WidgetClass;
			}
		}
		else if (Cast<UBlueprintGeneratedClass>(WidgetClass))
		{
			// Note: Don't check IsChildOf above, since superstruct of BPGC is the C++ class
			LoadedGeneratedBlueprintClassesByName.Add(WidgetClass->GetFName()) = WidgetClass;
			TSharedPtr<FWidgetTemplateClass> Template = MakeShareable(new FWidgetTemplateClass(WidgetClass));
			AddWidgetTemplate(Template);
		}
		else
		{
			TSharedPtr<FWidgetTemplateClass> Template = MakeShareable(new FWidgetTemplateClass(WidgetClass));
			AddWidgetTemplate(Template);
		}

		//TODO UMG does not prevent deep nested circular references
	}

	// Locate all widget BP assets (include unloaded)
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> AllBPsAssetData;
	AssetRegistryModule.Get().GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), AllBPsAssetData, true);

	for (FAssetData& BPAssetData : AllBPsAssetData)
	{
		// Blueprints get the class type actions for their parent native class - this avoids us having to load the blueprint
		UClass* ParentClass = nullptr;
		FString ParentClassName;
		if (!BPAssetData.GetTagValue(FBlueprintTags::NativeParentClassPath, ParentClassName))
		{
			BPAssetData.GetTagValue(FBlueprintTags::ParentClassPath, ParentClassName);
		}
		if (!ParentClassName.IsEmpty())
		{
			ParentClass = UClass::TryFindTypeSlow<UClass>(FPackageName::ExportTextPathToObjectPath(ParentClassName));
			// UUserWidgets have their own loading section, and we don't want to process any blueprints that don't have UWidget parents
			if (ParentClass)
			{
				if (!ParentClass->IsChildOf(UWidget::StaticClass()) || ParentClass->IsChildOf(UUserWidget::StaticClass()))
				{
					continue;
				}
			}
		}

		if (!FilterAssetData(BPAssetData))
		{
			// If this object isn't currently loaded, add it to the palette view
			if (BPAssetData.ToSoftObjectPath().ResolveObject() == nullptr)
			{
				if (bUseEditorConfigPaletteFiltering)
				{
					// Still check the soft object path even if we can't check category due to BP asset being unloaded
					if (!WidgetPassesConfigFiltering(BPAssetData, InvalidCategoryName, nullptr))
					{
						continue;
					}
				}

				auto Template = MakeShareable(new FWidgetTemplateClass(BPAssetData, nullptr));
				AddWidgetTemplate(Template);
			}
		}
	}

	TArray<FAssetData> AllWidgetBPsAssetData;
	AssetRegistryModule.Get().GetAssetsByClass(UWidgetBlueprint::StaticClass()->GetClassPathName(), AllWidgetBPsAssetData, true);

	FName ActiveWidgetBlueprintName = ActiveWidgetBlueprintClass->ClassGeneratedBy->GetFName();
	for (FAssetData& WidgetBPAssetData : AllWidgetBPsAssetData)
	{
		// Excludes the blueprint you're currently in
		if (WidgetBPAssetData.AssetName == ActiveWidgetBlueprintName)
		{
			continue;
		}

		if (!FilterAssetData(WidgetBPAssetData))
		{
			// If the blueprint generated class was found earlier, pass it to the template
			TSubclassOf<UUserWidget> WidgetBPClass = nullptr;
			auto LoadedWidgetBPClass = LoadedWidgetBlueprintClassesByName.Find(WidgetBPAssetData.AssetName);
			if (LoadedWidgetBPClass)
			{
				WidgetBPClass = *LoadedWidgetBPClass;
			}

			if (bUseEditorConfigPaletteFiltering)
			{
				FText Category = FWidgetTemplateBlueprintClass(WidgetBPAssetData, WidgetBPClass).GetCategory();
				if (!WidgetPassesConfigFiltering(WidgetBPAssetData, Category.ToString(), WidgetBPClass))
				{
					continue;
				}
			}
			else
			{
				// Excludes this widget if it is on the hide list
				bool bIsOnList = false;
				for (FSoftClassPath Widget : WidgetClassesToHide)
				{
					if (Widget.ToString().Find(WidgetBPAssetData.GetObjectPathString()) == 0)
					{
						bIsOnList = true;
						break;
					}
				}
				if (bIsOnList)
				{
					continue;
				}
			}

			uint32 BPFlags = WidgetBPAssetData.GetTagValueRef<uint32>(FBlueprintTags::ClassFlags);
			if ((BPFlags & (CLASS_Hidden | CLASS_HideDropDown | CLASS_Deprecated | CLASS_Abstract | CLASS_NewerVersionExists)) == 0)
			{
				auto Template = MakeShareable(new FWidgetTemplateBlueprintClass(WidgetBPAssetData, WidgetBPClass));

				AddWidgetTemplate(Template);
			}
		}
	}


	TArray<FAssetData> AllGeneratedBPsAssetData;
	AssetRegistryModule.Get().GetAssetsByClass(UBlueprintGeneratedClass::StaticClass()->GetClassPathName(), AllGeneratedBPsAssetData, true);

	// Search generated class assets if using palette filtering, only do this with palette filtering since otherwise searching all generated BP's would be expensive
	if (bUseEditorConfigPaletteFiltering)
	{
		for (FAssetData& GeneratedBPAssetData : AllGeneratedBPsAssetData)
		{
			// Excludes the blueprint you're currently in
			if (GeneratedBPAssetData.AssetName == ActiveWidgetBlueprintName)
			{
				continue;
			}

			if (!FilterAssetData(GeneratedBPAssetData) 
				&& AllowedPaletteWidgets.PassesFilter(GeneratedBPAssetData.GetObjectPathString()) 
				&& !LoadedGeneratedBlueprintClassesByName.Contains(GeneratedBPAssetData.AssetName))
			{
				uint32 BPFlags = GeneratedBPAssetData.GetTagValueRef<uint32>(FBlueprintTags::ClassFlags);
				if ((BPFlags & (CLASS_Hidden | CLASS_HideDropDown | CLASS_Deprecated | CLASS_Abstract | CLASS_NewerVersionExists)) == 0)
				{
					bool bIsBlueprintGeneratedClass = true;
					auto Template = MakeShareable(new FWidgetTemplateBlueprintClass(GeneratedBPAssetData, nullptr, bIsBlueprintGeneratedClass));

					AddWidgetTemplate(Template);
				}
			}
		}
	}
}

bool FWidgetCatalogViewModel::FilterAssetData(FAssetData &InAssetData)
{
	// Excludes engine content if user sets it to false
	if (!GetDefault<UContentBrowserSettings>()->GetDisplayEngineFolder() || !GetDefault<UUMGEditorProjectSettings>()->bShowWidgetsFromEngineContent)
	{
		if (InAssetData.PackagePath.ToString().Find(TEXT("/Engine")) == 0)
		{
			return true;
		}
	}

	// Excludes developer content if user sets it to false
	if (!GetDefault<UContentBrowserSettings>()->GetDisplayDevelopersFolder() || !GetDefault<UUMGEditorProjectSettings>()->bShowWidgetsFromDeveloperContent)
	{
		if (InAssetData.PackagePath.ToString().Find(TEXT("/Game/Developers")) == 0)
		{
			return true;
		}
	}
	return false;
}

void FWidgetCatalogViewModel::AddWidgetTemplate(TSharedPtr<FWidgetTemplate> Template)
{
	FString Category = Template->GetCategory().ToString();

	// Hide user specific categories
	TArray<FString> CategoriesToHide = GetDefault<UUMGEditorProjectSettings>()->CategoriesToHide;
	for (FString CategoryName : CategoriesToHide)
	{
		if (Category == CategoryName)
		{
			return;
		}
	}
	WidgetTemplateArray& Group = WidgetTemplateCategories.FindOrAdd(Category);
	Group.Add(Template);
}

void FWidgetCatalogViewModel::OnObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap)
{
}

void FWidgetCatalogViewModel::OnBlueprintReinstanced()
{
	bRebuildRequested = true;
}

void FWidgetCatalogViewModel::OnFavoritesUpdated()
{
	bRebuildRequested = true;
}

void FWidgetCatalogViewModel::OnReloadComplete(EReloadCompleteReason Reason)
{
	bRebuildRequested = true;
}

void FWidgetCatalogViewModel::HandleOnAssetsDeleted(const TArray<UClass*>& DeletedAssetClasses)
{
	for (const UClass* DeletedAssetClass : DeletedAssetClasses)
	{
		if ((DeletedAssetClass == nullptr) || DeletedAssetClass->IsChildOf(UWidgetBlueprint::StaticClass()))
		{
			bRebuildRequested = true;
		}
	}
}

void FPaletteViewModel::BuildWidgetTemplateCategory(FString& Category, TArray<TSharedPtr<FWidgetTemplate>>& Templates, TArray<FString>& FavoritesList)
{
	TSharedPtr<FWidgetHeaderViewModel> Header = MakeShareable(new FWidgetHeaderViewModel());
	Header->GroupName = FText::FromString(Category);

	// Copy of the list of favorites to be able to do some cleanup in the real list
	UWidgetPaletteFavorites* FavoritesPalette = GetDefault<UWidgetDesignerSettings>()->Favorites;

	for (auto& Template : Templates)
	{
		TSharedPtr<FWidgetTemplateViewModel> TemplateViewModel = MakeShareable(new FWidgetTemplateViewModel());
		TemplateViewModel->Template = Template;
		TemplateViewModel->FavortiesViewModel = this;
		Header->Children.Add(TemplateViewModel);

		// If it's a favorite, we also add it to the Favorite section
		int32 index = FavoritesList.Find(Template->Name.ToString());
		if (index != INDEX_NONE)
		{
			TemplateViewModel->SetFavorite();

			// We have to create a second copy of the ViewModel for the treeview has it doesn't support to have the same element twice.
			TSharedPtr<FWidgetTemplateViewModel> FavoriteTemplateViewModel = MakeShareable(new FWidgetTemplateViewModel());
			FavoriteTemplateViewModel->Template = Template;
			FavoriteTemplateViewModel->FavortiesViewModel = this;
			FavoriteTemplateViewModel->SetFavorite();

			FavoriteHeader->Children.Add(FavoriteTemplateViewModel);

			// Remove the favorite from the temporary list
			FavoritesList.RemoveAt(index);
		}

	}
	
	Header->Children.Sort([](const TSharedPtr<FWidgetViewModel>& L, const TSharedPtr<FWidgetViewModel>& R) { return R->GetName().CompareTo(L->GetName()) > 0; });

	WidgetViewModels.Add(Header);
}

void FPaletteViewModel::AddToFavorites(const FWidgetTemplateViewModel* WidgetTemplateViewModel)
{
	UWidgetPaletteFavorites* Favorites = GetDefault<UWidgetDesignerSettings>()->Favorites;
	Favorites->Add(WidgetTemplateViewModel->GetName().ToString());
}

void FPaletteViewModel::RemoveFromFavorites(const FWidgetTemplateViewModel* WidgetTemplateViewModel)
{
	UWidgetPaletteFavorites* Favorites = GetDefault<UWidgetDesignerSettings>()->Favorites;
	Favorites->Remove(WidgetTemplateViewModel->GetName().ToString());
}

#undef LOCTEXT_NAMESPACE

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/AssetBrowser/SNiagaraAssetBrowser.h"

#include "ContentBrowserMenuContexts.h"
#include "ContentBrowserModule.h"
#include "NiagaraEditorSettings.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraRecentAndFavoritesManager.h"
#include "NiagaraSystem.h"
#include "SlateOptMacros.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Toolkits/GlobalEditorCommonCommands.h"
#include "Toolkits/NiagaraSystemToolkit.h"
#include "Widgets/SItemSelector.h"
#include "Widgets/SNiagaraSystemViewport.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/AssetBrowser/NiagaraAssetBrowserConfig.h"
#include "Widgets/AssetBrowser/NiagaraSemanticTagsFrontEndFilterExtension.h"
#include "Widgets/AssetBrowser/SNiagaraSelectedAssetDetails.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

#define LOCTEXT_NAMESPACE "NiagaraAssetBrowser"

void SNiagaraAssetBrowser::Construct(const FArguments& InArgs)
{
	AvailableClasses = InArgs._AvailableClasses;
	RecentAndFavoritesList = InArgs._RecentAndFavoritesList;
	SaveSettingsName = InArgs._SaveSettingsName;
	EmptySelectionMessage = InArgs._EmptySelectionMessage;
	AssetSelectionMode = InArgs._AssetSelectionMode;
	OnAssetSelectedDelegate = InArgs._OnAssetSelected;
	OnAssetsActivatedDelegate = InArgs._OnAssetsActivated;

	PreviewViewport = SNew(SNiagaraAssetBrowserPreview)
	.Visibility(this, &SNiagaraAssetBrowser::OnGetViewportVisibility);
	
	ensureMsgf(AvailableClasses.Num() >= 1, TEXT("The Niagara asset browser has to be supplied at least one available class."));
	
	ChildSlot
	[
		SNew(SBorder)
		.Padding(0.f)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(0.f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Horizontal)
				.PhysicalSplitterHandleSize(2.f)
				+ SSplitter::Slot().Expose(FiltersSlot)
					.Value(0.15f)
					.MinSize(50.f)
				+ SSplitter::Slot().Expose(AssetBrowserContentSlot)
					.Value(0.6f)
				+ SSplitter::Slot().Expose(AssetBrowserDetailsAreaSlot)
					.Value(0.25f)
					.MinSize(100.f)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSeparator)
				.Orientation(Orient_Horizontal)
				.Thickness(2.f)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Expose(AdditionalWidgetSlot)
		]
	];

	bSuppressSaveAndLoad = true;
	PopulateAssetBrowserContentSlot();
	PopulateFiltersSlot();
	PopulateAssetBrowserDetailsSlot();

	if(InArgs._AdditionalWidget.IsValid())
	{
		AdditionalWidgetSlot->AttachWidget(InArgs._AdditionalWidget.ToSharedRef());
	}

	bSuppressSaveAndLoad = false;
	LoadSettings();

	InitContextMenu();
}

SNiagaraAssetBrowser::~SNiagaraAssetBrowser()
{
	PreviewViewport.Reset();
	SaveSettings();
}

TArray<UClass*> SNiagaraAssetBrowser::GetDisplayedAssetTypes() const
{
	return AvailableClasses;
}

void SNiagaraAssetBrowser::InitContextMenu()
{
	static FName MenuName("NiagaraAssetBrowser.ContextMenu");
	if(UToolMenus::Get()->IsMenuRegistered(MenuName) == false)
	{
		UToolMenu* Menu = UToolMenus::Get()->RegisterMenu(MenuName);
		FToolMenuSection& DefaultSection = Menu->AddSection("Default");

		FToolMenuExecuteAction ToolMenuAction = FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext& Context)
		{
			if(UContentBrowserAssetContextMenuContext* ContentBrowserContext = Context.FindContext<UContentBrowserAssetContextMenuContext>())
			{
				if(GEditor)
				{
					GEditor->SyncBrowserToObjects(ContentBrowserContext->SelectedAssets);
				}
			}
		});
		FToolUIActionChoice Action(ToolMenuAction);		
		DefaultSection.AddEntry(FToolMenuEntry::InitMenuEntry("NavigateTo", LOCTEXT("NavigateTo", "Find in Content Browser"), FText::GetEmpty(), FSlateIcon(), Action));
	}
}

FARFilter SNiagaraAssetBrowser::GetCurrentBackendFilter() const
{
	FARFilter Filter;

	if(MainFilterSelector.IsValid())
	{
		FARFilter AssetTagSelectorFilter;

		for(const TSharedRef<FNiagaraAssetBrowserMainFilter>& MainFilter : MainFilterSelector->GetSelectedItems())
		{
			if(MainFilter->FilterMode == FNiagaraAssetBrowserMainFilter::EFilterMode::NiagaraAssetTag)
			{
				AssetTagSelectorFilter.TagsAndValues.Add(FName(MainFilter->AssetTagDefinition.GetGuidAsString()), TOptional<FString>());
			}
		}

		Filter.Append(AssetTagSelectorFilter);
	}
	
	// if the filter list hasn't specified any classes of the available ones, we explicitly set the filter to the available classes.
	// this is because if no classes are specified, we could potentially receive assets outside the available classes
	if(Filter.ClassPaths.IsEmpty())
	{
		for(UClass* AvailableClass : AvailableClasses)
		{
			Filter.ClassPaths.Add(AvailableClass->GetClassPathName());
		}
	}

	return Filter;
}

bool SNiagaraAssetBrowser::ShouldFilterAsset(const FAssetData& AssetData) const
{
	const UNiagaraEditorSettings* Settings = GetDefault<UNiagaraEditorSettings>();
	if(Settings->IsAllowedAssetInNiagaraAssetBrowser(AssetData) == false)
	{
		return true;
	}
	
	if(Settings->IsAllowedAssetByClassUsage(AssetData) == false)
	{
		return true;
	}
	
	// TODO (ME) This currently implies only one main filter/folder can be active at a time. is this wanted?
	for(const TSharedRef<FNiagaraAssetBrowserMainFilter>& MainFilter : MainFilterSelector->GetSelectedItems())
	{
		if(MainFilter->FilterMode == FNiagaraAssetBrowserMainFilter::EFilterMode::All)
		{
			return false;
		}
		else if(MainFilter->FilterMode == FNiagaraAssetBrowserMainFilter::EFilterMode::Recent)
		{
			return MainFilter->IsAssetRecent(AssetData) == false;
		}
		else if(MainFilter->FilterMode == FNiagaraAssetBrowserMainFilter::EFilterMode::NiagaraAssetTag)
		{
			return MainFilter->DoesAssetHaveTag(AssetData) == false;
		}
		else if(MainFilter->FilterMode == FNiagaraAssetBrowserMainFilter::EFilterMode::NiagaraAssetTagDefinitionsAsset)
		{
			return MainFilter->DoesAssetHaveAnyTagFromTagDefinitionsAsset(AssetData) == false;
		}
		else if(MainFilter->FilterMode == FNiagaraAssetBrowserMainFilter::EFilterMode::Custom)
		{
			return MainFilter->CustomShouldFilterAsset.Execute(AssetData);
		}
	}

	return false;
}

void SNiagaraAssetBrowser::RefreshBackendFilter() const
{
	AssetBrowserContent->SetARFilter(GetCurrentBackendFilter());
}

void SNiagaraAssetBrowser::PopulateFiltersSlot()
{
	AssetBrowserMainFilters = GetMainFilters();

	SAssignNew(MainFilterSelector, STreeView<TSharedRef<FNiagaraAssetBrowserMainFilter>>)
		.TreeItemsSource(&AssetBrowserMainFilters)
		.OnSelectionChanged(this, &SNiagaraAssetBrowser::OnMainFilterSelected)
		.OnGenerateRow(this, &SNiagaraAssetBrowser::GenerateWidgetRowForMainFilter)
		.OnGetChildren(this, &SNiagaraAssetBrowser::OnGetChildFiltersForFilter)
		.SelectionMode(ESelectionMode::Single);
	
	TSharedRef<FNiagaraAssetBrowserMainFilter>* AllFilter = AssetBrowserMainFilters.FindByPredicate([](const TSharedRef<FNiagaraAssetBrowserMainFilter>& MainFilterCandidate)
	{
		return MainFilterCandidate->FilterMode == FNiagaraAssetBrowserMainFilter::EFilterMode::All;
	});
	
	if(AllFilter)
	{
		MainFilterSelector->SetSelection(*AllFilter);
		MainFilterSelector->SetItemExpansion(*AllFilter, true);
	}
	
	FiltersSlot->AttachWidget(MainFilterSelector.ToSharedRef());
}

void SNiagaraAssetBrowser::PopulateAssetBrowserContentSlot()
{
	FAssetPickerConfig Config;
	Config.Filter = GetCurrentBackendFilter();
	Config.bCanShowClasses = false;
	Config.bAddFilterUI = true;
	Config.DefaultFilterMenuExpansion = EAssetTypeCategories::FX;
	Config.ExtraFrontendFilters = OnGetExtraFrontendFilters();
	Config.OnExtendAddFilterMenu = FOnExtendAddFilterMenu::CreateSP(this, &SNiagaraAssetBrowser::OnExtendAddFilterMenu);
	Config.bUseSectionsForCustomFilterCategories = true;
	Config.OnShouldFilterAsset = FOnShouldFilterAsset::CreateSP(this, &SNiagaraAssetBrowser::ShouldFilterAsset);
	Config.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SNiagaraAssetBrowser::OnAssetSelected);
	Config.OnAssetsActivated = FOnAssetsActivated::CreateSP(this, &SNiagaraAssetBrowser::OnAssetsActivated);
	Config.OnGetCustomAssetToolTip = FOnGetCustomAssetToolTip::CreateSP(this, &SNiagaraAssetBrowser::OnGetCustomAssetTooltip);
	Config.OnIsAssetValidForCustomToolTip = FOnIsAssetValidForCustomToolTip::CreateLambda([](FAssetData& AssetData)
	{
		return true;
	});
	Config.bForceShowEngineContent = true;
	Config.bForceShowPluginContent = true;
	Config.SelectionMode = AssetSelectionMode;
	Config.bAllowDragging = false;
	Config.OnGetAssetContextMenu = FOnGetAssetContextMenu::CreateSP(this, &SNiagaraAssetBrowser::OnGetAssetContextMenu);

	if(SaveSettingsName.IsSet())
	{
		Config.SaveSettingsName = SaveSettingsName.GetValue().ToString();
	}
	
	SAssignNew(AssetBrowserContent, SNiagaraAssetBrowserContent)
	.InitialConfig(Config);
	
	AssetBrowserContentSlot->AttachWidget(AssetBrowserContent.ToSharedRef());
}

void SNiagaraAssetBrowser::PopulateAssetBrowserDetailsSlot()
{
	DetailsContainer = SNew(SBox)
		.Padding(5.f)
		[
			SAssignNew(DetailsSwitcher, SWidgetSwitcher)
			+ SWidgetSwitcher::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Top)
			.Padding(30.f, 60.f)
			[
				SNew(STextBlock)
				.Text(EmptySelectionMessage.Get(FText::GetEmpty()))
				.AutoWrapText(true)
				.TextStyle(&FAppStyle::GetWidgetStyle<FTextBlockStyle>("NormalText.Subdued"))
			]
			+ SWidgetSwitcher::Slot()
			.Expose(AssetBrowserDetailsSlot)
		];
	
	AssetBrowserDetailsAreaSlot->AttachWidget(DetailsContainer.ToSharedRef());
}

TArray<TSharedRef<FNiagaraAssetBrowserMainFilter>> SNiagaraAssetBrowser::GetMainFilters() const
{
	using namespace FNiagaraEditorUtilities::AssetBrowser;

	TArray<TSharedRef<FNiagaraAssetBrowserMainFilter>> MainFilters;

	// Recent
	{
		TSharedRef<FNiagaraAssetBrowserMainFilter> RecentFilter = MakeShared<FNiagaraAssetBrowserMainFilter>(FNiagaraAssetBrowserMainFilter::EFilterMode::Recent);
		RecentFilter->IsAssetRecentDelegate = FNiagaraAssetBrowserMainFilter::FIsAssetRecent::CreateSP(this, &SNiagaraAssetBrowser::OnIsAssetRecent);
		MainFilters.Add(RecentFilter);
	}

	// Tag Filters
	TArray<TSharedRef<FNiagaraAssetBrowserMainFilter>> TagFilters;
	{
		TArray<UClass*> DisplayedAssetTypes = GetDisplayedAssetTypes();
		TArray<UNiagaraAssetTagDefinitions*> DisplayedAssetTagDefinitionAssets;
		TArray<FNiagaraAssetTagDefinition> DisplayedFlatAssetTagDefinitionsList;

		auto IsAssetTagDefinitionValid = [DisplayedAssetTypes](const FNiagaraAssetTagDefinition& AssetTagDefinition) -> bool
		{
			if(AssetTagDefinition.DisplayType != ENiagaraAssetTagDefinitionImportance::Primary)
			{
				return false;
			}
				
			TArray<UClass*> SupportedClasses = AssetTagDefinition.GetSupportedClasses();
	
			bool bCanAssetTagContainDisplayedAssetType = false;
			for(UClass* SupportedClass : SupportedClasses)
			{
				if(DisplayedAssetTypes.Contains(SupportedClass))
				{
					bCanAssetTagContainDisplayedAssetType = true;
					break;
				}
			}
	
			if(bCanAssetTagContainDisplayedAssetType == false)
			{
				return false;
			}

			return true;
		};
		
		for(const FStructuredAssetTagDefinitionLookupData& AssetTagDefinitionData : GetStructuredSortedAssetTagDefinitions())
		{
			// If there is no definitions asset, the tags have been declared internally
			if(AssetTagDefinitionData.DefinitionsAsset == nullptr)
			{
				for(const FNiagaraAssetTagDefinition& AssetTagDefinition : AssetTagDefinitionData.AssetTagDefinitions)
				{
					if(IsAssetTagDefinitionValid(AssetTagDefinition))
					{
						DisplayedFlatAssetTagDefinitionsList.Add(AssetTagDefinition);
					}	
				}
			}
			// If there is an asset, we check if we want to display a parent-entry per asset first
			else
			{
				// If we only want to display the tags or we only have 1 tag defined, we add the tags directly to a flat list
				if(AssetTagDefinitionData.DefinitionsAsset->DisplayTagsAsFlatList() || AssetTagDefinitionData.AssetTagDefinitions.Num() == 1)
				{
					for(const FNiagaraAssetTagDefinition& AssetTagDefinition : AssetTagDefinitionData.AssetTagDefinitions)
					{
						if(IsAssetTagDefinitionValid(AssetTagDefinition))
						{
							DisplayedFlatAssetTagDefinitionsList.Add(AssetTagDefinition);
						}	
					}
				}
				// If not, we keep track of the asset here so we can construct a hierarchy of filters per asset
				else
				{
					DisplayedAssetTagDefinitionAssets.Add(AssetTagDefinitionData.DefinitionsAsset);
				}
			}			
		}

		// First we add all 'flat list' entries at the top
		for(const FNiagaraAssetTagDefinition& AssetTagDefinition : DisplayedFlatAssetTagDefinitionsList)
		{
			TSharedRef<FNiagaraAssetBrowserMainFilter> AssetTagFilter = MakeShared<FNiagaraAssetBrowserMainFilter>(FNiagaraAssetBrowserMainFilter::EFilterMode::NiagaraAssetTag);
			AssetTagFilter->AssetTagDefinition = AssetTagDefinition;
			TagFilters.Add(AssetTagFilter);
		}

		for(const UNiagaraAssetTagDefinitions* AssetTagDefinitionsAsset : DisplayedAssetTagDefinitionAssets)
		{
			// This code should only execute for assets with > 1 tag. If there is only 1 tag, it should have been automatically added to the flat list instead
			if(ensure(AssetTagDefinitionsAsset->GetAssetTagDefinitions().Num() > 1))
			{
				TSharedRef<FNiagaraAssetBrowserMainFilter> AssetTagDefinitionsAssetsFilter = MakeShared<FNiagaraAssetBrowserMainFilter>(FNiagaraAssetBrowserMainFilter::EFilterMode::NiagaraAssetTagDefinitionsAsset);
				AssetTagDefinitionsAssetsFilter->AssetTagDefinitionsAsset = AssetTagDefinitionsAsset;
				TagFilters.Add(AssetTagDefinitionsAssetsFilter);

				TArray<TSharedRef<FNiagaraAssetBrowserMainFilter>> TagChildFilters;
				for(const FNiagaraAssetTagDefinition& AssetTagDefinition : AssetTagDefinitionsAsset->GetAssetTagDefinitions())
				{
					if(IsAssetTagDefinitionValid(AssetTagDefinition))
					{
						TSharedRef<FNiagaraAssetBrowserMainFilter> AssetTagFilter = MakeShared<FNiagaraAssetBrowserMainFilter>(FNiagaraAssetBrowserMainFilter::EFilterMode::NiagaraAssetTag);
						AssetTagFilter->AssetTagDefinition = AssetTagDefinition;
						TagChildFilters.Add(AssetTagFilter);
					}
				}

				AssetTagDefinitionsAssetsFilter->ChildFilters = TagChildFilters;
			}			
		}
	}
	
	// All
	{
		TSharedRef<FNiagaraAssetBrowserMainFilter> AllFilter = MakeShared<FNiagaraAssetBrowserMainFilter>(FNiagaraAssetBrowserMainFilter::EFilterMode::All);
		AllFilter->ChildFilters = TagFilters;
		MainFilters.Add(AllFilter);
	}

	return MainFilters;
}

void SNiagaraAssetBrowser::OnFilterChanged() const
{
	RefreshBackendFilter();
}

void SNiagaraAssetBrowser::OnGetChildFiltersForFilter(TSharedRef<FNiagaraAssetBrowserMainFilter> NiagaraAssetBrowserMainFilter, TArray<TSharedRef<FNiagaraAssetBrowserMainFilter>>& OutChildren) const
{
	OutChildren.Append(NiagaraAssetBrowserMainFilter->ChildFilters);
}

bool SNiagaraAssetBrowser::OnCompareMainFiltersForEquality(const FNiagaraAssetBrowserMainFilter& MainFilterA, const FNiagaraAssetBrowserMainFilter& MainFilterB) const
{
	return MainFilterA == MainFilterB;
}

// TSharedRef<SWidget> SNiagaraAssetBrowser::GenerateWidgetForMainFilter(const FNiagaraAssetBrowserMainFilter& MainFilter) const
// {
// 	FLinearColor FolderColor = FAppStyle::Get().GetSlateColor("ContentBrowser.DefaultFolderColor").GetSpecifiedColor();
// 	
// 	TSharedRef<SWidget> Widget = SNew(SHorizontalBox)
// 	+ SHorizontalBox::Slot()
// 	.AutoWidth()
// 	.Padding(2.f)
// 	[
// 		SNew(SImage)
// 		.Image_Lambda([this, MainFilter]()
// 		{
// 			if(MainFilterSelector->GetSelectedItems().Contains(MainFilter))
// 			{
// 				return FAppStyle::GetBrush("ContentBrowser.AssetTreeFolderOpen");
// 			}
// 			else
// 			{
// 				return FAppStyle::GetBrush("ContentBrowser.AssetTreeFolderClosed");
// 			}
// 		})
// 		.ColorAndOpacity(FolderColor)
// 	]
// 	+ SHorizontalBox::Slot()
// 	[
// 		SNew(STextBlock)
// 		.Text(MainFilter.GetDisplayName())
// 	];
//
// 	if(MainFilter.FilterMode == FNiagaraAssetBrowserMainFilter::EFilterMode::NiagaraAssetTag)
// 	{
// 		Widget->SetToolTipText(MainFilter.AssetTagDefinition.Description);	
// 	}
// 	
// 	return Widget;
// }

TSharedRef<ITableRow> SNiagaraAssetBrowser::GenerateWidgetRowForMainFilter(TSharedRef<FNiagaraAssetBrowserMainFilter> MainFilter, const TSharedRef<STableViewBase>& OwningTable) const
{
	FLinearColor FolderColor = FAppStyle::Get().GetSlateColor("ContentBrowser.DefaultFolderColor").GetSpecifiedColor();
	
	TSharedRef<SWidget> Widget = SNew(SHorizontalBox)
	+ SHorizontalBox::Slot()
	.AutoWidth()
	.Padding(2.f)
	[
		SNew(SImage)
		.Image_Lambda([this, MainFilter]()
		{
			if(MainFilterSelector->GetSelectedItems().Contains(MainFilter))
			{
				return FAppStyle::GetBrush("ContentBrowser.AssetTreeFolderOpen");
			}
			else
			{
				return FAppStyle::GetBrush("ContentBrowser.AssetTreeFolderClosed");
			}
		})
		.ColorAndOpacity(FolderColor)
	]
	+ SHorizontalBox::Slot()
	[
		SNew(STextBlock)
		.Text(MainFilter->GetDisplayName())
	];
	
	if(MainFilter->FilterMode == FNiagaraAssetBrowserMainFilter::EFilterMode::NiagaraAssetTag)
	{
		Widget->SetToolTipText(MainFilter->AssetTagDefinition.Description);	
	}

	if(MainFilter->FilterMode == FNiagaraAssetBrowserMainFilter::EFilterMode::NiagaraAssetTagDefinitionsAsset)
	{
		Widget->SetToolTipText(MainFilter->AssetTagDefinitionsAsset->GetDescription());	
	}
	
	return SNew(STableRow<TSharedRef<FNiagaraAssetBrowserMainFilter>>, OwningTable)
	[
		Widget
	];
}

TArray<FAssetData> SNiagaraAssetBrowser::GetSelectedAssets() const
{
	return AssetBrowserContent->GetCurrentSelection();
}

void SNiagaraAssetBrowser::OnAssetSelected(const FAssetData& AssetData)
{
	if(AssetData.IsValid() == false)
	{
		AssetBrowserDetailsSlot->AttachWidget(SNullWidget::NullWidget);
		DetailsSwitcher->SetActiveWidgetIndex(0);
	}
	
	if(FNiagaraAssetDetailDatabase::NiagaraAssetDetailDatabase.Contains(AssetData.GetClass()))
	{
		if(GetShouldDisplayViewport())
		{
			if(AssetData.GetClass() == UNiagaraSystem::StaticClass())
			{
				PreviewViewport->SetSystem(*Cast<UNiagaraSystem>(AssetData.GetAsset()));
			}
			else
			{
				PreviewViewport->ResetAsset();
			}
		}
		
		TSharedRef<SWidget> SelectedAssetDetails = SNew(SNiagaraSelectedAssetDetails, AssetData)
			.ShowThumbnail(TAttribute<EVisibility>::CreateSP(this, &SNiagaraAssetBrowser::OnGetThumbnailVisibility))
			.OnAssetTagActivated(this, &SNiagaraAssetBrowser::OnAssetTagActivated)
			.OnAssetTagActivatedTooltip(LOCTEXT("SecondaryAssetTagButtonTooltip", "\n\nClicking this tag will activate/deactivate its corresponding filter."));
		
		TSharedRef<SWidget> Details = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		[
			SNew(SCheckBox)
			.IsChecked(this, &SNiagaraAssetBrowser::OnShouldDisplayViewport)
			.OnCheckStateChanged(this, &SNiagaraAssetBrowser::OnShouldDisplayViewportChanged)
			.ToolTipText(this, &SNiagaraAssetBrowser::OnGetShouldDisplayViewportTooltip)
			.Visibility(this, &SNiagaraAssetBrowser::OnGetShouldDisplayVisibilityCheckbox)
			[
				SNew(STextBlock).Text(LOCTEXT("DisplayViewport", "Display Viewport"))
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			PreviewViewport.ToSharedRef()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SelectedAssetDetails
		];

		// We don't attach the preview currently until some of the issues are resolved
		AssetBrowserDetailsSlot->AttachWidget(SelectedAssetDetails);

		DetailsSwitcher->SetActiveWidgetIndex(1);
	}

	OnAssetSelectedDelegate.ExecuteIfBound(AssetData);
}

void SNiagaraAssetBrowser::OnAssetsActivated(const TArray<FAssetData>& AssetData, EAssetTypeActivationMethod::Type ActivationType) const
{
	OnAssetsActivatedDelegate.ExecuteIfBound(AssetData, ActivationType);
}

TSharedPtr<SWidget> SNiagaraAssetBrowser::OnGetAssetContextMenu(const TArray<FAssetData>& AssetData) const
{
	if(AssetData.Num() == 1)
	{
		FName MenuName("NiagaraAssetBrowser.ContextMenu");
		UContentBrowserAssetContextMenuContext* MenuContext = NewObject<UContentBrowserAssetContextMenuContext>();
		MenuContext->SelectedAssets = AssetData;
		FToolMenuContext Context(MenuContext);
		return UToolMenus::Get()->GenerateWidget(MenuName, Context);
	}

	return nullptr;
}

TSharedRef<SToolTip> SNiagaraAssetBrowser::OnGetCustomAssetTooltip(FAssetData& AssetData)
{
	return SNew(SToolTip)
	[
		SNew(SNiagaraSelectedAssetDetails, AssetData).ShowThumbnail(EVisibility::Collapsed)
	];
}

void SNiagaraAssetBrowser::OnMainFilterSelected(TSharedPtr<FNiagaraAssetBrowserMainFilter> MainFilter, ESelectInfo::Type Arg)
{
	if(MainFilter.IsValid())
	{
		OnFilterChanged();

		LastSelectedMainFilterIdentifierFallback = MainFilter->GetIdentifier();
		SaveSettings();
	}
}

void SNiagaraAssetBrowser::OnAssetTagActivated(const FNiagaraAssetTagDefinition& NiagaraAssetTagDefinition)
{
	// First we attempt to select it as a main filter
	// TArray<FNiagaraAssetBrowserMainFilter> MainFilters = GetMainFilters();
	TArray<TSharedRef<FNiagaraAssetBrowserMainFilter>> MainFilters = GetMainFilters();

	TArray<TSharedRef<FNiagaraAssetBrowserMainFilter>> AllFilters = MainFilters;
	while(AllFilters.IsEmpty() == false)
	{
		TSharedRef<FNiagaraAssetBrowserMainFilter> CurrentFilter = AllFilters[0];
		AllFilters.RemoveAt(0);
		MainFilters.AddUnique(CurrentFilter);
		AllFilters.Append(CurrentFilter->ChildFilters);
	}
	
	TSharedRef<FNiagaraAssetBrowserMainFilter>* FoundMainFilter = MainFilters.FindByPredicate([NiagaraAssetTagDefinition](const TSharedRef<FNiagaraAssetBrowserMainFilter>& MainFilterCandidate)
	{
		return MainFilterCandidate->AssetTagDefinition == NiagaraAssetTagDefinition;
	});

	if(FoundMainFilter)
	{
		MainFilterSelector->SetItemSelection({*FoundMainFilter}, true);
	}
	else if(DropdownFilterCache.Contains(NiagaraAssetTagDefinition))
	{
		bool bNewState = !DropdownFilterCache[NiagaraAssetTagDefinition]->IsActive();
		DropdownFilterCache[NiagaraAssetTagDefinition]->SetActive(bNewState);
	}
}

TArray<TSharedRef<FFrontendFilter>> SNiagaraAssetBrowser::OnGetExtraFrontendFilters() const
{
	TArray<TSharedRef<FFrontendFilter>> Result;

	{
		using namespace FNiagaraEditorUtilities::AssetBrowser;
		
		TSharedRef<FFrontendFilterCategory> NiagaraTagFiltersCategory = MakeShared<FFrontendFilterCategory>(LOCTEXT("NiagaraTagFilterCategoryLabel", "Niagara Tags"), LOCTEXT("NiagaraTagFiltersTooltip", "Secondary Niagara Tags used for filtering"));

		// This requires the asset registry to be done loading
		for(const FNiagaraAssetTagDefinition& AssetTagDefinition : GetFlatSortedAssetTagDefinitions(false))
		{
			if(AssetTagDefinition.DisplayType == ENiagaraAssetTagDefinitionImportance::Secondary)
			{
				TSharedRef<FFrontendFilter_NiagaraTag> NiagaraTagFilter = MakeShared<FFrontendFilter_NiagaraTag>(AssetTagDefinition, NiagaraTagFiltersCategory);
				DropdownFilterCache.Add(AssetTagDefinition, NiagaraTagFilter);
				Result.Add(NiagaraTagFilter);
			}
		}
	}
	
	{
		TSharedRef<FFrontendFilterCategory> NiagaraAdditionalFiltersCategory = MakeShared<FFrontendFilterCategory>(LOCTEXT("NiagaraPropertyFilterCategoryLabel", "Niagara Filters"), LOCTEXT("NiagaraAdditionalFiltersTooltip", "Additional filters for filtering Niagara assets"));
		if(AvailableClasses.Contains(UNiagaraEmitter::StaticClass()))
		{
			Result.Add(MakeShared<FFrontendFilter_NiagaraEmitterInheritance>(true, NiagaraAdditionalFiltersCategory));
			Result.Add(MakeShared<FFrontendFilter_NiagaraEmitterInheritance>(false, NiagaraAdditionalFiltersCategory));
		}
		if(AvailableClasses.Contains(UNiagaraSystem::StaticClass()))
		{
			Result.Add(MakeShared<FFrontendFilter_NiagaraSystemEffectType>(true, NiagaraAdditionalFiltersCategory));
			Result.Add(MakeShared<FFrontendFilter_NiagaraSystemEffectType>(false, NiagaraAdditionalFiltersCategory));
		}
	}
	
	return Result;
}

void SNiagaraAssetBrowser::OnExtendAddFilterMenu(UToolMenu* ToolMenu) const
{
	for (FToolMenuSection& Section : ToolMenu->Sections)
	{
		Section.Blocks.RemoveAll([](FToolMenuEntry& ToolMenuEntry)
		{
			return ToolMenuEntry.Name == FName("Common");
		});
	}
	
	TArray<FName> SectionsToKeep { FName("FilterBarResetFilters"), FName("FX Filters"), FName("AssetFilterBarFilterAdvancedAsset"), FName("Niagara Filters"), FName("Niagara Tags") };
	ToolMenu->Sections.RemoveAll([&SectionsToKeep](FToolMenuSection& ToolMenuSection)
	{
		return SectionsToKeep.Contains(ToolMenuSection.Name) == false;
	});

	TArray<FNiagaraAssetTagDefinition> AssetTagDefinitions = FNiagaraEditorUtilities::AssetBrowser::GetFlatSortedAssetTagDefinitions();
	AssetTagDefinitions.RemoveAll([](const FNiagaraAssetTagDefinition& AssetTagDefinition)
	{
		return AssetTagDefinition.DisplayType == ENiagaraAssetTagDefinitionImportance::Primary;
	});
	
	for (FToolMenuSection& Section : ToolMenu->Sections)
	{
		if(Section.Name == FName("Niagara Tags"))
		{
			Section.Blocks.RemoveAll([AssetTagDefinitions](FToolMenuEntry& ToolMenuEntry)
			{
				return AssetTagDefinitions.ContainsByPredicate([ToolMenuEntry](const FNiagaraAssetTagDefinition& AssetTagDefinition)
				{;
					return AssetTagDefinition.AssetTag.EqualTo(ToolMenuEntry.Label.Get());
				}) == false;
			});
		}
	}
}

bool SNiagaraAssetBrowser::OnIsAssetRecent(const FAssetData& AssetCandidate) const
{
	return RecentAndFavoritesList.Get()->FindMRUItemIdx(AssetCandidate.PackageName.ToString()) != INDEX_NONE;
}

EVisibility SNiagaraAssetBrowser::OnGetViewportVisibility() const
{
	return (GetShouldDisplayViewport() == true && PreviewViewport->GetPreviewComponent()->GetAsset() != nullptr) ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SNiagaraAssetBrowser::OnGetThumbnailVisibility() const
{
	return OnGetViewportVisibility() == EVisibility::Visible ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility SNiagaraAssetBrowser::OnGetShouldDisplayVisibilityCheckbox() const
{
	return AvailableClasses.Contains(UNiagaraSystem::StaticClass()) && GetSelectedAssets().Num() == 1 && GetSelectedAssets()[0].GetClass() == UNiagaraSystem::StaticClass()
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

ECheckBoxState SNiagaraAssetBrowser::OnShouldDisplayViewport() const
{
	return bShouldDisplayViewport == true ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SNiagaraAssetBrowser::OnShouldDisplayViewportChanged(ECheckBoxState CheckBoxState)
{
	bShouldDisplayViewport = CheckBoxState == ECheckBoxState::Checked ? true : false;
}

FText SNiagaraAssetBrowser::OnGetShouldDisplayViewportTooltip() const
{
	return LOCTEXT("ShouldDisplayViewportTooltip", "If activated, displays Niagara Systems live in a viewport instead of a thumbnail.\nThis will compile the Niagara System if necessary and might slow down performance.");
}

void SNiagaraAssetBrowser::SaveSettings() const
{
	if(SaveSettingsName.IsSet() && bSuppressSaveAndLoad == false)
	{
		FNiagaraAssetBrowserConfiguration& Config = UNiagaraAssetBrowserConfig::Get()->MainFilterSelection.Add(SaveSettingsName.GetValue());
		
		for(const TSharedRef<FNiagaraAssetBrowserMainFilter>& SelectedMainFilter : MainFilterSelector->GetSelectedItems())
		{
			Config.MainFilterSelection.Add(SelectedMainFilter->GetIdentifier());
		}

		if(Config.MainFilterSelection.Num() == 0 && LastSelectedMainFilterIdentifierFallback != NAME_None)
		{
			Config.MainFilterSelection.Add(LastSelectedMainFilterIdentifierFallback);
		}

		Config.bShouldDisplayViewport = bShouldDisplayViewport;

		UNiagaraAssetBrowserConfig::Get()->SaveEditorConfig();
	}
}

void SNiagaraAssetBrowser::LoadSettings()
{
	if(SaveSettingsName.IsSet() && bSuppressSaveAndLoad == false)
	{
		UNiagaraAssetBrowserConfig::Get()->LoadEditorConfig();

		if(UNiagaraAssetBrowserConfig::Get()->MainFilterSelection.Contains(FName(SaveSettingsName.GetValue())))
		{
			FNiagaraAssetBrowserConfiguration& Config = UNiagaraAssetBrowserConfig::Get()->MainFilterSelection[FName(SaveSettingsName.GetValue())];

			bShouldDisplayViewport = Config.bShouldDisplayViewport;
			
			TArray<TSharedRef<FNiagaraAssetBrowserMainFilter>> MainFilters = AssetBrowserMainFilters;

			TArray<TSharedRef<FNiagaraAssetBrowserMainFilter>> AllFilters = MainFilters;
			while(AllFilters.IsEmpty() == false)
			{
				TSharedRef<FNiagaraAssetBrowserMainFilter> CurrentFilter = AllFilters[0];
				AllFilters.RemoveAt(0);
				MainFilters.AddUnique(CurrentFilter);
				AllFilters.Append(CurrentFilter->ChildFilters);
			}
			
			MainFilters.RemoveAll([&Config](const TSharedRef<FNiagaraAssetBrowserMainFilter>& MainFilterCandidate)
			{
				return Config.MainFilterSelection.Contains(MainFilterCandidate->GetIdentifier()) == false;
			});

			if(Config.MainFilterSelection.Num() == 1)
			{
				LastSelectedMainFilterIdentifierFallback = Config.MainFilterSelection[0];
			}

			if(Config.MainFilterSelection.Num() >= 1 && MainFilters.Num() >= 1)
			{
				MainFilterSelector->SetSelection(MainFilters[0]);
			}
		}
	}
}

void SNiagaraAssetBrowserWindow::Construct(const FArguments& InArgs)
{
	OnAssetsActivatedDelegate = InArgs._AssetBrowserArgs._OnAssetsActivated;
	
	SWindow::Construct(SWindow::FArguments()
		.Title(InArgs._WindowTitle.Get(LOCTEXT("NiagaraAssetBrowserWindowTitle", "Niagara Asset Browser")))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(1400, 750))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
	[
		SAssignNew(AssetBrowser, SNiagaraAssetBrowser)
		.AvailableClasses(InArgs._AssetBrowserArgs._AvailableClasses)
		.RecentAndFavoritesList(InArgs._AssetBrowserArgs._RecentAndFavoritesList)
		.SaveSettingsName(InArgs._AssetBrowserArgs._SaveSettingsName)
		.EmptySelectionMessage(InArgs._AssetBrowserArgs._EmptySelectionMessage)
		.AssetSelectionMode(InArgs._AssetBrowserArgs._AssetSelectionMode)
		.OnAssetSelected(InArgs._AssetBrowserArgs._OnAssetSelected)
		.OnAssetsActivated(this, &SNiagaraAssetBrowserWindow::OnAssetsActivated)
		.AdditionalWidget(InArgs._AssetBrowserArgs._AdditionalWidget)
	]);
}

bool SNiagaraAssetBrowserWindow::HasSelectedAssets() const
{
	return GetSelectedAssets().Num() > 0;
}

TArray<FAssetData> SNiagaraAssetBrowserWindow::GetSelectedAssets() const
{
	return AssetBrowser->GetSelectedAssets();
}

void SNiagaraAssetBrowserWindow::OnAssetsActivated(const TArray<FAssetData>& AssetData, EAssetTypeActivationMethod::Type ActivationType)
{
	if(OnAssetsActivatedDelegate.ExecuteIfBound(AssetData, ActivationType))
	{
		RequestDestroyWindow();
	}
}

void SNiagaraAssetBrowserWindow::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	SWindow::OnFocusLost(InFocusEvent);
}

FReply SNiagaraAssetBrowserWindow::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	FReply Reply = SWindow::OnKeyDown(MyGeometry, InKeyEvent);

	if(InKeyEvent.GetKey() == EKeys::Escape)
	{
		RequestDestroyWindow();
	}

	return Reply;
}

void SNiagaraCreateAssetWindow::Construct(const FArguments& InArgs, UClass& InCreatedClass)
{
	CreatedClass = &InCreatedClass;

	FArguments Args = InArgs;

	TSharedRef<SWidget> CreateAssetControls = SNew(SBox)
		.Padding(16.f, 16.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.f, 1.f)
			.HAlign(HAlign_Left)
			[
				SNew(SButton)
				.OnClicked(this, &SNiagaraCreateAssetWindow::Proceed)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2.f)
					[
						SNew(SImage)
						.Image(FSlateIconFinder::FindIconForClass(CreatedClass.Get()).GetIcon())
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2.f)
					[
						SNew(STextBlock).Text(FText::FormatOrdered(LOCTEXT("CreateEmptyAssetButtonLabel", "Create Empty {0}"), CreatedClass->GetDisplayNameText()))
					]
				]
			]
			+ SHorizontalBox::Slot()
			[
				SNew(SSpacer)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.f, 1.f)
			[
				SNew(SButton)
				.ContentPadding(FMargin(50.f, 4.f, 50.f, 1.f))
				.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
				.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("PrimaryButtonText"))
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("CreatePrimaryButtonLabel", "Create"))
				// .Text(FText::FormatOrdered(LOCTEXT("CreatePrimaryButtonLabel", "Create {0}"), CreatedClass->GetDisplayNameText()))
				.OnClicked(this, &SNiagaraCreateAssetWindow::Proceed)
				.IsEnabled(this, &SNiagaraCreateAssetWindow::HasSelectedAssets)
				.ToolTipText(this, &SNiagaraCreateAssetWindow::GetCreateButtonTooltip)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.f, 1.f)
			[
				SNew(SButton)
				.ContentPadding(FMargin(50.f, 4.f, 50.f, 1.f))
				.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("PrimaryButtonText"))
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("CancelButtonLabel", "Cancel"))
				.OnClicked(this, &SNiagaraCreateAssetWindow::Cancel)
			]
		];
	
	Args._AssetBrowserWindowArgs._AssetBrowserArgs._AdditionalWidget = CreateAssetControls;
	Args._AssetBrowserWindowArgs._AssetBrowserArgs._RecentAndFavoritesList = FNiagaraEditorModule::Get().GetRecentsManager()->GetRecentEmitterAndSystemsList();
	Args._AssetBrowserWindowArgs._AssetBrowserArgs._OnAssetsActivated = FOnAssetsActivated::CreateSP(this, &SNiagaraCreateAssetWindow::OnAssetsActivatedInternal);
	SNiagaraAssetBrowserWindow::Construct(Args._AssetBrowserWindowArgs);
}

void SNiagaraCreateAssetWindow::OnAssetsActivatedInternal(const TArray<FAssetData>& AssetData, EAssetTypeActivationMethod::Type)
{
	bProceedWithAction = true;
}

FReply SNiagaraCreateAssetWindow::Proceed()
{
	// The assets don't matter here as the factories making use of this window will retrieve the selected assets afterwards
	OnAssetsActivated({}, EAssetTypeActivationMethod::Opened);
	return FReply::Handled();
}

FReply SNiagaraCreateAssetWindow::Cancel()
{
	bProceedWithAction = false;
	RequestDestroyWindow();
	return FReply::Handled();
}

FText SNiagaraCreateAssetWindow::GetCreateButtonTooltip() const
{
	return HasSelectedAssets()
	? FText::FormatOrdered(LOCTEXT("CreateAssetButtonTooltip_Enabled", "Create a new {0} with selected asset {1}"), CreatedClass->GetDisplayNameText(), FText::FromName(GetSelectedAssets()[0].AssetName))
	: LOCTEXT("CreateAssetButtonTooltip_Disabled", "Please select an asset as a base for your new effect. Alternatively, create an empty asset.");
}

void SNiagaraAddEmitterToSystemWindow::Construct(const FArguments& InArgs, TSharedRef<FNiagaraSystemViewModel> SystemViewModel)
{
	WeakSystemViewModel = SystemViewModel;
	
	SNiagaraAssetBrowser::FArguments AssetBrowserArgs;
	AssetBrowserArgs._AvailableClasses = {UNiagaraEmitter::StaticClass()};
	AssetBrowserArgs._AssetSelectionMode = ESelectionMode::Single;
	AssetBrowserArgs._SaveSettingsName = FName("NiagaraAssetBrowser.AddEmitter");
	AssetBrowserArgs._EmptySelectionMessage = LOCTEXT("EmptyEmitterSelectionUserText", "Select an emitter to add it to your system, or add a new empty emitter");
	AssetBrowserArgs._OnAssetsActivated = FOnAssetsActivated::CreateSP(this, &SNiagaraAddEmitterToSystemWindow::OnAssetsActivatedInternal);
	AssetBrowserArgs._RecentAndFavoritesList = FNiagaraEditorModule::Get().GetRecentsManager()->GetRecentEmitterAndSystemsList();

	AssetBrowserArgs._AdditionalWidget = SNew(SBox)
	   .Padding(16.f, 16.f)
	   [
		   SNew(SHorizontalBox)
		   + SHorizontalBox::Slot()
		   .AutoWidth()
		   .Padding(5.f, 1.f)
		   .HAlign(HAlign_Left)
		   [
			   SNew(SButton)
			   .OnClicked_Lambda([this]()
			   {
			   		if(WeakSystemViewModel.IsValid())
			   		{
						WeakSystemViewModel.Pin()->AddEmptyEmitter();
			   			RequestDestroyWindow();
			   		}
			   	
				   return FReply::Handled();
			   })
			   [
				   SNew(SHorizontalBox)
				   + SHorizontalBox::Slot()
				   .AutoWidth()
				   .Padding(2.f)
				   [
					   SNew(SImage)
					   .Image(FSlateIconFinder::FindIconForClass(UNiagaraEmitter::StaticClass()).GetIcon())
				   ]
				   + SHorizontalBox::Slot()
				   .AutoWidth()
				   .Padding(2.f)
				   [
					   SNew(STextBlock).Text(FText::FormatOrdered(LOCTEXT("AddEmptyEmitterButtonLabel", "Add Empty {0}"), UNiagaraEmitter::StaticClass()->GetDisplayNameText()))
				   ]
			   ]
		   ]
		   + SHorizontalBox::Slot()
		   [
			   SNew(SSpacer)
		   ]
		   + SHorizontalBox::Slot()
		   .AutoWidth()
		   .Padding(5.f, 1.f)
		   [
			   SNew(SButton)
			   .ContentPadding(FMargin(50.f, 4.f, 50.f, 1.f))
			   .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
			   .TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("PrimaryButtonText"))
			   .HAlign(HAlign_Center)
			   .Text(LOCTEXT("AddEmitterPrimaryButtonLabel", "Add"))
			   .OnClicked(this, &SNiagaraAddEmitterToSystemWindow::AddSelectedEmitters)
			   .IsEnabled(this, &SNiagaraAddEmitterToSystemWindow::HasSelectedAssets)
			   .ToolTipText(this, &SNiagaraAddEmitterToSystemWindow::GetAddButtonTooltip)
		   ]
		   + SHorizontalBox::Slot()
		   .AutoWidth()
		   .Padding(5.f, 1.f)
		   [
			   SNew(SButton)
			   .ContentPadding(FMargin(50.f, 4.f, 50.f, 1.f))
			   .TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("PrimaryButtonText"))
			   .HAlign(HAlign_Center)
			   .Text(LOCTEXT("CancelButtonLabel", "Cancel"))
			   .OnClicked(this, &SNiagaraAddEmitterToSystemWindow::Cancel)
		   ]
	   ];

	SNiagaraAssetBrowserWindow::FArguments WindowArgs;
	WindowArgs._WindowTitle = LOCTEXT("AddEmitterToSystemWindowTitle", "Add Emitter to your System");
	WindowArgs._AssetBrowserArgs = AssetBrowserArgs;
	
	SNiagaraAssetBrowserWindow::Construct(WindowArgs);
}

void SNiagaraAddEmitterToSystemWindow::OnAssetsActivatedInternal(const TArray<FAssetData>& AssetData, EAssetTypeActivationMethod::Type) const
{
	if(WeakSystemViewModel.IsValid())
	{
		if(AssetData.Num() == 0)
		{
			WeakSystemViewModel.Pin()->AddEmptyEmitter();
		}
		else
		{
			for(const FAssetData& Asset : AssetData)
			{
				WeakSystemViewModel.Pin()->AddEmitterFromAssetData(Asset);
			}
		}
	}
}

FReply SNiagaraAddEmitterToSystemWindow::AddEmptyEmitter()
{
	OnAssetsActivated({}, EAssetTypeActivationMethod::Opened);
	return FReply::Handled();
}

FReply SNiagaraAddEmitterToSystemWindow::AddSelectedEmitters()
{
	OnAssetsActivated(AssetBrowser->GetSelectedAssets(), EAssetTypeActivationMethod::Opened);
	return FReply::Handled();
}

FReply SNiagaraAddEmitterToSystemWindow::Cancel()
{
	RequestDestroyWindow();
	return FReply::Handled();
}

FText SNiagaraAddEmitterToSystemWindow::GetAddButtonTooltip() const
{
	return GetSelectedAssets().Num() > 0
	? LOCTEXT("AddEmitterButtonTooltip_ValidSelection", "Add the selected emitter to your System")
	: LOCTEXT("AddEmitterButtonTooltip_InvalidSelection", "Select an emitter to add to your System");
}


#undef LOCTEXT_NAMESPACE

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

// Copyright Epic Games, Inc. All Rights Reserved.

#include "NewAssetContextMenu.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Textures/SlateIcon.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Settings/ContentBrowserSettings.h"
#include "Factories/Factory.h"
#include "IAssetTools.h"
#include "IAssetTypeActions.h"
#include "AssetToolsModule.h"
#include "Widgets/SToolTip.h"
#include "IDocumentation.h"
#include "ClassIconFinder.h"
#include "AssetToolsModule.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"

struct FFactoryItem
{
	UFactory* Factory;
	FText DisplayName;

	FFactoryItem(UFactory* InFactory, const FText& InDisplayName)
		: Factory(InFactory)
		, DisplayName(InDisplayName)
	{
	}
};

struct FCategorySubMenuItem
{
	FText Name;
	TArray<FFactoryItem> Factories;
	TMap<FString, TSharedPtr<FCategorySubMenuItem>> Children;

	void SortSubMenus(FCategorySubMenuItem* SubMenu = nullptr)
	{
		if (!SubMenu)
		{
			SubMenu = this;
		}

		// Sort the factories by display name
		SubMenu->Factories.Sort([](const FFactoryItem& A, const FFactoryItem& B) -> bool
		{
			return A.DisplayName.CompareToCaseIgnored(B.DisplayName) < 0;
		});

		for (TPair<FString, TSharedPtr<FCategorySubMenuItem>>& Pair : SubMenu->Children)
		{
			if (Pair.Value.IsValid())
			{
				FCategorySubMenuItem* MenuData = Pair.Value.Get();
				SortSubMenus(MenuData);
			}
		}
	}
};

TArray<FFactoryItem> FindFactoriesInCategory(EAssetTypeCategories::Type AssetTypeCategory)
{
	TArray<FFactoryItem> FactoriesInThisCategory;

	static const FName NAME_AssetTools = "AssetTools";
	const IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(NAME_AssetTools).Get();
	TArray<UFactory*> Factories = AssetTools.GetNewAssetFactories();
	for (UFactory* Factory : Factories)
	{
		uint32 FactoryCategories = Factory->GetMenuCategories();
		if (FactoryCategories & AssetTypeCategory)
		{
			FactoriesInThisCategory.Emplace(Factory, Factory->GetDisplayName());
		}
	}

	return FactoriesInThisCategory;
}

class SFactoryMenuEntry : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SFactoryMenuEntry)
		: _Width(32)
		, _Height(32)
	{}
		SLATE_ARGUMENT(uint32, Width)
		SLATE_ARGUMENT(uint32, Height)
	SLATE_END_ARGS()

	/**
	 * Construct this widget.  Called by the SNew() Slate macro.
	 *
	 * @param	InArgs				Declaration used by the SNew() macro to construct this widget
	 * @param	Factory				The factory this menu entry represents
	 */
	void Construct(const FArguments& InArgs, UFactory* Factory)
	{
		const FName ClassThumbnailBrushOverride = Factory->GetNewAssetThumbnailOverride();
		const FSlateBrush* ClassThumbnail = nullptr;
		if (ClassThumbnailBrushOverride.IsNone())
		{
			ClassThumbnail = FClassIconFinder::FindThumbnailForClass(Factory->GetSupportedClass());
		}
		else
		{
			// Instead of getting the override thumbnail directly from the editor style here get it from the
			// ClassIconFinder since it may have additional styles registered which can be searched by passing
			// it as a default with no class to search for.
			ClassThumbnail = FClassIconFinder::FindThumbnailForClass(nullptr, ClassThumbnailBrushOverride);
		}

		FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
		TWeakPtr<IAssetTypeActions> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(Factory->GetSupportedClass());

		FLinearColor AssetColor = FLinearColor::White;
		if (AssetTypeActions.IsValid())
		{
			AssetColor = AssetTypeActions.Pin()->GetTypeColor();
		}

		ChildSlot
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.Padding(4, 0, 0, 0)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SOverlay)

					+SOverlay::Slot()
					[
						SNew(SBox)
						.WidthOverride(InArgs._Width + 4)
						.HeightOverride(InArgs._Height + 4)
						[
							SNew(SBorder)
							.BorderImage(FAppStyle::GetBrush("AssetThumbnail.AssetBackground"))
							.BorderBackgroundColor(AssetColor.CopyWithNewOpacity(0.3f))
							.Padding(2.0f)
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							[
								SNew(SImage)
								.Image(ClassThumbnail)
							]
						]
					]

					+SOverlay::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Bottom)
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
						.BorderBackgroundColor(AssetColor)
						.Padding(FMargin(0, FMath::Max(FMath::CeilToFloat(InArgs._Width * 0.025f), 3.0f), 0, 0))
					]
				]

			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(4, 0, 4, 0)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.Padding(0, 0, 0, 1)
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle("LevelViewportContextMenu.AssetLabel.Text.Font"))
					.Text(Factory->GetDisplayName())
				]
			]
		];

		SetToolTip(IDocumentation::Get()->CreateToolTip(Factory->GetToolTip(), nullptr, Factory->GetToolTipDocumentationPage(), Factory->GetToolTipDocumentationExcerpt()));
	}
};

void FNewAssetContextMenu::MakeContextMenu(
	UToolMenu* Menu,
	const TArray<FName>& InSelectedAssetPaths,
	const FOnImportAssetRequested& InOnImportAssetRequested,
	const FOnNewAssetRequested& InOnNewAssetRequested
	)
{
	if (InSelectedAssetPaths.Num() == 0)
	{
		return;
	}

	static const FName NAME_AssetTools = "AssetTools";
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(NAME_AssetTools);

	// Ensure we can modify assets at these paths
	{
		TArray<FString> SelectedAssetPathStrs;
		for (const FName& SelectedPath : InSelectedAssetPaths)
		{
			SelectedAssetPathStrs.Add(SelectedPath.ToString());
		}

		if (!AssetToolsModule.Get().AllPassWritableFolderFilter(SelectedAssetPathStrs))
		{
			return;
		}
	}

	const FCanExecuteAction CanExecuteAssetActionsDelegate = FCanExecuteAction::CreateLambda([NumSelectedAssetPaths = InSelectedAssetPaths.Num()]()
	{
		// We can execute asset actions when we only have a single asset path selected
		return NumSelectedAssetPaths == 1;
	});

	const FName FirstSelectedPath = (InSelectedAssetPaths.Num() > 0) ? InSelectedAssetPaths[0] : FName();

	// Import
	if (InOnImportAssetRequested.IsBound() && !FirstSelectedPath.IsNone())
	{
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("ContentBrowserGetContent");
			Section.AddMenuEntry(
				"ImportAsset",
				FText::Format(LOCTEXT("ImportAsset", "Import to {0}..."), FText::FromName(FirstSelectedPath)),
				LOCTEXT("ImportAssetTooltip_NewAsset", "Imports an asset from file to this folder."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Import"),
				FUIAction(
					FExecuteAction::CreateStatic(&FNewAssetContextMenu::ExecuteImportAsset, InOnImportAssetRequested, FirstSelectedPath),
					CanExecuteAssetActionsDelegate
					)
				).InsertPosition = FToolMenuInsert(NAME_None, EToolMenuInsertType::First);
		}
	}


	if (InOnNewAssetRequested.IsBound())
	{
		// Add Basic Asset
		{
			FToolMenuSection& Section = Menu->AddSection("ContentBrowserNewBasicAsset", LOCTEXT("CreateBasicAssetsMenuHeading", "Create Basic Asset"));
			CreateNewAssetMenuCategory(
				Menu,
				"ContentBrowserNewBasicAsset",
				EAssetTypeCategories::Basic,
				FirstSelectedPath,
				InOnNewAssetRequested,
				CanExecuteAssetActionsDelegate
				);
		}

		// Add Advanced Asset
		{
			FToolMenuSection& Section = Menu->AddSection("ContentBrowserNewAdvancedAsset", LOCTEXT("CreateAdvancedAssetsMenuHeading", "Create Advanced Asset"));

			TArray<FAdvancedAssetCategory> AdvancedAssetCategories;
			AssetToolsModule.Get().GetAllAdvancedAssetCategories(/*out*/ AdvancedAssetCategories);
			AdvancedAssetCategories.Sort([](const FAdvancedAssetCategory& A, const FAdvancedAssetCategory& B) {
				return (A.CategoryName.CompareToCaseIgnored(B.CategoryName) < 0);
			});

			for (const FAdvancedAssetCategory& AdvancedAssetCategory : AdvancedAssetCategories)
			{
				TArray<FFactoryItem> Factories = FindFactoriesInCategory(AdvancedAssetCategory.CategoryType);
				if (Factories.Num() > 0)
				{
					Section.AddSubMenu(
						NAME_None,
						AdvancedAssetCategory.CategoryName,
						FText::GetEmpty(),
						FNewToolMenuDelegate::CreateStatic(
							&FNewAssetContextMenu::CreateNewAssetMenuCategory,
							FName("Section"),
							AdvancedAssetCategory.CategoryType,
							FirstSelectedPath,
							InOnNewAssetRequested,
							FCanExecuteAction() // We handle this at this level, rather than at the sub-menu item level
						),
						FUIAction(
							FExecuteAction(),
							CanExecuteAssetActionsDelegate
						),
						EUserInterfaceActionType::Button
					);
				}
			}
		}
	}
}

void FNewAssetContextMenu::CreateNewAssetMenuCategory(UToolMenu* Menu, FName SectionName, EAssetTypeCategories::Type AssetTypeCategory, FName InPath, FOnNewAssetRequested InOnNewAssetRequested, FCanExecuteAction InCanExecuteAction)
{
	// Find UFactory classes that can create new objects in this category.
	TArray<FFactoryItem> FactoriesInThisCategory = FindFactoriesInCategory(AssetTypeCategory);
	if (FactoriesInThisCategory.Num() == 0)
	{
		return;
	}

	TSharedPtr<FCategorySubMenuItem> ParentMenuData = MakeShared<FCategorySubMenuItem>();
	for (FFactoryItem& Item : FactoriesInThisCategory)
	{
		FCategorySubMenuItem* SubMenu = ParentMenuData.Get();
		const TArray<FText>& CategoryNames = Item.Factory->GetMenuCategorySubMenus();
		for (FText CategoryName : CategoryNames)
		{
			const FString SourceString = CategoryName.BuildSourceString();
			if (TSharedPtr<FCategorySubMenuItem> SubMenuData = SubMenu->Children.FindRef(SourceString))
			{
				check(SubMenuData.IsValid());
				SubMenu = SubMenuData.Get();
			}
			else
			{
				TSharedPtr<FCategorySubMenuItem> NewSubMenu = MakeShared<FCategorySubMenuItem>();
				NewSubMenu->Name = CategoryName;
				SubMenu->Children.Add(SourceString, NewSubMenu);
				SubMenu = NewSubMenu.Get();
			}
		}
		SubMenu->Factories.Add(Item);
	}
	ParentMenuData->SortSubMenus();
	CreateNewAssetMenus(Menu, SectionName, ParentMenuData, InPath, InOnNewAssetRequested, InCanExecuteAction);
}

void FNewAssetContextMenu::CreateNewAssetMenus(UToolMenu* Menu, FName SectionName, TSharedPtr<FCategorySubMenuItem> SubMenuData, FName InPath, FOnNewAssetRequested InOnNewAssetRequested, FCanExecuteAction InCanExecuteAction)
{
	FToolMenuSection& Section = Menu->FindOrAddSection(SectionName);
	for (const FFactoryItem& FactoryItem : SubMenuData->Factories)
	{
		TWeakObjectPtr<UClass> WeakFactoryClass = FactoryItem.Factory->GetClass();

		Section.AddEntry(FToolMenuEntry::InitMenuEntry(
			NAME_None,
			FUIAction(
				FExecuteAction::CreateStatic(&FNewAssetContextMenu::ExecuteNewAsset, InOnNewAssetRequested, InPath, WeakFactoryClass),
				InCanExecuteAction
			),
			SNew(SFactoryMenuEntry, FactoryItem.Factory)));
	}

	if (SubMenuData->Children.Num() == 0)
	{
		return;
	}

	Section.AddSeparator(NAME_None);

	TArray<TSharedPtr<FCategorySubMenuItem>> SortedMenus;
	SubMenuData->Children.GenerateValueArray(SortedMenus);
	SortedMenus.Sort([](const TSharedPtr<FCategorySubMenuItem>& A, const TSharedPtr<FCategorySubMenuItem>& B) -> bool
	{
		return A->Name.CompareToCaseIgnored(B->Name) < 0;
	});

	for (TSharedPtr<FCategorySubMenuItem>& ChildMenuData : SortedMenus)
	{
		check(ChildMenuData.IsValid());

		Section.AddSubMenu(
			NAME_None,
			ChildMenuData->Name,
			FText::GetEmpty(),
			FNewToolMenuDelegate::CreateStatic(
				&FNewAssetContextMenu::CreateNewAssetMenus,
				FName("Section"),
				ChildMenuData,
				InPath,
				InOnNewAssetRequested,
				InCanExecuteAction
			),
			FUIAction(
				FExecuteAction(),
				InCanExecuteAction
			),
			EUserInterfaceActionType::Button
		);
	}
}

void FNewAssetContextMenu::ExecuteImportAsset(FOnImportAssetRequested InOnInportAssetRequested, FName InPath)
{
	InOnInportAssetRequested.ExecuteIfBound(InPath);
}

void FNewAssetContextMenu::ExecuteNewAsset(FOnNewAssetRequested InOnNewAssetRequested, FName InPath, TWeakObjectPtr<UClass> FactoryClass)
{
	if (ensure(FactoryClass.IsValid()) && ensure(!InPath.IsNone()))
	{
		InOnNewAssetRequested.ExecuteIfBound(InPath, FactoryClass);
	}
}

#undef LOCTEXT_NAMESPACE

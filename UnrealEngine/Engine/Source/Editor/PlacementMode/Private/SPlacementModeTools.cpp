// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPlacementModeTools.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SCheckBox.h"
#include "Styling/AppStyle.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "Framework/Application/SlateApplication.h"
#include "AssetThumbnail.h"
#include "LevelEditor.h"
#include "LevelEditorActions.h"
#include "LevelEditorViewport.h"
#include "ContentBrowserDataDragDropOp.h"
#include "EditorClassUtils.h"
#include "Widgets/Input/SSearchBox.h"
#include "ClassIconFinder.h"
#include "Widgets/Docking/SDockTab.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "AssetSelection.h"
#include "ActorFactories/ActorFactory.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "PlacementMode"

namespace PlacementModeTools
{
	bool bItemInternalsInTooltip = false;
	FAutoConsoleVariableRef CVarItemInternalsInTooltip(TEXT("PlacementMode.ItemInternalsInTooltip"), bItemInternalsInTooltip, TEXT("Shows placeable item internal information in its tooltip"));
}

struct FSortPlaceableItems
{
	static bool SortItemsByOrderThenName(const TSharedPtr<FPlaceableItem>& A, const TSharedPtr<FPlaceableItem>& B)
	{
		if (A->SortOrder.IsSet())
		{
			if (B->SortOrder.IsSet())
			{
				return A->SortOrder.GetValue() < B->SortOrder.GetValue();
			}
			else
			{
				return true;
			}
		}
		else if (B->SortOrder.IsSet())
		{
			return false;
		}
		else
		{
			return SortItemsByName(A, B);
		}
	}

	static bool SortItemsByName(const TSharedPtr<FPlaceableItem>& A, const TSharedPtr<FPlaceableItem>& B)
	{
		return A->DisplayName.CompareTo(B->DisplayName) < 0;
	}
};

namespace PlacementViewFilter
{
	void GetBasicStrings(const FPlaceableItem& InPlaceableItem, TArray<FString>& OutBasicStrings)
	{
		OutBasicStrings.Add(InPlaceableItem.DisplayName.ToString());

		if (!InPlaceableItem.NativeName.IsEmpty())
		{
			OutBasicStrings.Add(InPlaceableItem.NativeName);
		}

		const FString* SourceString = FTextInspector::GetSourceString(InPlaceableItem.DisplayName);
		if (SourceString)
		{
			OutBasicStrings.Add(*SourceString);
		}
	}
} // namespace PlacementViewFilter

/**
 * These are the asset thumbnails.
 */
class SPlacementAssetThumbnail : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SPlacementAssetThumbnail )
		: _Width( 32 )
		, _Height( 32 )
		, _AlwaysUseGenericThumbnail( false )
		, _AssetTypeColorOverride()
	{}

	SLATE_ARGUMENT( uint32, Width )

	SLATE_ARGUMENT( uint32, Height )

	SLATE_ARGUMENT( FName, ClassThumbnailBrushOverride )

	SLATE_ARGUMENT( bool, AlwaysUseGenericThumbnail )

	SLATE_ARGUMENT( TOptional<FLinearColor>, AssetTypeColorOverride )
	SLATE_END_ARGS()


	void Construct( const FArguments& InArgs, const FAssetData& InAsset)
	{
		Asset = InAsset;

		TSharedPtr<FAssetThumbnailPool> ThumbnailPool = UThumbnailManager::Get().GetSharedThumbnailPool();

		Thumbnail = MakeShareable(new FAssetThumbnail(Asset, InArgs._Width, InArgs._Height, ThumbnailPool));

		FAssetThumbnailConfig Config;
		Config.bForceGenericThumbnail = InArgs._AlwaysUseGenericThumbnail;
		Config.ClassThumbnailBrushOverride = InArgs._ClassThumbnailBrushOverride;
		Config.AssetTypeColorOverride = InArgs._AssetTypeColorOverride;
		ChildSlot
		[
			Thumbnail->MakeThumbnailWidget( Config )
		];
	}

private:

	FAssetData Asset;
	TSharedPtr< FAssetThumbnail > Thumbnail;
};

void SPlacementAssetEntry::Construct(const FArguments& InArgs, const TSharedPtr<const FPlaceableItem>& InItem)
{	
	bIsPressed = false;

	Item = InItem;

	TSharedPtr< SHorizontalBox > ActorType = SNew( SHorizontalBox );

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

	TSharedPtr<IToolTip> AssetEntryToolTip;
	if (PlacementModeTools::bItemInternalsInTooltip)
	{
		AssetEntryToolTip = FSlateApplicationBase::Get().MakeToolTip(
			FText::Format(LOCTEXT("ItemInternalsTooltip", "Native Name: {0}\nAsset Path: {1}\nFactory Class: {2}"), 
			FText::FromString(Item->NativeName), 
			FText::FromString(Item->AssetData.GetObjectPathString()),
			FText::FromString(Item->Factory ? Item->Factory->GetClass()->GetName() : TEXT("None"))));
	}

	UClass* DocClass = nullptr;
	if(DefaultActor != nullptr)
	{
		DocClass = DefaultActor->GetClass();
		if (!AssetEntryToolTip)
		{
			AssetEntryToolTip = FEditorClassUtils::GetTooltip(DefaultActor->GetClass());
		}
	}

	if (!AssetEntryToolTip)
	{
		AssetEntryToolTip = FSlateApplicationBase::Get().MakeToolTip(Item->DisplayName);
	}

	const FButtonStyle& ButtonStyle = FAppStyle::GetWidgetStyle<FButtonStyle>( "PlacementBrowser.Asset" );

	NormalImage = &ButtonStyle.Normal;
	HoverImage = &ButtonStyle.Hovered;
	PressedImage = &ButtonStyle.Pressed; 

	// Create doc link widget if there is a class to link to
	TSharedRef<SWidget> DocWidget = SNew(SSpacer);
	if(DocClass != NULL)
	{
		DocWidget = FEditorClassUtils::GetDocumentationLinkWidget(DocClass);
		DocWidget->SetCursor( EMouseCursor::Default );
	}

	ChildSlot
	.Padding(FMargin(8.f, 2.f, 12.f, 2.f))
	[

		SNew(SOverlay)

		+SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage( FAppStyle::Get().GetBrush("PlacementBrowser.Asset.Background"))
			.Cursor( EMouseCursor::GrabHand )
			.ToolTip( AssetEntryToolTip )
			.Padding(0)
			[

				SNew( SHorizontalBox )

				+ SHorizontalBox::Slot()
				.Padding(8.0f, 4.f)
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew( SBox )
					.WidthOverride(40)
					.HeightOverride(40)
					[
						SNew( SPlacementAssetThumbnail, Item->AssetData )
						.ClassThumbnailBrushOverride( Item->ClassThumbnailBrushOverride )
						.AlwaysUseGenericThumbnail( Item->bAlwaysUseGenericThumbnail )
						.AssetTypeColorOverride( FLinearColor::Transparent )
					]
				]

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Fill)
				.Padding(0)
				[

					SNew(SBorder)
					.BorderImage(FAppStyle::Get().GetBrush("PlacementBrowser.Asset.LabelBack"))
					[
						SNew( SHorizontalBox)
						+SHorizontalBox::Slot()
						.Padding(9, 0, 0, 1)
						.VAlign(VAlign_Center)
						[
							SNew( STextBlock )
							.TextStyle( FAppStyle::Get(), "PlacementBrowser.Asset.Name" )
							.Text( Item->DisplayName )
							.HighlightText(InArgs._HighlightText)
						]

						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							DocWidget
						]
					]
				]
			]
		]

		+SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage( this, &SPlacementAssetEntry::GetBorder )
			.Cursor( EMouseCursor::GrabHand )
			.ToolTip( AssetEntryToolTip )
		]
	];
}

FReply SPlacementAssetEntry::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		bIsPressed = true;

		return FReply::Handled().DetectDrag( SharedThis( this ), MouseEvent.GetEffectingButton() );
	}

	return FReply::Unhandled();
}

FReply SPlacementAssetEntry::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		bIsPressed = false;
	}

	return FReply::Unhandled();
}

FReply SPlacementAssetEntry::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	bIsPressed = false;

	if (FEditorDelegates::OnAssetDragStarted.IsBound())
	{
		TArray<FAssetData> DraggedAssetDatas;
		DraggedAssetDatas.Add( Item->AssetData );
		FEditorDelegates::OnAssetDragStarted.Broadcast( DraggedAssetDatas, Item->Factory );
		return FReply::Handled();
	}

	if( MouseEvent.IsMouseButtonDown( EKeys::LeftMouseButton ) )
	{
		return FReply::Handled().BeginDragDrop(FAssetDragDropOp::New(Item->AssetData, Item->AssetFactory));
	}
	else
	{
		return FReply::Handled();
	}
}

bool SPlacementAssetEntry::IsPressed() const
{
	return bIsPressed;
}

const FSlateBrush* SPlacementAssetEntry::GetBorder() const
{
	if ( IsPressed() )
	{
		return PressedImage;
	}
	else if ( IsHovered() )
	{
		return HoverImage;
	}
	else
	{
		return NormalImage;
	}
}


void SPlacementAssetMenuEntry::Construct(const FArguments& InArgs, const TSharedPtr<const FPlaceableItem>& InItem)
{	
	bIsPressed = false;

	check(InItem.IsValid());

	Item = InItem;

	AssetImage = nullptr;

	TSharedPtr< SHorizontalBox > ActorType = SNew( SHorizontalBox );

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

	UClass* DocClass = nullptr;
	TSharedPtr<IToolTip> AssetEntryToolTip;
	if(DefaultActor != nullptr)
	{
		DocClass = DefaultActor->GetClass();
		AssetEntryToolTip = FEditorClassUtils::GetTooltip(DefaultActor->GetClass());
	}

	if (!AssetEntryToolTip.IsValid())
	{
		AssetEntryToolTip = FSlateApplicationBase::Get().MakeToolTip(Item->DisplayName);
	}
	
	const FButtonStyle& ButtonStyle = FAppStyle::Get().GetWidgetStyle<FButtonStyle>( "Menu.Button" );
	const float MenuIconSize = FAppStyle::Get().GetFloat("Menu.MenuIconSize");

	Style = &ButtonStyle;

	// Create doc link widget if there is a class to link to
	TSharedRef<SWidget> DocWidget = SNew(SSpacer);
	if(DocClass != NULL)
	{
		DocWidget = FEditorClassUtils::GetDocumentationLinkWidget(DocClass);
		DocWidget->SetCursor( EMouseCursor::Default );
	}

	ChildSlot
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Fill)
	[
		SNew(SBorder)
		.BorderImage( this, &SPlacementAssetMenuEntry::GetBorder )
		.Cursor( EMouseCursor::GrabHand )
		.ToolTip( AssetEntryToolTip )
		.Padding(FMargin(27.f, 3.f, 5.f, 3.f))
		[
			SNew( SHorizontalBox )

			+ SHorizontalBox::Slot()
			.Padding(14.0f, 0.f, 10.f, 0.0f)
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(MenuIconSize)
				.HeightOverride(MenuIconSize)
				[
					SNew(SImage)
					.Image(this, &SPlacementAssetMenuEntry::GetIcon)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.Padding(1.f, 0.f, 0.f, 0.f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[

				SNew( STextBlock )
				.ColorAndOpacity(FSlateColor::UseForeground())
				.Text( Item->DisplayName )
			]


			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			.AutoWidth()
			[
				SNew(SImage)
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Image(FAppStyle::Get().GetBrush("Icons.DragHandle"))
			]
		]
	];
}


const FSlateBrush* SPlacementAssetMenuEntry::GetIcon() const
{

	if (AssetImage != nullptr)
	{
		return AssetImage;
	}

	if (Item->ClassIconBrushOverride != NAME_None)
	{
		AssetImage = FSlateIconFinder::FindCustomIconBrushForClass(nullptr, TEXT("ClassIcon"), Item->ClassIconBrushOverride);
	}
	else
	{
		AssetImage = FSlateIconFinder::FindIconBrushForClass(FClassIconFinder::GetIconClassForAssetData(Item->AssetData));
	}

	return AssetImage;
}


FReply SPlacementAssetMenuEntry::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		bIsPressed = true;

		return FReply::Handled().DetectDrag( SharedThis( this ), MouseEvent.GetEffectingButton() );
	}

	return FReply::Unhandled();
}

FReply SPlacementAssetMenuEntry::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		bIsPressed = false;

		UActorFactory* Factory = Item->Factory;
		if (!Item->Factory)
		{
			// If no actor factory was found or failed, add the actor from the uclass
			UClass* AssetClass = Item->AssetData.GetClass();
			if (AssetClass)
			{
				UObject* ClassObject = AssetClass->GetDefaultObject();
				FActorFactoryAssetProxy::GetFactoryForAssetObject(ClassObject);
			}
		}

		{
			// Note: Capture the add and the move within a single transaction, so that the placed actor position is calculated correctly by the transaction diff
			FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "CreateActor", "Create Actor"));

			AActor* NewActor = FLevelEditorActionCallbacks::AddActor(Factory, Item->AssetData, nullptr);
			if (NewActor && GCurrentLevelEditingViewportClient)
			{
				GEditor->MoveActorInFrontOfCamera(*NewActor,
					GCurrentLevelEditingViewportClient->GetViewLocation(),
					GCurrentLevelEditingViewportClient->GetViewRotation().Vector()
				);
			}
		}

		if (!MouseEvent.IsControlDown())
		{
			FSlateApplication::Get().DismissAllMenus();
		}

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SPlacementAssetMenuEntry::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	bIsPressed = false;

	if (FEditorDelegates::OnAssetDragStarted.IsBound())
	{
		TArray<FAssetData> DraggedAssetDatas;
		DraggedAssetDatas.Add( Item->AssetData );
		FEditorDelegates::OnAssetDragStarted.Broadcast( DraggedAssetDatas, Item->Factory );
		return FReply::Handled();
	}

	if( MouseEvent.IsMouseButtonDown( EKeys::LeftMouseButton ) )
	{
		return FReply::Handled().BeginDragDrop(FAssetDragDropOp::New(Item->AssetData, Item->AssetFactory));
	}
	else
	{
		return FReply::Handled();
	}
}

bool SPlacementAssetMenuEntry::IsPressed() const
{
	return bIsPressed;
}

const FSlateBrush* SPlacementAssetMenuEntry::GetBorder() const
{
	if ( IsPressed() )
	{
		return &(Style->Pressed);
	}
	else if ( IsHovered() )
	{
		return &(Style->Hovered);
	}
	else
	{
		return &(Style->Normal);
	}
}

FSlateColor SPlacementAssetMenuEntry::GetForegroundColor() const
{
	if (IsPressed())
	{
		return Style->PressedForeground;
	}
	else if (IsHovered())
	{
		return Style->HoveredForeground;
	}
	else
	{
		return Style->NormalForeground;
	}
}


SPlacementModeTools::~SPlacementModeTools()
{
	if ( IPlacementModeModule::IsAvailable() )
	{
		IPlacementModeModule& PlacementModeModule = IPlacementModeModule::Get();
		PlacementModeModule.OnRecentlyPlacedChanged().RemoveAll(this);
		PlacementModeModule.OnAllPlaceableAssetsChanged().RemoveAll(this);
		PlacementModeModule.OnPlacementModeCategoryListChanged().RemoveAll(this);
		PlacementModeModule.OnPlaceableItemFilteringChanged().RemoveAll(this);
	}
}

void SPlacementModeTools::Construct( const FArguments& InArgs, TSharedRef<SDockTab> ParentTab )
{
	bRefreshAllClasses = false;
	bRefreshRecentlyPlaced = false;
	bUpdateShownItems = true;

	ActiveTabName = FBuiltInPlacementCategories::Basic();

	ParentTab->SetOnTabDrawerOpened(FSimpleDelegate::CreateSP(this, &SPlacementModeTools::OnTabDrawerOpened));

	SearchTextFilter = MakeShareable(new FPlacementAssetEntryTextFilter(
		FPlacementAssetEntryTextFilter::FItemToStringArray::CreateStatic(&PlacementViewFilter::GetBasicStrings)
		));

	SAssignNew(CategoryFilterPtr, SUniformWrapPanel)
	.HAlign(HAlign_Center)
	.SlotPadding(FMargin(2.0f, 1.0f));

	UpdatePlacementCategories();

	TSharedRef<SScrollBar> ScrollBar = SNew(SScrollBar)
		.Thickness(FVector2D(9.0f, 9.0f));

	ChildSlot
	[
		SNew( SVerticalBox )

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(8)
			[
				SAssignNew( SearchBoxPtr, SSearchBox )
				.HintText(LOCTEXT("SearchPlaceables", "Search Classes"))
				.OnTextChanged(this, &SPlacementModeTools::OnSearchChanged)
				.OnTextCommitted(this, &SPlacementModeTools::OnSearchCommitted)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(8.f, 6.f, 8.f, 8.f))
			.HAlign(HAlign_Fill)
			.Visibility(this, &SPlacementModeTools::GetTabsVisibility)
			[
				CategoryFilterPtr.ToSharedRef()
			]
		]


		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(8.f, 6.f, 8.f, 8.f))
			.HAlign(HAlign_Center)
			.Visibility(this, &SPlacementModeTools::GetTabsVisibility)
			[
				SAssignNew(FilterLabelPtr, STextBlock)
				.Text(LOCTEXT("CategoryLabel", "CategoryLabel"))
				.Font(FAppStyle::Get().GetFontStyle("SmallFontBold"))
				.TransformPolicy(ETextTransformPolicy::ToUpper)
			]
		]

		+ SVerticalBox::Slot()
		.Padding(FMargin(0.0f, 3.f))
		[
			SNew(SOverlay)

			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Fill)
			.Padding(12.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoResultsFound", "No Results Found"))
				.Visibility(this, &SPlacementModeTools::GetFailedSearchVisibility)
			]

			+ SOverlay::Slot()
			[
				SAssignNew(CustomContent, SBox)
			]

			+ SOverlay::Slot()
			[
				SAssignNew(DataDrivenContent, SBox)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					[
						SAssignNew(ListView, SListView<TSharedPtr<FPlaceableItem>>)
						.ListItemsSource(&FilteredItems)
						.OnGenerateRow(this, &SPlacementModeTools::OnGenerateWidgetForItem)
						.ExternalScrollbar(ScrollBar)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						ScrollBar
					]
				]
			]
		]
	];

	IPlacementModeModule& PlacementModeModule = IPlacementModeModule::Get();
	PlacementModeModule.OnRecentlyPlacedChanged().AddSP(this, &SPlacementModeTools::RequestRefreshRecentlyPlaced);
	PlacementModeModule.OnAllPlaceableAssetsChanged().AddSP(this, &SPlacementModeTools::RequestRefreshAllClasses);
	PlacementModeModule.OnPlaceableItemFilteringChanged().AddSP(this, &SPlacementModeTools::RequestUpdateShownItems);
	PlacementModeModule.OnPlacementModeCategoryListChanged().AddSP(this, &SPlacementModeTools::UpdatePlacementCategories);
	PlacementModeModule.OnPlacementModeCategoryRefreshed().AddSP(this, &SPlacementModeTools::OnCategoryRefresh);
}

FName SPlacementModeTools::GetActiveTab() const
{
	return IsSearchActive() ? FBuiltInPlacementCategories::AllClasses() : ActiveTabName;
}

void SPlacementModeTools::SetActiveTab(FName TabName)
{
	if (TabName != ActiveTabName)
	{
		ActiveTabName = TabName;
		IPlacementModeModule::Get().RegenerateItemsForCategory(ActiveTabName);
	}
}

void SPlacementModeTools::UpdateShownItems()
{
	bUpdateShownItems = false;

	IPlacementModeModule& PlacementModeModule = IPlacementModeModule::Get();

	const FPlacementCategoryInfo* Category = PlacementModeModule.GetRegisteredPlacementCategory(GetActiveTab());
	if (!Category)
	{
		return;
	}
	else if (Category->CustomGenerator)
	{
		CustomContent->SetContent(Category->CustomGenerator());

		CustomContent->SetVisibility(EVisibility::Visible);
		DataDrivenContent->SetVisibility(EVisibility::Collapsed);

		FilterLabelPtr->SetText(Category->DisplayName);
	}
	else
	{
		FilteredItems.Reset();

		if (IsSearchActive())
		{
			auto Filter = [&](const TSharedPtr<FPlaceableItem>& Item) { return SearchTextFilter->PassesFilter(*Item); };
			PlacementModeModule.GetFilteredItemsForCategory(Category->UniqueHandle, FilteredItems, Filter);
			
			if (Category->bSortable)
			{
				FilteredItems.Sort(&FSortPlaceableItems::SortItemsByName);
			}
		}
		else
		{
			PlacementModeModule.GetItemsForCategory(Category->UniqueHandle, FilteredItems);

			if (Category->bSortable)
			{
				FilteredItems.Sort(&FSortPlaceableItems::SortItemsByOrderThenName);
			}
		}

		CustomContent->SetVisibility(EVisibility::Collapsed);
		DataDrivenContent->SetVisibility(EVisibility::Visible);
		ListView->RequestListRefresh();
		FilterLabelPtr->SetText(Category->DisplayName);
	}
}

bool SPlacementModeTools::IsSearchActive() const
{
	return !SearchTextFilter->GetRawFilterText().IsEmpty();
}

ECheckBoxState SPlacementModeTools::GetPlacementTabCheckedState( FName CategoryName ) const
{
	return ActiveTabName == CategoryName ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

EVisibility SPlacementModeTools::GetFailedSearchVisibility() const
{
	if (!IsSearchActive() || FilteredItems.Num())
	{
		return EVisibility::Collapsed;
	}
	return EVisibility::Visible;
}

EVisibility SPlacementModeTools::GetTabsVisibility() const
{
	return IsSearchActive() ? EVisibility::Collapsed : EVisibility::Visible;
}

TSharedRef<ITableRow> SPlacementModeTools::OnGenerateWidgetForItem(TSharedPtr<FPlaceableItem> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FPlaceableItem>>, OwnerTable)
		.Style(&FAppStyle::Get(), "PlacementBrowser.PlaceableItemRow")
		[
			SNew(SPlacementAssetEntry, InItem.ToSharedRef())
			.HighlightText(this, &SPlacementModeTools::GetHighlightText)
		];
}

void SPlacementModeTools::OnCategoryChanged(const ECheckBoxState NewState, FName InCategory)
{
	if (NewState == ECheckBoxState::Checked)
	{
		SetActiveTab(InCategory);
	}
}

void SPlacementModeTools::OnTabDrawerOpened()
{
	FSlateApplication::Get().SetKeyboardFocus(SearchBoxPtr, EFocusCause::SetDirectly);
}

void SPlacementModeTools::RequestUpdateShownItems()
{
	bUpdateShownItems = true;
}

void SPlacementModeTools::RequestRefreshRecentlyPlaced( const TArray< FActorPlacementInfo >& RecentlyPlaced )
{
	if (GetActiveTab() == FBuiltInPlacementCategories::RecentlyPlaced())
	{
		bRefreshRecentlyPlaced = true;
	}
}

void SPlacementModeTools::RequestRefreshAllClasses()
{
	if (GetActiveTab() == FBuiltInPlacementCategories::AllClasses())
	{
		bRefreshAllClasses = true;
	}
}

void SPlacementModeTools::OnCategoryRefresh(FName CategoryName)
{
	if (GetActiveTab() == CategoryName)
	{
		RequestUpdateShownItems();
	}
}

void SPlacementModeTools::UpdatePlacementCategories()
{
	bool BasicTabExists = false;
	FName TabToActivate;

	CategoryFilterPtr->ClearChildren();

	TArray<FPlacementCategoryInfo> Categories;
	IPlacementModeModule::Get().GetSortedCategories(Categories);
	for (const FPlacementCategoryInfo& Category : Categories)
	{
		if (Category.UniqueHandle == FBuiltInPlacementCategories::Basic())
		{
			BasicTabExists = true;
		}

		if (Category.UniqueHandle == ActiveTabName)
		{
			TabToActivate = ActiveTabName;
		}

		CategoryFilterPtr->AddSlot()
		[
			SNew(SCheckBox)
			.Padding(FMargin(4.f, 4.f))
			.Style( &FAppStyle::Get(),  "PaletteToolBar.Tab" )
			.OnCheckStateChanged(this, &SPlacementModeTools::OnCategoryChanged, Category.UniqueHandle)
			.IsChecked(this, &SPlacementModeTools::GetPlacementTabCheckedState, Category.UniqueHandle)
			.ToolTipText(Category.DisplayName)
			[
				SNew(SImage)
				.ColorAndOpacity(FSlateColor::UseForeground())
				.Image(Category.DisplayIcon.GetIcon())
			]
		];
	}

	if (TabToActivate.IsNone())
	{
		if (BasicTabExists)
		{
			TabToActivate = FBuiltInPlacementCategories::Basic();
		}
		else if (Categories.Num() > 0)
		{
			TabToActivate = Categories[0].UniqueHandle;
		}
	}

	SetActiveTab(TabToActivate);
}

void SPlacementModeTools::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if (bRefreshAllClasses)
	{
		IPlacementModeModule::Get().RegenerateItemsForCategory(FBuiltInPlacementCategories::AllClasses());
		bRefreshAllClasses = false;
	}

	if (bRefreshRecentlyPlaced)
	{
		IPlacementModeModule::Get().RegenerateItemsForCategory(FBuiltInPlacementCategories::RecentlyPlaced());
		bRefreshRecentlyPlaced = false;
	}

	if (bUpdateShownItems)
	{
		UpdateShownItems();
	}
}

void SPlacementModeTools::OnSearchChanged(const FText& InFilterText)
{
	// If the search text was previously empty we do a full rebuild of our cached widgets
	// for the placeable widgets.
	if ( !IsSearchActive() )
	{
		bRefreshAllClasses = true;
	}
	else
	{
		bUpdateShownItems = true;
	}

	SearchTextFilter->SetRawFilterText( InFilterText );
	SearchBoxPtr->SetError( SearchTextFilter->GetFilterErrorText() );
}

void SPlacementModeTools::OnSearchCommitted(const FText& InFilterText, ETextCommit::Type InCommitType)
{
	OnSearchChanged(InFilterText);
}

FText SPlacementModeTools::GetHighlightText() const
{
	return SearchTextFilter->GetRawFilterText();
}

#undef LOCTEXT_NAMESPACE
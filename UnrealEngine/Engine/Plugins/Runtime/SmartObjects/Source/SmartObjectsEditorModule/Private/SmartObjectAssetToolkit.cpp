// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectAssetToolkit.h"
#include "AssetEditorModeManager.h"
#include "Engine/StaticMesh.h"
#include "ScopedTransaction.h"
#include "SmartObjectAssetEditorViewportClient.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectTypes.h"
#include "SmartObjectViewModel.h"
#include "Viewports.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SScrollBox.h"
#include "SSmartObjectViewport.h"
#include "IStructureDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/SListView.h"
#include "Styling/StyleColors.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "SPositiveActionButton.h"
#include "PropertyPath.h"
#include "SmartObjectBindingExtension.h"

#define LOCTEXT_NAMESPACE "SmartObjectAssetToolkit"

const FName FSmartObjectAssetToolkit::PreviewSettingsTabID(TEXT("SmartObjectAssetToolkit_Preview"));
const FName FSmartObjectAssetToolkit::OutlinerTabID(TEXT("SmartObjectAssetToolkit_Outliner"));
const FName FSmartObjectAssetToolkit::SceneViewportTabID(TEXT("SmartObjectAssetToolkit_Viewport"));
const FName FSmartObjectAssetToolkit::DetailsTabID(TEXT("SmartObjectAssetToolkit_Details"));

/** Outliner item type. */
enum class ESmartObjectSlotItemType : uint8
{
	Object,
	ObjectDefinitionData,
	Slot,
	SlotDefinitionData,
};

/** Struct used to describe an item in the outliner. */
struct FSmartObjectOutlinerItem : public TSharedFromThis<FSmartObjectOutlinerItem>
{
	bool IsAllowedToDrag() const { return Type == ESmartObjectSlotItemType::Slot; }
	
	bool operator==(const FSmartObjectOutlinerItem& RHS) const { return ID == RHS.ID; }
	
	FGuid ID;
	ESmartObjectSlotItemType Type = ESmartObjectSlotItemType::Object;
	TWeakPtr<FSmartObjectOutlinerItem> Parent;
	TArray<TSharedPtr<FSmartObjectOutlinerItem>> ChildItems;
};

/** Outliner drag and drop operation. */
class FSmartObjectItemDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FSmartObjectItemDragDropOp, FDecoratedDragDropOp)

	static TSharedRef<FSmartObjectItemDragDropOp> New(TWeakPtr<FSmartObjectOutlinerItem> InItem, const FText InDescription)
	{
		TSharedRef<FSmartObjectItemDragDropOp> Operation = MakeShared<FSmartObjectItemDragDropOp>();
		Operation->Item = InItem;
		Operation->Description = InDescription;
		Operation->Construct();
		return Operation;
	}
	
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
	{
		return SNew(SBorder)
			.Visibility(EVisibility::Visible)
			.BorderImage(FAppStyle::GetBrush("Menu.Background"))
			[
				SNew(STextBlock)
				.Text(Description)
			];		
	}
	
	TWeakPtr<FSmartObjectOutlinerItem> Item;
	FText Description;
};


namespace UE::SmartObject::Editor
{
TSharedPtr<FSmartObjectOutlinerItem> FindItem(const FGuid ItemToFind, TConstArrayView<TSharedPtr<FSmartObjectOutlinerItem>> Items)
{
	for (const TSharedPtr<FSmartObjectOutlinerItem>& Item : Items)
	{
		if (Item.IsValid())
		{
			if (Item->ID == ItemToFind)
			{
				return Item;
			}

			const TSharedPtr<FSmartObjectOutlinerItem> ChildItem = FindItem(ItemToFind, Item->ChildItems);
			if (ChildItem.IsValid())
			{
				return ChildItem;
			}
		}
	}
			
	return nullptr;
}

void FlattenItemList(TArray<TSharedPtr<FSmartObjectOutlinerItem>>& InItems, TArray<TSharedPtr<FSmartObjectOutlinerItem>>& OutItems)
{
	for (TSharedPtr<FSmartObjectOutlinerItem>& Item : InItems)
	{
		if (Item.IsValid())
		{
			if (Item->ChildItems.Num() > 0)
			{
				FlattenItemList(Item->ChildItems, OutItems);
			}
			OutItems.Add(Item);
		}
	}
	InItems.Reset();
}
}; // UE::SmartObject::Editor

//----------------------------------------------------------------------//
// FSmartObjectAssetToolkit
//----------------------------------------------------------------------//
FSmartObjectAssetToolkit::FSmartObjectAssetToolkit(UAssetEditor* InOwningAssetEditor)
	: FBaseAssetToolkit(InOwningAssetEditor)
{
	FPreviewScene::ConstructionValues PreviewSceneArgs;
	AdvancedPreviewScene = MakeUnique<FAdvancedPreviewScene>(PreviewSceneArgs);

	// Apply small Z offset to not hide the grid
	constexpr float DefaultFloorOffset = 1.0f;
	AdvancedPreviewScene->SetFloorOffset(DefaultFloorOffset);

	// Setup our default layout
	StandaloneDefaultLayout = FTabManager::NewLayout(FName("SmartObjectAssetEditorLayout3"))
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.7f)
					->AddTab(SceneViewportTabID, ETabState::OpenedTab)
					->SetHideTabWell(true)
				)
				->Split
				(
					FTabManager::NewSplitter()
					->SetSizeCoefficient(0.3f)
					->SetOrientation(Orient_Vertical)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.3f)
						->AddTab(OutlinerTabID, ETabState::OpenedTab)
						->AddTab(PreviewSettingsTabID, ETabState::OpenedTab)
						->SetForegroundTab(OutlinerTabID)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.7f)
						->AddTab(DetailsTabID, ETabState::OpenedTab)
					)
				)
			)
		);
}

FSmartObjectAssetToolkit::~FSmartObjectAssetToolkit()
{
	if (ViewModel.IsValid())
	{
		ViewModel->Unregister();
	}
	SelectionChangedHandle.Reset();
	SlotsChangedHandle.Reset();
}

TSharedPtr<FEditorViewportClient> FSmartObjectAssetToolkit::CreateEditorViewportClient() const
{
	// Set our advanced preview scene in the EditorModeManager
	StaticCastSharedPtr<FAssetEditorModeManager>(EditorModeManager)->SetPreviewScene(AdvancedPreviewScene.Get());

	// Create and setup our custom viewport client
	SmartObjectViewportClient = MakeShared<FSmartObjectAssetEditorViewportClient>(SharedThis(this), AdvancedPreviewScene.Get());

	SmartObjectViewportClient->ViewportType = LVT_Perspective;
	SmartObjectViewportClient->SetViewLocation(EditorViewportDefs::DefaultPerspectiveViewLocation);
	SmartObjectViewportClient->SetViewRotation(EditorViewportDefs::DefaultPerspectiveViewRotation);

	return SmartObjectViewportClient;
}

void FSmartObjectAssetToolkit::SetEditingObject(UObject* InObject)
{
	// Override the default as we want to do our own selection of the details panel
}

void FSmartObjectAssetToolkit::PostInitAssetEditor()
{
	USmartObjectDefinition* Definition = Cast<USmartObjectDefinition>(GetEditingObject());
	check(Definition);

	if (!ViewModel)
	{
		ViewModel = FSmartObjectViewModel::Register(Definition);
		check(ViewModel.IsValid());
	}
	
	// Allow the viewport client to interact with the preview component
	checkf(SmartObjectViewportClient.IsValid(), TEXT("ViewportClient is created in CreateEditorViewportClient before calling PostInitAssetEditor"));
	SmartObjectViewportClient->SetSmartObjectDefinition(*Definition);
	SmartObjectViewportClient->SetViewModel(ViewModel);

	if (PreviewDetailsView.IsValid())
	{
		CachedPreviewData = MakeShared<FStructOnScope>(TBaseStructure<FSmartObjectDefinitionPreviewData>::Get(), (uint8*)&Definition->PreviewData);
		CachedPreviewData->SetPackage(Definition->GetPackage());
		PreviewDetailsView->SetStructureData(CachedPreviewData);
	}

	UpdatePreviewActor();
	UpdateItemList();

	SelectionChangedHandle = ViewModel->GetOnSelectionChanged().AddSP(this, &FSmartObjectAssetToolkit::HandleSelectionChanged);
	SlotsChangedHandle = ViewModel->GetOnSlotsChanged().AddSP(this, &FSmartObjectAssetToolkit::HandleSlotsChanged);
	
	// Register to be notified when properties are edited
	FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(this, &FSmartObjectAssetToolkit::OnPropertyChanged);

	UE::SmartObject::Delegates::OnParametersChanged.AddSP(this, &FSmartObjectAssetToolkit::OnParametersChanged);
}

void FSmartObjectAssetToolkit::OnParametersChanged(const USmartObjectDefinition& SmartObjectDefinition)
{
	if (ViewModel && ViewModel->GetAsset() == &SmartObjectDefinition)
	{
		// Accessible structs might be different after modifying parameters so forcing refresh
		// so the FStateTreeBindingExtension can rebuild the list of bindable structs
		if (DetailsAssetView.IsValid())
		{
			DetailsAssetView->ForceRefresh();
		}
	}
}

void FSmartObjectAssetToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	if (!AssetEditorTabsCategory.IsValid())
	{
		// Use the first child category of the local workspace root if there is one, otherwise use the root itself
		const TArray<TSharedRef<FWorkspaceItem>>& LocalCategories = InTabManager->GetLocalWorkspaceMenuRoot()->GetChildItems();
		AssetEditorTabsCategory = LocalCategories.Num() > 0 ? LocalCategories[0] : InTabManager->GetLocalWorkspaceMenuRoot();
	}

	InTabManager->RegisterTabSpawner(DetailsTabID, FOnSpawnTab::CreateSP(this, &FSmartObjectAssetToolkit::SpawnTab_SelectionDetails))
		.SetDisplayName(LOCTEXT("Details", "Details"))
		.SetGroup(AssetEditorTabsCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
	
	InTabManager->RegisterTabSpawner(SceneViewportTabID, FOnSpawnTab::CreateSP(this, &FSmartObjectAssetToolkit::SpawnTab_SceneViewport))
		.SetDisplayName(LOCTEXT("Viewport", "Viewport"))
		.SetGroup(AssetEditorTabsCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"));

	InTabManager->RegisterTabSpawner(OutlinerTabID, FOnSpawnTab::CreateSP(this, &FSmartObjectAssetToolkit::SpawnTab_Outliner))
		.SetDisplayName(LOCTEXT("Outliner", "Outliner"))
		.SetGroup(AssetEditorTabsCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

	InTabManager->RegisterTabSpawner(PreviewSettingsTabID, FOnSpawnTab::CreateSP(this, &FSmartObjectAssetToolkit::SpawnTab_PreviewSettings))
		.SetDisplayName(LOCTEXT("PreviewSettings", "Preview Settings"))
		.SetGroup(AssetEditorTabsCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Visibility"));
}

TSharedRef<SDockTab> FSmartObjectAssetToolkit::SpawnTab_SceneViewport(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == FSmartObjectAssetToolkit::SceneViewportTabID);

	const TSharedRef<SSmartObjectViewport> ViewportWidget = SNew(SSmartObjectViewport)
		.EditorViewportClient(StaticCastSharedPtr<FSmartObjectAssetEditorViewportClient>(ViewportClient))
		.AssetEditorToolkit(StaticCastSharedRef<FSmartObjectAssetToolkit>(AsShared()))
		.PreviewScene(AdvancedPreviewScene.Get());

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab).Label(LOCTEXT("ViewportTab_Title", "Viewport"));
	SpawnedTab->SetContent(ViewportWidget);

	return SpawnedTab;
}

TSharedRef<SDockTab> FSmartObjectAssetToolkit::SpawnTab_Outliner(const FSpawnTabArgs& Args)
{
	UpdateItemList();
	
	const TSharedPtr<SDockTab> OutlinerTab = SNew(SDockTab)
		.Label(LOCTEXT("OutlinerTitle", "Outliner"))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2)
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(0,0,4,0))
				[
					SNew(SPositiveActionButton)
					.Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
					.Text(LOCTEXT("AddSlot", "Add Slot"))
					.OnClicked_Lambda([this]()
					{
						if (ViewModel.IsValid())
						{
							const FGuid NewSlotID = ViewModel->AddSlot(FGuid());

							ViewModel->SetSelection({ NewSlotID });
							
							return FReply::Handled();
						}

						return FReply::Unhandled();
					})
				]
			]

			+ SVerticalBox::Slot()
			.FillHeight(1)
			.Padding(2)
			[
				SAssignNew(ItemTreeWidget, STreeView<TSharedPtr<FSmartObjectOutlinerItem>>)
				.TreeItemsSource(&ItemList)
				.OnGenerateRow(this, &FSmartObjectAssetToolkit::OnGenerateRow)
				.OnGetChildren(this, &FSmartObjectAssetToolkit::OnGetChildren)
				.OnSelectionChanged(this, &FSmartObjectAssetToolkit::OnOutlinerSelectionChanged)
				.OnContextMenuOpening(this, &FSmartObjectAssetToolkit::OnOutlinerContextMenu)
				.SelectionMode(ESelectionMode::Multi)
			]
		];
	
	return OutlinerTab.ToSharedRef();
}

TSharedRef<SDockTab> FSmartObjectAssetToolkit::SpawnTab_SelectionDetails(const FSpawnTabArgs& Args)
{
	DetailsTab = SNew(SDockTab).Label(LOCTEXT("DetailsTitle", "Details"));

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.bAllowSearch = true;

	DetailsAssetView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	DetailsAssetView->SetExtensionHandler(MakeShared<FSmartObjectDefinitionBindingExtension>());

	if (USmartObjectDefinition* Definition = CastChecked<USmartObjectDefinition>(GetEditingObject()))
	{
		if (!ViewModel)
		{
			ViewModel = FSmartObjectViewModel::Register(Definition);
			check(ViewModel.IsValid());
		}
		
		DetailsAssetView->SetObject(Definition);
	}

	DetailsTab->SetContent(DetailsAssetView.ToSharedRef());
	
	return DetailsTab.ToSharedRef();
}

void FSmartObjectAssetToolkit::UpdateDetailsSelection()
{
	if (!DetailsTab.IsValid()
		|| !ViewModel.IsValid())
	{
		return;
	}

	USmartObjectDefinition* Definition = ViewModel->GetAsset();
	if (!Definition)
	{
		return;
	}

	// Find the type of last selection and required indices for finding the item. 
	bool bIsSelectedAsset = false; 
	int32 SelectedSlotIndex = INDEX_NONE;
	int32 SelectedDefinitionDataIndex = INDEX_NONE;

	const TConstArrayView<FGuid> Selection = ViewModel->GetSelection();

	for (int32 Index = Selection.Num() - 1; Index >= 0; Index--)
	{
		const FGuid ItemID = Selection[Index];

		// If the selected item is empty guid, we have selected the asset in the outliner view.
		if (!ItemID.IsValid())
		{
			bIsSelectedAsset = true;
			break;
		}

		// Find selected slot and definition data (if feasible) indices.
		if (Definition->FindSlotAndDefinitionDataIndexByID(ItemID, SelectedSlotIndex, SelectedDefinitionDataIndex))
		{
			break;
		}
	}
	
	FPropertyPath HighlightPath;

	if (SelectedDefinitionDataIndex != INDEX_NONE)
	{
		// Selected definition data.
		FArrayProperty* SlotsProperty = CastFieldChecked<FArrayProperty>(USmartObjectDefinition::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(USmartObjectDefinition, Slots)));

		HighlightPath.AddProperty(FPropertyInfo(SlotsProperty));
		HighlightPath.AddProperty(FPropertyInfo(SlotsProperty->Inner, SelectedSlotIndex));

		FArrayProperty* DefinitionDataProperty = CastFieldChecked<FArrayProperty>(TBaseStructure<FSmartObjectSlotDefinition>::Get()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FSmartObjectSlotDefinition, DefinitionData)));

		HighlightPath.AddProperty(FPropertyInfo(DefinitionDataProperty));
		HighlightPath.AddProperty(FPropertyInfo(DefinitionDataProperty->Inner, SelectedDefinitionDataIndex));
	}
	else if (SelectedSlotIndex != INDEX_NONE)
	{
		// Selected slot
		FArrayProperty* SlotsProperty = CastFieldChecked<FArrayProperty>(USmartObjectDefinition::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(USmartObjectDefinition, Slots)));

		HighlightPath.AddProperty(FPropertyInfo(SlotsProperty));
		HighlightPath.AddProperty(FPropertyInfo(SlotsProperty->Inner, SelectedSlotIndex));
	}
	else if (bIsSelectedAsset)
	{
		// Beginning of the asset
		FArrayProperty* DefaultBehaviorDefinitionsProperty = CastFieldChecked<FArrayProperty>(USmartObjectDefinition::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(USmartObjectDefinition, DefaultBehaviorDefinitions)));
		HighlightPath.AddProperty(FPropertyInfo(DefaultBehaviorDefinitionsProperty));
	}
	
	if (DetailsAssetView.IsValid())
	{
		DetailsAssetView->ScrollPropertyIntoView(HighlightPath);
	}
}

FText FSmartObjectAssetToolkit::GetOutlinerItemDescription(TSharedPtr<FSmartObjectOutlinerItem> Item) const
{
	if (!Item.IsValid())
	{
		return FText::GetEmpty();
	}
	const USmartObjectDefinition* Definition = CastChecked<USmartObjectDefinition>(GetEditingObject());
	if (!Definition)
	{
		return FText::GetEmpty();
	}
	
	if (Item->Type == ESmartObjectSlotItemType::Object)
	{
		return FText::FromString(Definition->GetName());
	}
	else if (Item->Type == ESmartObjectSlotItemType::ObjectDefinitionData)
	{
		const FSmartObjectDefinitionDataProxy* DataProxy = Definition->DefinitionData.FindByPredicate([&ID = Item->ID](const FSmartObjectDefinitionDataProxy& DataProxy)
		{
			return DataProxy.ID == ID;
		});
		if (DataProxy && DataProxy->Data.GetScriptStruct() != nullptr)
		{
			return DataProxy->Data.GetScriptStruct()->GetDisplayNameText();
		}
		return LOCTEXT("None", "None");
	}
	else if (Item->Type == ESmartObjectSlotItemType::Slot)
	{
		const int32 SlotIndex = Definition->FindSlotByID(Item->ID);
		if (SlotIndex != INDEX_NONE)
		{
			const FSmartObjectSlotDefinition& SlotDefinition = Definition->GetSlot(SlotIndex);
			return FText::FromName(SlotDefinition.Name);
		}
	}
	else if (Item->Type == ESmartObjectSlotItemType::SlotDefinitionData)
	{
		int32 SlotIndex = INDEX_NONE;
		int32 DefinitionDataIndex = INDEX_NONE;
		if (Definition->FindSlotAndDefinitionDataIndexByID(Item->ID, SlotIndex, DefinitionDataIndex))
		{
			const FSmartObjectSlotDefinition& SlotDefinition = Definition->GetSlot(SlotIndex);
			const FSmartObjectDefinitionDataProxy& DataProxy = SlotDefinition.DefinitionData[DefinitionDataIndex];
			if (DataProxy.Data.GetScriptStruct() != nullptr)
			{
				return DataProxy.Data.GetScriptStruct()->GetDisplayNameText();
			}
		}
		return LOCTEXT("None", "None");
	}
	
	return FText::GetEmpty();
}

FSlateColor FSmartObjectAssetToolkit::GetOutlinerItemColor(TSharedPtr<FSmartObjectOutlinerItem> Item) const
{
	if (!Item.IsValid())
	{
		return FColor::Silver;
	}
	const USmartObjectDefinition* Definition = CastChecked<USmartObjectDefinition>(GetEditingObject());
	if (!Definition)
	{
		return FColor::Silver;
	}
	
	if (Item->Type == ESmartObjectSlotItemType::Object)
	{
		return FColor::Silver;
	}
	else if (Item->Type == ESmartObjectSlotItemType::ObjectDefinitionData)
	{
		return FColor::Silver;
	}
	else if (Item->Type == ESmartObjectSlotItemType::Slot)
	{
		const int32 SlotIndex = Definition->FindSlotByID(Item->ID);
		if (SlotIndex != INDEX_NONE)
		{
			const FSmartObjectSlotDefinition& Slot = Definition->GetSlot(SlotIndex);
			return Slot.DEBUG_DrawColor;
		}
		return FColor::Silver;
	}
	else if (Item->Type == ESmartObjectSlotItemType::SlotDefinitionData)
	{
		return FColor::Silver;
	}
	
	return FColor::Silver;
}

TSharedRef<ITableRow> FSmartObjectAssetToolkit::OnGenerateRow(TSharedPtr<FSmartObjectOutlinerItem> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow< TSharedPtr<FSmartObjectOutlinerItem> >, OwnerTable)
		.ShowSelection(true)
		.OnDragDetected(this, &FSmartObjectAssetToolkit::OnOutlinerDragDetected)
		.OnCanAcceptDrop(this, &FSmartObjectAssetToolkit::OnOutlinerCanAcceptDrop)
		.OnAcceptDrop(this, &FSmartObjectAssetToolkit::OnOutlinerAcceptDrop)
		[
			SNew(SBox)
			.Padding(FMargin(0, 2))
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(5, 0, 0, 0)
				[
					SNew(SImage)
					.Image(FCoreStyle::Get().GetBrush("VerticalBoxDragIndicatorShort"))
					.Visibility_Lambda([InItem]()
					{
						return InItem->IsAllowedToDrag() ? EVisibility::Visible : EVisibility::Hidden;
					})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(5, 0, 0, 0)
				[
					SNew(SImage)
					.Image_Lambda([InItem]() -> const FSlateBrush*
					{
						if (InItem->Type == ESmartObjectSlotItemType::Object)
						{
							return FAppStyle::Get().GetBrush("Icons.Settings");
						}
						if (InItem->Type == ESmartObjectSlotItemType::ObjectDefinitionData)
						{
							return FAppStyle::Get().GetBrush("SCS.Component");
						}
						if (InItem->Type == ESmartObjectSlotItemType::Slot)
						{
							return FAppStyle::Get().GetBrush("Icons.Transform");
						}
						if (InItem->Type == ESmartObjectSlotItemType::SlotDefinitionData)
						{
							return FAppStyle::Get().GetBrush("SCS.Component");
						}
						return nullptr;
					})
					.ColorAndOpacity(this, &FSmartObjectAssetToolkit::GetOutlinerItemColor, InItem)
				]

				+ SHorizontalBox::Slot()
				.Padding(FMargin(10, 0, 0, 0))
				[
					SNew(STextBlock)
					.Font_Lambda([InItem]() -> FSlateFontInfo
					{
						if (InItem->Type == ESmartObjectSlotItemType::Object)
						{
							return FCoreStyle::Get().GetFontStyle("BoldFont");
						}
						return FCoreStyle::Get().GetFontStyle("NormalFont");
					})
					.MinDesiredWidth(150)
					.Text(this, &FSmartObjectAssetToolkit::GetOutlinerItemDescription, InItem)
					.Justification(ETextJustify::Left)
				]
			]
		];
}

void FSmartObjectAssetToolkit::OnGetChildren(TSharedPtr<FSmartObjectOutlinerItem> InItem, TArray<TSharedPtr<FSmartObjectOutlinerItem>>& OutChildren) const
{
	OutChildren = InItem->ChildItems;
}

void FSmartObjectAssetToolkit::OnOutlinerSelectionChanged(TSharedPtr<FSmartObjectOutlinerItem> SelectedItem, ESelectInfo::Type SelectType)
{
	if (bUpdatingOutlinerSelection
		|| !ItemTreeWidget.IsValid()
		|| !ViewModel.IsValid())
	{
		return;
	}

	TGuardValue<bool> UpdatingOutlinerSelectionGuard(bUpdatingViewSelection, true);

	const TArray<TSharedPtr<FSmartObjectOutlinerItem>>& OutlinerSelectedItems = ItemTreeWidget->GetSelectedItems();
	TArray<FGuid> SelectedItems;

	for (const TSharedPtr<FSmartObjectOutlinerItem>& OutlinerItem : OutlinerSelectedItems)
	{
		if (OutlinerItem.IsValid())
		{
			SelectedItems.Add(OutlinerItem->ID);
		}
	}
	
	ViewModel->SetSelection(SelectedItems);
	
	UpdateDetailsSelection();
}

TSharedPtr<SWidget> FSmartObjectAssetToolkit::OnOutlinerContextMenu()
{
	TArray<TSharedPtr<FSmartObjectOutlinerItem>> OutlinerSelectedItems = ItemTreeWidget->GetSelectedItems();
	if (OutlinerSelectedItems.IsEmpty())
	{
		return {};
	}

	TSharedPtr<FSmartObjectOutlinerItem> OutlinerSelectedItem = OutlinerSelectedItems[0];
	if (!OutlinerSelectedItem.IsValid())
	{
		return {};
	}

	const bool bIsSlot = OutlinerSelectedItem->Type == ESmartObjectSlotItemType::Slot;
	
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection*/true, /*InCommandList*/nullptr);

	if (bIsSlot)
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("AddSlot", "Add Slot"),
			FText::GetEmpty(),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"),
			FUIAction(
				FExecuteAction::CreateLambda([ViewModel = ViewModel, SlotID = OutlinerSelectedItem->ID]()
				{
					if (ViewModel.IsValid())
					{
						ViewModel->AddSlot(SlotID);
					}
				}))
			);
		
		MenuBuilder.AddMenuEntry(
			LOCTEXT("RemoveSlot", "Remove Slot"),
			 FText::GetEmpty(),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"),
			FUIAction(
				FExecuteAction::CreateLambda([ViewModel = ViewModel, SlotID = OutlinerSelectedItem->ID]()
				{
					if (ViewModel.IsValid())
					{
						ViewModel->RemoveSlot(SlotID);
					}
				}))
			);
	}
	
	return MenuBuilder.MakeWidget();
}

void FSmartObjectAssetToolkit::HandleSelectionChanged(TConstArrayView<FGuid> InSelection)
{
	if (bUpdatingViewSelection)
	{
		return;
	}
	
	TGuardValue<bool> UpdatingOutlinerSelectionGuard(bUpdatingOutlinerSelection, true);
	
	ItemTreeWidget->ClearSelection();
	
	TArray<TSharedPtr<FSmartObjectOutlinerItem>> Selection;
	Selection.Reserve(InSelection.Num());
	for (const FGuid& Item : InSelection)
	{
		TSharedPtr<FSmartObjectOutlinerItem> OutlinerItem = UE::SmartObject::Editor::FindItem(Item, ItemList);
		if (OutlinerItem.IsValid())
		{
			Selection.Add(OutlinerItem);
			
			// Make sure selected is visible, by expanding parents.
			TSharedPtr<FSmartObjectOutlinerItem> ExpandingOutlinerItem = OutlinerItem->Parent.Pin();
			while (ExpandingOutlinerItem.IsValid())
			{
				ItemTreeWidget->SetItemExpansion(ExpandingOutlinerItem, true);
				ExpandingOutlinerItem = ExpandingOutlinerItem->Parent.Pin();
			}
		}
	}
	ItemTreeWidget->SetItemSelection(Selection, true);

	UpdateDetailsSelection();
}

void FSmartObjectAssetToolkit::HandleSlotsChanged(USmartObjectDefinition* Definition)
{
	UpdateItemList();
	UpdateDetailsSelection();
}

FReply FSmartObjectAssetToolkit::OnOutlinerDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) const
{
	const TArray<TSharedPtr<FSmartObjectOutlinerItem>> SelectedItems = ItemTreeWidget.Get()->GetSelectedItems();
	if (SelectedItems.Num() != 1)
	{
		return FReply::Unhandled();
	}

	if (!SelectedItems[0]->IsAllowedToDrag())
	{
		return FReply::Unhandled();
	}
	
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		const TSharedPtr<FSmartObjectOutlinerItem> DraggedItem = SelectedItems[0];
		const TSharedRef<FSmartObjectItemDragDropOp> DragDropOp = FSmartObjectItemDragDropOp::New(DraggedItem, GetOutlinerItemDescription(DraggedItem));
		return FReply::Handled().BeginDragDrop(DragDropOp);
	}

	return FReply::Unhandled();
}

TOptional<EItemDropZone> FSmartObjectAssetToolkit::OnOutlinerCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, const TSharedPtr<FSmartObjectOutlinerItem> TargetItem) const
{
	const USmartObjectDefinition* Definition = CastChecked<USmartObjectDefinition>(GetEditingObject());
	if (!Definition)
	{
		return {};
	}
	
	const TSharedPtr<FSmartObjectItemDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FSmartObjectItemDragDropOp>();
	if (!DragDropOp.IsValid())
	{
		return {};
	}
	
	const TSharedPtr<FSmartObjectOutlinerItem> SourceItem = DragDropOp->Item.Pin();
	if (!SourceItem.IsValid() || !TargetItem.IsValid())
	{
		return {};
	}

	int32 SourceSlotIndex = INDEX_NONE;
	int32 SourceDataDefinitionIndex = INDEX_NONE;
	Definition->FindSlotAndDefinitionDataIndexByID(SourceItem->ID, SourceSlotIndex, SourceDataDefinitionIndex);

	int32 TargetSlotIndex = INDEX_NONE;
	int32 TargetDataDefinitionIndex = INDEX_NONE;
	Definition->FindSlotAndDefinitionDataIndexByID(TargetItem->ID, TargetSlotIndex, TargetDataDefinitionIndex);

	if (SourceItem->Type == ESmartObjectSlotItemType::Slot
		&& (TargetItem->Type == ESmartObjectSlotItemType::Slot
			|| TargetItem->Type == ESmartObjectSlotItemType::SlotDefinitionData))
	{
		if (TargetSlotIndex < SourceSlotIndex)
		{
			return EItemDropZone::AboveItem;
		}
		if (TargetSlotIndex > SourceSlotIndex)
		{
			return EItemDropZone::BelowItem;
		}
	}
	
	return {};
}

FReply FSmartObjectAssetToolkit::OnOutlinerAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, const TSharedPtr<FSmartObjectOutlinerItem> TargetItem)
{
	if (!ViewModel.IsValid())
	{
		return FReply::Unhandled();
	}
	
	const TSharedPtr<FSmartObjectItemDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FSmartObjectItemDragDropOp>();
	if (!DragDropOp.IsValid())
	{
		return FReply::Unhandled();
	}

	const TSharedPtr<FSmartObjectOutlinerItem> SourceItem = DragDropOp->Item.Pin();
	if (!SourceItem.IsValid() || !TargetItem.IsValid())
	{
		return FReply::Unhandled();
	}

	ViewModel->MoveSlot(SourceItem->ID, TargetItem->ID);
	
	UpdateCachedPreviewDataFromDefinition();
	UpdateDetailsSelection();
	
	return FReply::Handled();
}

TSharedRef<SDockTab> FSmartObjectAssetToolkit::SpawnTab_PreviewSettings(const FSpawnTabArgs& Args)
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	FStructureDetailsViewArgs StructureViewArgs;
	StructureViewArgs.bShowObjects = true;
	StructureViewArgs.bShowAssets = true;
	StructureViewArgs.bShowClasses = true;
	StructureViewArgs.bShowInterfaces = true;

	PreviewDetailsView = PropertyEditorModule.CreateStructureDetailView(DetailsViewArgs, StructureViewArgs, nullptr);
	PreviewDetailsView->GetOnFinishedChangingPropertiesDelegate().AddLambda([&](const FPropertyChangedEvent& PropertyChangedEvent)
	{
		USmartObjectDefinition* Definition = Cast<USmartObjectDefinition>(GetEditingObject());
		// Ignore temporary interaction (dragging sliders, etc.)
		if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet
			&& CachedPreviewData.IsValid()
			&& Definition)
		{
			{
				const FText PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetDisplayNameText() : FText::GetEmpty(); 
				FScopedTransaction Transaction(FText::Format(LOCTEXT("OnPreviewValueChanged", "Set {0}"), PropertyName));
				Definition->Modify();

				if (const FSmartObjectDefinitionPreviewData* PreviewData = reinterpret_cast<const FSmartObjectDefinitionPreviewData*>(CachedPreviewData->GetStructMemory()))
				{
					Definition->PreviewData = *PreviewData;
				}
			}
			
			UpdatePreviewActor();
		}
	});

	if (USmartObjectDefinition* Definition = Cast<USmartObjectDefinition>(GetEditingObject()))
	{
		CachedPreviewData = MakeShared<TStructOnScope<FSmartObjectDefinitionPreviewData>>(Definition->PreviewData);
		CachedPreviewData->SetPackage(Definition->GetPackage());
		PreviewDetailsView->SetStructureData(CachedPreviewData);
	}
	
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("PreviewSettingsTitle", "Preview Settings"))
		[
			PreviewDetailsView->GetWidget().ToSharedRef()
		];

	return SpawnedTab;
}

void FSmartObjectAssetToolkit::PostUndo(bool bSuccess)
{
	UpdateItemList();
	UpdateDetailsSelection();
	UpdateCachedPreviewDataFromDefinition();
}

void FSmartObjectAssetToolkit::PostRedo(bool bSuccess)
{
	UpdateItemList();
	UpdateDetailsSelection();
	UpdateCachedPreviewDataFromDefinition();
}

void FSmartObjectAssetToolkit::UpdateCachedPreviewDataFromDefinition()
{
	const USmartObjectDefinition* Definition = Cast<USmartObjectDefinition>(GetEditingObject());
	if (Definition
		&& CachedPreviewData.IsValid())
	{
		if (FSmartObjectDefinitionPreviewData* PreviewData = reinterpret_cast<FSmartObjectDefinitionPreviewData*>(CachedPreviewData->GetStructMemory()))
		{
			*PreviewData = Definition->PreviewData;
		}
		
		UpdateDetailsSelection();
		PreviewDetailsView->GetDetailsView()->ForceRefresh();
		UpdatePreviewActor();
	}
}

void FSmartObjectAssetToolkit::UpdatePreviewActor()
{
	SmartObjectViewportClient->ResetPreviewActor();
	
	const USmartObjectDefinition* Definition = CastChecked<USmartObjectDefinition>(GetEditingObject());
	if (!Definition)
	{
		return;
	}

	if (!Definition->PreviewData.ObjectActorClass.IsNull())
	{
		SmartObjectViewportClient->SetPreviewActorClass(Definition->PreviewData.ObjectActorClass.LoadSynchronous());
	}
	else if (Definition->PreviewData.ObjectMeshPath.IsValid())
	{
		UStaticMesh* PreviewMesh = Cast<UStaticMesh>(Definition->PreviewData.ObjectMeshPath.TryLoad());
		if (PreviewMesh)
		{
			SmartObjectViewportClient->SetPreviewMesh(PreviewMesh);
		}
	}
}

void FSmartObjectAssetToolkit::UpdateItemList()
{
	const USmartObjectDefinition* Definition = CastChecked<USmartObjectDefinition>(GetEditingObject());
	if (!Definition)
	{
		return;
	}

	// Flatten old items to make it easy to reuse them.
	TArray<TSharedPtr<FSmartObjectOutlinerItem>> OldItemList;
	UE::SmartObject::Editor::FlattenItemList(ItemList, OldItemList);

	// Creates item by recycling old items. Recycling allows selections to persist across updates.
	auto CreateItem = [&OldItemList](const FGuid ID, const ESmartObjectSlotItemType Type, TSharedPtr<FSmartObjectOutlinerItem> Parent) -> TSharedPtr<FSmartObjectOutlinerItem>
	{
		const int32 MatchIndex = OldItemList.IndexOfByPredicate([ID, Type](const TSharedPtr<FSmartObjectOutlinerItem>& Item)
		{
			return Item->ID == ID && Item->Type == Type;
		});
		
		if (MatchIndex != INDEX_NONE)
		{
			TSharedPtr<FSmartObjectOutlinerItem> Item = OldItemList[MatchIndex];
			OldItemList.RemoveAtSwap(MatchIndex);
			return Item;
		}
		
		TSharedPtr<FSmartObjectOutlinerItem> NewItem = MakeShared<FSmartObjectOutlinerItem>();
		NewItem->ID = ID;
		NewItem->Type = Type;
		NewItem->Parent = Parent;
		return NewItem;
	};
	
	// Object
	{
		TSharedPtr<FSmartObjectOutlinerItem> Item = CreateItem(FGuid(), ESmartObjectSlotItemType::Object, /*Parent*/nullptr);
		ItemList.Add(Item);
		
		// Definition data
		for (const FSmartObjectDefinitionDataProxy& DataProxy : Definition->DefinitionData)
		{
			TSharedPtr<FSmartObjectOutlinerItem> DataOutlinerItem = CreateItem(DataProxy.ID, ESmartObjectSlotItemType::ObjectDefinitionData, Item);
			Item->ChildItems.Add(DataOutlinerItem);
		}
	}

	// Slots
	for (const FSmartObjectSlotDefinition& Slot : Definition->GetSlots())
	{
		TSharedPtr<FSmartObjectOutlinerItem> SlotOutlinerItem = CreateItem(Slot.ID, ESmartObjectSlotItemType::Slot, /*Parent*/nullptr);
		ItemList.Add(SlotOutlinerItem);

		// Definition data
		for (const FSmartObjectDefinitionDataProxy& DataProxy : Slot.DefinitionData)
		{
			TSharedPtr<FSmartObjectOutlinerItem> DataOutlinerItem = CreateItem(DataProxy.ID, ESmartObjectSlotItemType::SlotDefinitionData, SlotOutlinerItem);
			SlotOutlinerItem->ChildItems.Add(DataOutlinerItem);
		}
	}

	if (ItemTreeWidget)
	{
		ItemTreeWidget->RequestTreeRefresh();
	}
}


void FSmartObjectAssetToolkit::OnClose()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
	FBaseAssetToolkit::OnClose();
}

void FSmartObjectAssetToolkit::AddReferencedObjects(FReferenceCollector& Collector)
{
}

void FSmartObjectAssetToolkit::OnPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent)
{
	if (ObjectBeingModified == nullptr || ObjectBeingModified != GetEditingObject())
	{
		return;
	}

	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(USmartObjectDefinition, Slots)
		|| PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(USmartObjectDefinition, DefinitionData))
	{
		GEditor->GetTimerManager()->SetTimerForNextTick([this]()
		{
			UpdateItemList();
		});
	}
}

#undef LOCTEXT_NAMESPACE

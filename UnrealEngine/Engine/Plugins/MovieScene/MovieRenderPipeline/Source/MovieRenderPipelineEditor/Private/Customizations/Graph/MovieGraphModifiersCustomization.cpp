// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieGraphModifiersCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Graph/MovieGraphConfig.h"
#include "Graph/MovieGraphSharedWidgets.h"
#include "Graph/MovieGraphRenderLayerSubsystem.h"
#include "Graph/Nodes/MovieGraphCollectionNode.h"
#include "Graph/Nodes/MovieGraphModifierNode.h"
#include "ScopedTransaction.h"
#include "Widgets/Images/SLayeredImage.h"
#include "Widgets/Layout/SWidgetSwitcher.h"

#define LOCTEXT_NAMESPACE "MovieGraphModifiersCustomization"

/** Discovers collections that are pickable from a specific graph, and presents them in a list. */
class SMovieGraphCollectionPicker final : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnCollectionPicked, FName);
	DECLARE_DELEGATE_RetVal_OneParam(bool, FOnFilter, FName);

	SLATE_BEGIN_ARGS(SMovieGraphCollectionPicker)
		{}
		/** The graph to begin discovering collections from. */
		SLATE_ATTRIBUTE(UMovieGraphConfig*, Graph)

		/** Called when a collection is picked in the list. */
		SLATE_EVENT(FOnCollectionPicked, OnCollectionPicked);

		/** Optional filter that can prevent discovered collections from showing up in the list. */
		SLATE_EVENT(FOnFilter, OnFilter);
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		CurrentGraph = InArgs._Graph.Get();
		OnCollectionPicked = InArgs._OnCollectionPicked;
		OnFilter = InArgs._OnFilter;

		UpdateDataSource();
		
		ChildSlot
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(5.f)
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PickCollectionHelpText", "Pick a Collection"))
				.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
			]
			
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Fill)
			[
				SNew(SWidgetSwitcher)
				.WidgetIndex_Lambda([this] { return DataSource.IsEmpty() ? 0 : 1; })

				+ SWidgetSwitcher::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("NoCollectionsFoundWarning", "No collections found."))
				]

				+ SWidgetSwitcher::Slot()
				[
					SNew(SListView<FName>)
					.ListItemsSource(&DataSource)
					.SelectionMode(ESelectionMode::Single)
					.OnSelectionChanged(this, &SMovieGraphCollectionPicker::OnCollectionSelected)
					.OnGenerateRow(this, &SMovieGraphCollectionPicker::GenerateRow)
				]
			]
		];
	}

private:
	/** Discovers all available collections, and refreshes the data the list is displaying. */
	TArray<UMovieGraphCollectionNode*> UpdateDataSource()
	{
		TArray<UMovieGraphCollectionNode*> CollectionNodes;
		
		if (!CurrentGraph)
		{
			return CollectionNodes;
		}

		TSet<UMovieGraphConfig*> Subgraphs;
		CurrentGraph->GetAllContainedSubgraphs(Subgraphs);

		TArray<UMovieGraphConfig*> AllGraphs = { CurrentGraph };
		for (UMovieGraphConfig* Subgraph : Subgraphs)
		{
			AllGraphs.Add(Subgraph);
		}

		for (const UMovieGraphConfig* Graph : AllGraphs)
		{
			for (const TObjectPtr<UMovieGraphNode>& Node : Graph->GetNodes())
			{
				if (const TObjectPtr<UMovieGraphCollectionNode> CollectionNode = Cast<UMovieGraphCollectionNode>(Node))
				{
					const FName CollectionName = FName(CollectionNode->Collection->GetCollectionName());
					bool bIncludeCollection = true;

					if (OnFilter.IsBound())
					{
						bIncludeCollection = OnFilter.Execute(CollectionName);
					}

					if (bIncludeCollection)
					{
						DataSource.AddUnique(CollectionName);
					}
				}
			}
		}

		return CollectionNodes;
	}

	/** Handles a collection selected event. */
	void OnCollectionSelected(const FName CollectionName, ESelectInfo::Type Type) const
	{
		if (OnCollectionPicked.IsBound())
		{
			OnCollectionPicked.Execute(CollectionName);
		}

		FSlateApplication::Get().DismissAllMenus();
	}

	/** Generates a row which displays a single collection. */
	TSharedRef<ITableRow> GenerateRow(const FName CollectionName, const TSharedRef<STableViewBase>& InOwnerTable) const
	{
		return
			SNew(STableRow<FName>, InOwnerTable)
			.Style(FAppStyle::Get(), "TableView.AlternatingRow")
			.ShowWires(false)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(7.f, 5.f, 7.f, 5.f)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.FilledCircle"))
				]
					
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Fill)
				[
					SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.Text(FText::FromName(CollectionName))
				]
			];
	}

private:
	/** The data source for the list view widget. Names of the collection nodes that can be picked. */
	TArray<FName> DataSource;

	/** The current graph being viewed. The data source will be populated from this graph. */
	UMovieGraphConfig* CurrentGraph = nullptr;
	
	FOnCollectionPicked OnCollectionPicked;
	FOnFilter OnFilter;
};

TSharedRef<IDetailCustomization> FMovieGraphModifiersCustomization::MakeInstance()
{
	return MakeShared<FMovieGraphModifiersCustomization>();
}

void FMovieGraphModifiersCustomization::CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder)
{
	TArray<TWeakObjectPtr<UMovieGraphModifierNode>> ModifierNodes =
		InDetailBuilder.GetObjectsOfTypeBeingCustomized<UMovieGraphModifierNode>();
	if (ModifierNodes.Num() != 1)
	{
		// Showing more than one modifier node is not supported
		return;
	}

	const TWeakObjectPtr<UMovieGraphModifierNode> ModifierNode = ModifierNodes[0];
	if (!ModifierNode.IsValid())
	{
		return;
	}

	// Update the data source
	ListDataSource = ModifierNode->GetCollections();
	
	// Generate a (multi-layered) icon for the "Add" menu
	const TSharedRef<SLayeredImage> AddIcon =
		SNew(SLayeredImage)
		.ColorAndOpacity(FSlateColor::UseForeground())
		.Image(FAppStyle::GetBrush("LevelEditor.OpenAddContent.Background"));
	AddIcon->AddLayer(FAppStyle::GetBrush("LevelEditor.OpenAddContent.Overlay"));

	// Replace the "Collection" category row with a custom whole-row widget which includes an add-collection button
	IDetailCategoryBuilder& CollectionCategory = InDetailBuilder.EditCategory(FName("Collection"), FText::GetEmpty(), ECategoryPriority::Uncommon);
	CollectionCategory.HeaderContent
	(
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(FText::FromString("Collections"))
			.Font(FAppStyle::Get().GetFontStyle("DetailsView.CategoryFontStyle"))
		]

		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(5.f, 0, 0, 0)
		[
			SNew(SComboButton)
			.ToolTipText(LOCTEXT("AddCollectionToModifierTooltip", "Add a collection that will be affected by the configured modifiers."))
			.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
			.ContentPadding(0)
			.HasDownArrow(false)
			.OnGetMenuContent_Lambda([ModifierNode, this]()
			{
				return
					SNew(SBox)
					.WidthOverride(200.f)
					.HeightOverride(200.f)
					[
						SNew(SMovieGraphCollectionPicker)
						.Graph(ModifierNode->GetTypedOuter<UMovieGraphConfig>())
						.OnFilter_Lambda([ModifierNode](const FName CollectionName)
						{
							if (ModifierNode.IsValid())
							{
								return !ModifierNode.Get()->GetCollections().Contains(CollectionName);
							}

							return false;
						})
						.OnCollectionPicked_Lambda([ModifierNode, this](const FName PickedCollectionName)
						{
							if (ModifierNode.IsValid())
							{
								const FScopedTransaction Transaction(LOCTEXT("AddCollectionToModifier", "Add Collection to Modifier"));
								
								ModifierNode->AddCollection(PickedCollectionName);
								ListDataSource = ModifierNode->GetCollections();
								CollectionsList->Refresh();
							}
						})
					];
			})
			.ButtonContent()
			[
				SNew(SBox)
				.WidthOverride(16)
				.HeightOverride(16)
				[
					AddIcon
				]
			]
		]
	, /* bWholeRowContent */ true);

	// Add a collections browser
	CollectionCategory.AddCustomRow(FText::GetEmpty())
	.WholeRowWidget
	[
		SAssignNew(CollectionsList, SMovieGraphSimpleList<FName>)
		.DataSource(&ListDataSource)
		.DataType(FText::FromString("Collection"))
		.DataTypePlural(FText::FromString("Collections"))
		.OnDelete_Lambda([this, ModifierNode](const FName DeletedCollectionName)
		{
			if (ModifierNode.IsValid())
			{
				const FScopedTransaction Transaction(LOCTEXT("RemoveCollectionFromModifier", "Remove Collection from Modifier"));
				
				ModifierNode.Get()->RemoveCollection(DeletedCollectionName);
				ListDataSource = ModifierNode->GetCollections();
				CollectionsList->Refresh();
			}
		})
		.OnGetRowIcon_Static(&GetCollectionRowIcon)
		.OnGetRowText_Static(&GetCollectionRowText)
	];

	// For all modifiers added to the node, add a category for each, and add each modifier's EditAnywhere properties to the category
	for (UMovieGraphCollectionModifier* Modifier : ModifierNode->GetModifiers())
	{
		if (!Modifier)
		{
			continue;
		}
		
		const UClass* ModifierClass = Modifier->GetClass();
		const FText DisplayName = ModifierClass->GetDisplayNameText();

		// Add category as "Uncommon" to display after the general modifier properties
		IDetailCategoryBuilder& Category = InDetailBuilder.EditCategory(FName(DisplayName.ToString()), DisplayName, ECategoryPriority::Uncommon);

		for (TFieldIterator<FProperty> PropertyIterator(ModifierClass); PropertyIterator; ++PropertyIterator)
		{
			// Add any EditAnywhere properties, but skip the bOverride_* properties.
			const FProperty* ModifierProperty = *PropertyIterator;
			if (ModifierProperty && ModifierProperty->HasAnyPropertyFlags(CPF_Edit) && !ModifierProperty->HasMetaData(TEXT("InlineEditConditionToggle")))
			{
				Category.AddExternalObjectProperty({Modifier}, ModifierProperty->GetFName());
			}
		}
	}
}

void FMovieGraphModifiersCustomization::CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& InDetailBuilder)
{
	DetailBuilder = InDetailBuilder;
	CustomizeDetails(*InDetailBuilder);
}

const FSlateBrush* FMovieGraphModifiersCustomization::GetCollectionRowIcon(const FName CollectionName)
{
	return FAppStyle::GetBrush("Icons.FilledCircle");
}

FText FMovieGraphModifiersCustomization::GetCollectionRowText(const FName CollectionName)
{
	return FText::FromName(CollectionName);
}

#undef LOCTEXT_NAMESPACE
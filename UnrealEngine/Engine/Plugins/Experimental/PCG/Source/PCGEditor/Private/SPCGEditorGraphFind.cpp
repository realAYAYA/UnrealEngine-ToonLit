// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPCGEditorGraphFind.h"

#include "PCGEditor.h"
#include "PCGEditorGraph.h"
#include "PCGEditorGraphNodeBase.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphSchema.h"
#include "Widgets/SWidget.h"

#define LOCTEXT_NAMESPACE "PCGEditorGraphFind"

FPCGEditorGraphFindResult::FPCGEditorGraphFindResult(const FString& InValue)
: Parent(nullptr)
, Value(InValue)
, GraphNode(nullptr)
, Pin()
{
}

FPCGEditorGraphFindResult::FPCGEditorGraphFindResult(const FText& InValue)
: FPCGEditorGraphFindResult(InValue.ToString())
{
}

FPCGEditorGraphFindResult::FPCGEditorGraphFindResult(const FString& InValue, TSharedPtr<FPCGEditorGraphFindResult>& InParent, UEdGraphNode* InNode)
: Parent(InParent)
, Value(InValue)
, GraphNode(InNode)
, Pin()
{
}

FPCGEditorGraphFindResult::FPCGEditorGraphFindResult(const FString& InValue, TSharedPtr<FPCGEditorGraphFindResult>& InParent, UEdGraphPin* InPin)
: Parent(InParent)
, Value(InValue)
, GraphNode(nullptr)
, Pin(InPin)
{
}

FReply FPCGEditorGraphFindResult::OnClick(TWeakPtr<FPCGEditor> InPCGEditorPtr)
{
	if (UEdGraphPin* ResolvedPin = Pin.Get())
	{
		InPCGEditorPtr.Pin()->JumpToNode(ResolvedPin->GetOwningNode());
	}
	else if (GraphNode.IsValid())
	{
		InPCGEditorPtr.Pin()->JumpToNode(GraphNode.Get());
	}

	return FReply::Handled();
}

FText FPCGEditorGraphFindResult::GetToolTip() const
{
	FText ToolTip;

	if (UEdGraphPin* ResolvedPin = Pin.Get())
	{
		if (UEdGraphNode* OwningNode = ResolvedPin->GetOwningNode())
		{
			FString ToolTipString;
			OwningNode->GetPinHoverText(*ResolvedPin, ToolTipString);
			ToolTip = FText::FromString(ToolTipString);
		}
	}
	else if (GraphNode.IsValid())
	{
		ToolTip = GraphNode->GetTooltipText();
	}

	return ToolTip;
}

FText FPCGEditorGraphFindResult::GetCategory() const
{
	if (Pin.Get())
	{
		return LOCTEXT("PinCategory", "Pin");
	}
	else if (GraphNode.IsValid())
	{
		return LOCTEXT("NodeCategory", "Node");
	}
	return FText::GetEmpty();
}

FText FPCGEditorGraphFindResult::GetComment() const
{
	if (GraphNode.IsValid())
	{
		const FString NodeComment = GraphNode->NodeComment;
		if (!NodeComment.IsEmpty())
		{
			return FText::Format(LOCTEXT("NodeCommentFmt", "Node Comment:[{0}]"), FText::FromString(NodeComment));
		}
	}

	return FText::GetEmpty();
}

TSharedRef<SWidget> FPCGEditorGraphFindResult::CreateIcon() const
{
	FSlateColor IconColor = FSlateColor::UseForeground();
	const FSlateBrush* Brush = nullptr;

	if (UEdGraphPin* ResolvedPin = Pin.Get())
	{
		// TODO get pin icon from nodebase?
		Brush = FAppStyle::GetBrush(TEXT("GraphEditor.PinIcon"));
		const UEdGraphSchema* Schema = ResolvedPin->GetSchema();
		IconColor = Schema->GetPinTypeColor(ResolvedPin->PinType);
	}
	else if (GraphNode.IsValid())
	{
		// TODO get icon and tint from nodebase?
		Brush = FAppStyle::GetBrush(TEXT("GraphEditor.NodeGlyph"));
	}

	return SNew(SImage)
		.Image(Brush)
		.ColorAndOpacity(IconColor)
		.ToolTipText(GetCategory());
}

void SPCGEditorGraphFind::Construct(const FArguments& InArgs, TSharedPtr<FPCGEditor> InPCGEditor)
{
	PCGEditorPtr = InPCGEditor;

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(SearchTextField, SSearchBox)
			.HintText(LOCTEXT("PCGGraphSearchHint", "Enter text to find nodes..."))
			.OnTextChanged(this, &SPCGEditorGraphFind::OnSearchTextChanged)
			.OnTextCommitted(this, &SPCGEditorGraphFind::OnSearchTextCommitted)
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(0.f, 4.f, 0.f, 0.f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Menu.Background"))
			[
				SAssignNew(TreeView, STreeViewType)
				.ItemHeight(24)
				.TreeItemsSource(&ItemsFound)
				.OnGenerateRow(this, &SPCGEditorGraphFind::OnGenerateRow)
				.OnGetChildren(this, &SPCGEditorGraphFind::OnGetChildren)
				.OnSelectionChanged(this, &SPCGEditorGraphFind::OnTreeSelectionChanged)
				.SelectionMode(ESelectionMode::Single)
			]
		]
	];
}

void SPCGEditorGraphFind::FocusForUse()
{
	// NOTE: Careful, GeneratePathToWidget can be reentrant in that it can call visibility delegates and such
	FWidgetPath FilterTextBoxWidgetPath;
	FSlateApplication::Get().GeneratePathToWidgetUnchecked(SearchTextField.ToSharedRef(), FilterTextBoxWidgetPath);

	// Set keyboard focus directly
	FSlateApplication::Get().SetKeyboardFocus(FilterTextBoxWidgetPath, EFocusCause::SetDirectly);
}

void SPCGEditorGraphFind::OnSearchTextChanged(const FText& InText)
{
	SearchValue = InText.ToString();
	InitiateSearch();
}

void SPCGEditorGraphFind::OnSearchTextCommitted(const FText& /*InText*/, ETextCommit::Type /*InCommitType*/)
{
	InitiateSearch();
}

void SPCGEditorGraphFind::OnGetChildren(FPCGEditorGraphFindResultPtr InItem, TArray<FPCGEditorGraphFindResultPtr>& OutChildren)
{
	OutChildren += InItem->Children;
}

void SPCGEditorGraphFind::OnTreeSelectionChanged(FPCGEditorGraphFindResultPtr Item, ESelectInfo::Type)
{
	if (Item.IsValid())
	{
		Item->OnClick(PCGEditorPtr);
	}
}

TSharedRef<ITableRow> SPCGEditorGraphFind::OnGenerateRow(FPCGEditorGraphFindResultPtr InItem, const TSharedRef<STableViewBase>& InOwnerTable)
{
	return SNew(STableRow<TSharedPtr<FPCGEditorGraphFindResult>>, InOwnerTable)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				InItem->CreateIcon()
			]
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(2, 0)
			[
				SNew(STextBlock)
				.Text(FText::FromString(InItem->Value))
				.HighlightText(HighlightText)
				.ToolTipText(InItem->GetToolTip())
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1)
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(2, 0)
			[
				SNew(STextBlock)
				.Text(InItem->GetComment()) 
				.ColorAndOpacity(FLinearColor::Yellow)
				.HighlightText(HighlightText)
			]
		];
}

void SPCGEditorGraphFind::InitiateSearch()
{
	TArray<FString> Tokens;
	SearchValue.ParseIntoArray(Tokens, TEXT(" "), true);

	ItemsFound.Empty();
	if (Tokens.Num() > 0)
	{
		HighlightText = FText::FromString(SearchValue);
		MatchTokens(Tokens);
	}

	// Insert a fake result to inform user if none found
	if (ItemsFound.Num() == 0)
	{
		ItemsFound.Add(MakeShared<FPCGEditorGraphFindResult>(LOCTEXT("PCGGraphSearchNoResults", "No Results found")));
	}

	TreeView->RequestTreeRefresh();

	for (const FPCGEditorGraphFindResultPtr& Item : ItemsFound)
	{
		TreeView->SetItemExpansion(Item, true);
	}
}

void SPCGEditorGraphFind::MatchTokens(const TArray<FString>& InTokens)
{
	RootFindResult.Reset();

	UPCGEditorGraph* PCGEditorGraph = PCGEditorPtr.Pin()->GetPCGEditorGraph();
	if (!PCGEditorGraph)
	{
		return;
	}

	RootFindResult = MakeShared<FPCGEditorGraphFindResult>(FString("PCGTreeRoot"));

	for (UEdGraphNode* Node : PCGEditorGraph->Nodes)
	{
		const FString NodeName = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
		const FString NodeType = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();

		FString NodeSearchString = NodeName + NodeType + Node->NodeComment;
		NodeSearchString = NodeSearchString.Replace(TEXT(" "), TEXT(""));

		FPCGEditorGraphFindResultPtr NodeResult;
		auto GetOrCreateNodeResult = [&]() -> FPCGEditorGraphFindResultPtr&
		{
			if (!NodeResult.IsValid())
			{
				NodeResult = MakeShared<FPCGEditorGraphFindResult>(NodeName == NodeType ? NodeName : NodeName + " - " + NodeType, RootFindResult, Node);
			}
			return NodeResult;
		};

		if (StringMatchesSearchTokens(InTokens, NodeSearchString))
		{
			GetOrCreateNodeResult();
		}
	
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->PinFriendlyName.CompareTo(FText::FromString(TEXT(" "))) != 0)
			{
				const FText PinName = Pin->GetSchema()->GetPinDisplayName(Pin);
				FString PinSearchString = Pin->PinName.ToString() + Pin->PinFriendlyName.ToString() + Pin->DefaultValue + Pin->PinType.PinCategory.ToString() + Pin->PinType.PinSubCategory.ToString() + (Pin->PinType.PinSubCategoryObject.IsValid() ? Pin->PinType.PinSubCategoryObject.Get()->GetFullName() : TEXT(""));
				PinSearchString = PinSearchString.Replace(TEXT(" "), TEXT(""));
				if (StringMatchesSearchTokens(InTokens, PinSearchString))
				{
					FPCGEditorGraphFindResultPtr PinResult(MakeShared<FPCGEditorGraphFindResult>(PinName.ToString(), GetOrCreateNodeResult(), Pin));
					NodeResult->Children.Add(PinResult);
				}
			}
		}

		if (NodeResult.IsValid())
		{
			ItemsFound.Add(NodeResult);
		}
	}
}

bool SPCGEditorGraphFind::StringMatchesSearchTokens(const TArray<FString>& InTokens, const FString& InComparisonString)
{
	bool bFoundAllTokens = true;

	//search the entry for each token, it must have all of them to pass
	for (const FString& Token : InTokens)
	{
		if (!InComparisonString.Contains(Token))
		{
			bFoundAllTokens = false;
			break;
		}
	}
	return bFoundAllTokens;
}

#undef LOCTEXT_NAMESPACE

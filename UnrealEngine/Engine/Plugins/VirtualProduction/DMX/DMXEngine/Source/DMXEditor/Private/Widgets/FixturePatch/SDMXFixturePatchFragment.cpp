// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXFixturePatchFragment.h"

#include "DMXEditor.h"
#include "DMXEditorStyle.h"
#include "DMXFixturePatchNode.h"
#include "DMXFixturePatchEditorDefinitions.h"
#include "DMXFixturePatchSharedData.h"
#include "Library/DMXEntityFixturePatch.h"

#include "Styling/AppStyle.h"
#include "SlateOptMacros.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWidgetSwitcher.h"

#define LOCTEXT_NAMESPACE "SDMXFixturePatchFragment"


void SDMXFixturePatchFragment::Construct(const FArguments& InArgs, const TSharedPtr<FDMXFixturePatchNode>& InFixturePatchNode, const TArray<TSharedPtr<FDMXFixturePatchNode>>& InFixturePatchNodeGroup)
{
	check(InFixturePatchNode.IsValid());
	check(InFixturePatchNodeGroup.Contains(InFixturePatchNode));
	check(Column != -1);
	check(Row != -1);
	check(ColumnSpan != -1);

	ColorBrush = MakeShared<FSlateColorBrush>(FLinearColor::Red);

	FixturePatchNode = InFixturePatchNode;
	FixturePatchNodeGroup = InFixturePatchNodeGroup;
	DMXEditorPtr = InArgs._DMXEditor;
	bIsHead = InArgs._IsHead;
	bIsTail = InArgs._IsTail;
	bIsText = InArgs._IsText;
	bIsConflicting = InArgs._IsConflicting;
	Column = InArgs._Column;
	Row = InArgs._Row;
	ColumnSpan = InArgs._ColumnSpan;
	bHighlight = InArgs._bHighlight;

	if (bIsText)
	{
		SAssignNew(ContentBorder, SBorder)
			.BorderImage(FAppStyle::GetBrush("NoBorder"))
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Visibility(EVisibility::SelfHitTestInvisible);
	}
	else
	{
		SAssignNew(ContentBorder, SBorder)
			.Padding(1.f)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Visibility(EVisibility::SelfHitTestInvisible)
			.BorderImage_Lambda([this]()
				{
					if (UDMXEntityFixturePatch* FixturePatch = FixturePatchNode->GetFixturePatch().Get())
					{
						if (bIsConflicting)
						{
							ColorBrush->TintColor = FixturePatch->EditorColor.CopyWithNewOpacity(.25f / FixturePatchNodeGroup.Num());
						}
						else if (bIsTopmost)
						{
							ColorBrush->TintColor = FixturePatch->EditorColor.CopyWithNewOpacity(.5f);
						}
						else
						{
							ColorBrush->TintColor = FLinearColor::Transparent;
						}
					}
					check(ColorBrush.IsValid());
					return ColorBrush.Get();
				});
	}

	ChildSlot
		[
			SNew(SBox) 	
			.Visibility(EVisibility::SelfHitTestInvisible)
			.MaxDesiredHeight(1.0f) // Prevents from some wiggling issues
			.MaxDesiredWidth(1.0f) // Prevents from some wiggling issues
			.Padding_Lambda([this]()
				{
					return BorderPadding;
				})
			[
				SNew(SBorder)
				.Padding(1.f)
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.Visibility(EVisibility::SelfHitTestInvisible)
				.BorderImage(&GetBorderImage())
				.BorderBackgroundColor_Lambda([this]()
					{
						return bHighlight ? FLinearColor(.8f, .8f, .8f) : FLinearColor(.2f, .2f, .2f);
					})
				[	
					ContentBorder.ToSharedRef()
				]
			]
		];

	SetVisibility(EVisibility::SelfHitTestInvisible);
	Refresh(InFixturePatchNodeGroup);
}

void SDMXFixturePatchFragment::Refresh(const TArray<TSharedPtr<FDMXFixturePatchNode>>& InFixturePatchNodeGroup)
{
	FixturePatchNodeGroup = InFixturePatchNodeGroup;
	
	// Update if it is topmost first so further methods can rely on bIsTopmost
	UpdateIsTopmost();

	UpdateFixturePatchNameText();
	UpdateNodeGroupText();
	UpdateBorderPadding();

	if (bIsText)
	{
		// Redraw text
		ContentBorder->SetContent(CreateTextWidget());
	}
}

void SDMXFixturePatchFragment::OnFixturePatchSharedDataSelectedFixturePatch()
{
	TSharedPtr<FDMXEditor> DMXEditor = DMXEditorPtr.Pin();
	if (!DMXEditor.IsValid())
	{
		return;
	}

	const TSharedPtr<FDMXFixturePatchSharedData> SharedData = DMXEditor->GetFixturePatchSharedData();
	if (!SharedData.IsValid())
	{
		return;
	}
	const TArray<TWeakObjectPtr<UDMXEntityFixturePatch>>& SelectedFixturePatches = SharedData->GetSelectedFixturePatches();
}

void SDMXFixturePatchFragment::UpdateFixturePatchNameText()
{
	if (UDMXEntityFixturePatch* Patch = FixturePatchNode->GetFixturePatch().Get())
	{
		FixturePatchNameText = FText::FromString(Patch->GetDisplayName());
	}
}

void SDMXFixturePatchFragment::UpdateNodeGroupText()
{
	NodeGroupText =
		FixturePatchNodeGroup.Num() > 1 ?
		FText::Format(LOCTEXT("NodeGroupText", "(and {0} more)"), FixturePatchNodeGroup.Num() - 1) :
		FText::GetEmpty();
}

void SDMXFixturePatchFragment::UpdateIsTopmost()
{
	if (!ensureMsgf(FixturePatchNodeGroup.Num() > 0, TEXT("Unexpected empty node group for Fixture Patch Fragment Widget.")))
	{
		return;
	}

	bIsTopmost = !FixturePatchNodeGroup.ContainsByPredicate([this](const TSharedPtr<FDMXFixturePatchNode>& OtherNode)
		{
			return OtherNode->GetZOrder() > FixturePatchNode->GetZOrder();
		});

	if (FixturePatchNodeGroup.Num() > 1)
	{
		UpdateFixturePatchNameText();
	}
}

void SDMXFixturePatchFragment::UpdateBorderPadding()
{
	if (bIsHead && FixturePatchNodeGroup.Num() > 1)
	{
		// Sort by ZOrder ascending
		FixturePatchNodeGroup.StableSort([](const TSharedPtr<FDMXFixturePatchNode>& NodeA, const TSharedPtr<FDMXFixturePatchNode>& NodeB)
			{
				return 
					NodeA->GetZOrder() < NodeB->GetZOrder();
			});

		const int32 IndexOfThis = FixturePatchNodeGroup.IndexOfByKey(FixturePatchNode);

		constexpr int32 OffsetPerIndex = 3;
		const int32 ClampedIndexOfThis = FMath::Min(IndexOfThis, 5);
		const int32 OffsetLeft = ClampedIndexOfThis * OffsetPerIndex;

		BorderPadding = FMargin(OffsetLeft, 0.f, 0.f, 0.f);
	}
	else
	{
		BorderPadding = FMargin(0.f);
	}
}

const FSlateBrush& SDMXFixturePatchFragment::GetBorderImage() const
{
	const FSlateBrush* BorderImage = nullptr;
	if (bIsText)
	{
		BorderImage = FCoreStyle::Get().GetBrush("NoBorder");
	}
	else if (bIsConflicting)
	{
		BorderImage = FAppStyle::GetBrush("Graph.Node.DevelopmentBanner");
	}
	else if (bIsHead && bIsTail)
	{
		BorderImage = FDMXEditorStyle::Get().GetBrush("FixturePatcher.FragmentBorder.Normal");
	}
	else if (bIsHead)
	{
		BorderImage = FDMXEditorStyle::Get().GetBrush("FixturePatcher.FragmentBorder.L");
	}
	else if (bIsTail)
	{
		BorderImage = FDMXEditorStyle::Get().GetBrush("FixturePatcher.FragmentBorder.R");
	}
	else
	{
		BorderImage = FDMXEditorStyle::Get().GetBrush("FixturePatcher.FragmentBorder.TB");
	}
	check(BorderImage);

	return *BorderImage;
}

TSharedRef<SWidget> SDMXFixturePatchFragment::CreateTextWidget()
{
	if (bIsTopmost)
	{
		return
			SNew(SBox)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Top)
			.Visibility(EVisibility::HitTestInvisible)
			[
				SNew(SBorder)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Top)
				.Padding(FMargin(2.f, 2.f, 0.f, 2.f))
				.BorderImage(FDMXEditorStyle::Get().GetBrush("DMXEditor.RoundedPropertyBorder"))
				[
					SNew(SHorizontalBox)
					.Clipping(EWidgetClipping::ClipToBoundsAlways)

					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Top)
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
							{
								return FixturePatchNameText;
							})
						.TextStyle(FAppStyle::Get(), "SmallText")
						.ColorAndOpacity(FLinearColor::White)
					]
				
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Top)
					.AutoWidth()
					.Padding(FMargin(10.f, 0.f, 0.f, 0.f))
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
							{
								return NodeGroupText;
							})
						.TextStyle(FAppStyle::Get(), "SmallText")
						.ColorAndOpacity(FLinearColor(.5f, .5f, .5f))
					]
				]
			];
	}

	return SNullWidget::NullWidget;
}

#undef LOCTEXT_NAMESPACE

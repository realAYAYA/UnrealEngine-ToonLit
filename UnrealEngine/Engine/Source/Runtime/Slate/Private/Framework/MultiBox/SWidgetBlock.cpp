// Copyright Epic Games, Inc. All Rights Reserved.

#include "Framework/MultiBox/SWidgetBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/ToolBarStyle.h"
#include "Widgets/SToolTip.h"


/**
 * Constructor
 *
 * @param	InHeadingText	Heading text
 */
FWidgetBlock::FWidgetBlock( TSharedRef<SWidget> InContent, const FText& InLabel, bool bInNoIndent, EHorizontalAlignment InHorizontalAlignment, const TAttribute<FText>& InToolTipText)
	: FMultiBlock( nullptr, nullptr, NAME_None, EMultiBlockType::Widget )
	, ContentWidget( InContent )
	, Label( InLabel )
	, ToolTipText(InToolTipText)
	, bNoIndent( bInNoIndent )
	, HorizontalAlignment(InHorizontalAlignment)
{
}


void FWidgetBlock::SetCustomMenuDelegate( FNewMenuDelegate& InCustomMenuDelegate )
{
	CustomMenuDelegate = InCustomMenuDelegate;	
}


void FWidgetBlock::CreateMenuEntry(FMenuBuilder& MenuBuilder) const
{
	if ( CustomMenuDelegate.IsBound() )
	{
		CustomMenuDelegate.Execute(MenuBuilder);
	}
	else
	{
		FText EntryLabel = (!Label.IsEmpty()) ? Label : NSLOCTEXT("WidgetBlock", "CustomControl", "Custom Control");
		MenuBuilder.AddWidget(ContentWidget, FText::GetEmpty(), true);
	}
}


/**
 * Allocates a widget for this type of MultiBlock.  Override this in derived classes.
 *
 * @return  MultiBlock widget object
 */
TSharedRef< class IMultiBlockBaseWidget > FWidgetBlock::ConstructWidget() const
{
	return SNew( SWidgetBlock )
			.Cursor(EMouseCursor::Default);
}


bool FWidgetBlock::GetAlignmentOverrides(EHorizontalAlignment& OutHorizontalAlignment, EVerticalAlignment& OutVerticalAlignment, bool& bOutAutoWidth) const
{
	OutHorizontalAlignment = HorizontalAlignment;
	OutVerticalAlignment = VAlign_Fill;
	bOutAutoWidth = HorizontalAlignment != HAlign_Fill ? false : true;
	return true;
}

/**
 * Construct this widget
 *
 * @param	InArgs	The declaration data for this widget
 */
void SWidgetBlock::Construct( const FArguments& InArgs )
{
}



/**
 * Builds this MultiBlock widget up from the MultiBlock associated with it
 */
void SWidgetBlock::BuildMultiBlockWidget(const ISlateStyle* StyleSet, const FName& StyleName)
{
	TSharedPtr<SMultiBoxWidget> OwnerMultiBoxWidgetPinned = OwnerMultiBoxWidget.Pin();

	TSharedPtr< const FMultiBox > MultiBox = OwnerMultiBoxWidgetPinned->GetMultiBox();
	TSharedRef< const FWidgetBlock > WidgetBlock = StaticCastSharedRef< const FWidgetBlock >( MultiBlock.ToSharedRef() );

	// Support menus which do not have a defined widget style yet
	bool bHasLabel = !WidgetBlock->Label.IsEmpty();
	FMargin Padding;
	const FTextBlockStyle* LabelStyle = nullptr;
	if(StyleSet->HasWidgetStyle<FToolBarStyle>(StyleName))
	{
		const FToolBarStyle& ToolBarStyle = StyleSet->GetWidgetStyle<FToolBarStyle>(StyleName);

		Padding = WidgetBlock->bNoIndent ? ToolBarStyle.BlockPadding : ToolBarStyle.IndentedBlockPadding;
		LabelStyle = &ToolBarStyle.LabelStyle;

	}
	else
	{
		Padding = WidgetBlock->bNoIndent ? StyleSet->GetMargin(StyleName, ".Block.Padding") : StyleSet->GetMargin(StyleName, ".Block.IndentedPadding");

		LabelStyle = &StyleSet->GetWidgetStyle<FTextBlockStyle>(ISlateStyle::Join(StyleName, ".Label"));
	}

	if(OwnerMultiBoxWidgetPinned->GetMultiBox()->GetType() == EMultiBoxType::Menu)
	{
		// Account for checkmark used in other menu blocks but not used in for widget rows
		Padding = Padding + FMargin(14, 0, 8, 0);

		// If there is no label, allow the custom menu widget to consume the entire space
		if (!bHasLabel)
		{
			// We use 1 pixel of padding to ensure the menu border shows up
			Padding = FMargin(1);
		}
	}

	// For searchable menus with a custom widgets without a label, find a TextBlock to connect with for search
	// This is similar to how SMenuEntryBlock works
	FText SearchLabel = WidgetBlock->Label;
	TAttribute<FText> SearchHighlightText;

	if( OwnerMultiBoxWidgetPinned->GetMultiBox()->GetType() == EMultiBoxType::Menu && OwnerMultiBoxWidgetPinned->GetSearchable())
	{
		SearchHighlightText.Bind(OwnerMultiBoxWidgetPinned.Get(), &SMultiBoxWidget::GetSearchText);

		if (!bHasLabel)
		{
			TSharedRef<SWidget> TextBlock = FindTextBlockWidget(WidgetBlock->ContentWidget);
			if (TextBlock != SNullWidget::NullWidget )
			{
				// Bind the search text to the widgets text to highlight
				TSharedRef<STextBlock> TheTextBlock = StaticCastSharedRef<STextBlock>(TextBlock);
				TheTextBlock->SetHighlightText(SearchHighlightText);

				SearchLabel = TheTextBlock.Get().GetText();
			}
		}
	}

	// Add this widget to the search list of the multibox
	OwnerMultiBoxWidgetPinned->AddElement(this->AsWidget(), SearchLabel, MultiBlock->GetSearchable());

	// This widget holds the search text, set it as the search block widget
	if (OwnerMultiBoxWidgetPinned->GetSearchTextWidget() && OwnerMultiBoxWidgetPinned->GetSearchTextWidget()->GetParentWidget() == WidgetBlock->ContentWidget)
	{
		OwnerMultiBoxWidgetPinned->SetSearchBlockWidget(this->AsWidget());

		// When we are always showing the search widget, we should not hide it here.
		if (!OwnerMultiBoxWidgetPinned->ShouldShowMenuSearchField())
		{
			this->AsWidget()->SetVisibility(EVisibility::Collapsed);
		}
	}

	ChildSlot
	.Padding( Padding )	// Large left margin mimics the indent of normal menu items when bNoIndent is false
	[
		SNew(SHorizontalBox)
		.ToolTip(FMultiBoxSettings::ToolTipConstructor.Execute(WidgetBlock->ToolTipText, nullptr, nullptr ))
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SHorizontalBox)
			.Visibility( bHasLabel ? EVisibility::Visible : EVisibility::Collapsed )
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding( bHasLabel ? FMargin(0.f, 0.f, 4.f, 0.f) : FMargin(0))
			.VAlign( VAlign_Center )
			[
				SNew( STextBlock )
				.TextStyle(LabelStyle)
				.Text(WidgetBlock->Label)
				.HighlightText(SearchHighlightText)
				.ColorAndOpacity(FAppStyle::Get().GetSlateColor("Colors.ForegroundHover"))
			]
		]
		+SHorizontalBox::Slot()
		.VAlign(bHasLabel ? VAlign_Center : VAlign_Fill)
		.FillWidth(1.f)
		[
			WidgetBlock->ContentWidget
		]
	];
}

TSharedRef<SWidget> SWidgetBlock::FindTextBlockWidget( TSharedRef<SWidget> Content )
{
	if (Content->GetType() == FName(TEXT("STextBlock")))
	{
		return Content;
	}

	FChildren* Children = Content->GetChildren();
	int32 NumChildren = Children->Num();

	for( int32 Index = 0; Index < NumChildren; ++Index )
	{
		TSharedRef<SWidget> Found = FindTextBlockWidget( Children->GetChildAt( Index ));
		if (Found != SNullWidget::NullWidget)
		{
			return Found;
		}
	}
	return SNullWidget::NullWidget;
}

void SWidgetBlock::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// If this widget is in a Menu, attempt to close any other open submenus within that menu
	TSharedPtr<SMultiBoxWidget> OwnerMultiBoxWidgetPinned = OwnerMultiBoxWidget.Pin();
	if(OwnerMultiBoxWidgetPinned->GetMultiBox()->GetType() == EMultiBoxType::Menu)
	{
		const TSharedPtr< const SMenuAnchor > OpenedMenuAnchor( OwnerMultiBoxWidgetPinned->GetOpenMenu() );
		if ( OpenedMenuAnchor.IsValid() && OpenedMenuAnchor->IsOpen() )
		{
			OwnerMultiBoxWidgetPinned->CloseSummonedMenus();
		}
	}

	SMultiBlockBaseWidget::OnMouseEnter(MyGeometry, MouseEvent);
}


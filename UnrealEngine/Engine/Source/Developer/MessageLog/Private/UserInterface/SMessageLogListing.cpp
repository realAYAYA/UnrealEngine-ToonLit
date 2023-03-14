// Copyright Epic Games, Inc. All Rights Reserved.

#include "UserInterface/SMessageLogListing.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "UserInterface/SMessageLogMessageListRow.h"
#include "Framework/Commands/GenericCommands.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Styling/StyleColors.h"


#define LOCTEXT_NAMESPACE "Developer.MessageLog"


SMessageLogListing::SMessageLogListing()
	: UICommandList( MakeShareable( new FUICommandList ) )
	, bUpdatingSelection( false )
{ }


SMessageLogListing::~SMessageLogListing()
{
	MessageLogListingViewModel->OnDataChanged().RemoveAll( this );
	MessageLogListingViewModel->OnSelectionChanged().RemoveAll( this );
}


void SMessageLogListing::Construct( const FArguments& InArgs, const TSharedRef< IMessageLogListing >& InModelView )
{
	MessageLogListingViewModel = StaticCastSharedRef<FMessageLogListingViewModel>(InModelView);

	TSharedPtr<SHorizontalBox> HorizontalBox = NULL;
	TSharedPtr<SVerticalBox> VerticalBox = NULL;

	TSharedRef<SScrollBar> ScrollBar = SNew(SScrollBar);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
		.Padding(15.0f)
		[
			SAssignNew(VerticalBox, SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(0.f)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(1)
				[
					SNew(SScrollBox)
					.Orientation(EOrientation::Orient_Horizontal)
					+ SScrollBox::Slot()
					[
						SAssignNew(MessageListView, SListView<TSharedRef<FTokenizedMessage>>)
						.ListItemsSource(&MessageLogListingViewModel->GetFilteredMessages())
						.OnGenerateRow(this, &SMessageLogListing::MakeMessageLogListItemWidget)
						.OnSelectionChanged(this, &SMessageLogListing::OnLineSelectionChanged)
						.ExternalScrollbar(ScrollBar)
						.ItemHeight(24.0f)
						.ConsumeMouseWheel(EConsumeMouseWheel::Always)
					]
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(FOptionalSize(16))
					[
						ScrollBar
					]
				]
			]
		]
	];

	//If we have some content below the message log, add a separator and a new box.
	if (MessageLogListingViewModel->GetShowFilters() || 
		MessageLogListingViewModel->GetShowPages() || 
		MessageLogListingViewModel->GetAllowClear() )
	{
		VerticalBox->AddSlot()
			.AutoHeight()
			.Padding(FMargin(6, 6, 0, 0))
			[
				SAssignNew(HorizontalBox, SHorizontalBox)
			];

		if (MessageLogListingViewModel->GetShowFilters())
		{
			HorizontalBox->AddSlot()
				.FillWidth(1.0f)
				.HAlign(HAlign_Left)
				[
					SNew(SComboButton)
					.ButtonStyle(FAppStyle::Get(), "Button")
					.ForegroundColor(FStyleColors::White)
					.OnGetMenuContent(this, &SMessageLogListing::OnGetFilterMenuContent)
					.ButtonContent()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Show", "SHOW"))
						.ToolTipText(LOCTEXT("ShowToolTip", "Only show messages of the selected types"))
					]
				];
		}

		HorizontalBox->AddSlot()
			.FillWidth(1.0f)
			.HAlign(HAlign_Right)
			[
				SNew(SComboButton)
				.IsEnabled(this, &SMessageLogListing::IsPageWidgetEnabled)
				.Visibility(this, &SMessageLogListing::GetPageWidgetVisibility)
				.ButtonStyle(FAppStyle::Get(), "Button")
				.ForegroundColor(FStyleColors::White)
				.OnGetMenuContent(this, &SMessageLogListing::OnGetPageMenuContent)
				.ButtonColorAndOpacity(FStyleColors::White)
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text(this, &SMessageLogListing::OnGetPageMenuLabel)
					.ToolTipText(LOCTEXT("PageToolTip", "Choose the log page to view"))
				]
			];

		HorizontalBox->AddSlot()
			.FillWidth(1.0f)
			.HAlign(HAlign_Right)
			[
				SNew(SButton)
				.OnClicked(this, &SMessageLogListing::OnClear)
				.IsEnabled(this, &SMessageLogListing::IsClearWidgetEnabled)
				.Visibility(this, &SMessageLogListing::GetClearWidgetVisibility)
				.ForegroundColor(FStyleColors::White)
				.ContentPadding(FMargin(6.5, 2))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ClearMessageLog", "CLEAR"))
					.ToolTipText(LOCTEXT("ClearMessageLog_ToolTip", "Clear the messages in this log"))
				]
			];
	}

	// Register with the the view object so that it will notify if any data changes
	MessageLogListingViewModel->OnDataChanged().AddSP( this, &SMessageLogListing::OnChanged );
	MessageLogListingViewModel->OnSelectionChanged().AddSP( this, &SMessageLogListing::OnSelectionChanged );

#if !PLATFORM_WINDOWS || !defined(__clang__) // FIXME // @todo clang: this causes an LLVM codegen crash that doesn't go away even with optimizations disabled
	if (FGenericCommands::IsRegistered())
	{
		UICommandList->MapAction(FGenericCommands::Get().Copy,
			FExecuteAction::CreateSP(this, &SMessageLogListing::CopySelectedToClipboard),
			FCanExecuteAction());
	}
#endif
}


void SMessageLogListing::OnChanged()
{
	ClearSelectedMessages();
	RefreshVisibility();
}


void SMessageLogListing::OnSelectionChanged()
{
	if( bUpdatingSelection )
	{
		return;
	}

	bUpdatingSelection = true;
	const auto& SelectedMessages = MessageLogListingViewModel->GetSelectedMessages();
	MessageListView->ClearSelection();
	for( auto It = SelectedMessages.CreateConstIterator(); It; ++It )
	{
		MessageListView->SetItemSelection( *It, true );
	}

	if( SelectedMessages.Num() > 0 )
	{
		ScrollToMessage( SelectedMessages[0] );
	}
	bUpdatingSelection = false;
}


void SMessageLogListing::RefreshVisibility()
{
	const TArray< TSharedRef<FTokenizedMessage> >& Messages = MessageLogListingViewModel->GetFilteredMessages();
	if (Messages.Num() > 0)
	{
		if (MessageLogListingViewModel->GetScrollToBottom())
		{
			ScrollToMessage( Messages.Last() );
		}
		else
		{
			ScrollToMessage( Messages[0] );
		}
	}
	
	MessageListView->RequestListRefresh();
}


void SMessageLogListing::BroadcastMessageTokenClicked( TSharedPtr<FTokenizedMessage> Message, const TSharedRef<IMessageToken>& Token )
{
	ClearSelectedMessages();
	SelectMessage( Message.ToSharedRef(), true );
	MessageLogListingViewModel->ExecuteToken(Token);
}

void SMessageLogListing::BroadcastMessageDoubleClicked(TSharedPtr< class FTokenizedMessage > Message)
{
	if (Message->GetMessageTokens().Num() > 0)
	{
		TSharedPtr<IMessageToken> MessageLink = Message->GetMessageLink();
		if (MessageLink.IsValid())
		{
			MessageLogListingViewModel->ExecuteToken(MessageLink->AsShared());
		}
	}	
}

const TArray< TSharedRef<FTokenizedMessage> > SMessageLogListing::GetSelectedMessages() const
{
	return MessageLogListingViewModel->GetSelectedMessages();
}


void SMessageLogListing::SelectMessage( const TSharedRef<class FTokenizedMessage>& Message, bool bSelected ) const
{
	MessageLogListingViewModel->SelectMessage( Message, bSelected );
}


bool SMessageLogListing::IsMessageSelected( const TSharedRef<class FTokenizedMessage>& Message ) const
{
	return MessageLogListingViewModel->IsMessageSelected( Message );
}


void SMessageLogListing::ScrollToMessage( const TSharedRef<class FTokenizedMessage>& Message ) const
{
	if(!MessageListView->IsItemVisible(Message))
	{
		MessageListView->RequestScrollIntoView( Message );
	}
}


void SMessageLogListing::ClearSelectedMessages() const
{
	MessageLogListingViewModel->ClearSelectedMessages();
}


void SMessageLogListing::InvertSelectedMessages() const
{
	MessageLogListingViewModel->InvertSelectedMessages();
}


FString SMessageLogListing::GetSelectedMessagesAsString() const
{
	return MessageLogListingViewModel->GetSelectedMessagesAsString();
}


FString SMessageLogListing::GetAllMessagesAsString() const
{
	return MessageLogListingViewModel->GetAllMessagesAsString();
}


TSharedRef<ITableRow> SMessageLogListing::MakeMessageLogListItemWidget( TSharedRef<FTokenizedMessage> Message, const TSharedRef<STableViewBase>& OwnerTable )
{
	return
		SNew(SMessageLogMessageListRow, OwnerTable)
		.Message(Message)
		.OnTokenClicked( this, &SMessageLogListing::BroadcastMessageTokenClicked )
		.OnMessageDoubleClicked( this, &SMessageLogListing::BroadcastMessageDoubleClicked );
}


void SMessageLogListing::OnLineSelectionChanged( TSharedPtr< FTokenizedMessage > Selection, ESelectInfo::Type /*SelectInfo*/ )
{
	if (bUpdatingSelection)
	{
		return;
	}

	bUpdatingSelection = true;
	{
		MessageLogListingViewModel->SelectMessages(MessageListView->GetSelectedItems());
	}
	bUpdatingSelection = false;
}


void SMessageLogListing::CopySelectedToClipboard() const
{
	CopyLogOutput( true, true );
}


FString SMessageLogListing::CopyLogOutput( bool bSelected, bool bClipboard ) const
{
	FString CombinedString;

	if( bSelected )
	{
		// Get the selected item and then get the selected messages as a string.
		CombinedString = GetSelectedMessagesAsString();
	}
	else
	{
		// Get the selected item and then get all the messages as a string.
		CombinedString = GetAllMessagesAsString();
	}
	if( bClipboard )
	{
		// Pass that to the clipboard.
		FPlatformApplicationMisc::ClipboardCopy( *CombinedString );
	}

	return CombinedString;
}


const TSharedRef< const FUICommandList > SMessageLogListing::GetCommandList() const 
{ 
	return UICommandList;
}


FReply SMessageLogListing::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	return UICommandList->ProcessCommandBindings( InKeyEvent ) ? FReply::Handled() : FReply::Unhandled();
}


EVisibility SMessageLogListing::GetFilterMenuVisibility()
{
	if( MessageLogListingViewModel->GetShowFilters() )
	{
		return EVisibility::Visible;
	}

	return EVisibility::Hidden;
}


TSharedRef<ITableRow> SMessageLogListing::MakeShowWidget(TSharedRef<FMessageFilter> Selection, const TSharedRef<STableViewBase>& OwnerTable)
{
	return
		SNew(STableRow< TSharedRef<FMessageFilter> >, OwnerTable)
		[
			SNew(SCheckBox)
			.IsChecked(Selection, &FMessageFilter::OnGetDisplayCheckState)
			.OnCheckStateChanged(Selection, &FMessageFilter::OnDisplayCheckStateChanged)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.WidthOverride(16)
					.HeightOverride(16)
					[
						SNew(SImage)
						.Image(Selection->GetIcon().GetIcon())
					]
				]
				+SHorizontalBox::Slot().AutoWidth()
				[
					SNew(STextBlock)
					.Text(Selection->GetName())
				]
			]
		];
}


TSharedRef<SWidget> SMessageLogListing::OnGetFilterMenuContent()
{
	return 
		SNew( SListView< TSharedRef<FMessageFilter> >)
		.ListItemsSource(&MessageLogListingViewModel->GetMessageFilters())
		.OnGenerateRow(this, &SMessageLogListing::MakeShowWidget)
		.ItemHeight(24);
}


FText SMessageLogListing::OnGetPageMenuLabel() const
{
	if(MessageLogListingViewModel->GetPageCount() > 1)
	{
		return MessageLogListingViewModel->GetPageTitle(MessageLogListingViewModel->GetCurrentPageIndex());
	}
	else
	{
		return LOCTEXT("PageMenuLabel", "PAGE");
	}
}


TSharedRef<SWidget> SMessageLogListing::OnGetPageMenuContent() const
{
	if(MessageLogListingViewModel->GetPageCount() > 1)
	{
		FMenuBuilder MenuBuilder(true, NULL);
		for(uint32 PageIndex = 0; PageIndex < MessageLogListingViewModel->GetPageCount(); PageIndex++)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("PageName"), MessageLogListingViewModel->GetPageTitle(PageIndex));
			MenuBuilder.AddMenuEntry( 
				MessageLogListingViewModel->GetPageTitle(PageIndex), 
				FText::Format(LOCTEXT("PageMenuEntry_Tooltip", "View page: {PageName}"), Arguments), 
				FSlateIcon(),
				FExecuteAction::CreateSP(const_cast<SMessageLogListing*>(this), &SMessageLogListing::OnPageSelected, PageIndex));
		}

		return MenuBuilder.MakeWidget();
	}

	return SNullWidget::NullWidget;
}


void SMessageLogListing::OnPageSelected(uint32 PageIndex)
{
	MessageLogListingViewModel->SetCurrentPageIndex(PageIndex);
}


bool SMessageLogListing::IsPageWidgetEnabled() const
{
	return MessageLogListingViewModel->GetPageCount() > 1;
}


EVisibility SMessageLogListing::GetPageWidgetVisibility() const
{
	if(MessageLogListingViewModel->GetShowPages())
	{
		return EVisibility::Visible;
	}
	else
	{
		return EVisibility::Collapsed;
	}
}


bool SMessageLogListing::IsClearWidgetEnabled() const
{
	return MessageLogListingViewModel->NumMessages() > 0;
}


EVisibility SMessageLogListing::GetClearWidgetVisibility() const
{
	if (MessageLogListingViewModel->GetAllowClear())
	{
		return EVisibility::Visible;
	}
	else
	{
		return EVisibility::Collapsed;
	}
}


FReply SMessageLogListing::OnClear()
{
	MessageLogListingViewModel->ClearMessages();
	return FReply::Handled();
}


#undef LOCTEXT_NAMESPACE

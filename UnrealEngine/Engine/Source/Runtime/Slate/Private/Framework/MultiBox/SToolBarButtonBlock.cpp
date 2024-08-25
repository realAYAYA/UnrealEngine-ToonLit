// Copyright Epic Games, Inc. All Rights Reserved.

#include "Framework/MultiBox/SToolBarButtonBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Framework/MultiBox/SToolBarComboButtonBlock.h"
#include "Styling/ToolBarStyle.h"
#include "Widgets/Images/SLayeredImage.h"
#include "Widgets/Layout/SSeparator.h"

FToolBarButtonBlock::FToolBarButtonBlock( FButtonArgs ButtonArgs )
: FMultiBlock( ButtonArgs.Command, ButtonArgs.CommandList, NAME_None, EMultiBlockType::ToolBarButton )
	, LabelOverride( ButtonArgs.LabelOverride )
	, ToolTipOverride( ButtonArgs.ToolTipOverride )
	, IconOverride( ButtonArgs.IconOverride )
   // , BorderBrushName(ButtonArgs.BorderBrushName)
	, LabelVisibility()
	, UserInterfaceActionType(ButtonArgs.UserInterfaceActionType != EUserInterfaceActionType::None ?
		ButtonArgs.UserInterfaceActionType : EUserInterfaceActionType::Button)
	, bIsFocusable(false)
	, bForceSmallIcons(false)
{
}

FToolBarButtonBlock::FToolBarButtonBlock( const TSharedPtr< const FUICommandInfo > InCommand, TSharedPtr< const FUICommandList > InCommandList, const TAttribute<FText>& InLabelOverride, const TAttribute<FText>& InToolTipOverride, const TAttribute<FSlateIcon>& InIconOverride )
	: FMultiBlock( InCommand, InCommandList, NAME_None, EMultiBlockType::ToolBarButton )
	, LabelOverride( InLabelOverride )
	, ToolTipOverride( InToolTipOverride )
	, IconOverride( InIconOverride )
	, LabelVisibility()
	, UserInterfaceActionType(EUserInterfaceActionType::Button)
	, bIsFocusable(false)
	, bForceSmallIcons(false)
{
}

FToolBarButtonBlock::FToolBarButtonBlock( const TAttribute<FText>& InLabel, const TAttribute<FText>& InToolTip, const TAttribute<FSlateIcon>& InIcon, const FUIAction& InUIAction, const EUserInterfaceActionType InUserInterfaceActionType )
	: FMultiBlock( InUIAction )
	, LabelOverride( InLabel )
	, ToolTipOverride( InToolTip )
	, IconOverride( InIcon )
	, LabelVisibility()
	, UserInterfaceActionType(InUserInterfaceActionType)
	, bIsFocusable(false)
	, bForceSmallIcons(false)
{
}

void FToolBarButtonBlock::SetCustomMenuDelegate(const FNewMenuDelegate& InCustomMenuDelegate)
{
	CustomMenuDelegate = InCustomMenuDelegate;
}

void FToolBarButtonBlock::SetOnGetMenuContent(const FOnGetContent& InOnGetMenuContent)
{
	OnGetMenuContent = InOnGetMenuContent;
}

void FToolBarButtonBlock::CreateMenuEntry(FMenuBuilder& MenuBuilder) const
{
	// Setup Command Context 
	TSharedPtr<const FUICommandInfo> MenuEntryAction = GetAction();
	TSharedPtr<const FUICommandList> MenuEntryActionList = GetActionList();
	bool bHasValidCommand = MenuEntryAction.IsValid() && MenuEntryActionList.IsValid();
	if (bHasValidCommand) 
	{	
		MenuBuilder.PushCommandList(MenuEntryActionList.ToSharedRef());
	}

	if ( CustomMenuDelegate.IsBound() )
	{
		CustomMenuDelegate.Execute(MenuBuilder);
	}
	else if (bHasValidCommand)
	{
		MenuBuilder.AddMenuEntry(MenuEntryAction);
	}
	else if ( LabelOverride.IsSet() )
	{
		const FUIAction& DirectAction = GetDirectActions();
		MenuBuilder.AddMenuEntry( LabelOverride.Get(), ToolTipOverride.Get(), IconOverride.Get(), DirectAction, NAME_None, UserInterfaceActionType);
	}

	if (bHasValidCommand) 
	{	
		MenuBuilder.PopCommandList();
	}
}

bool FToolBarButtonBlock::HasIcon() const
{
	const FSlateIcon ActionIcon = GetAction().IsValid() ? GetAction()->GetIcon() : FSlateIcon();
	const FSlateIcon& ActualIcon = IconOverride.IsSet() ? IconOverride.Get() : ActionIcon;

	if (ActualIcon.IsSet())
	{
		return ActualIcon.GetIcon()->GetResourceName() != NAME_None;
	}

	return false;
}

/**
 * Allocates a widget for this type of MultiBlock.  Override this in derived classes.
 *
 * @return  MultiBlock widget object
 */
TSharedRef< class IMultiBlockBaseWidget > FToolBarButtonBlock::ConstructWidget() const
{
	return SNew( SToolBarButtonBlock )
		.LabelVisibility(LabelVisibility)
		.IsFocusable(bIsFocusable)
		.ForceSmallIcons(bForceSmallIcons)
		.TutorialHighlightName(GetTutorialHighlightName())
		.Cursor(EMouseCursor::Default);
}


/**
 * Construct this widget
 *
 * @param	InArgs	The declaration data for this widget
 */
void SToolBarButtonBlock::Construct( const FArguments& InArgs )
{
	LabelVisibilityOverride = InArgs._LabelVisibility;
	bIsFocusable = InArgs._IsFocusable;
	bForceSmallIcons = InArgs._ForceSmallIcons;
	TutorialHighlightName = InArgs._TutorialHighlightName;
}


/**
 * Builds this MultiBlock widget up from the MultiBlock associated with it
 */
void SToolBarButtonBlock::BuildMultiBlockWidget(const ISlateStyle* StyleSet, const FName& StyleName)
{
	const FToolBarStyle& ToolBarStyle = StyleSet->GetWidgetStyle<FToolBarStyle>(StyleName);

	// If override is set use that
	if (LabelVisibilityOverride.IsSet())
	{
		LabelVisibility = LabelVisibilityOverride.GetValue();
	}
	else if (!ToolBarStyle.bShowLabels)
	{
		// Otherwise check the style
		LabelVisibility = EVisibility::Collapsed;
	}
	else
	{
		// Finally if the style doesnt disable labels, use the default
		LabelVisibility = TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(SharedThis(this), &SToolBarButtonBlock::GetIconVisibility, false));
	}

	struct Local
	{
		/** Appends the key binding to the end of the provided ToolTip */
		static FText AppendKeyBindingToToolTip( const TAttribute<FText> ToolTip, TWeakPtr< const FUICommandInfo> Command )
		{
			TSharedPtr<const FUICommandInfo> CommandPtr = Command.Pin();
			if( CommandPtr.IsValid() && (CommandPtr->GetFirstValidChord()->IsValidChord()) )
			{
				FFormatNamedArguments Args;
				Args.Add( TEXT("ToolTipDescription"), ToolTip.Get() );
				Args.Add( TEXT("Keybinding"), CommandPtr->GetInputText() );
				return FText::Format( NSLOCTEXT("ToolBar", "ToolTip + Keybinding", "{ToolTipDescription} ({Keybinding})"), Args );
			}
			else
			{
				return ToolTip.Get();
			}
		}
	};

	TSharedRef<const FMultiBox> MultiBox = OwnerMultiBoxWidget.Pin()->GetMultiBox();

	TSharedRef<const FToolBarButtonBlock> ToolBarButtonBlock = StaticCastSharedRef<const FToolBarButtonBlock>(MultiBlock.ToSharedRef());

	TSharedPtr<const FUICommandInfo> UICommand = ToolBarButtonBlock->GetAction();

	// Allow the block to override the action's label and tool tip string, if desired
	TAttribute<FText> ActualLabel;
	if (ToolBarButtonBlock->LabelOverride.IsSet())
	{
		ActualLabel = ToolBarButtonBlock->LabelOverride;
	}
	else
	{
		ActualLabel = UICommand.IsValid() ? UICommand->GetLabel() : FText::GetEmpty();
	}

	// Add this widget to the search list of the multibox
	OwnerMultiBoxWidget.Pin()->AddElement(this->AsWidget(), ActualLabel.Get(), MultiBlock->GetSearchable());

	TAttribute<FText> ActualToolTip;
	if (ToolBarButtonBlock->ToolTipOverride.IsSet())
	{
		ActualToolTip = ToolBarButtonBlock->ToolTipOverride;
	}
	else
	{
		ActualToolTip = UICommand.IsValid() ? UICommand->GetDescription() : FText::GetEmpty();
	}

	// If a key is bound to the command, append it to the tooltip text.
	TWeakPtr<const FUICommandInfo> Action = ToolBarButtonBlock->GetAction();
	ActualToolTip = TAttribute< FText >::Create(TAttribute<FText>::FGetter::CreateStatic(&Local::AppendKeyBindingToToolTip, ActualToolTip, Action ) );
	
	// If we were supplied an image than go ahead and use that, otherwise we use a null widget
	TSharedRef<SLayeredImage> IconWidget =
		SNew(SLayeredImage)
		.ColorAndOpacity(this, &SToolBarButtonBlock::GetIconForegroundColor)
		.Visibility(EVisibility::HitTestInvisible)
		.Image(this, &SToolBarButtonBlock::GetIconBrush);

	IconWidget->AddLayer(TAttribute<const FSlateBrush*>(this, &SToolBarButtonBlock::GetOverlayIconBrush));
	const bool bIsSlimHorizontalUniformToolBar = MultiBox->GetType() == EMultiBoxType::SlimHorizontalUniformToolBar;

	// Create the content for our button
	TSharedRef<SWidget> ButtonContent = SNullWidget::NullWidget;
	if (MultiBox->GetType() == EMultiBoxType::SlimHorizontalToolBar 
		|| bIsSlimHorizontalUniformToolBar)
	{
		const FVector2f IconSize = ToolBarStyle.IconSize;
		const TSharedRef<STextBlock> TextBlock = SNew(STextBlock)
				.Visibility(LabelVisibility)
				.Text(ActualLabel)
				.TextStyle(&ToolBarStyle.LabelStyle); // Smaller font for tool tip labels

		if (bIsSlimHorizontalUniformToolBar)
		{
			TextBlock->SetOverflowPolicy(ETextOverflowPolicy::Ellipsis);
			TextBlock->SetVisibility(EVisibility::Visible);
		}

		IconWidget->SetDesiredSizeOverride(FVector2D(IconSize));
		ButtonContent =
			SNew(SHorizontalBox)
			.AddMetaData<FTagMetaData>(FTagMetaData(TutorialHighlightName))
			+ SHorizontalBox::Slot()
			.AutoWidth()
		    .Padding(ToolBarStyle.IconPadding)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				IconWidget
			]
			// Label text
		+ (bIsSlimHorizontalUniformToolBar ?
		SHorizontalBox::Slot()
		.Padding(ToolBarStyle.LabelPadding)
		.VAlign(VAlign_Center) :
		SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(ToolBarStyle.LabelPadding)
		.VAlign(VAlign_Center))
		[ TextBlock ];
	}
	else
	{
		const FMargin IconPadding = !LabelVisibility.IsSet() ? ToolBarStyle.IconPadding :
		(LabelVisibility.Get() == EVisibility::Collapsed ? ToolBarStyle.IconPadding : ToolBarStyle.IconPaddingWithVisibleLabel);
		const TSharedRef ContentVBox = 
			SNew(SVerticalBox)
					// Icon image
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(IconPadding)
					.HAlign(HAlign_Center)	// Center the icon horizontally, so that large labels don't stretch out the artwork
					[
						IconWidget
					]
					// Label text
					+ SVerticalBox::Slot().AutoHeight()
				.Padding(ToolBarStyle.LabelPadding)
					.HAlign(HAlign_Center)	// Center the label text horizontally
					[
						SNew(STextBlock)
						.Visibility(LabelVisibility)
						.Text(ActualLabel)
						.TextStyle(&ToolBarStyle.LabelStyle)	// Smaller font for tool tip labels
					];

		ButtonContent =
			SNew(SHorizontalBox)
			.AddMetaData<FTagMetaData>(FTagMetaData(TutorialHighlightName))
			+ ( MultiBox->GetType() == EMultiBoxType::VerticalToolBar ?
				SHorizontalBox::Slot()
			.MaxWidth(ToolBarStyle.ButtonContentMaxWidth)
			.SizeParam(FStretch())
			.VAlign(VAlign_Center) :
			SHorizontalBox::Slot()
			.FillWidth(ToolBarStyle.ButtonContentFillWidth)
			.VAlign(VAlign_Center))
			[
				ContentVBox
			];
		}

	EMultiBlockLocation::Type BlockLocation = GetMultiBlockLocation();
	
	// What type of UI should we create for this block?
	EUserInterfaceActionType UserInterfaceType = ToolBarButtonBlock->UserInterfaceActionType;
	if ( Action.IsValid() )
	{
		// If we have a UICommand, then this is specified in the command.
		UserInterfaceType = Action.Pin()->GetUserInterfaceType();
	}
	
	if( UserInterfaceType == EUserInterfaceActionType::Button )
	{
		FName BlockStyle = EMultiBlockLocation::ToName(ISlateStyle::Join( StyleName, ".Button" ), BlockLocation);
		const FButtonStyle* ToolbarButtonStyle = BlockLocation == EMultiBlockLocation::None ? &ToolBarStyle.ButtonStyle : &StyleSet->GetWidgetStyle<FButtonStyle>(BlockStyle);

		if (OptionsBlockWidget.IsValid())
		{
			ToolbarButtonStyle = &ToolBarStyle.SettingsButtonStyle;
		}

		ChildSlot
		[
			// Create a button
			SNew(SButton)
			.ContentPadding(0.f)
			.ButtonStyle(ToolbarButtonStyle)
			.IsEnabled(this, &SToolBarButtonBlock::IsEnabled)
			.OnClicked(this, &SToolBarButtonBlock::OnClicked)
			.ToolTip(FMultiBoxSettings::ToolTipConstructor.Execute(ActualToolTip, nullptr, Action.Pin()))
			.IsFocusable(bIsFocusable)
			[
				ButtonContent
			]
		];
	}
	else if( ensure( UserInterfaceType == EUserInterfaceActionType::ToggleButton || UserInterfaceType == EUserInterfaceActionType::RadioButton ) )
	{
		FName BlockStyle = EMultiBlockLocation::ToName(ISlateStyle::Join( StyleName, ".ToggleButton" ), BlockLocation);
	
		const FCheckBoxStyle* CheckStyle = BlockLocation == EMultiBlockLocation::None ? &ToolBarStyle.ToggleButton : &StyleSet->GetWidgetStyle<FCheckBoxStyle>(BlockStyle);

		if (OptionsBlockWidget.IsValid())
		{
			CheckStyle = &ToolBarStyle.SettingsToggleButton;
		}

		const TSharedPtr<SWidget> CheckBox = SNew(SCheckBox)
						// Use the tool bar style for this check box
						.Style(CheckStyle)
						.CheckBoxContentUsesAutoWidth(false)
						.IsFocusable(bIsFocusable)
						.ToolTip( FMultiBoxSettings::ToolTipConstructor.Execute( ActualToolTip, nullptr, Action.Pin()))		
						.OnCheckStateChanged(this, &SToolBarButtonBlock::OnCheckStateChanged )
						.OnGetMenuContent( ToolBarButtonBlock->OnGetMenuContent )
						.IsChecked(this, &SToolBarButtonBlock::GetCheckState)
						.IsEnabled(this, &SToolBarButtonBlock::IsEnabled)
						[
							ButtonContent
						];

		TSharedRef<SWidget> CheckBoxWidget = CheckBox.ToSharedRef();

		if (!ToolBarButtonBlock->BorderBrushName.Get().IsNone())
		{
			const FSlateBrush* Brush = FAppStyle::GetBrush(ToolBarButtonBlock->BorderBrushName.Get());
			CheckBoxWidget =
				SNew(SBorder)
				.BorderImage(Brush)
				.Padding(2.f)
				[
					CheckBox.ToSharedRef()
				];
		}

		ChildSlot
		[
			CheckBoxWidget
		];
	}

	if (OptionsBlockWidget.IsValid())
	{
		ChildSlot
		.Padding(ToolBarStyle.ComboButtonPadding.Left, 0.0f, ToolBarStyle.ComboButtonPadding.Right, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SAssignNew(ButtonBorder, SBorder)
				.Padding(0)
				.BorderImage(this, &SToolBarButtonBlock::GetOptionsBlockLeftBrush)
				.VAlign(VAlign_Center)
				[
					ChildSlot.GetWidget()
				]
			]
			+ SHorizontalBox::Slot()
	
			.AutoWidth()
			[
				SAssignNew(OptionsBorder, SBorder)
				.Padding(0)
				.BorderImage(this, &SToolBarButtonBlock::GetOptionsBlockRightBrush)
				.VAlign(VAlign_Center)
				[
					OptionsBlockWidget.ToSharedRef()
				]
			]
		];
	}
	else
	{
		// Space between buttons. It does not make the buttons larger
		ChildSlot.Padding(ToolBarStyle.ButtonPadding);
	}

	// Bind our widget's visible state to whether or not the button should be visible
	SetVisibility(TAttribute<EVisibility>(this, &SToolBarButtonBlock::GetBlockVisibility));
}

bool FToolBarButtonBlock::GetIsFocusable() const
{
	return bIsFocusable;
}


/**
 * Called by Slate when this tool bar button's button is clicked
 */
FReply SToolBarButtonBlock::OnClicked()
{
	// Button was clicked, so trigger the action!
	TSharedPtr< const FUICommandList > ActionList = MultiBlock->GetActionList();
	TSharedPtr< const FUICommandInfo > Action = MultiBlock->GetAction();
	const FUIAction& DirectActions = MultiBlock->GetDirectActions();
	
	if( ActionList.IsValid() && Action.IsValid() )
	{
		ActionList->ExecuteAction( Action.ToSharedRef() );
	}
	else
	{
		// There is no action list or action associated with this block via a UI command.  Execute any direct action we have
		MultiBlock->GetDirectActions().Execute();
	}

	TSharedRef< const FMultiBox > MultiBox( OwnerMultiBoxWidget.Pin()->GetMultiBox() );

	// If this is a context menu, then we'll also dismiss the window after the user clicked on the item
	const bool ClosingMenu = MultiBox->ShouldCloseWindowAfterMenuSelection();
	if( ClosingMenu )
	{
		FSlateApplication::Get().DismissMenuByWidget(AsShared());
	}

	return FReply::Handled();
}



/**
 * Called by Slate when this tool bar check box button is toggled
 */
void SToolBarButtonBlock::OnCheckStateChanged( const ECheckBoxState NewCheckedState )
{
	OnClicked();
}

/**
 * Called by slate to determine if this button should appear checked
 *
 * @return ECheckBoxState::Checked if it should be checked, ECheckBoxState::Unchecked if not.
 */
ECheckBoxState SToolBarButtonBlock::GetCheckState() const
{
	TSharedPtr<const FUICommandList> ActionList = MultiBlock->GetActionList();
	TSharedPtr<const FUICommandInfo> Action = MultiBlock->GetAction();
	const FUIAction& DirectActions = MultiBlock->GetDirectActions();

	ECheckBoxState CheckState = ECheckBoxState::Unchecked;
	if( ActionList.IsValid() && Action.IsValid() )
	{
		CheckState = ActionList->GetCheckState( Action.ToSharedRef() );
	}
	else
	{
		// There is no action list or action associated with this block via a UI command.  Execute any direct action we have
		CheckState = DirectActions.GetCheckState();
	}

	return CheckState;
}

/**
 * Called by Slate to determine if this button is enabled
 * 
 * @return True if the menu entry is enabled, false otherwise
 */
bool SToolBarButtonBlock::IsEnabled() const
{
	TSharedPtr< const FUICommandList > ActionList = MultiBlock->GetActionList();
	TSharedPtr< const FUICommandInfo > Action = MultiBlock->GetAction();
	const FUIAction& DirectActions = MultiBlock->GetDirectActions();

	bool bEnabled = true;
	if( ActionList.IsValid() && Action.IsValid() )
	{
		bEnabled = ActionList->CanExecuteAction( Action.ToSharedRef() );
	}
	else
	{
		// There is no action list or action associated with this block via a UI command.  Execute any direct action we have
		bEnabled = DirectActions.CanExecute();
	}

	return bEnabled;
}


/**
 * Called by Slate to determine if this button is visible
 *
 * @return EVisibility::Visible or EVisibility::Collapsed, depending on if the button should be displayed
 */
EVisibility SToolBarButtonBlock::GetBlockVisibility() const
{
	TSharedPtr< const FUICommandList > ActionList = MultiBlock->GetActionList();
	const FUIAction& DirectActions = MultiBlock->GetDirectActions();
	if( ActionList.IsValid() )
	{
		return ActionList->GetVisibility( MultiBlock->GetAction().ToSharedRef() );
	}
	else if(DirectActions.IsActionVisibleDelegate.IsBound())
	{
		return DirectActions.IsActionVisibleDelegate.Execute() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	return EVisibility::Visible;
}

EVisibility SToolBarButtonBlock::GetIconVisibility(bool bIsASmallIcon) const
{
	return ((bForceSmallIcons || FMultiBoxSettings::UseSmallToolBarIcons.Get()) ^ bIsASmallIcon) ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
}

const FSlateBrush* SToolBarButtonBlock::GetIconBrush() const
{
	return bForceSmallIcons || FMultiBoxSettings::UseSmallToolBarIcons.Get() ? GetSmallIconBrush() : GetNormalIconBrush();
}

const FSlateBrush* SToolBarButtonBlock::GetOverlayIconBrush() const
{
	TSharedRef<const FToolBarButtonBlock> ToolBarButtonBlock = StaticCastSharedRef<const FToolBarButtonBlock >(MultiBlock.ToSharedRef());

	const FSlateIcon ActionIcon = ToolBarButtonBlock->GetAction().IsValid() ? ToolBarButtonBlock->GetAction()->GetIcon() : FSlateIcon();
	const FSlateIcon& ActualIcon = ToolBarButtonBlock->IconOverride.IsSet() ? ToolBarButtonBlock->IconOverride.Get() : ActionIcon;

	if (ActualIcon.IsSet())
	{
		return ActualIcon.GetOverlayIcon();
	}

	return nullptr;
}

const FSlateBrush* SToolBarButtonBlock::GetNormalIconBrush() const
{
	TSharedRef<const FToolBarButtonBlock> ToolBarButtonBlock = StaticCastSharedRef<const FToolBarButtonBlock >(MultiBlock.ToSharedRef());

	const FSlateIcon ActionIcon = ToolBarButtonBlock->GetAction().IsValid() ? ToolBarButtonBlock->GetAction()->GetIcon() : FSlateIcon();
	const FSlateIcon& ActualIcon = ToolBarButtonBlock->IconOverride.IsSet() ? ToolBarButtonBlock->IconOverride.Get() : ActionIcon;

	if (ActualIcon.IsSet())
	{
		return ActualIcon.GetIcon();
	}
	else
	{
		check(OwnerMultiBoxWidget.IsValid());

		TSharedPtr<SMultiBoxWidget> MultiBoxWidget = OwnerMultiBoxWidget.Pin();
		const ISlateStyle* const StyleSet = MultiBoxWidget->GetStyleSet();

		static const FName IconName("MultiBox.GenericToolBarIcon");
		return StyleSet->GetBrush(IconName);
	}
}

const FSlateBrush* SToolBarButtonBlock::GetSmallIconBrush() const
{
	TSharedRef< const FToolBarButtonBlock > ToolBarButtonBlock = StaticCastSharedRef< const FToolBarButtonBlock >( MultiBlock.ToSharedRef() );
	
	const FSlateIcon ActionIcon = ToolBarButtonBlock->GetAction().IsValid() ? ToolBarButtonBlock->GetAction()->GetIcon() : FSlateIcon();
	const FSlateIcon& ActualIcon = ToolBarButtonBlock->IconOverride.IsSet() ? ToolBarButtonBlock->IconOverride.Get() : ActionIcon;
	
	if( ActualIcon.IsSet() )
	{
		return ActualIcon.GetSmallIcon();
	}
	else
	{
		check( OwnerMultiBoxWidget.IsValid() );

		TSharedPtr<SMultiBoxWidget> MultiBoxWidget = OwnerMultiBoxWidget.Pin();
		const ISlateStyle* const StyleSet = MultiBoxWidget->GetStyleSet();

		static const FName IconName("MultiBox.GenericToolBarIcon.Small" );
		return StyleSet->GetBrush(IconName);
	}
}

FSlateColor SToolBarButtonBlock::GetIconForegroundColor() const
{
	// If any brush has a tint, don't assume it should be subdued
	const FSlateBrush* Brush = GetIconBrush();
	if (Brush && Brush->TintColor != FLinearColor::White)
	{
		return FLinearColor::White;
	}

	return FSlateColor::UseForeground();
}

const FSlateBrush* SToolBarButtonBlock::GetOptionsBlockLeftBrush() const
{
	static const FName ToggledLeft("ToolbarSettingsRegion.LeftToggle");

	if (ButtonBorder->IsHovered())
	{
		static const FName LeftHover("ToolbarSettingsRegion.LeftHover");
		static const FName ToggledLeftHover("ToolbarSettingsRegion.LeftToggleHover");

		return GetCheckState() == ECheckBoxState::Checked ? FAppStyle::Get().GetBrush(ToggledLeftHover) : FAppStyle::Get().GetBrush(LeftHover);
	}
	else if (OptionsBorder->IsHovered())
	{
		static const FName Left("ToolbarSettingsRegion.Left");
		return GetCheckState() == ECheckBoxState::Checked ? FAppStyle::Get().GetBrush(ToggledLeft) : FAppStyle::Get().GetBrush(Left);
	}
	else
	{
		return GetCheckState() == ECheckBoxState::Checked ? FAppStyle::Get().GetBrush(ToggledLeft) : FStyleDefaults::GetNoBrush();
	}

}

const FSlateBrush* SToolBarButtonBlock::GetOptionsBlockRightBrush() const
{
	if (OptionsBorder->IsHovered())
	{
		static const FName RightHover("ToolbarSettingsRegion.RightHover");
		return FAppStyle::Get().GetBrush(RightHover);
	}
	else if (ButtonBorder->IsHovered() || GetCheckState() == ECheckBoxState::Checked)
	{
		static const FName Right("ToolbarSettingsRegion.Right");
		return FAppStyle::Get().GetBrush(Right);
	}
	else
	{
		return FStyleDefaults::GetNoBrush();
	}
}

EVisibility SToolBarButtonBlock::GetOptionsSeparatorVisibility() const
{
	return IsHovered() ? EVisibility::HitTestInvisible : EVisibility::Hidden;
}

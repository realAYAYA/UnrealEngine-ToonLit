// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SBoxPanel.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#if WITH_ACCESSIBILITY
#include "Widgets/Accessibility/SlateAccessibleWidgets.h"
#include "Widgets/Accessibility/SlateAccessibleMessageHandler.h"
#endif

SCheckBox::SCheckBox()
{
#if WITH_ACCESSIBILITY
	AccessibleBehavior = EAccessibleBehavior::Summary;
	bCanChildrenBeAccessible = false;
#endif
}

/**
 * Construct this widget
 *
 * @param	InArgs	The declaration data for this widget
 */
void SCheckBox::Construct( const SCheckBox::FArguments& InArgs )
{
	check(InArgs._Style != nullptr);
	Style = InArgs._Style;

	UncheckedImage = InArgs._UncheckedImage;
	UncheckedHoveredImage = InArgs._UncheckedHoveredImage;
	UncheckedPressedImage = InArgs._UncheckedPressedImage;

	CheckedImage = InArgs._CheckedImage;
	CheckedHoveredImage = InArgs._CheckedHoveredImage;
	CheckedPressedImage = InArgs._CheckedPressedImage;

	UndeterminedImage = InArgs._UndeterminedImage;
	UndeterminedHoveredImage = InArgs._UndeterminedHoveredImage;
	UndeterminedPressedImage = InArgs._UndeterminedPressedImage;

	BackgroundImage = InArgs._BackgroundImage;
	BackgroundHoveredImage = InArgs._BackgroundHoveredImage;
	BackgroundPressedImage = InArgs._BackgroundPressedImage;
	
	PaddingOverride = InArgs._Padding;
	ForegroundColorOverride = InArgs._ForegroundColor;
	BorderBackgroundColorOverride = InArgs._BorderBackgroundColor;
	CheckBoxTypeOverride = InArgs._Type;

	HorizontalAlignment = InArgs._HAlign;
	bCheckBoxContentUsesAutoWidth = InArgs._CheckBoxContentUsesAutoWidth;

	bIsPressed = false;
	bIsFocusable = InArgs._IsFocusable;

	BuildCheckBox(InArgs._Content.Widget);

	IsCheckboxChecked = InArgs._IsChecked;
	OnCheckStateChanged = InArgs._OnCheckStateChanged;

	ClickMethod = InArgs._ClickMethod;
	TouchMethod = InArgs._TouchMethod;
	PressMethod = InArgs._PressMethod;

	OnGetMenuContent = InArgs._OnGetMenuContent;

	HoveredSound = InArgs._HoveredSoundOverride.Get(InArgs._Style->HoveredSlateSound);
	CheckedSound = InArgs._CheckedSoundOverride.Get(InArgs._Style->CheckedSlateSound);
	UncheckedSound = InArgs._UncheckedSoundOverride.Get(InArgs._Style->UncheckedSlateSound);
}

/**
 * See SWidget::SupportsKeyboardFocus().
 *
 * @return  True if this widget can take keyboard focus
 */
bool SCheckBox::SupportsKeyboardFocus() const
{
	// Buttons are focusable by default
	return bIsFocusable;
}

FReply SCheckBox::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (FSlateApplication::Get().GetNavigationActionFromKey(InKeyEvent) == EUINavigationAction::Accept)
	{
		bIsPressed = true;

		if (PressMethod == EButtonPressMethod::ButtonPress)
		{
			ToggleCheckedState();

			const ECheckBoxState State = IsCheckboxChecked.Get();
			if (State == ECheckBoxState::Checked)
			{
				PlayCheckedSound();
			}
			else if (State == ECheckBoxState::Unchecked)
			{
				PlayUncheckedSound();
			}
		}

		return FReply::Handled();
	}

	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);

}

FReply SCheckBox::OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	if (FSlateApplication::Get().GetNavigationActionFromKey(InKeyEvent) == EUINavigationAction::Accept)
	{
		const bool bWasPressed = bIsPressed;
		bIsPressed = false;

		if (PressMethod == EButtonPressMethod::ButtonRelease || (PressMethod == EButtonPressMethod::DownAndUp && bWasPressed))
		{
			ToggleCheckedState();

			const ECheckBoxState State = IsCheckboxChecked.Get();
			if (State == ECheckBoxState::Checked)
			{
				PlayCheckedSound();
			}
			else if (State == ECheckBoxState::Unchecked)
			{
				PlayUncheckedSound();
			}
		}

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

/**
 * See SWidget::OnMouseButtonDown.
 *
 * @param MyGeometry The Geometry of the widget receiving the event
 * @param MouseEvent Information about the input event
 *
 * @return Whether the event was handled along with possible requests for the system to take action.
 */
FReply SCheckBox::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		bIsPressed = true;

		EButtonClickMethod::Type InputClickMethod = GetClickMethodFromInputType(MouseEvent);

		if(InputClickMethod == EButtonClickMethod::MouseDown )
		{
			ToggleCheckedState();
			const ECheckBoxState State = IsCheckboxChecked.Get();
			if(State == ECheckBoxState::Checked)
			{
				PlayCheckedSound();
			}
			else if(State == ECheckBoxState::Unchecked)
			{
				PlayUncheckedSound();
			}

			// Set focus to this button, but don't capture the mouse
			return FReply::Handled().SetUserFocus(AsShared(), EFocusCause::Mouse);
		}
		else
		{
			// Capture the mouse, and also set focus to this button
			return FReply::Handled().CaptureMouse(AsShared()).SetUserFocus(AsShared(), EFocusCause::Mouse);
		}
	}
	else if ( MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && OnGetMenuContent.IsBound() )
	{
		FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();

		FSlateApplication::Get().PushMenu(
			AsShared(),
			WidgetPath,
			OnGetMenuContent.Execute(),
			MouseEvent.GetScreenSpacePosition(),
			FPopupTransitionEffect( FPopupTransitionEffect::ContextMenu )
			);

		return FReply::Handled();
	}
	else
	{
		return FReply::Unhandled();
	}
}

/**
 * See SWidget::OnMouseButtonDoubleClick.
 *
 * @param MyGeometry The Geometry of the widget receiving the event
 * @param MouseEvent Information about the input event
 *
 * @return Whether the event was handled along with possible requests for the system to take action.
 */
FReply SCheckBox::OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	return OnMouseButtonDown( InMyGeometry, InMouseEvent );
}

/**
 * See SWidget::OnMouseButtonUp.
 *
 * @param MyGeometry The Geometry of the widget receiving the event
 * @param MouseEvent Information about the input event
 *
 * @return Whether the event was handled along with possible requests for the system to take action.
 */
FReply SCheckBox::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	const EButtonClickMethod::Type InputClickMethod = GetClickMethodFromInputType(MouseEvent);
	const bool bMustBePressed = InputClickMethod == EButtonClickMethod::DownAndUp || InputClickMethod == EButtonClickMethod::PreciseClick;
	const bool bMeetsPressedRequirements = (!bMustBePressed || (bIsPressed && bMustBePressed));

	if (bMeetsPressedRequirements && ((MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton || MouseEvent.IsTouchEvent())))
	{
		bIsPressed = false;

		if(InputClickMethod == EButtonClickMethod::MouseDown )
		{
			// NOTE: If we're configured to click on mouse-down/precise-tap, then we never capture the mouse thus
			//       may never receive an OnMouseButtonUp() call.  We make sure that our bIsPressed
			//       state is reset by overriding OnMouseLeave().
		}
		else
		{
			const bool IsUnderMouse = MyGeometry.IsUnderLocation( MouseEvent.GetScreenSpacePosition() );
			if ( IsUnderMouse )
			{
				// If we were asked to allow the button to be clicked on mouse up, regardless of whether the user
				// pressed the button down first, then we'll allow the click to proceed without an active capture
				if(InputClickMethod == EButtonClickMethod::MouseUp || HasMouseCapture() )
				{
					ToggleCheckedState();
					const ECheckBoxState State = IsCheckboxChecked.Get();
					if(State == ECheckBoxState::Checked)
					{
						PlayCheckedSound();
					}
					else if(State == ECheckBoxState::Unchecked)
					{
						PlayUncheckedSound();
					}
				}
			}
		}

		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}

void SCheckBox::OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	PlayHoverSound();
	SCompoundWidget::OnMouseEnter( MyGeometry, MouseEvent );
}


void SCheckBox::OnMouseLeave( const FPointerEvent& MouseEvent )
{
	// Call parent implementation
	SWidget::OnMouseLeave( MouseEvent );

	// If we're setup to click on mouse-down, then we never capture the mouse and may not receive a
	// mouse up event, so we need to make sure our pressed state is reset properly here
	if( ClickMethod == EButtonClickMethod::MouseDown || IsPreciseTapOrClick(MouseEvent) )
	{
		bIsPressed = false;
	}
}


bool SCheckBox::IsInteractable() const
{
	return IsEnabled();
}

FSlateColor SCheckBox::GetForegroundColor() const
{
	FSlateColor UserColor = ForegroundColorOverride.Get();

	if (UserColor == FSlateColor::UseStyle())
	{
		ECheckBoxState State = IsCheckboxChecked.Get();

		switch (State)
		{
		case ECheckBoxState::Unchecked:
			return bIsPressed ? Style->PressedForeground : IsHovered() ? Style->HoveredForeground : Style->ForegroundColor;
			break;
		case ECheckBoxState::Checked:
			return bIsPressed ? Style->CheckedPressedForeground : IsHovered() ? Style->CheckedHoveredForeground : Style->CheckedForeground;
			break;
		default:
		case ECheckBoxState::Undetermined:
			return Style->UndeterminedForeground;
			break;
		}
	}

	return UserColor;
}

/**
 * Gets the check image to display for the current state of the check box
 * @return	The name of the image to display
 */
const FSlateBrush* SCheckBox::OnGetCheckImage() const
{
	ECheckBoxState State = IsCheckboxChecked.Get();

	const FSlateBrush* ImageToUse;
	switch( State )
	{
		case ECheckBoxState::Unchecked:
			ImageToUse = IsPressed() ? GetUncheckedPressedImage() : ( IsHovered() ? GetUncheckedHoveredImage() : GetUncheckedImage() );
			break;
	
		case ECheckBoxState::Checked:
			ImageToUse = IsPressed() ? GetCheckedPressedImage() : ( IsHovered() ? GetCheckedHoveredImage() : GetCheckedImage() );
			break;
	
		default:
		case ECheckBoxState::Undetermined:
			ImageToUse = IsPressed() ? GetUndeterminedPressedImage() : ( IsHovered() ? GetUndeterminedHoveredImage() : GetUndeterminedImage() );
			break;
	}

	return ImageToUse;
}

const FSlateBrush* SCheckBox::OnGetBackgroundImage() const 
{
	return IsPressed() ? GetBackgroundPressedImage() : ( IsHovered() ? GetBackgroundHoveredImage() : GetBackgroundImage() );
}


ECheckBoxState SCheckBox::GetCheckedState() const
{
	return IsCheckboxChecked.Get();
}

/**
 * Toggles the checked state for this check box, fire events as needed
 */
void SCheckBox::ToggleCheckedState()
{
	const ECheckBoxState State = IsCheckboxChecked.Get();

	// If the current check box state is checked OR undetermined we set the check box to checked.
	if( State == ECheckBoxState::Checked || State == ECheckBoxState::Undetermined )
	{
		if ( !IsCheckboxChecked.IsBound() )
		{
			// When we are not bound, just toggle the current state.
			IsCheckboxChecked.Set( ECheckBoxState::Unchecked );
		}

		// The state of the check box changed.  Execute the delegate to notify users
		OnCheckStateChanged.ExecuteIfBound( ECheckBoxState::Unchecked );
	}
	else if( State == ECheckBoxState::Unchecked )
	{
		if ( !IsCheckboxChecked.IsBound() )
		{
			// When we are not bound, just toggle the current state.
			IsCheckboxChecked.Set( ECheckBoxState::Checked );
		}

		// The state of the check box changed.  Execute the delegate to notify users
		OnCheckStateChanged.ExecuteIfBound( ECheckBoxState::Checked );
	}

#if WITH_ACCESSIBILITY
	// @TODOAccessibility: Technically we should pass the Id of the user that toggled the checkbox, but we don't want to change the Slate API as much as possible
	FSlateApplicationBase::Get().GetAccessibleMessageHandler()->OnWidgetEventRaised(FSlateAccessibleMessageHandler::FSlateWidgetAccessibleEventArgs(AsShared(), EAccessibleEvent::Activate, State == ECheckBoxState::Checked, IsCheckboxChecked.Get() == ECheckBoxState::Checked));
#endif
}

void SCheckBox::SetIsChecked(TAttribute<ECheckBoxState> InIsChecked)
{
	IsCheckboxChecked = InIsChecked;
}

TEnumAsByte<EButtonClickMethod::Type> SCheckBox::GetClickMethodFromInputType(const FPointerEvent& MouseEvent) const
{
	if (MouseEvent.IsTouchEvent())
	{
		switch (TouchMethod)
		{
		case EButtonTouchMethod::Down:
			return EButtonClickMethod::MouseDown;
		case EButtonTouchMethod::DownAndUp:
			return EButtonClickMethod::DownAndUp;
		case EButtonTouchMethod::PreciseTap:
			return EButtonClickMethod::PreciseClick;
		}
	}

	return ClickMethod;
}

bool SCheckBox::IsPreciseTapOrClick(const FPointerEvent& MouseEvent) const
{
	return GetClickMethodFromInputType(MouseEvent) == EButtonClickMethod::PreciseClick;
}

void SCheckBox::PlayCheckedSound() const
{
	FSlateApplication::Get().PlaySound( CheckedSound );
}

void SCheckBox::PlayUncheckedSound() const
{
	FSlateApplication::Get().PlaySound( UncheckedSound );
}

void SCheckBox::PlayHoverSound() const
{
	FSlateApplication::Get().PlaySound( HoveredSound );
}

void SCheckBox::SetContent(const TSharedRef< SWidget >& InContent)
{
	ContentContainer->SetContent(InContent);
}

void SCheckBox::SetStyle(const FCheckBoxStyle* InStyle)
{
	Style = InStyle;

	if (Style == nullptr)
	{
		FArguments Defaults;
		Style = Defaults._Style;
	}

	check(Style);

	BuildCheckBox(ContentContainer->GetContent());
}

void SCheckBox::SetUncheckedImage(const FSlateBrush* Brush)
{
	UncheckedImage = Brush;
}

void SCheckBox::SetUncheckedHoveredImage(const FSlateBrush* Brush)
{
	UncheckedHoveredImage = Brush;
}

void SCheckBox::SetUncheckedPressedImage(const FSlateBrush* Brush)
{
	UncheckedPressedImage = Brush;
}

void SCheckBox::SetCheckedImage(const FSlateBrush* Brush)
{
	CheckedImage = Brush;
}

void SCheckBox::SetCheckedHoveredImage(const FSlateBrush* Brush)
{
	CheckedHoveredImage = Brush;
}

void SCheckBox::SetCheckedPressedImage(const FSlateBrush* Brush)
{
	CheckedPressedImage = Brush;
}

void SCheckBox::SetUndeterminedImage(const FSlateBrush* Brush)
{
	UndeterminedImage = Brush;
}

void SCheckBox::SetUndeterminedHoveredImage(const FSlateBrush* Brush)
{
	UndeterminedHoveredImage = Brush;
}

void SCheckBox::SetUndeterminedPressedImage(const FSlateBrush* Brush)
{
	UndeterminedPressedImage = Brush;
}

void SCheckBox::BuildCheckBox(TSharedRef<SWidget> InContent)
{
	if (ContentContainer.IsValid())
	{
		ContentContainer->SetContent(SNullWidget::NullWidget);
	}

	ESlateCheckBoxType::Type CheckBoxType = OnGetCheckBoxType();

	if (CheckBoxType == ESlateCheckBoxType::CheckBox)
	{
		// Check boxes use a separate check button to the side of the user's content (often, a text label or icon.)
		SHorizontalBox::FSlot* ContentSlot;
		this->ChildSlot
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SOverlay)

				+SOverlay::Slot()
				[
					SNew(SImage)
					.Image(this, &SCheckBox::OnGetBackgroundImage)
				]
				+SOverlay::Slot()
				[
					SNew(SImage)
					.Image(this, &SCheckBox::OnGetCheckImage)
					.ColorAndOpacity(FSlateColor::UseForeground())
				]	
			]
			+ SHorizontalBox::Slot()
			.Padding(TAttribute<FMargin>(this, &SCheckBox::OnGetPadding))
			.VAlign(VAlign_Center)
			.Expose(ContentSlot)
			[
				SAssignNew(ContentContainer, SBorder)
				.BorderImage(FStyleDefaults::GetNoBrush())
				.Padding(0.0f)
				[
					InContent
				]
			]
		];
		if (bCheckBoxContentUsesAutoWidth)
		{
			ContentSlot->SetAutoWidth();
		}
	}
	else if (ensure(CheckBoxType == ESlateCheckBoxType::ToggleButton))
	{
		// Toggle buttons have a visual appearance that is similar to a Slate button
		this->ChildSlot
		[
			SAssignNew(ContentContainer, SBorder)
			.BorderImage(this, &SCheckBox::OnGetCheckImage)
			.Padding(this, &SCheckBox::OnGetPadding)
			.BorderBackgroundColor(this, &SCheckBox::OnGetBorderBackgroundColor)
			.HAlign(HorizontalAlignment)
			[
				InContent
			]
		];
	}
}


FMargin SCheckBox::OnGetPadding() const
{
	return PaddingOverride.IsSet() ? PaddingOverride.Get() : Style->Padding;
}

FSlateColor SCheckBox::OnGetBorderBackgroundColor() const
{
	return BorderBackgroundColorOverride.IsSet() ? BorderBackgroundColorOverride.Get() : Style->BorderBackgroundColor;
}

ESlateCheckBoxType::Type SCheckBox::OnGetCheckBoxType() const
{
	return CheckBoxTypeOverride.IsSet() ? CheckBoxTypeOverride.GetValue() : Style->CheckBoxType.GetValue();
}

const FSlateBrush* SCheckBox::GetUncheckedImage() const
{
	return UncheckedImage ? UncheckedImage : &Style->UncheckedImage;
}

const FSlateBrush* SCheckBox::GetUncheckedHoveredImage() const
{
	return UncheckedHoveredImage ? UncheckedHoveredImage : &Style->UncheckedHoveredImage;
}

const FSlateBrush* SCheckBox::GetUncheckedPressedImage() const
{
	return UncheckedPressedImage ? UncheckedPressedImage : &Style->UncheckedPressedImage;
}

const FSlateBrush* SCheckBox::GetCheckedImage() const
{
	return CheckedImage ? CheckedImage : &Style->CheckedImage;
}

const FSlateBrush* SCheckBox::GetCheckedHoveredImage() const
{
	return CheckedHoveredImage ? CheckedHoveredImage : &Style->CheckedHoveredImage;
}

const FSlateBrush* SCheckBox::GetCheckedPressedImage() const
{
	return CheckedPressedImage ? CheckedPressedImage : &Style->CheckedPressedImage;
}

const FSlateBrush* SCheckBox::GetUndeterminedImage() const
{
	return UndeterminedImage ? UndeterminedImage : &Style->UndeterminedImage;
}

const FSlateBrush* SCheckBox::GetUndeterminedHoveredImage() const
{
	return UndeterminedHoveredImage ? UndeterminedHoveredImage : &Style->UndeterminedHoveredImage;
}

const FSlateBrush* SCheckBox::GetUndeterminedPressedImage() const
{
	return UndeterminedPressedImage ? UndeterminedPressedImage : &Style->UndeterminedPressedImage;
}

const FSlateBrush* SCheckBox::GetBackgroundImage() const
{
	return BackgroundImage ? BackgroundImage : &Style->BackgroundImage;
}

const FSlateBrush* SCheckBox::GetBackgroundHoveredImage() const
{
	return BackgroundHoveredImage ? BackgroundHoveredImage : &Style->BackgroundHoveredImage;
}

const FSlateBrush* SCheckBox::GetBackgroundPressedImage() const
{
	return BackgroundPressedImage ? BackgroundPressedImage : &Style->BackgroundPressedImage;
}

void SCheckBox::SetClickMethod(EButtonClickMethod::Type InClickMethod)
{
	ClickMethod = InClickMethod;
}

void SCheckBox::SetTouchMethod(EButtonTouchMethod::Type InTouchMethod)
{
	TouchMethod = InTouchMethod;
}

void SCheckBox::SetPressMethod(EButtonPressMethod::Type InPressMethod)
{
	PressMethod = InPressMethod;
}

#if WITH_ACCESSIBILITY
TSharedRef<FSlateAccessibleWidget> SCheckBox::CreateAccessibleWidget()
{
	return MakeShareable<FSlateAccessibleWidget>(new FSlateAccessibleCheckBox(SharedThis(this)));
}
#endif

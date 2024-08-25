// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/Text/SMultiLineEditableText.h"
#include "Rendering/DrawElements.h"
#include "Types/SlateConstants.h"
#include "Framework/Application/SlateApplication.h"

#if WITH_FANCY_TEXT

#include "Framework/Text/TextEditHelper.h"
#include "Framework/Text/PlainTextLayoutMarshaller.h"
#include "Widgets/Text/SlateEditableTextLayout.h"
#include "Types/ReflectionMetadata.h"
#include "Types/TrackedMetaData.h"

SMultiLineEditableText::SMultiLineEditableText()
	: bSelectAllTextWhenFocused(false)
	, bIsReadOnly(false)
	, AmountScrolledWhileRightMouseDown(0.0f)
	, bIsSoftwareCursor(false)
{
}

SMultiLineEditableText::~SMultiLineEditableText()
{
	// Needed to avoid "deletion of pointer to incomplete type 'FSlateEditableTextLayout'; no destructor called" error when using TUniquePtr
}

void SMultiLineEditableText::Construct( const FArguments& InArgs )
{
	bIsReadOnly = InArgs._IsReadOnly;

	OnIsTypedCharValid = InArgs._OnIsTypedCharValid;
	OnTextChangedCallback = InArgs._OnTextChanged;
	OnTextCommittedCallback = InArgs._OnTextCommitted;
	OnCursorMovedCallback = InArgs._OnCursorMoved;
	bAllowMultiLine = InArgs._AllowMultiLine;
	bSelectAllTextWhenFocused = InArgs._SelectAllTextWhenFocused;
	bClearTextSelectionOnFocusLoss = InArgs._ClearTextSelectionOnFocusLoss;
	bClearKeyboardFocusOnCommit = InArgs._ClearKeyboardFocusOnCommit;
	bAllowContextMenu = InArgs._AllowContextMenu;
	bSelectWordOnMouseDoubleClick = InArgs._SelectWordOnMouseDoubleClick;
	OnContextMenuOpening = InArgs._OnContextMenuOpening;
	bRevertTextOnEscape = InArgs._RevertTextOnEscape;
	VirtualKeyboardOptions = InArgs._VirtualKeyboardOptions;
	VirtualKeyboardTrigger = InArgs._VirtualKeyboardTrigger;
	VirtualKeyboardDismissAction = InArgs._VirtualKeyboardDismissAction;
	OnHScrollBarUserScrolled = InArgs._OnHScrollBarUserScrolled;
	OnVScrollBarUserScrolled = InArgs._OnVScrollBarUserScrolled;
	OnKeyCharHandler = InArgs._OnKeyCharHandler;
	OnKeyDownHandler = InArgs._OnKeyDownHandler;
	ModiferKeyForNewLine = InArgs._ModiferKeyForNewLine;

	HScrollBar = InArgs._HScrollBar;
	if (HScrollBar.IsValid())
	{
		HScrollBar->SetUserVisibility(EVisibility::Collapsed);
		HScrollBar->SetOnUserScrolled(FOnUserScrolled::CreateSP(this, &SMultiLineEditableText::OnHScrollBarMoved));
	}

	VScrollBar = InArgs._VScrollBar;
	if (VScrollBar.IsValid())
	{
		VScrollBar->SetUserVisibility(EVisibility::Collapsed);
		VScrollBar->SetOnUserScrolled(FOnUserScrolled::CreateSP(this, &SMultiLineEditableText::OnVScrollBarMoved));
	}

	FTextBlockStyle TextStyle = *InArgs._TextStyle;
	if (InArgs._Font.IsSet() || InArgs._Font.IsBound())
	{
		TextStyle.SetFont(InArgs._Font.Get());
	}

	TSharedPtr<ITextLayoutMarshaller> Marshaller = InArgs._Marshaller;
	if (!Marshaller.IsValid())
	{
		Marshaller = FPlainTextLayoutMarshaller::Create();
	}

	EditableTextLayout = MakeUnique<FSlateEditableTextLayout>(*this, InArgs._Text, TextStyle, InArgs._TextShapingMethod, InArgs._TextFlowDirection, InArgs._CreateSlateTextLayout, Marshaller.ToSharedRef(), Marshaller.ToSharedRef());
	EditableTextLayout->SetHintText(InArgs._HintText);
	EditableTextLayout->SetSearchText(InArgs._SearchText);
	EditableTextLayout->SetTextWrapping(InArgs._WrapTextAt, InArgs._AutoWrapText, InArgs._WrappingPolicy);
	EditableTextLayout->SetMargin(InArgs._Margin);
	EditableTextLayout->SetJustification(InArgs._Justification);
	EditableTextLayout->SetLineHeightPercentage(InArgs._LineHeightPercentage);
	EditableTextLayout->SetDebugSourceInfo(TAttribute<FString>::Create(TAttribute<FString>::FGetter::CreateLambda([this]{ return FReflectionMetaData::GetWidgetDebugInfo(this); })));
	EditableTextLayout->SetOverflowPolicy(InArgs._OverflowPolicy);

	// build context menu extender
	MenuExtender = MakeShareable(new FExtender);
	MenuExtender->AddMenuExtension("EditText", EExtensionHook::Before, TSharedPtr<FUICommandList>(), InArgs._ContextMenuExtender);

	AddMetadata(MakeShared<FTrackedMetaData>(this, FName(TEXT("EditableText"))));
}

void SMultiLineEditableText::GetCurrentTextLine(FString& OutTextLine) const
{
	EditableTextLayout->GetCurrentTextLine(OutTextLine);
}

void SMultiLineEditableText::GetTextLine(const int32 InLineIndex, FString& OutTextLine) const
{
	EditableTextLayout->GetTextLine(InLineIndex, OutTextLine);
}

void SMultiLineEditableText::SetText(const TAttribute< FText >& InText)
{
	EditableTextLayout->SetText(InText);
}

int32 SMultiLineEditableText::GetTextLineCount()
{
	return EditableTextLayout->GetTextLineCount();
}

FText SMultiLineEditableText::GetText() const
{
	return EditableTextLayout->GetText();
}

FText SMultiLineEditableText::GetPlainText() const
{
	return EditableTextLayout->GetPlainText();
}

void SMultiLineEditableText::SetHintText(const TAttribute< FText >& InHintText)
{
	EditableTextLayout->SetHintText(InHintText);
}

FText SMultiLineEditableText::GetHintText() const
{
	return EditableTextLayout->GetHintText();
}

void SMultiLineEditableText::SetSearchText(const TAttribute<FText>& InSearchText)
{
	EditableTextLayout->SetSearchText(InSearchText);
}

FText SMultiLineEditableText::GetSearchText() const
{
	return EditableTextLayout->GetSearchText();
}

int32 SMultiLineEditableText::GetSearchResultIndex() const
{
	return EditableTextLayout->GetSearchResultIndex();
}

int32 SMultiLineEditableText::GetNumSearchResults() const
{
	return EditableTextLayout->GetNumSearchResults();
}

void SMultiLineEditableText::SetTextStyle(const FTextBlockStyle* InTextStyle)
{
	if (InTextStyle)
	{
		EditableTextLayout->SetTextStyle(*InTextStyle);
	}
	else
	{
		FArguments Defaults;
		check(Defaults._TextStyle);
		EditableTextLayout->SetTextStyle(*Defaults._TextStyle);
	}
}

void SMultiLineEditableText::SetFont(const TAttribute< FSlateFontInfo >& InNewFont)
{
	FTextBlockStyle TextStyle = EditableTextLayout->GetTextStyle();
	TextStyle.SetFont(InNewFont.Get());
	EditableTextLayout->SetTextStyle(TextStyle);
}

FSlateFontInfo SMultiLineEditableText::GetFont() const
{
	FTextBlockStyle TextStyle = EditableTextLayout->GetTextStyle();
	return TextStyle.Font;
}

void SMultiLineEditableText::SetTextShapingMethod(const TOptional<ETextShapingMethod>& InTextShapingMethod)
{
	EditableTextLayout->SetTextShapingMethod(InTextShapingMethod);
}

void SMultiLineEditableText::SetTextFlowDirection(const TOptional<ETextFlowDirection>& InTextFlowDirection)
{
	EditableTextLayout->SetTextFlowDirection(InTextFlowDirection);
}

void SMultiLineEditableText::SetWrapTextAt(const TAttribute<float>& InWrapTextAt)
{
	EditableTextLayout->SetWrapTextAt(InWrapTextAt);
}

void SMultiLineEditableText::SetAutoWrapText(const TAttribute<bool>& InAutoWrapText)
{
	EditableTextLayout->SetAutoWrapText(InAutoWrapText);
}

void SMultiLineEditableText::SetWrappingPolicy(const TAttribute<ETextWrappingPolicy>& InWrappingPolicy)
{
	EditableTextLayout->SetWrappingPolicy(InWrappingPolicy);
}

void SMultiLineEditableText::SetLineHeightPercentage(const TAttribute<float>& InLineHeightPercentage)
{
	EditableTextLayout->SetLineHeightPercentage(InLineHeightPercentage);
}

void SMultiLineEditableText::SetApplyLineHeightToBottomLine(const TAttribute<bool>& InApplyLineHeightToBottomLine)
{
	EditableTextLayout->SetApplyLineHeightToBottomLine(InApplyLineHeightToBottomLine);
}

void SMultiLineEditableText::SetMargin(const TAttribute<FMargin>& InMargin)
{
	EditableTextLayout->SetMargin(InMargin);
}

void SMultiLineEditableText::SetJustification(const TAttribute<ETextJustify::Type>& InJustification)
{
	EditableTextLayout->SetJustification(InJustification);
}

void SMultiLineEditableText::SetOverflowPolicy(TOptional<ETextOverflowPolicy> InOverflowPolicy)
{
	EditableTextLayout->SetOverflowPolicy(InOverflowPolicy);
}

void SMultiLineEditableText::SetAllowContextMenu(const TAttribute< bool >& InAllowContextMenu)
{
	bAllowContextMenu = InAllowContextMenu;
}

void SMultiLineEditableText::SetVirtualKeyboardDismissAction(TAttribute< EVirtualKeyboardDismissAction > InVirtualKeyboardDismissAction)
{
	VirtualKeyboardDismissAction = InVirtualKeyboardDismissAction;
}

void SMultiLineEditableText::SetIsReadOnly(const TAttribute<bool>& InIsReadOnly)
{
	bIsReadOnly = InIsReadOnly;
}

void SMultiLineEditableText::SetSelectAllTextWhenFocused(const TAttribute<bool>& InSelectAllTextWhenFocused)
{
	bSelectAllTextWhenFocused = InSelectAllTextWhenFocused;
}

void SMultiLineEditableText::SetSelectWordOnMouseDoubleClick(const TAttribute<bool>& InSelectWordOnMouseDoubleClick)
{
	bSelectWordOnMouseDoubleClick = InSelectWordOnMouseDoubleClick;
}

void SMultiLineEditableText::SetClearTextSelectionOnFocusLoss(const TAttribute<bool>& InClearTextSelectionOnFocusLoss)
{
	bClearTextSelectionOnFocusLoss = InClearTextSelectionOnFocusLoss;
}

void SMultiLineEditableText::SetRevertTextOnEscape(const TAttribute<bool>& InRevertTextOnEscape)
{
	bRevertTextOnEscape = InRevertTextOnEscape;
}

void SMultiLineEditableText::SetClearKeyboardFocusOnCommit(const TAttribute<bool>& InClearKeyboardFocusOnCommit)
{
	bClearKeyboardFocusOnCommit = InClearKeyboardFocusOnCommit;
}

void SMultiLineEditableText::OnHScrollBarMoved(const float InScrollOffsetFraction)
{
	EditableTextLayout->SetHorizontalScrollFraction(InScrollOffsetFraction);
	OnHScrollBarUserScrolled.ExecuteIfBound(InScrollOffsetFraction);
}

void SMultiLineEditableText::OnVScrollBarMoved(const float InScrollOffsetFraction)
{
	EditableTextLayout->SetVerticalScrollFraction(InScrollOffsetFraction);
	OnVScrollBarUserScrolled.ExecuteIfBound(InScrollOffsetFraction);
}

bool SMultiLineEditableText::IsTextReadOnly() const
{
	return bIsReadOnly.Get(false);
}

bool SMultiLineEditableText::IsTextPassword() const
{
	return false;
}

bool SMultiLineEditableText::IsMultiLineTextEdit() const
{
	return bAllowMultiLine.Get(true);
}

bool SMultiLineEditableText::ShouldJumpCursorToEndWhenFocused() const
{
	return false;
}

bool SMultiLineEditableText::ShouldSelectAllTextWhenFocused() const
{
	return bSelectAllTextWhenFocused.Get(false);
}

bool SMultiLineEditableText::ShouldClearTextSelectionOnFocusLoss() const
{
	return bClearTextSelectionOnFocusLoss.Get(false);
}

bool SMultiLineEditableText::ShouldRevertTextOnEscape() const
{
	return bRevertTextOnEscape.Get(false);
}

bool SMultiLineEditableText::ShouldClearKeyboardFocusOnCommit() const
{
	return bClearKeyboardFocusOnCommit.Get(false);
}

bool SMultiLineEditableText::ShouldSelectAllTextOnCommit() const
{
	return false;
}

bool SMultiLineEditableText::ShouldSelectWordOnMouseDoubleClick() const
{
	return bSelectWordOnMouseDoubleClick.Get(true);
}

bool SMultiLineEditableText::CanInsertCarriageReturn() const
{
	return FSlateApplication::Get().GetModifierKeys().AreModifersDown(ModiferKeyForNewLine);
}

bool SMultiLineEditableText::CanTypeCharacter(const TCHAR InChar) const
{
	if (OnIsTypedCharValid.IsBound())
	{
		return OnIsTypedCharValid.Execute(InChar);
	}

	return InChar != TEXT('\t');
}

void SMultiLineEditableText::EnsureActiveTick()
{
	TSharedPtr<FActiveTimerHandle> ActiveTickTimerPin = ActiveTickTimer.Pin();
	if (ActiveTickTimerPin.IsValid())
	{
		return;
	}

	auto DoActiveTick = [this](double InCurrentTime, float InDeltaTime) -> EActiveTimerReturnType
	{
		// Continue if we still have focus, otherwise treat as a fire-and-forget Tick() request
		const bool bShouldAppearFocused = HasKeyboardFocus() || EditableTextLayout->HasActiveContextMenu();
		return (bShouldAppearFocused) ? EActiveTimerReturnType::Continue : EActiveTimerReturnType::Stop;
	};

	const float TickPeriod = EditableTextDefs::BlinksPerSecond * 0.5f;
	ActiveTickTimer = RegisterActiveTimer(TickPeriod, FWidgetActiveTimerDelegate::CreateLambda(DoActiveTick));
}

EKeyboardType SMultiLineEditableText::GetVirtualKeyboardType() const
{
	return Keyboard_Default;
}

FVirtualKeyboardOptions SMultiLineEditableText::GetVirtualKeyboardOptions() const
{
	return VirtualKeyboardOptions;
}

EVirtualKeyboardTrigger SMultiLineEditableText::GetVirtualKeyboardTrigger() const
{
	return VirtualKeyboardTrigger.Get();
}

EVirtualKeyboardDismissAction SMultiLineEditableText::GetVirtualKeyboardDismissAction() const
{
	return VirtualKeyboardDismissAction.Get();
}

TSharedRef<SWidget> SMultiLineEditableText::GetSlateWidget()
{
	return AsShared();
}

TSharedPtr<SWidget> SMultiLineEditableText::GetSlateWidgetPtr()
{
	if (DoesSharedInstanceExist())
	{
		return AsShared();
	}
	return nullptr;
}

TSharedPtr<SWidget> SMultiLineEditableText::BuildContextMenuContent() const
{
	if (!bAllowContextMenu.Get())
	{
		return nullptr;
	}

	if (OnContextMenuOpening.IsBound())
	{
		return OnContextMenuOpening.Execute();
	}

	return EditableTextLayout->BuildDefaultContextMenu(MenuExtender);
}

void SMultiLineEditableText::OnTextChanged(const FText& InText)
{
	OnTextChangedCallback.ExecuteIfBound(InText);
}

void SMultiLineEditableText::OnTextCommitted(const FText& InText, const ETextCommit::Type InTextAction)
{
	OnTextCommittedCallback.ExecuteIfBound(InText, InTextAction);
}

void SMultiLineEditableText::OnCursorMoved(const FTextLocation& InLocation)
{
	OnCursorMovedCallback.ExecuteIfBound(InLocation);
	Invalidate(EInvalidateWidget::Layout);
}

float SMultiLineEditableText::UpdateAndClampHorizontalScrollBar(const float InViewOffset, const float InViewFraction, const EVisibility InVisiblityOverride)
{
	if (HScrollBar.IsValid())
	{
		HScrollBar->SetState(InViewOffset, InViewFraction);
		HScrollBar->SetUserVisibility(InVisiblityOverride);
		if (!HScrollBar->IsNeeded())
		{
			// We cannot scroll, so ensure that there is no offset
			return 0.0f;
		}
	}

	return EditableTextLayout->GetScrollOffset().X;
}

float SMultiLineEditableText::UpdateAndClampVerticalScrollBar(const float InViewOffset, const float InViewFraction, const EVisibility InVisiblityOverride)
{
	if (VScrollBar.IsValid())
	{
		VScrollBar->SetState(InViewOffset, InViewFraction);
		VScrollBar->SetUserVisibility(InVisiblityOverride);
		if (!VScrollBar->IsNeeded())
		{
			// We cannot scroll, so ensure that there is no offset
			return 0.0f;
		}
	}

	return EditableTextLayout->GetScrollOffset().Y;
}

void SMultiLineEditableText::BeginEditTransaction()
{
	EditableTextLayout->BeginEditTransation();
}

void SMultiLineEditableText::EndEditTransaction()
{
	EditableTextLayout->EndEditTransaction();
}

FReply SMultiLineEditableText::OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent )
{
	EditableTextLayout->HandleFocusReceived(InFocusEvent);
	return FReply::Handled();
}

void SMultiLineEditableText::OnFocusLost( const FFocusEvent& InFocusEvent )
{
	bIsSoftwareCursor = false;
	EditableTextLayout->HandleFocusLost(InFocusEvent);
}

bool SMultiLineEditableText::AnyTextSelected() const
{
	return EditableTextLayout->AnyTextSelected();
}

void SMultiLineEditableText::SelectAllText()
{
	EditableTextLayout->SelectAllText();
}

void SMultiLineEditableText::SelectText(const FTextLocation& InSelectionStart, const FTextLocation& InCursorLocation)
{
	EditableTextLayout->SelectText(InSelectionStart, InCursorLocation);
}

void SMultiLineEditableText::ClearSelection()
{
	EditableTextLayout->ClearSelection();
}

FText SMultiLineEditableText::GetSelectedText() const
{
	return EditableTextLayout->GetSelectedText();
}

FTextSelection SMultiLineEditableText::GetSelection() const
{
	return EditableTextLayout->GetSelection();
}

void SMultiLineEditableText::DeleteSelectedText()
{
	EditableTextLayout->DeleteSelectedText();
}

void SMultiLineEditableText::InsertTextAtCursor(const FText& InText)
{
	EditableTextLayout->InsertTextAtCursor(InText.ToString());
}

void SMultiLineEditableText::InsertTextAtCursor(const FString& InString)
{
	EditableTextLayout->InsertTextAtCursor(InString);
}

void SMultiLineEditableText::InsertRunAtCursor(TSharedRef<IRun> InRun)
{
	EditableTextLayout->InsertRunAtCursor(InRun);
}

void SMultiLineEditableText::GoTo(const FTextLocation& NewLocation)
{
	EditableTextLayout->GoTo(NewLocation);
}

void SMultiLineEditableText::GoTo(const ETextLocation NewLocation)
{
	EditableTextLayout->GoTo(NewLocation);
}

void SMultiLineEditableText::ScrollTo(const FTextLocation& NewLocation)
{
	EditableTextLayout->ScrollTo(NewLocation);
}

void SMultiLineEditableText::ScrollTo(const ETextLocation NewLocation)
{
	EditableTextLayout->ScrollTo(NewLocation);
}

void SMultiLineEditableText::ApplyToSelection(const FRunInfo& InRunInfo, const FTextBlockStyle& InStyle)
{
	EditableTextLayout->ApplyToSelection(InRunInfo, InStyle);
}

void SMultiLineEditableText::BeginSearch(const FText& InSearchText, const ESearchCase::Type InSearchCase, const bool InReverse)
{
	EditableTextLayout->BeginSearch(InSearchText, InSearchCase, InReverse);
}

void SMultiLineEditableText::AdvanceSearch(const bool InReverse)
{
	EditableTextLayout->AdvanceSearch(InReverse);
}

TSharedPtr<const IRun> SMultiLineEditableText::GetRunUnderCursor() const
{
	return EditableTextLayout->GetRunUnderCursor();
}

TArray<TSharedRef<const IRun>> SMultiLineEditableText::GetSelectedRuns() const
{
	return EditableTextLayout->GetSelectedRuns();
}

FTextLocation SMultiLineEditableText::GetCursorLocation() const
{
	return EditableTextLayout->GetCursorLocation();
}

TCHAR SMultiLineEditableText::GetCharacterAt(const FTextLocation& Location) const
{
	return EditableTextLayout->GetCharacterAt(Location);
}

TSharedPtr<const SScrollBar> SMultiLineEditableText::GetHScrollBar() const
{
	return HScrollBar;
}

TSharedPtr<const SScrollBar> SMultiLineEditableText::GetVScrollBar() const
{
	return VScrollBar;
}

void SMultiLineEditableText::Refresh()
{
	EditableTextLayout->Refresh();
}

void SMultiLineEditableText::ForceScroll(int32 UserIndex, float ScrollAxisMagnitude)
{
	const FGeometry& CachedGeom = GetCachedGeometry();
	FVector2f ScrollPos = (CachedGeom.LocalToAbsolute(FVector2f::ZeroVector) + CachedGeom.LocalToAbsolute(CachedGeom.GetLocalSize())) * 0.5f;
	TSet<FKey> PressedKeys;

	OnMouseWheel(CachedGeom, FPointerEvent(UserIndex, 0, ScrollPos, ScrollPos, PressedKeys, EKeys::Invalid, ScrollAxisMagnitude, FModifierKeysState()));
}

void SMultiLineEditableText::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	EditableTextLayout->Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

int32 SMultiLineEditableText::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	const FTextBlockStyle& EditableTextStyle = EditableTextLayout->GetTextStyle();
	const FLinearColor ForegroundColor = EditableTextStyle.ColorAndOpacity.GetColor(InWidgetStyle);

	FWidgetStyle TextWidgetStyle = FWidgetStyle(InWidgetStyle)
		.SetForegroundColor(ForegroundColor);

	const bool bAutoWrap = EditableTextLayout->GetAutoWrapText();
	if (bAutoWrap && EditableTextLayout->GetComputedWrappingWidth() == 0)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_Slate_AutoWrapRecompute)
		const_cast<SMultiLineEditableText*>(this)->CacheDesiredSize(GetPrepassLayoutScaleMultiplier());
	}

	LayerId = EditableTextLayout->OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, TextWidgetStyle, ShouldBeEnabled(bParentEnabled));

	if (bIsSoftwareCursor)
	{
		const FSlateBrush* Brush = FCoreStyle::Get().GetBrush(TEXT("SoftwareCursor_Grab"));
		const FVector2f CursorSize = Brush->ImageSize / AllottedGeometry.Scale;

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(CursorSize, FSlateLayoutTransform(SoftwareCursorPosition - (CursorSize *.5f))),
			Brush
			);
	}

	return LayerId;
}

void SMultiLineEditableText::CacheDesiredSize(float LayoutScaleMultiplier)
{
	EditableTextLayout->CacheDesiredSize(LayoutScaleMultiplier);
	SWidget::CacheDesiredSize(LayoutScaleMultiplier);
}

FVector2D SMultiLineEditableText::ComputeDesiredSize( float LayoutScaleMultiplier ) const
{
	return EditableTextLayout->ComputeDesiredSize(LayoutScaleMultiplier);
}

FChildren* SMultiLineEditableText::GetChildren()
{
	return EditableTextLayout->GetChildren();
}

void SMultiLineEditableText::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	EditableTextLayout->OnArrangeChildren(AllottedGeometry, ArrangedChildren);
}

bool SMultiLineEditableText::SupportsKeyboardFocus() const
{
	return true;
}

FReply SMultiLineEditableText::OnKeyChar( const FGeometry& MyGeometry,const FCharacterEvent& InCharacterEvent )
{
	FReply Reply = FReply::Unhandled();

	// First call the user defined key handler, there might be overrides to normal functionality
	if (OnKeyCharHandler.IsBound())
	{
		Reply = OnKeyCharHandler.Execute(MyGeometry, InCharacterEvent);
	}

	if (!Reply.IsEventHandled())
	{
		Reply = EditableTextLayout->HandleKeyChar(InCharacterEvent);
	}

	return Reply;
}

FReply SMultiLineEditableText::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	FReply Reply = FReply::Unhandled();

	// First call the user defined key handler, there might be overrides to normal functionality
	if (OnKeyDownHandler.IsBound())
	{
		Reply = OnKeyDownHandler.Execute(MyGeometry, InKeyEvent);
	}

	if (!Reply.IsEventHandled())
	{
		Reply = EditableTextLayout->HandleKeyDown(InKeyEvent);
	}

	return Reply;
}

FReply SMultiLineEditableText::OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	return EditableTextLayout->HandleKeyUp(InKeyEvent);
}

FReply SMultiLineEditableText::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) 
{
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		AmountScrolledWhileRightMouseDown = 0.0f;
	}

	return EditableTextLayout->HandleMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SMultiLineEditableText::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) 
{
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		bool bWasRightClickScrolling = IsRightClickScrolling();
		AmountScrolledWhileRightMouseDown = 0.0f;

		if (bWasRightClickScrolling)
		{
			bIsSoftwareCursor = false;
			const FVector2f CursorPosition = MyGeometry.LocalToAbsolute(SoftwareCursorPosition);
			const FIntPoint OriginalMousePos(CursorPosition.X, CursorPosition.Y);
			return FReply::Handled().ReleaseMouseCapture().SetMousePos(OriginalMousePos);
		}
	}

	return EditableTextLayout->HandleMouseButtonUp(MyGeometry, MouseEvent);
}

FReply SMultiLineEditableText::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (MouseEvent.IsMouseButtonDown(EKeys::RightMouseButton))
	{
		const float ScrollByAmount = MouseEvent.GetCursorDelta().Y / MyGeometry.Scale;

		// If scrolling with the right mouse button, we need to remember how much we scrolled.
		// If we did not scroll at all, we will bring up the context menu when the mouse is released.
		AmountScrolledWhileRightMouseDown += FMath::Abs(ScrollByAmount);

		if (IsRightClickScrolling())
		{
			const FVector2f PreviousScrollOffset = EditableTextLayout->GetScrollOffset();

			FVector2f NewScrollOffset = PreviousScrollOffset;
			NewScrollOffset.Y -= ScrollByAmount;
			EditableTextLayout->SetScrollOffset(NewScrollOffset, MyGeometry);

			if (!bIsSoftwareCursor)
			{
				SoftwareCursorPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
				bIsSoftwareCursor = true;
			}

			if (PreviousScrollOffset.Y != NewScrollOffset.Y)
			{
				const float ScrollMax = EditableTextLayout->GetSize().Y - MyGeometry.GetLocalSize().Y;
				const float ScrollbarOffset = (ScrollMax != 0.0f) ? NewScrollOffset.Y / ScrollMax : 0.0f;
				OnVScrollBarUserScrolled.ExecuteIfBound(ScrollbarOffset);
				SoftwareCursorPosition.Y += (PreviousScrollOffset.Y - NewScrollOffset.Y);
			}

			return FReply::Handled().UseHighPrecisionMouseMovement(AsShared());
		}
	}

	return EditableTextLayout->HandleMouseMove(MyGeometry, MouseEvent);
}

FReply SMultiLineEditableText::OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (VScrollBar.IsValid() && VScrollBar->IsNeeded())
	{
		const float ScrollAmount = -MouseEvent.GetWheelDelta() * GetGlobalScrollAmount();

		const FVector2f PreviousScrollOffset = EditableTextLayout->GetScrollOffset();
		
		FVector2f NewScrollOffset = PreviousScrollOffset;
		NewScrollOffset.Y += ScrollAmount;
		EditableTextLayout->SetScrollOffset(NewScrollOffset, MyGeometry);

		if (PreviousScrollOffset.Y != NewScrollOffset.Y)
		{
			const float ScrollMax = EditableTextLayout->GetSize().Y - MyGeometry.GetLocalSize().Y;
			const float ScrollbarOffset = (ScrollMax != 0.0f) ? NewScrollOffset.Y / ScrollMax : 0.0f;
			OnVScrollBarUserScrolled.ExecuteIfBound(ScrollbarOffset);
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

FReply SMultiLineEditableText::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return EditableTextLayout->HandleMouseButtonDoubleClick(MyGeometry, MouseEvent);
}

FCursorReply SMultiLineEditableText::OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const
{
	if (IsRightClickScrolling() && CursorEvent.IsMouseButtonDown(EKeys::RightMouseButton))
	{
		return FCursorReply::Cursor(EMouseCursor::None);
	}
	else
	{
		return FCursorReply::Cursor(EMouseCursor::TextEditBeam);
	}
}

bool SMultiLineEditableText::IsInteractable() const
{
	return IsEnabled();
}

bool SMultiLineEditableText::ComputeVolatility() const
{
	return SWidget::ComputeVolatility()
		|| HasKeyboardFocus()
		|| EditableTextLayout->ComputeVolatility()
		|| bIsReadOnly.IsBound();
}

bool SMultiLineEditableText::IsRightClickScrolling() const
{
	return AmountScrolledWhileRightMouseDown >= FSlateApplication::Get().GetDragTriggerDistance() && VScrollBar.IsValid() && VScrollBar->IsNeeded();
}

#endif //WITH_FANCY_TEXT

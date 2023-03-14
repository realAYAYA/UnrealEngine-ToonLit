// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommonTextBlock.h"
#include "CommonUIPrivate.h"
#include "Containers/Ticker.h"
#include "CommonUIEditorSettings.h"
#include "CommonUIUtils.h"
#include "CommonWidgetPaletteCategories.h"
#include "Widgets/Text/STextBlock.h"
#include "Layout/LayoutUtils.h"
#include "Types/ReflectionMetadata.h"
#include "StyleSheet/CommonStyleSheet.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonTextBlock)

void STextScroller::Construct(const FArguments& InArgs)
{
	ScrollStyle = InArgs._ScrollStyle;
	ensure(ScrollStyle.IsValid());

	// We don't tick, we use an active ticker.
	SetCanTick(false);

	ChildSlot
	[
		InArgs._Content.Widget
	];
}

void STextScroller::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	const EVisibility ChildVisibility = ChildSlot.GetWidget()->GetVisibility();
	if (ArrangedChildren.Accepts(ChildVisibility))
	{
		const FVector2D ThisContentScale = GetContentScale();
		const FMargin SlotPadding(LayoutPaddingWithFlow(GSlateFlowDirection, ChildSlot.GetPadding()));
		const AlignmentArrangeResult XResult = AlignChild<Orient_Horizontal>(GSlateFlowDirection, AllottedGeometry.GetLocalSize().X, ChildSlot, SlotPadding, ThisContentScale.X, false);
		const AlignmentArrangeResult YResult = AlignChild<Orient_Vertical>(AllottedGeometry.GetLocalSize().Y, ChildSlot, SlotPadding, ThisContentScale.Y);
		const FVector2D MinimumSize = ChildSlot.GetWidget()->GetDesiredSize();

		ArrangedChildren.AddWidget(ChildVisibility, AllottedGeometry.MakeChild(
			ChildSlot.GetWidget(),
			FVector2D(XResult.Offset, YResult.Offset),
			FVector2D(XResult.Size, YResult.Size)
		));
	}
}

int32 STextScroller::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const_cast<STextScroller*>(this)->UpdateTickability(AllottedGeometry);

	if (ScrollOffset != 0)
	{
		const float ScrollDirection = GSlateFlowDirection == EFlowDirection::LeftToRight ? -1 : 1;
		const FGeometry ScrolledGeometry = AllottedGeometry.MakeChild(FSlateRenderTransform(FVector2D(ScrollOffset * ScrollDirection, 0)));
		return SCompoundWidget::OnPaint(Args, ScrolledGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}
	else
	{
		return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}
}

void STextScroller::ResetScrollState()
{
	FontAlpha = 1.f;
	TimeElapsed = 0.f;
	ScrollOffset = 0.f;
	// If suspended, make sure we stay suspended until receiving an explicit call to resume
	if (IsScrollingEnabled())
	{
		ActiveState = EActiveState::EStart;
	}
	SetRenderOpacity(1.0f);
}

void STextScroller::StartScrolling()
{
	ActiveState = EActiveState::EStart;
	ResetScrollState();
}

void STextScroller::SuspendScrolling()
{
	ActiveState = EActiveState::ESuspend;
	ResetScrollState();
}

void STextScroller::UpdateTickability(const FGeometry& AllottedGeometry)
{
	const float VisibleWidth = AllottedGeometry.GetLocalSize().X;
	const float DesiredWidth = VisibleWidth == 0 ? VisibleWidth : ChildSlot.GetWidget()->GetDesiredSize().X;

	if (DesiredWidth > (VisibleWidth + 2) && IsScrollingEnabled())
	{
		if (!ActiveTimerHandle.IsValid())
		{
			ActiveTimerHandle = RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &STextScroller::OnScrollTextTick));
			// If we need to scroll, then it's imperative that we arrange from the left, rather than fill, so that we flip to right aligning and scrolling
			// to the right (potentially).
			SetFlowDirectionPreference(EFlowDirectionPreference::Culture);
			ChildSlot.HAlign(HAlign_Left);
		}
	}
	else if (ActiveTimerHandle.IsValid())
	{
		UnRegisterActiveTimer(ActiveTimerHandle.ToSharedRef());
		ActiveTimerHandle.Reset();
		ResetScrollState();

		// If we no longer need to scroll, just inherit the flow direction.
		SetFlowDirectionPreference(EFlowDirectionPreference::Inherit);
		ChildSlot.HAlign(HAlign_Fill);
	}
}

EActiveTimerReturnType STextScroller::OnScrollTextTick(double CurrentTime, float DeltaTime)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UCommonTextBlock_OnTick);

	const UCommonTextScrollStyle* TextScrollStyle = ScrollStyle.Get();
	check(TextScrollStyle);

	const float ContentSize = ChildSlot.GetWidget()->GetDesiredSize().X;
	TimeElapsed += DeltaTime;

	switch (ActiveState)
	{
	case EActiveState::EFadeIn:
	{
		FontAlpha = FMath::Clamp<float>(TimeElapsed / TextScrollStyle->FadeInDelay, 0.f, 1.f);
		if (TimeElapsed >= TextScrollStyle->FadeInDelay)
		{
			FontAlpha = 1.f;
			TimeElapsed = 0.f;
			ScrollOffset = 0.f;
			ActiveState = EActiveState::EStart;
		}
		break;
	}
	case EActiveState::EStart:
	{
		TimeElapsed = 0.f;
		ScrollOffset = 0.f;
		ActiveState = EActiveState::EStartWait;
		break;
	}
	case EActiveState::EStartWait:
	{
		if (TimeElapsed >= TextScrollStyle->StartDelay)
		{
			TimeElapsed = 0.f;
			ScrollOffset = 0.f;
			ActiveState = EActiveState::EScrolling;
		}
		break;
	}
	case EActiveState::EScrolling:
	{
		ScrollOffset += TextScrollStyle->Speed * DeltaTime;
		if ((ScrollOffset + GetCachedGeometry().GetLocalSize().X) >= ContentSize)
		{
			TimeElapsed = 0.0f;
			ActiveState = EActiveState::EStop;
		}
		break;
	}
	case EActiveState::EStop:
	{
		TimeElapsed = 0.f;
		ActiveState = EActiveState::EStopWait;
		break;
	}
	case EActiveState::EStopWait:
	{
		if (TimeElapsed >= TextScrollStyle->EndDelay)
		{
			TimeElapsed = 0.f;
			ActiveState = EActiveState::EFadeOut;
		}
		break;
	}
	case EActiveState::EFadeOut:
	{
		FontAlpha = 1.0f - FMath::Clamp<float>(TimeElapsed / TextScrollStyle->FadeOutDelay, 0.f, 1.f);
		if (TimeElapsed >= TextScrollStyle->FadeOutDelay)
		{
			FontAlpha = 0.0f;
			TimeElapsed = 0.0f;
			ScrollOffset = 0.0f;
			ActiveState = EActiveState::EFadeIn;
		}
		break;
	}
	}

	SetRenderOpacity(FontAlpha);
	Invalidate(EInvalidateWidgetReason::Paint);

	return EActiveTimerReturnType::Continue;
}

// UCommonTextStyle
////////////////////////////////////////////////////////////////////////////////////

UCommonTextStyle::UCommonTextStyle()
	: Color(FLinearColor::Black)
	, LineHeightPercentage(1.0f)
{
}

void UCommonTextStyle::GetFont(FSlateFontInfo& OutFont) const
{
	OutFont = Font;
}

void UCommonTextStyle::GetColor(FLinearColor& OutColor) const
{
	OutColor = Color;
}

void UCommonTextStyle::GetMargin(FMargin& OutMargin) const
{
	OutMargin = Margin;
}

float UCommonTextStyle::GetLineHeightPercentage() const
{
	return LineHeightPercentage;
}

void UCommonTextStyle::GetShadowOffset(FVector2D& OutShadowOffset) const
{
	OutShadowOffset = ShadowOffset;
}

void UCommonTextStyle::GetShadowColor(FLinearColor& OutColor) const
{
	OutColor = ShadowColor;
}

void UCommonTextStyle::GetStrikeBrush(FSlateBrush& OutStrikeBrush) const
{
	OutStrikeBrush = StrikeBrush;
}

void UCommonTextStyle::ToTextBlockStyle(FTextBlockStyle& OutTextBlockStyle) const
{
	OutTextBlockStyle.SetFont(Font).SetColorAndOpacity(Color).SetStrikeBrush(StrikeBrush);
	if (bUsesDropShadow)
	{
		OutTextBlockStyle.SetShadowOffset(ShadowOffset).SetShadowColorAndOpacity(ShadowColor);
	}
}

void UCommonTextStyle::ApplyToTextBlock(const TSharedRef<STextBlock>& TextBlock) const
{
	TextBlock->SetFont(Font);
	TextBlock->SetStrikeBrush(&StrikeBrush);
	TextBlock->SetMargin(Margin);
	TextBlock->SetLineHeightPercentage(LineHeightPercentage);
	TextBlock->SetColorAndOpacity(Color);
	if (bUsesDropShadow)
	{
		TextBlock->SetShadowOffset(ShadowOffset);
		TextBlock->SetShadowColorAndOpacity(ShadowColor);
	}
	else
	{
		TextBlock->SetShadowOffset(FVector2D::ZeroVector);
		TextBlock->SetShadowColorAndOpacity(FLinearColor::Transparent);
	}
}

// UCommonTextBlock
////////////////////////////////////////////////////////////////////////////////////

UCommonTextBlock::UCommonTextBlock(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibilityInternal(ESlateVisibility::SelfHitTestInvisible);
	SetClipping(EWidgetClipping::Inherit);
}

void UCommonTextBlock::PostInitProperties()
{
	Super::PostInitProperties();

	UpdateFromStyle();
}

void UCommonTextBlock::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	// We will remove this once existing content is fixed up. Since previously the native CDO was actually the default style, this code will attempt to set the style on assets that were once using this default
	if (!Style && !bStyleNoLongerNeedsConversion && !IsRunningDedicatedServer())
	{
		UCommonUIEditorSettings& Settings = ICommonUIModule::GetEditorSettings();
		Settings.ConditionalPostLoad();
		TSubclassOf<UCommonTextStyle> DefaultStyle = Settings.GetTemplateTextStyle();
		UCommonTextStyle* DefaultStyleCDO = DefaultStyle ? Cast<UCommonTextStyle>(DefaultStyle->ClassDefaultObject) : nullptr;
		if (DefaultStyleCDO)
		{
			DefaultStyleCDO->ConditionalPostLoad();
			UCommonTextBlock* CDO = CastChecked<UCommonTextBlock>(GetClass()->GetDefaultObject());
			bool bAllDefaults = true;

			// Font
			{
				bool bFontHasChanged = false;
				FSlateFontInfo MyFont = GetFont();
				const FSlateFontInfo& CDOFont = CDO->GetFont();
				if (MyFont.FontObject == CDOFont.FontObject)
				{
					MyFont.FontObject = DefaultStyleCDO->Font.FontObject;
					bFontHasChanged = true;
				}
				if (MyFont.FontMaterial == CDOFont.FontMaterial)
				{
					MyFont.FontMaterial = DefaultStyleCDO->Font.FontMaterial;
					bFontHasChanged = true;
				}
				if (MyFont.OutlineSettings.OutlineSize == CDOFont.OutlineSettings.OutlineSize)
				{
					MyFont.OutlineSettings.OutlineSize = DefaultStyleCDO->Font.OutlineSettings.OutlineSize;
					bFontHasChanged = true;
				}
				if (MyFont.OutlineSettings.bSeparateFillAlpha == CDOFont.OutlineSettings.bSeparateFillAlpha)
				{
					MyFont.OutlineSettings.bSeparateFillAlpha = DefaultStyleCDO->Font.OutlineSettings.bSeparateFillAlpha;
					bFontHasChanged = true;
				}
				if (MyFont.OutlineSettings.bApplyOutlineToDropShadows == CDOFont.OutlineSettings.bApplyOutlineToDropShadows)
				{
					MyFont.OutlineSettings.bApplyOutlineToDropShadows = DefaultStyleCDO->Font.OutlineSettings.bApplyOutlineToDropShadows;
					bFontHasChanged = true;
				}
				if (MyFont.OutlineSettings.OutlineMaterial == CDOFont.OutlineSettings.OutlineMaterial)
				{
					MyFont.OutlineSettings.OutlineMaterial = DefaultStyleCDO->Font.OutlineSettings.OutlineMaterial;
					bFontHasChanged = true;
				}
				if (MyFont.OutlineSettings.OutlineColor.R == CDOFont.OutlineSettings.OutlineColor.R)
				{
					MyFont.OutlineSettings.OutlineColor.R = DefaultStyleCDO->Font.OutlineSettings.OutlineColor.R;
					bFontHasChanged = true;
				}
				if (MyFont.OutlineSettings.OutlineColor.G == CDOFont.OutlineSettings.OutlineColor.G)
				{
					MyFont.OutlineSettings.OutlineColor.G = DefaultStyleCDO->Font.OutlineSettings.OutlineColor.G;
					bFontHasChanged = true;
				}
				if (MyFont.OutlineSettings.OutlineColor.B == CDOFont.OutlineSettings.OutlineColor.B)
				{
					MyFont.OutlineSettings.OutlineColor.B = DefaultStyleCDO->Font.OutlineSettings.OutlineColor.B;
					bFontHasChanged = true;
				}
				if (MyFont.OutlineSettings.OutlineColor.A == CDOFont.OutlineSettings.OutlineColor.A)
				{
					MyFont.OutlineSettings.OutlineColor.A = DefaultStyleCDO->Font.OutlineSettings.OutlineColor.A;
					bFontHasChanged = true;
				}
				if (MyFont.TypefaceFontName == CDOFont.TypefaceFontName)
				{
					MyFont.TypefaceFontName = DefaultStyleCDO->Font.TypefaceFontName;
					bFontHasChanged = true;
				}
				if (MyFont.Size == CDOFont.Size)
				{
					MyFont.Size = DefaultStyleCDO->Font.Size;
					bFontHasChanged = true;
				}

				if (bFontHasChanged)
				{
					SetFont(MyFont);
				}
				bAllDefaults = bAllDefaults && !bFontHasChanged;
			}

			// Margin
			if (Margin.Left == CDO->Margin.Left) { Margin.Left = DefaultStyleCDO->Margin.Left; } else { bAllDefaults = false; }
			if (Margin.Top == CDO->Margin.Top) { Margin.Top = DefaultStyleCDO->Margin.Top; } else { bAllDefaults = false; }
			if (Margin.Right == CDO->Margin.Right) { Margin.Right = DefaultStyleCDO->Margin.Right; } else { bAllDefaults = false; }
			if (Margin.Bottom == CDO->Margin.Bottom) { Margin.Bottom = DefaultStyleCDO->Margin.Bottom; } else { bAllDefaults = false; }

			// LineHeightPercentage
			if (LineHeightPercentage == CDO->LineHeightPercentage) { LineHeightPercentage = DefaultStyleCDO->LineHeightPercentage; } else { bAllDefaults = false; }

			// ColorAndOpacity
			if (GetColorAndOpacity() == CDO->GetColorAndOpacity()) { SetColorAndOpacity(DefaultStyleCDO->Color); } else { bAllDefaults = false; }

			// ShadowOffset
			{
				bool bShadowOffsetHasChanged = false;
				FVector2D MyShadowOffset = GetShadowOffset();
				const FVector2D CDOShadowOffset = CDO->GetShadowOffset();
				if (MyShadowOffset.X == CDOShadowOffset.X)
				{
					DefaultStyleCDO->bUsesDropShadow ? MyShadowOffset.X = DefaultStyleCDO->ShadowOffset.X : 0.f;
					bShadowOffsetHasChanged = true;
				}
				if (MyShadowOffset.Y == CDOShadowOffset.Y)
				{
					DefaultStyleCDO->bUsesDropShadow ? MyShadowOffset.Y = DefaultStyleCDO->ShadowOffset.Y : 0.f;
					bShadowOffsetHasChanged = true;
				}

				if (bShadowOffsetHasChanged)
				{
					SetShadowOffset(MyShadowOffset);
				}
				bAllDefaults = bAllDefaults && !bShadowOffsetHasChanged;
			}

			// ShadowColorAndOpacity
			{
				bool bShadowColorAndOpacityHasChanged = false;
				FLinearColor MyShadowOffset = GetShadowColorAndOpacity();
				const FLinearColor CDOShadowOffset = CDO->GetShadowColorAndOpacity();
				if (MyShadowOffset.R == CDOShadowOffset.R)
				{
					DefaultStyleCDO->bUsesDropShadow ? MyShadowOffset.R = DefaultStyleCDO->ShadowColor.R : 0.f;
					bShadowColorAndOpacityHasChanged = true;
				}
				if (MyShadowOffset.G == CDOShadowOffset.G)
				{
					DefaultStyleCDO->bUsesDropShadow ? MyShadowOffset.G = DefaultStyleCDO->ShadowColor.G : 0.f;
					bShadowColorAndOpacityHasChanged = true;
				}
				if (MyShadowOffset.B == CDOShadowOffset.B)
				{
					DefaultStyleCDO->bUsesDropShadow ? MyShadowOffset.B = DefaultStyleCDO->ShadowColor.B : 0.f;
					bShadowColorAndOpacityHasChanged = true;
				}
				if (MyShadowOffset.A == CDOShadowOffset.A)
				{
					DefaultStyleCDO->bUsesDropShadow ? MyShadowOffset.A = DefaultStyleCDO->ShadowColor.A : 0.f;
					bShadowColorAndOpacityHasChanged = true;
				}

				if (bShadowColorAndOpacityHasChanged)
				{
					SetShadowColorAndOpacity(MyShadowOffset);
				}
				bAllDefaults = bAllDefaults && !bShadowColorAndOpacityHasChanged;
			}

			if (bAllDefaults)
			{
				Style = Settings.GetTemplateTextStyle();
			}
		}
	}

	bStyleNoLongerNeedsConversion = true;
#endif

	UpdateFromStyle();
}

void UCommonTextBlock::Serialize(FArchive& Ar)
{
#if WITH_EDITORONLY_DATA
	// @fixme If a text style is set, clear style related properties on PreSave when cooking to avoid 
	// non-deterministic cooking issues. The root cause of this is still unclear, but we're sometimes 
	// getting either the wrong style CDO (a parent BP instead of the child BP) or the child BP CDO properties
	// are being initialized with the parent's values.
	if (Style && Ar.IsSaving())
	{
		FSlateFontInfo TempFont;
		FSlateBrush TempStrikeBrush;
		FMargin TempMargin;
		float TempLineHeightPercentage = 1.f;
		FSlateColor TempColorAndOpacity;
		FVector2D TempShadowOffset = FVector2D::ZeroVector;
		FLinearColor TempShadowColorAndOpacity = FLinearColor::Transparent;

		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		Swap(Font, TempFont);
		Swap(StrikeBrush, TempStrikeBrush);
		Swap(Margin, TempMargin);
		Swap(LineHeightPercentage, TempLineHeightPercentage);
		Swap(ColorAndOpacity, TempColorAndOpacity);
		Swap(ShadowOffset, TempShadowOffset);
		Swap(ShadowColorAndOpacity, TempShadowColorAndOpacity);

		Super::Serialize(Ar);

		Swap(TempFont, Font);
		Swap(TempStrikeBrush, StrikeBrush);
		Swap(TempMargin, Margin);
		Swap(TempLineHeightPercentage, LineHeightPercentage);
		Swap(TempColorAndOpacity, ColorAndOpacity);
		Swap(TempShadowOffset, ShadowOffset);
		Swap(TempShadowColorAndOpacity, ShadowColorAndOpacity);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
	else
#endif //if WITH_EDITORONLY_DATA
	{
		Super::Serialize(Ar);

		if (Ar.IsLoading() && bDisplayAllCaps_DEPRECATED)
		{
			SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
			bDisplayAllCaps_DEPRECATED = false;
		}
	}
}

void UCommonTextBlock::SetWrapTextWidth(int32 InWrapTextAt)
{
	WrapTextAt = InWrapTextAt;
	SynchronizeProperties();
}

void UCommonTextBlock::SetTextCase(bool bUseAllCaps)
{
	SetTextTransformPolicy(bUseAllCaps ? ETextTransformPolicy::ToUpper : ETextTransformPolicy::None);
	SynchronizeProperties();
}

void UCommonTextBlock::SetLineHeightPercentage(float InLineHeightPercentage)
{
	LineHeightPercentage = InLineHeightPercentage;
}

void UCommonTextBlock::SetStyle(TSubclassOf<UCommonTextStyle> InStyle)
{
	Style = InStyle;
	SynchronizeProperties();
}

const FMargin& UCommonTextBlock::GetMargin()
{
	return Margin;
}

void UCommonTextBlock::SetMargin(const FMargin& InMargin)
{
	Margin = InMargin;
}

void UCommonTextBlock::ResetScrollState()
{
	if (TextScroller.IsValid())
	{
		TextScroller->ResetScrollState();
	}
}

void UCommonTextBlock::SetScrollingEnabled(bool bInIsScrollingEnabled)
{
	bIsScrollingEnabled = bInIsScrollingEnabled;
	SynchronizeProperties();
}

void UCommonTextBlock::SynchronizeProperties()
{
	UpdateFromStyle();

	Super::SynchronizeProperties();

	if (bAutoCollapseWithEmptyText)
	{
		SetVisibility(GetText().IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}

	if (CommonUIUtils::ShouldDisplayMobileUISizes())
	{
		ApplyFontSizeMultiplier();
	}

	if (TextScroller.IsValid())
	{
		if (bIsScrollingEnabled)
		{
			TextScroller->StartScrolling();

			// Hide ellipsis when scrolling
			if (MyTextBlock.IsValid())
			{
				MyTextBlock->SetOverflowPolicy(ETextOverflowPolicy::Clip);
			}
		}
		else
		{
			TextScroller->SuspendScrolling();
		}
	}
}

void UCommonTextBlock::SetText(FText InText)
{
	Super::SetText(InText);
	
	if (bAutoCollapseWithEmptyText)
	{
		SetVisibility(InText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}

	if (CommonUIUtils::ShouldDisplayMobileUISizes())
	{
		ApplyFontSizeMultiplier();
	}
}

void UCommonTextBlock::UpdateFromStyle()
{
	if (const UCommonTextStyle* TextStyle = GetStyleCDO())
	{
		SetFont(TextStyle->Font);
		SetStrikeBrush(TextStyle->StrikeBrush);
		Margin = TextStyle->Margin;
		LineHeightPercentage = TextStyle->LineHeightPercentage;
		SetColorAndOpacity(TextStyle->Color);

		if (TextStyle->bUsesDropShadow)
		{
			SetShadowOffset(TextStyle->ShadowOffset);
			SetShadowColorAndOpacity(TextStyle->ShadowColor);
		}
		else
		{
			SetShadowOffset(FVector2D::ZeroVector);
			SetShadowColorAndOpacity(FLinearColor::Transparent);
		}
	}

	if (UCommonStyleSheet* StyleSheetPtr = StyleSheet.Get())
	{
		StyleSheetPtr->Apply(this);
	}
}

void UCommonTextBlock::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	TextScroller.Reset();
}

#if WITH_EDITOR
void UCommonTextBlock::OnCreationFromPalette()
{
	bStyleNoLongerNeedsConversion = true;
	if (!Style)
	{
		Style = ICommonUIModule::GetEditorSettings().GetTemplateTextStyle();
		UpdateFromStyle();
	}
	Super::OnCreationFromPalette();
}

const FText UCommonTextBlock::GetPaletteCategory()
{
	return CommonWidgetPaletteCategories::Default;
}

bool UCommonTextBlock::CanEditChange(const FProperty* InProperty) const
{
	if (Super::CanEditChange(InProperty))
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		if (const UCommonTextStyle* TextStyle = GetStyleCDO())
		{
			static TArray<FName> InvalidPropertiesWithStyle =
			{
				GET_MEMBER_NAME_CHECKED(UCommonTextBlock, Font),
				GET_MEMBER_NAME_CHECKED(UCommonTextBlock, StrikeBrush),
				GET_MEMBER_NAME_CHECKED(UCommonTextBlock, Margin),
				GET_MEMBER_NAME_CHECKED(UCommonTextBlock, LineHeightPercentage),
				GET_MEMBER_NAME_CHECKED(UCommonTextBlock, ColorAndOpacity),
				GET_MEMBER_NAME_CHECKED(UCommonTextBlock, ShadowOffset),
				GET_MEMBER_NAME_CHECKED(UCommonTextBlock, ShadowColorAndOpacity)
			};

			return !InvalidPropertiesWithStyle.Contains(InProperty->GetFName());
		}
		if (bAutoCollapseWithEmptyText && InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UCommonTextBlock, Visibility))
		{
			return false;
		}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		return true;
	}

	return false;
}

#endif // WITH_EDITOR

TSharedRef<SWidget> UCommonTextBlock::RebuildWidget()
{
	const UCommonTextScrollStyle* TextScrollStyle = UCommonTextBlock::GetScrollStyleCDO();
	if (!TextScrollStyle)
	{
		return Super::RebuildWidget();
	}

	// If the clipping mode is the default, but we're using a scrolling style,
	// we need to switch over to a clip to bounds style.
	if (GetClipping() == EWidgetClipping::Inherit)
	{
		SetClipping(EWidgetClipping::OnDemand);
	}

	// clang-format off
	TextScroller = 
		SNew(STextScroller)
		.ScrollStyle(GetScrollStyleCDO())
		[ 
			Super::RebuildWidget() 
		];
	// clang-format on

	// Set the inner text block to self hit test invisible
	MyTextBlock->SetVisibility(EVisibility::SelfHitTestInvisible);

#if WIDGET_INCLUDE_RELFECTION_METADATA
	MyTextBlock->AddMetadata<FReflectionMetaData>(MakeShared<FReflectionMetaData>(GetFName(), GetClass(), this, GetSourceAssetOrClass()));
#endif

	return TextScroller.ToSharedRef();
}

const UCommonTextStyle* UCommonTextBlock::GetStyleCDO() const
{
	return Style ? Cast<UCommonTextStyle>(Style->ClassDefaultObject) : nullptr;
}

const UCommonTextScrollStyle* UCommonTextBlock::GetScrollStyleCDO() const
{
	return ScrollStyle ? Cast<UCommonTextScrollStyle>(ScrollStyle->ClassDefaultObject) : nullptr;
}

void UCommonTextBlock::ApplyFontSizeMultiplier() const
{
	if (MyTextBlock.IsValid())
	{
		FSlateFontInfo FontInfo = GetFont();
		FontInfo.Size *= MobileFontSizeMultiplier;
		MyTextBlock->SetFont(FontInfo);
	}
}


// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SWidgetReflectorTreeWidgetItem.h"
#include "SlateOptMacros.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Input/SHyperlink.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Widgets/Input/SCheckBox.h"

#define LOCTEXT_NAMESPACE "SWidgetReflector"

/* SMultiColumnTableRow overrides
 *****************************************************************************/

FName SReflectorTreeWidgetItem::NAME_WidgetName(TEXT("WidgetName"));
FName SReflectorTreeWidgetItem::NAME_WidgetInfo(TEXT("WidgetInfo"));
FName SReflectorTreeWidgetItem::NAME_Visibility(TEXT("Visibility"));
FName SReflectorTreeWidgetItem::NAME_Focusable(TEXT("Focusable"));
FName SReflectorTreeWidgetItem::NAME_Enabled(TEXT("Enabled"));
FName SReflectorTreeWidgetItem::NAME_Volatile(TEXT("Volatile"));
FName SReflectorTreeWidgetItem::NAME_HasActiveTimer(TEXT("HasActiveTimer"));
FName SReflectorTreeWidgetItem::NAME_Clipping(TEXT("Clipping"));
FName SReflectorTreeWidgetItem::NAME_LayerId(TEXT("LayerId"));
FName SReflectorTreeWidgetItem::NAME_ForegroundColor(TEXT("ForegroundColor"));
FName SReflectorTreeWidgetItem::NAME_ActualSize(TEXT("ActualSize"));
FName SReflectorTreeWidgetItem::NAME_Address(TEXT("Address"));

void SReflectorTreeWidgetItem::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	this->WidgetInfo = InArgs._WidgetInfoToVisualize;
	this->OnAccessSourceCode = InArgs._SourceCodeAccessor;
	this->OnAccessAsset = InArgs._AssetAccessor;
	this->SetPadding(0.f);

	check(WidgetInfo.IsValid());

	SMultiColumnTableRow< TSharedRef<FWidgetReflectorNodeBase> >::Construct(SMultiColumnTableRow< TSharedRef<FWidgetReflectorNodeBase> >::FArguments().Padding(0.f), InOwnerTableView);
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedRef<SWidget> SReflectorTreeWidgetItem::GenerateWidgetForColumn(const FName& ColumnName)
{
	SReflectorTreeWidgetItem* Self = this;
	auto BuildCheckBox = [Self](bool bIsChecked)
		{
			return SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Padding(FMargin(2.0f, 0.0f))
				[
					SNew(SCheckBox)
					.Style(FCoreStyle::Get(), TEXT("WidgetReflector.FocusableCheck"))
					.IsChecked(bIsChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
				];
		};

	if (ColumnName == NAME_WidgetName )
	{
		TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);

		HorizontalBox->AddSlot()
		.AutoWidth()
		[
			SNew(SExpanderArrow, SharedThis(this))
			.IndentAmount(16)
			.ShouldDrawWires(true)
		];

		if (WidgetInfo->GetWidgetIsInvalidationRoot())
		{
			HorizontalBox->AddSlot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("InvalidationRoot_Short", "[IR]"))
			];
		}

		HorizontalBox->AddSlot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(WidgetInfo->GetWidgetTypeAndShortName())
			.ColorAndOpacity(this, &SReflectorTreeWidgetItem::GetTint)
		];

		return HorizontalBox;
	}
	else if (ColumnName == NAME_WidgetInfo )
	{
		return SNew(SBox)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(FMargin(2.0f, 0.0f))
			[
				SNew(SHyperlink)
				.Text(WidgetInfo->GetWidgetReadableLocation())
				.OnNavigate(this, &SReflectorTreeWidgetItem::HandleHyperlinkNavigate)
			];
	}
	else if (ColumnName == NAME_Visibility )
	{
		return SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(FMargin(2.0f, 0.0f))
			[
				SNew(STextBlock)
				.Text(WidgetInfo->GetWidgetVisibilityText())
					.Justification(ETextJustify::Center)
			];
	}
	else if (ColumnName == NAME_Focusable)
	{
		return BuildCheckBox(WidgetInfo->GetWidgetFocusable());
	}
	else if (ColumnName == NAME_Enabled)
	{
		return BuildCheckBox(WidgetInfo->GetWidgetEnabled());
	}
	else if (ColumnName == NAME_Volatile)
	{
		return BuildCheckBox(WidgetInfo->GetWidgetIsVolatile());
	}
	else if (ColumnName == NAME_HasActiveTimer)
	{
		return BuildCheckBox(WidgetInfo->GetWidgetHasActiveTimers());
	}
	else if ( ColumnName == NAME_Clipping )
	{
		return SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(FMargin(2.0f, 0.0f))
			[
				SNew(STextBlock)
				.Text(WidgetInfo->GetWidgetClippingText())
			];
	}
	else if (ColumnName == NAME_LayerId)
	{
		return SNew(SBox)
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(FMargin(2.0f, 0.0f))
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("WidgetLayerIds", "[{0}, {1}]"), FText::AsNumber(WidgetInfo->GetWidgetLayerId()), FText::AsNumber(WidgetInfo->GetWidgetLayerIdOut())))
			];
	}
	else if (ColumnName == NAME_ForegroundColor )
	{
		const FSlateColor Foreground = WidgetInfo->GetWidgetForegroundColor();

		return SNew(SBorder)
			// Show unset color as an empty space.
			.Visibility(Foreground.IsColorSpecified() ? EVisibility::Visible : EVisibility::Hidden)
			// Show a checkerboard background so we can see alpha values well
			.BorderImage(FCoreStyle::Get().GetBrush("Checkerboard"))
			.VAlign(VAlign_Center)
			.Padding(FMargin(2.0f, 0.0f))
			[
				// Show a color block
				SNew(SColorBlock)
					.Color(Foreground.GetSpecifiedColor())
					.Size(FVector2D(16.0f, 16.0f))
			];
	}
	else if (ColumnName == NAME_ActualSize)
	{
		return SNew(STextBlock)
			.Text(FText::FromString(WidgetInfo->GetLocalSize().ToString()));
	}
	else if (ColumnName == NAME_Address )
	{
		const FString WidgetAddress = FWidgetReflectorNodeUtils::WidgetAddressToString(WidgetInfo->GetWidgetAddress());
		const FText Address = FText::FromString(WidgetAddress);
		const FString ConditionalBreakPoint = FString::Printf(TEXT("this == (SWidget*)%s"), *WidgetAddress);

		return SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(2.0f, 0.0f))
			[
				SNew(SHyperlink)
				.ToolTipText(LOCTEXT("ClickToCopyBreakpoint", "Click to copy conditional breakpoint for this instance."))
				.Text(LOCTEXT("CBP", "[CBP]"))
				.OnNavigate_Lambda([ConditionalBreakPoint](){ FPlatformApplicationMisc::ClipboardCopy(*ConditionalBreakPoint); })
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(0.f, 0.f, 2.f, 0.f))
			[
				SNew(SHyperlink)
				.ToolTipText(LOCTEXT("ClickToCopy", "Click to copy address."))
				.Text(Address)
				.OnNavigate_Lambda([Address]() { FPlatformApplicationMisc::ClipboardCopy(*Address.ToString()); })
			];
	}
	else
	{
		return SNullWidget::NullWidget;
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SReflectorTreeWidgetItem::HandleHyperlinkNavigate()
{
	FAssetData CachedAssetData = WidgetInfo->GetWidgetAssetData();
	if (CachedAssetData.IsValid())
	{
		if (OnAccessAsset.IsBound())
		{
			CachedAssetData.GetPackage();
			OnAccessAsset.Execute(CachedAssetData.GetAsset());
			return;
		}
	}

	if (OnAccessSourceCode.IsBound())
	{
		OnAccessSourceCode.Execute(WidgetInfo->GetWidgetFile(), WidgetInfo->GetWidgetLineNumber(), 0);
	}
}

#undef LOCTEXT_NAMESPACE
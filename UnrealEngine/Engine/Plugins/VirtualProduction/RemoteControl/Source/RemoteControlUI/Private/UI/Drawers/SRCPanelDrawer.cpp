// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Drawers/SRCPanelDrawer.h"

#include "Framework/Text/PlainTextLayoutMarshaller.h"

#include "Styling/AppStyle.h"

#include "Widgets/Colors/SComplexGradient.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Text/SlateTextBlockLayout.h"

const FVector2D FDockingPanelConstants::MaxMinorPanelSize(144.f, 25.f);
const FVector2D FDockingPanelConstants::MaxMajorPanelSize(210.f, 50.f);

const FVector2D FDockingPanelConstants::GetMaxPanelSizeFor(ETabRole TabRole)
{
	return (TabRole == ETabRole::MajorTab)
		? MaxMajorPanelSize
		: MaxMinorPanelSize;
}

#define LOCTEXT_NAMESPACE "RCPanelDrawer"

/**
 * Vertical text block for use in the tab drawer button.
 * Text is aligned to the top of the widget if it fits without clipping;
 * otherwise it is ellipsized and fills the widget height.
 */
class SRCPanelDrawerTextBlock : public SLeafWidget
{
public:

	enum class ERotation
	{
		Clockwise,
		CounterClockwise,
	};

	SLATE_BEGIN_ARGS(SRCPanelDrawerTextBlock)
		: _Text()
		, _TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
		, _Rotation(ERotation::Clockwise)
		, _OverflowPolicy()
	{}
	
		SLATE_ATTRIBUTE(FText, Text)

		SLATE_STYLE_ARGUMENT(FTextBlockStyle, TextStyle)

		SLATE_ATTRIBUTE(ERotation, Rotation)

		SLATE_ARGUMENT(TOptional<ETextOverflowPolicy>, OverflowPolicy)

	SLATE_END_ARGS()

	virtual ~SRCPanelDrawerTextBlock() {}

	void Construct(const FArguments& InArgs)
	{
		Text = InArgs._Text;
		TextStyle = *InArgs._TextStyle;
		Rotation = InArgs._Rotation;
		TextLayoutCache = MakeUnique<FSlateTextBlockLayout>(
			this, FTextBlockStyle::GetDefault(), TOptional<ETextShapingMethod>(), TOptional<ETextFlowDirection>(),
			FCreateSlateTextLayout(), FPlainTextLayoutMarshaller::Create(), nullptr);
		TextLayoutCache->SetTextOverflowPolicy(InArgs._OverflowPolicy.IsSet() ? InArgs._OverflowPolicy : TextStyle.OverflowPolicy);
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		// We're going to figure out the bounds of the corresponding horizontal text, and then rotate it into a vertical orientation.
		const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
		const FVector2D DesiredHorizontalTextSize = TextLayoutCache->GetDesiredSize();
		const FVector2D ActualHorizontalTextSize(FMath::Min(DesiredHorizontalTextSize.X, LocalSize.Y), FMath::Min(DesiredHorizontalTextSize.Y, LocalSize.X));

		// Now determine the center of the vertical text by rotating the dimensions of the horizontal text.
		// The center should align it to the top of the widget.
		const FVector2D VerticalTextSize(ActualHorizontalTextSize.Y, ActualHorizontalTextSize.X);
		const FVector2D VerticalTextCenter = VerticalTextSize / 2.f;

		// Now determine where the horizontal text should be positioned so that it is centered on the vertical text:
		//      +-+
		//      |v|
		//      |e|
		// [ horizontal ]
		//      |r|
		//      |t|
		//      +-+
		const FVector2D HorizontalTextPosition = VerticalTextCenter - ActualHorizontalTextSize / 2.f;

		// Define the text's geometry using the horizontal bounds, then rotate it 90/-90 degrees into place to become vertical.
		const FSlateRenderTransform RotationTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(Rotation.Get() == ERotation::Clockwise ? 90 : -90))));
		const FGeometry TextGeometry = AllottedGeometry.MakeChild(ActualHorizontalTextSize, FSlateLayoutTransform(HorizontalTextPosition), RotationTransform, FVector2D(0.5f, 0.5f));

		return TextLayoutCache->OnPaint(Args, TextGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, ShouldBeEnabled(bParentEnabled));
	}

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
	{
		// The text's desired size reflects the horizontal/untransformed text.
		// Switch the dimensions for vertical text.
		const FVector2D DesiredHorizontalTextSize = TextLayoutCache->ComputeDesiredSize(
			FSlateTextBlockLayout::FWidgetDesiredSizeArgs(
				Text.Get(),
				FText(),
				0.f,
				false,
				ETextWrappingPolicy::DefaultWrapping,
				ETextTransformPolicy::None,
				FMargin(),
				1.f,
				true,
				ETextJustify::Left),
			LayoutScaleMultiplier, TextStyle);
		return FVector2D(DesiredHorizontalTextSize.Y, DesiredHorizontalTextSize.X);
	}

	void SetText(TAttribute<FText> InText)
	{
		Text = InText;
	}

	void SetRotation(TAttribute<ERotation> InRotation)
	{
		Rotation = InRotation;
	}

private:

	TAttribute<FText> Text;
	FTextBlockStyle TextStyle;
	TAttribute<ERotation> Rotation;
	TUniquePtr<FSlateTextBlockLayout> TextLayoutCache;
};

class SRCPanelDrawerButton : public SCompoundWidget
{

	SLATE_BEGIN_ARGS(SRCPanelDrawerButton)
	{}
		SLATE_EVENT(FOnRCPanelDrawerButtonPressed, OnDrawerButtonPressed)
	
	SLATE_END_ARGS()

public:

	void Construct(const FArguments& InArgs, TSharedRef<FRCPanelDrawerArgs> ForPanel)
	{
		const FVector2D Size = FDockingPanelConstants::GetMaxPanelSizeFor(ETabRole::PanelTab);

		const FSlateRenderTransform Rotate90(FQuat2D(FMath::DegreesToRadians(90.f)));

		DockTabStyle = &FAppStyle::Get().GetWidgetStyle<FDockTabStyle>("Docking.Tab");

		OnDrawerButtonPressed = InArgs._OnDrawerButtonPressed;

		Panel = ForPanel;

		static FLinearColor ActiveBorderColor = FAppStyle::Get().GetSlateColor("Docking.Tab.ActiveTabIndicatorColor").GetSpecifiedColor();
		static FLinearColor ActiveBorderColorTransparent = FLinearColor(ActiveBorderColor.R, ActiveBorderColor.G, ActiveBorderColor.B, 0.f);
		static TArray<FLinearColor> GradientStops{ ActiveBorderColorTransparent, ActiveBorderColor, ActiveBorderColorTransparent };

		ChildSlot
			.Padding(0.f)
			[
				SNew(SBox)
				.WidthOverride(Size.Y) // Swap desired dimensions for a vertical tab
				.HeightOverride(Size.X)
				.Clipping(EWidgetClipping::ClipToBounds)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SAssignNew(MainButton, SButton)
						.ToolTipText(ForPanel->ToolTip)
						.ContentPadding(FMargin(0.f, DockTabStyle->TabPadding.Top, 0.f, DockTabStyle->TabPadding.Bottom))
						.OnPressed_Lambda([this]() { OnDrawerButtonPressed.ExecuteIfBound(Panel.ToSharedRef(), false); })
						.ForegroundColor(FSlateColor::UseForeground())
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							.Padding(0.f, 5.f, 0.f, 5.f)
							[
								SNew(SImage)
								.ColorAndOpacity(FSlateColor::UseForeground())
								.Image(ForPanel->Icon.GetIcon())
								.DesiredSizeOverride(FVector2D(16, 16))
								.RenderTransform(ForPanel->bRotateIconBy90 ? Rotate90 : FSlateRenderTransform())
								.RenderTransformPivot(FVector2D(.5f, .5f))
							]
							+ SVerticalBox::Slot()
							.Padding(0.f, 5.f, 0.f, 5.f)
							.FillHeight(1.f)
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SAssignNew(Label, SRCPanelDrawerTextBlock)
								.TextStyle(&DockTabStyle->TabTextStyle)
								.Text(ForPanel->Label)
								.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
								.Clipping(EWidgetClipping::ClipToBounds)
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.HAlign(HAlign_Center)
							.Padding(0.f, 5.f, 0.f, 4.f)
							[
								SNew(SBorder)
								.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
								.Visibility(this, &SRCPanelDrawerButton::GetPanelStateVisibility)
								.ToolTipText(this, &SRCPanelDrawerButton::GetPanelStateToolTipText)
								.Padding(2.f)
								.HAlign(HAlign_Center)
								[
									SNew(SImage)
									.ColorAndOpacity(FSlateColor::UseForeground())
									.Image(this, &SRCPanelDrawerButton::GetPanelStateImage)
								]
							]
						]
					]
					+ SOverlay::Slot()
					[
						SAssignNew(OpenBorder, SBorder)
						.Visibility(EVisibility::HitTestInvisible)
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Left)
					[
						SAssignNew(ActiveIndicator, SComplexGradient)
						.DesiredSizeOverride(FVector2D(1.f, 1.f))
						.GradientColors(GradientStops)
						.Orientation(EOrientation::Orient_Horizontal)
						.Visibility(this, &SRCPanelDrawerButton::GetActivePanelIndicatorVisibility)
					]
				]
			];

		UpdateAppearance(nullptr);
	}

	void UpdateAppearance(const TSharedPtr<FRCPanelDrawerArgs> OpenedDrawer)
	{
		bool bShouldAppearOpened = OpenedDrawer.IsValid();

		SRCPanelDrawerTextBlock::ERotation Rotation;

		Rotation = bShouldAppearOpened ? SRCPanelDrawerTextBlock::ERotation::CounterClockwise : SRCPanelDrawerTextBlock::ERotation::Clockwise;

		check(Label);
		Label->SetRotation(Rotation);

		if (OpenedDrawer == Panel)
		{
			// this button is the one with the tab that is actually opened so show the tab border
			OpenBorder->SetVisibility(EVisibility::HitTestInvisible);
			MainButton->SetButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Docking.SidebarButton.Opened"));

			OpenBorder->SetBorderImage(FAppStyle::Get().GetBrush("Docking.Sidebar.Border_SquareRight"));
		}
		else
		{
			OpenBorder->SetVisibility(EVisibility::Collapsed);
			MainButton->SetButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Docking.SidebarButton.Closed"));
		}
	}

	virtual FSlateColor GetForegroundColor() const
	{
		if (ActiveIndicator->GetVisibility() != EVisibility::Collapsed)
		{
			return DockTabStyle->ActiveForegroundColor;
		}
		else if (IsHovered())
		{
			return DockTabStyle->HoveredForegroundColor;
		}

		return FSlateColor::UseStyle();
	}

private:

	EVisibility GetActivePanelIndicatorVisibility() const
	{
		return Panel->IsActive() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
	}
	
	EVisibility GetPanelStateVisibility() const
	{
		return (IsHovered() || Panel->IsActive()) ? EVisibility::Visible : EVisibility::Hidden;
	}

	FText GetPanelStateToolTipText() const
	{
		if (Panel->IsActive())
		{
			return LOCTEXT("CollapsePanelToolTip", "Collapse panel to the drawer.");
		}
		else
		{
			return LOCTEXT("DrawPanelToolTip", "Draw panel from the drawer.");
		}
	}

	const FSlateBrush* GetPanelStateImage() const
	{
		if (Panel->IsActive())
		{
			return FAppStyle::Get().GetBrush("Icons.ChevronLeft");
		}
		else
		{
			return FAppStyle::Get().GetBrush("Icons.ChevronRight");
		}
	}

	TSharedPtr<FRCPanelDrawerArgs> Panel;
	TSharedPtr<SRCPanelDrawerTextBlock> Label;
	TSharedPtr<SWidget> ActiveIndicator;
	TSharedPtr<SBorder> OpenBorder;
	TSharedPtr<SButton> MainButton;
	FOnRCPanelDrawerButtonPressed OnDrawerButtonPressed;
	const FDockTabStyle* DockTabStyle;
};

void SRCPanelDrawer::Construct(const FArguments& InArgs)
{
	ChildSlot
		.Padding(0.f)
		[
			SNew(SBorder)
			.Padding(0.f)
			.BorderImage(FAppStyle::Get().GetBrush("Docking.Sidebar.Background"))
			[
				SAssignNew(PanelBox, SVerticalBox)
			]
		];
}

bool SRCPanelDrawer::IsRegistered(TSharedRef<FRCPanelDrawerArgs> InPanel) const
{
	return Panels.ContainsByPredicate(
		[InPanel](TPair<TSharedRef<FRCPanelDrawerArgs>, TSharedRef<SRCPanelDrawerButton>> PanelPair)
		{
			return PanelPair.Key == InPanel;
		});
}

void SRCPanelDrawer::RegisterPanel(TSharedRef<FRCPanelDrawerArgs> InPanel)
{
	// Only register panels with visible drawer.
	if (!IsRegistered(InPanel) && InPanel->DrawerVisibility == EVisibility::Visible)
	{
		SetVisibility(EVisibility::SelfHitTestInvisible);

		TSharedRef<SRCPanelDrawerButton> PanelButton = SNew(SRCPanelDrawerButton, InPanel)
			.OnDrawerButtonPressed(this, &SRCPanelDrawer::TogglePanel);

		PanelBox->AddSlot()
			// Make the tabs evenly fill the sidebar until they reach the max size
			.FillHeight(1.f)
			.MaxHeight(FDockingPanelConstants::GetMaxPanelSizeFor(ETabRole::PanelTab).X)
			.HAlign(HAlign_Left)
			.Padding(0.f, 0.f, 0.f, 4.f)
			[
				PanelButton
			];

		Panels.Emplace(InPanel, PanelButton);

		// Upon explicit request open the drawer automatically after it's added
		if (InPanel->bDrawnByDefault)
		{
			InPanel->SetState(ERCPanelState::Opened);
		}

		if (InPanel->IsActive())
		{
			PanelButton->UpdateAppearance(InPanel);

			OpenedDrawer = InPanel->GetPanelID();
		}
	}
}

bool SRCPanelDrawer::UnregisterPanel(TSharedRef<FRCPanelDrawerArgs> InPanelToRemove)
{
	int32 FoundIndex = Panels.IndexOfByPredicate(
		[InPanelToRemove](TPair<TSharedRef<FRCPanelDrawerArgs>, TSharedRef<SRCPanelDrawerButton>> PanelPair)
		{
			return PanelPair.Key == InPanelToRemove;
		});

	if (FoundIndex != INDEX_NONE)
	{
		TPair<TSharedRef<FRCPanelDrawerArgs>, TSharedRef<SRCPanelDrawerButton>> PanelPair = Panels[FoundIndex];

		Panels.RemoveAt(FoundIndex);
		PanelBox->RemoveSlot(PanelPair.Value);

		if (Panels.Num() == 0)
		{
			SetVisibility(EVisibility::Collapsed);
		}
	}

	return FoundIndex != INDEX_NONE;
}

void SRCPanelDrawer::TogglePanel(TSharedRef<FRCPanelDrawerArgs> InPanel, const bool bRemoveDrawer)
{
	if (!CanToggleRCPanelDelegate.IsBound())
	{
		return;
	}

	if (!CanToggleRCPanelDelegate.Execute())
	{
		return;
	}

	auto PanelFinderPredicate = [InPanel](const TPair<TSharedRef<FRCPanelDrawerArgs>, TSharedRef<SRCPanelDrawerButton>>& TabAndButton)
	{
		return TabAndButton.Key == InPanel;
	};

	if (bRemoveDrawer)
	{
		UnregisterPanel(InPanel);
	}

	if (OpenedDrawer != InPanel->GetPanelID())
	{
		OpenedDrawer = InPanel->GetPanelID();

		UpdateAppearance();
	}
	else if (TPair<TSharedRef<FRCPanelDrawerArgs>, TSharedRef<SRCPanelDrawerButton>>* Panel = Panels.FindByPredicate(PanelFinderPredicate))
	{
		OpenedDrawer = ERCPanels::RCP_None;

		Panel->Key->SetState(ERCPanelState::Collapsed);

		Panel->Value->UpdateAppearance(nullptr);
	}
	
	if (InPanel->DrawerVisibility == EVisibility::Visible)
	{
		// Register mode specific panels as they might not get registered initially based on the mode we are in.
		RegisterPanel(InPanel);
	}

	OnRCPanelToggledDelegate.ExecuteIfBound(OpenedDrawer);
}

void SRCPanelDrawer::UpdateAppearance()
{
	for (auto Panel = Panels.CreateIterator(); Panel; ++Panel)
	{
		const bool bShouldUpdateAppearance = Panel->Key->GetPanelID() == OpenedDrawer;

		Panel->Key->SetState(bShouldUpdateAppearance ? ERCPanelState::Opened : ERCPanelState::Collapsed);

		if (bShouldUpdateAppearance)
		{
			Panel->Value->UpdateAppearance(Panel->Key);
		}
		else
		{
			Panel->Value->UpdateAppearance(nullptr);
		}
	}
}

#undef LOCTEXT_NAMESPACE

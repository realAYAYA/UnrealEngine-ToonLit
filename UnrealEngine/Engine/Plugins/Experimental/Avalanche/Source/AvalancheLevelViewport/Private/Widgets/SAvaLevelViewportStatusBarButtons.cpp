// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SAvaLevelViewportStatusBarButtons.h"
#include "AvaEditorWidgetUtils.h"
#include "AvaLevelViewportCommands.h"
#include "AvaLevelViewportModule.h"
#include "AvaLevelViewportStyle.h"
#include "AvaViewportPostProcessManager.h"
#include "AvaViewportSettings.h"
#include "CoreGlobals.h"
#include "Engine/Texture.h"
#include "Interaction/AvaIsolateActorsOperation.h"
#include "LevelEditor.h"
#include "LevelViewportActions.h"
#include "PropertyCustomizationHelpers.h"
#include "SAvaLevelViewport.h"
#include "SAvaLevelViewportFrame.h"
#include "SAvaViewportInfo.h"
#include "Selection.h"
#include "Styling/AppStyle.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "ViewportClient/AvaLevelViewportClient.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SAvaLevelViewportActorAlignmentMenu.h"
#include "Widgets/SAvaLevelViewportActorColorMenu.h"
#include "Widgets/SAvaLevelViewportStatusBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"

#define LOCTEXT_NAMESPACE "SAvaLevelViewportStatusBarButtons"

namespace UE::Ava::LevelViewportStatusBarButtons::Private
{
	static const FName AvaLevelViewportStyleName = FAvaLevelViewportStyle::Get().GetStyleSetName();
	static const FName AppStyleSetName = FAppStyle::Get().GetStyleSetName();
	static const FSlateIcon RGBChannelIcon = FSlateIcon(AvaLevelViewportStyleName, "Icons.PostProcess.RGB");
	static const FSlateIcon BackgroundIcon = FSlateIcon(AppStyleSetName, "Icons.Role");
	static const FSlateIcon RedChannelIcon = FSlateIcon(AvaLevelViewportStyleName, "Icons.PostProcess.Red");
	static const FSlateIcon GreenChannelIcon = FSlateIcon(AvaLevelViewportStyleName, "Icons.PostProcess.Green");
	static const FSlateIcon BlueChannelIcon = FSlateIcon(AvaLevelViewportStyleName, "Icons.PostProcess.Blue");
	static const FSlateIcon AlphaChannelIcon = FSlateIcon(AvaLevelViewportStyleName, "Icons.PostProcess.Alpha");
	static const FSlateIcon CheckerboardIcon = FSlateIcon(AppStyleSetName, "Checker");

	static bool IsViewportPostProcessManagerEnabled(const TWeakPtr<SAvaLevelViewportFrame>& InViewportFrameWeak)
	{
		const FAvaLevelViewportGuideFrameAndClient FrameAndClient(InViewportFrameWeak);

		return FrameAndClient.IsValid() && FrameAndClient.ViewportClient->GetPostProcessManager().IsValid();
	}

	static FSlateColor GetPostProcessColor(const TWeakPtr<SAvaLevelViewportFrame>& InViewportFrameWeak, EAvaViewportPostProcessType InPostProcessType,
		const FSlateColor& InActiveColor, const FSlateColor& InEnabledColor, const FSlateColor& InDisabledColor)
	{
		const FAvaLevelViewportGuideFrameAndClient FrameAndClient(InViewportFrameWeak);

		if (FrameAndClient.IsValid())
		{
			if (const TSharedPtr<FAvaViewportPostProcessManager> PostProcessManager = FrameAndClient.ViewportClient->GetPostProcessManager())
			{
				return PostProcessManager->GetType() == InPostProcessType ? InActiveColor : InEnabledColor;
			}
		}

		return InDisabledColor;
	}

	static void TogglePostProcess(const TWeakPtr<SAvaLevelViewportFrame>& InViewportFrameWeak, EAvaViewportPostProcessType InPostProcessType)
	{
		const FAvaLevelViewportGuideFrameAndClient FrameAndClient(InViewportFrameWeak);

		if (FrameAndClient.IsValid())
		{
			if (const TSharedPtr<FAvaViewportPostProcessManager> PostProcessManager = FrameAndClient.ViewportClient->GetPostProcessManager())
			{
				// None should always apply None.
				if (InPostProcessType == EAvaViewportPostProcessType::None || PostProcessManager->GetType() == InPostProcessType)
				{
					PostProcessManager->SetType(EAvaViewportPostProcessType::None);
				}
				else
				{
					PostProcessManager->SetType(InPostProcessType);
				}
			}
		}
	}
}

void SAvaLevelViewportStatusBarButtons::Construct(const FArguments& InArgs, TSharedPtr<SAvaLevelViewportFrame> InViewportFrame)
{
	ViewportFrameWeak = InViewportFrame;

	constexpr float Padding = 5.f;

	TSharedPtr<SHorizontalBox> ActorButtons;
	TSharedPtr<SHorizontalBox> ViewportButtons;

	TSharedPtr<SWidget> StatusBarWidget = nullptr;
	{
		static FName MenuName = UE::AvaLevelViewport::Internal::StatusBarMenuName;
		UToolMenu* Menu = UToolMenus::Get()->FindMenu(MenuName);
		StatusBarWidget = UToolMenus::Get()->GenerateWidget(Menu);
	}

	ChildSlot
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 0, Padding, 0)
		[
			SNew(SSeparator)
			.Orientation(EOrientation::Orient_Vertical)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 0, Padding, 0)
		[
			StatusBarWidget.ToSharedRef()
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 0, Padding, 0)
		[
			SNew(SSeparator)
			.Orientation(EOrientation::Orient_Vertical)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 0, Padding, 0)
		[
			SAssignNew(ActorButtons, SHorizontalBox)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 0, Padding, 0)
		[
			SNew(SSeparator)
			.Orientation(EOrientation::Orient_Vertical)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0, 0, Padding, 0)
		[
			SAssignNew(ViewportButtons, SHorizontalBox)
		]
	];

	CreateContextMenuWigets();

	PopulateActorButtons(ActorButtons);
	PopulateViewportButtons(ViewportButtons);
}

void SAvaLevelViewportStatusBarButtons::CreateContextMenuWigets()
{
	FAvaLevelViewportGuideFrameAndWidget FrameAndWidget(ViewportFrameWeak);

	if (!FrameAndWidget.IsValid())
	{
		return;
	}

	SAvaLevelViewport* LevelViewport = FrameAndWidget.ViewportWidget.Get();

	if (!PostProcessOpacitySlider.IsValid())
	{
		PostProcessOpacitySlider = SNew(SSpinBox<float>)
			.ClearKeyboardFocusOnCommit(true)
			.MaxFractionalDigits(3)
			.MinDesiredWidth(50.f)
			.OnBeginSliderMovement(LevelViewport, &SAvaLevelViewport::OnBackgroundOpacitySliderBegin)
			.OnEndSliderMovement(LevelViewport, &SAvaLevelViewport::OnBackgroundOpacitySliderEnd)
			.OnValueCommitted(LevelViewport, &SAvaLevelViewport::OnBackgroundOpacityCommitted)
			.OnValueChanged(LevelViewport, &SAvaLevelViewport::OnBackgroundOpacityCommitted, ETextCommit::Default)
			.Value(LevelViewport, &SAvaLevelViewport::GetBackgroundOpacity)
			.MinValue(0.f)
			.MinSliderValue(0.f)
			.MaxValue(1.f)
			.MaxSliderValue(1.f);
	}

	if (!BackgroundTextureSelector.IsValid())
	{
		BackgroundTextureSelector = SNew(SObjectPropertyEntryBox)
			.AllowClear(true)
			.AllowedClass(UTexture::StaticClass())
			.DisplayBrowse(true)
			.DisplayThumbnail(true)
			.DisplayCompactSize(true)
			.DisplayUseSelected(true)
			.ThumbnailPool(UThumbnailManager::Get().GetSharedThumbnailPool())
			.EnableContentPicker(true)
			.ObjectPath(LevelViewport, &SAvaLevelViewport::GetBackgroundTextureObjectPath)
			.OnObjectChanged(LevelViewport, &SAvaLevelViewport::OnBackgroundTextureChanged)
			.OnShouldSetAsset(FOnShouldSetAsset::CreateLambda([](const FAssetData& InAssetData) { return false; }));
	}

	if (!GridSizeSlider.IsValid())
	{
		GridSizeSlider = SNew(SBox)
			.Padding(10.f, 0.f, 0.f, 0.f)
			[
				SNew(SSpinBox<int32>)
				.Justification(ETextJustify::Center)
				.Style(&FAppStyle::Get(), "Menu.SpinBox")
				.Font(FAppStyle::GetFontStyle("TinyText"))
				.MinValue(1)
				.MaxValue(256)
				.Value_Lambda([]() { return GetDefault<UAvaViewportSettings>()->GridSize; })
				.IsEnabled(this, &SAvaLevelViewportStatusBarButtons::CanChangeGridSize)
				.OnValueChanged(this, &SAvaLevelViewportStatusBarButtons::OnGridSizeChanged)
				.OnValueCommitted(this, &SAvaLevelViewportStatusBarButtons::OnGridSizeCommitted)
			];
	}
}

void SAvaLevelViewportStatusBarButtons::PopulateActorButtons(TSharedPtr<SHorizontalBox> InContainer)
{
	using namespace UE::AvaLevelViewport::Private;

	const FAvaLevelViewportCommands& CommandsRef = FAvaLevelViewportCommands::Get();

	InContainer->AddSlot()
		.AutoWidth()
		.Padding(ViewportStatusBarButton::Padding)
		[
			ViewportStatusBarButton::MakeMenuButton(
				LOCTEXT("ActorColor", "Actor Color"),
				FOnGetContent::CreateSP(this, &SAvaLevelViewportStatusBarButtons::GetActorColorMenuContent),
				FAppStyle::Get().GetBrush(TEXT("ColorPicker.Mode")),
				FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f))
			)
		];

	TSharedRef<SComboButton> AlignmentButton = ViewportStatusBarButton::MakeMenuButton(
		LOCTEXT("ActorAlign", "Align Actors"),
		FOnGetContent::CreateSP(this, &SAvaLevelViewportStatusBarButtons::GetActorAlignmentMenuContent),
		FAppStyle::Get().GetBrush(TEXT("Icons.Layout")),
		TAttribute<FSlateColor>::CreateSP(this, &SAvaLevelViewportStatusBarButtons::GetActorAlignmentColor)
	);

	AlignmentButton->SetEnabled(TAttribute<bool>::CreateSP(
		this,
		&SAvaLevelViewportStatusBarButtons::GetActorAlignmentEnabled
	));

	InContainer->AddSlot()
		.AutoWidth()
		.Padding(ViewportStatusBarButton::Padding)
		[
			AlignmentButton
		];
}

void SAvaLevelViewportStatusBarButtons::PopulateViewportButtons(TSharedPtr<SHorizontalBox> InContainer)
{
	using namespace UE::AvaLevelViewport::Private;

	const FAvaLevelViewportCommands& CommandsRef = FAvaLevelViewportCommands::Get();
	const FLevelViewportCommands& ViewportActionsRef = FLevelViewportCommands::Get();

	InContainer->AddSlot()
		.AutoWidth()
		.Padding(ViewportStatusBarButton::Padding)
		[
			ViewportStatusBarButton::MakeButton(
				this,
				ViewportActionsRef.HighResScreenshot,
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelViewport.HighResScreenshot").GetIcon(),
				&SAvaLevelViewportStatusBarButtons::HighResScreenshot,
				&SAvaLevelViewportStatusBarButtons::GetHighResScreenshotEnabled,
				&SAvaLevelViewportStatusBarButtons::GetHighResScreenshotColor
			)
		];

	TSharedRef<SComboButton> PostProcessButton = ViewportStatusBarButton::MakeMenuButton(
		LOCTEXT("PostProcessEffects", "Post Process Effects"),
		FOnGetContent::CreateSP(this, &SAvaLevelViewportStatusBarButtons::GetPostProcessMenuContent),
		TAttribute<const FSlateBrush*>::CreateSP(this, &SAvaLevelViewportStatusBarButtons::GetPostProcessIcon),
		TAttribute<FSlateColor>::CreateSP(this, &SAvaLevelViewportStatusBarButtons::GetPostProcessColor)
	);

	PostProcessButton->SetEnabled(TAttribute<bool>::CreateSP(this, &SAvaLevelViewportStatusBarButtons::GetPostProcessEnabled));

	InContainer->AddSlot()
		.AutoWidth()
		.Padding(ViewportStatusBarButton::Padding)
		[
			PostProcessButton
		];

	InContainer->AddSlot()
		.AutoWidth()
		.Padding(ViewportStatusBarButton::Padding)
		[
			ViewportStatusBarButton::MakeButton(
				this,
				ViewportActionsRef.ToggleGameView,
				FAvaLevelViewportStyle::Get().GetBrush(TEXT("Button.GameView")),
				&SAvaLevelViewportStatusBarButtons::ToggleGameView,
				&SAvaLevelViewportStatusBarButtons::GetToggleGameViewEnabled,
				&SAvaLevelViewportStatusBarButtons::GetToggleGameViewColor
			)
		];

	InContainer->AddSlot()
		.AutoWidth()
		.Padding(ViewportStatusBarButton::Padding)
		[
			ViewportStatusBarButton::MakeButton(
				this,
				CommandsRef.ToggleBoundingBoxes,
				FAvaLevelViewportStyle::Get().GetBrush(TEXT("Button.BoundingBoxes")),
				&SAvaLevelViewportStatusBarButtons::ToggleBoundingBoxes,
				&SAvaLevelViewportStatusBarButtons::GetToggleBoundingBoxesEnabled,
				&SAvaLevelViewportStatusBarButtons::GetToggleBoundingBoxesColor
			)
		];

	InContainer->AddSlot()
		.AutoWidth()
		.Padding(ViewportStatusBarButton::Padding)
		[
			ViewportStatusBarButton::MakeButton(
				this,
				CommandsRef.ToggleIsolateActors,
				FAvaLevelViewportStyle::Get().GetBrush(TEXT("Button.IsolateActors")),
				&SAvaLevelViewportStatusBarButtons::ToggleIsolateActors,
				&SAvaLevelViewportStatusBarButtons::GetToggleIsolateActorsEnabled,
				&SAvaLevelViewportStatusBarButtons::GetToggleIsolateActorsColor
			)
		];

	InContainer->AddSlot()
		.AutoWidth()
		.Padding(ViewportStatusBarButton::Padding)
		[
			ViewportStatusBarButton::MakeButton(
				this,
				CommandsRef.ToggleSafeFrames,
				FAvaLevelViewportStyle::Get().GetBrush(TEXT("Button.SafeFrames")),
				&SAvaLevelViewportStatusBarButtons::ToggleSafeFrames,
				&SAvaLevelViewportStatusBarButtons::GetToggleSafeFramesEnabled,
				&SAvaLevelViewportStatusBarButtons::GetToggleSafeFramesColor
			)
		];

	InContainer->AddSlot()
		.AutoWidth()
		.Padding(ViewportStatusBarButton::Padding)
		[
			ViewportStatusBarButton::MakeButton(
				this,
				CommandsRef.ToggleGuides,
				FAvaLevelViewportStyle::Get().GetBrush(TEXT("Button.Guides")),
				&SAvaLevelViewportStatusBarButtons::ToggleGuides,
				&SAvaLevelViewportStatusBarButtons::GetToggleGuidesEnabled,
				&SAvaLevelViewportStatusBarButtons::GetToggleGuidesColor
			)
		];

	InContainer->AddSlot()
		.AutoWidth()
		.Padding(ViewportStatusBarButton::Padding)
		[
			ViewportStatusBarButton::MakeButton(
				this,
				CommandsRef.ToggleShapeEditorOverlay,
				FAppStyle::Get().GetBrush(TEXT("Icons.Filter")),
				&SAvaLevelViewportStatusBarButtons::ToggleShapeEditorOverlay,
				&SAvaLevelViewportStatusBarButtons::GetToggleShapeEditorOverlayEnabled,
				&SAvaLevelViewportStatusBarButtons::GetToggleShapeEditorOverlayColor
			)
		];

	InContainer->AddSlot()
		.AutoWidth()
		.Padding(ViewportStatusBarButton::Padding)
		[
			ViewportStatusBarButton::MakeButton(
				this,
				CommandsRef.ToggleOverlay,
				FAppStyle::GetBrush("Icons.Visible"),
				&SAvaLevelViewportStatusBarButtons::ToggleOverlay,
				&SAvaLevelViewportStatusBarButtons::GetToggleOverlayEnabled,
				&SAvaLevelViewportStatusBarButtons::GetToggleOverlayColor
			)
		];

	TSharedRef<SAvaMultiComboButton> SnapButton = ViewportStatusBarButton::MakeMultiMenuButton(
		CommandsRef.ToggleSnapping->GetDescription(),
		FOnGetContent::CreateSP(this, &SAvaLevelViewportStatusBarButtons::GetSnappingMenuContent),
		FAvaLevelViewportStyle::Get().GetBrush(TEXT("Button.ToggleSnap")),
		TAttribute<FSlateColor>::CreateSP(this, &SAvaLevelViewportStatusBarButtons::GetToggleSnapColor),
		FOnClicked::CreateSP(this, &SAvaLevelViewportStatusBarButtons::ToggleSnap)
	);

	SnapButton->SetEnabled(TAttribute<bool>::CreateSP(
		this,
		&SAvaLevelViewportStatusBarButtons::GetToggleSnapEnabled
	));

	InContainer->AddSlot()
		.AutoWidth()
		.Padding(ViewportStatusBarButton::Padding)
		[
			SnapButton
		];

	TSharedRef<SAvaMultiComboButton> GridButton = ViewportStatusBarButton::MakeMultiMenuButton(
		CommandsRef.ToggleGrid->GetDescription(),
		FOnGetContent::CreateSP(this, &SAvaLevelViewportStatusBarButtons::GetGridMenuContent),
		FAvaLevelViewportStyle::Get().GetBrush(TEXT("Button.ToggleGrid")),
		TAttribute<FSlateColor>::CreateSP(this, &SAvaLevelViewportStatusBarButtons::GetToggleGridColor),
		FOnClicked::CreateSP(this, &SAvaLevelViewportStatusBarButtons::ToggleGrid)
	);

	GridButton->SetEnabled(TAttribute<bool>::CreateSP(
		this,
		&SAvaLevelViewportStatusBarButtons::GetToggleGridEnabled
	));

	InContainer->AddSlot()
		.AutoWidth()
		.Padding(ViewportStatusBarButton::Padding)
		[
			GridButton
		];

	TSharedRef<SComboButton> ViewportInfoButton = ViewportStatusBarButton::MakeMenuButton(
		LOCTEXT("ViewportInfomation", "Viewport Information"),
		FOnGetContent::CreateSP(this, &SAvaLevelViewportStatusBarButtons::GetViewportInfoWidget),
		FAppStyle::Get().GetBrush(TEXT("Icons.AutoFilter")),
		TAttribute<FSlateColor>::CreateSP(this, &SAvaLevelViewportStatusBarButtons::GetViewportInfoColor)
	);

	ViewportInfoButton->SetEnabled(TAttribute<bool>::CreateSP(this, &SAvaLevelViewportStatusBarButtons::GetViewportInfoEnabled));

	InContainer->AddSlot()
		.AutoWidth()
		.Padding(ViewportStatusBarButton::Padding)
		[
			ViewportInfoButton
		];
}

FSlateColor SAvaLevelViewportStatusBarButtons::GetPostProcessColor() const
{
	using namespace UE::AvaLevelViewport::Private;

	const FAvaLevelViewportGuideFrameAndClient FrameAndClient(ViewportFrameWeak);

	if (FrameAndClient.IsValid())
	{
		if (const TSharedPtr<FAvaViewportPostProcessManager> PostProcessManager = FrameAndClient.ViewportClient->GetPostProcessManager())
		{
			return ViewportStatusBarButton::EnabledColor;
		}
	}

	return ViewportStatusBarButton::DisabledColor;
}

const FSlateBrush* SAvaLevelViewportStatusBarButtons::GetPostProcessIcon() const
{
	using namespace UE::Ava::LevelViewportStatusBarButtons::Private;

	static const FSlateBrush* RGBBrush = RGBChannelIcon.GetIcon();
	static const FSlateBrush* BackgroundBrush = BackgroundIcon.GetIcon();
	static const FSlateBrush* RedChannelBrush = RedChannelIcon.GetIcon();
	static const FSlateBrush* GreenChannelBrush = GreenChannelIcon.GetIcon();
	static const FSlateBrush* BlueChannelBrush = BlueChannelIcon.GetIcon();
	static const FSlateBrush* AlphaChannelBrush = AlphaChannelIcon.GetIcon();
	static const FSlateBrush* CheckerBrush = CheckerboardIcon.GetIcon();

	const FAvaLevelViewportGuideFrameAndClient FrameAndClient(ViewportFrameWeak);

	if (FrameAndClient.IsValid())
	{
		if (const TSharedPtr<FAvaViewportPostProcessManager> PostProcessManager = FrameAndClient.ViewportClient->GetPostProcessManager())
		{
			switch (PostProcessManager->GetType())
			{
				case EAvaViewportPostProcessType::None:
					return RGBBrush;

				case EAvaViewportPostProcessType::Background:
					return BackgroundBrush;

				case EAvaViewportPostProcessType::RedChannel:
					return RedChannelBrush;

				case EAvaViewportPostProcessType::GreenChannel:
					return GreenChannelBrush;

				case EAvaViewportPostProcessType::BlueChannel:
					return BlueChannelBrush;

				case EAvaViewportPostProcessType::AlphaChannel:
					return AlphaChannelBrush;

				case EAvaViewportPostProcessType::Checkerboard:
					return CheckerBrush;
			}
		}
	}

	return RGBBrush;
}

bool SAvaLevelViewportStatusBarButtons::GetPostProcessEnabled() const
{
	const FAvaLevelViewportGuideFrameAndClient FrameAndClient(ViewportFrameWeak);

	if (FrameAndClient.IsValid())
	{
		return FrameAndClient.ViewportClient->GetPostProcessManager().IsValid();
	}

	return false;
}

TSharedRef<SWidget> SAvaLevelViewportStatusBarButtons::GetPostProcessMenuContent()
{
	UToolMenus* Menus = UToolMenus::Get();

	check(Menus);

	static const FName BackgroundMenuName = TEXT("AvaLevelViewport.StatusBar.PostProcess.Background");

	UToolMenu* ContextMenu = Menus->FindMenu(BackgroundMenuName);

	if (!ContextMenu)
	{
		using namespace UE::Ava::LevelViewportStatusBarButtons::Private;

		ContextMenu = Menus->RegisterMenu(BackgroundMenuName, NAME_None, EMultiBoxType::Menu);

		const FAvaLevelViewportCommands& CommandsRef = FAvaLevelViewportCommands::Get();

		FToolMenuSection& EffectsSection = ContextMenu->AddSection("Effects", LOCTEXT("Effects", "Effects"));

		FToolUIAction RGBAction;
		RGBAction.ExecuteAction = FToolMenuExecuteAction::CreateSP(this, &SAvaLevelViewportStatusBarButtons::TogglePostProcessMenu, EAvaViewportPostProcessType::None);
		RGBAction.GetActionCheckState = FToolMenuGetActionCheckState::CreateSP(this, &SAvaLevelViewportStatusBarButtons::GetPostProcessActiveMenu, EAvaViewportPostProcessType::None);
		RGBAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateSP(this, &SAvaLevelViewportStatusBarButtons::GetPostProcessEnabledMenu);

		EffectsSection.AddEntry(FToolMenuEntry::InitMenuEntry(
			"RGB",
			LOCTEXT("RGB", "RGB"),
			CommandsRef.TogglePostProcessNone->GetDescription(),
			RGBChannelIcon,
			FToolUIActionChoice(RGBAction),
			EUserInterfaceActionType::Check
		));

		FToolUIAction BackgroundAction;
		BackgroundAction.ExecuteAction = FToolMenuExecuteAction::CreateSP(this, &SAvaLevelViewportStatusBarButtons::TogglePostProcessMenu, EAvaViewportPostProcessType::Background);
		BackgroundAction.GetActionCheckState = FToolMenuGetActionCheckState::CreateSP(this, &SAvaLevelViewportStatusBarButtons::GetPostProcessActiveMenu, EAvaViewportPostProcessType::Background);
		BackgroundAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateSP(this, &SAvaLevelViewportStatusBarButtons::GetPostProcessEnabledMenu);

		EffectsSection.AddEntry(FToolMenuEntry::InitMenuEntry(
			"Background", 
			LOCTEXT("Background", "Background"), 
			CommandsRef.TogglePostProcessBackground->GetDescription(),
			BackgroundIcon,
			FToolUIActionChoice(BackgroundAction),
			EUserInterfaceActionType::Check
		));

		FToolUIAction RedAction;
		RedAction.ExecuteAction = FToolMenuExecuteAction::CreateSP(this, &SAvaLevelViewportStatusBarButtons::TogglePostProcessMenu, EAvaViewportPostProcessType::RedChannel);
		RedAction.GetActionCheckState = FToolMenuGetActionCheckState::CreateSP(this, &SAvaLevelViewportStatusBarButtons::GetPostProcessActiveMenu, EAvaViewportPostProcessType::RedChannel);
		RedAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateSP(this, &SAvaLevelViewportStatusBarButtons::GetPostProcessEnabledMenu);

		EffectsSection.AddEntry(FToolMenuEntry::InitMenuEntry(
			"Red",
			LOCTEXT("Red", "Red"),
			CommandsRef.TogglePostProcessChannelRed->GetDescription(),
			RedChannelIcon,
			FToolUIActionChoice(RedAction),
			EUserInterfaceActionType::Check
		));

		FToolUIAction GreenAction;
		GreenAction.ExecuteAction = FToolMenuExecuteAction::CreateSP(this, &SAvaLevelViewportStatusBarButtons::TogglePostProcessMenu, EAvaViewportPostProcessType::GreenChannel);
		GreenAction.GetActionCheckState = FToolMenuGetActionCheckState::CreateSP(this, &SAvaLevelViewportStatusBarButtons::GetPostProcessActiveMenu, EAvaViewportPostProcessType::GreenChannel);
		GreenAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateSP(this, &SAvaLevelViewportStatusBarButtons::GetPostProcessEnabledMenu);

		EffectsSection.AddEntry(FToolMenuEntry::InitMenuEntry(
			"Green",
			LOCTEXT("Green", "Green"),
			CommandsRef.TogglePostProcessChannelGreen->GetDescription(),
			GreenChannelIcon,
			FToolUIActionChoice(GreenAction),
			EUserInterfaceActionType::Check
		));

		FToolUIAction BlueAction;
		BlueAction.ExecuteAction = FToolMenuExecuteAction::CreateSP(this, &SAvaLevelViewportStatusBarButtons::TogglePostProcessMenu, EAvaViewportPostProcessType::BlueChannel);
		BlueAction.GetActionCheckState = FToolMenuGetActionCheckState::CreateSP(this, &SAvaLevelViewportStatusBarButtons::GetPostProcessActiveMenu, EAvaViewportPostProcessType::BlueChannel);
		BlueAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateSP(this, &SAvaLevelViewportStatusBarButtons::GetPostProcessEnabledMenu);

		EffectsSection.AddEntry(FToolMenuEntry::InitMenuEntry(
			"Blue",
			LOCTEXT("Blue", "Blue"),
			CommandsRef.TogglePostProcessChannelBlue->GetDescription(),
			BlueChannelIcon,
			FToolUIActionChoice(BlueAction),
			EUserInterfaceActionType::Check
		));

		FToolUIAction AlphaAction;
		AlphaAction.ExecuteAction = FToolMenuExecuteAction::CreateSP(this, &SAvaLevelViewportStatusBarButtons::TogglePostProcessMenu, EAvaViewportPostProcessType::AlphaChannel);
		AlphaAction.GetActionCheckState = FToolMenuGetActionCheckState::CreateSP(this, &SAvaLevelViewportStatusBarButtons::GetPostProcessActiveMenu, EAvaViewportPostProcessType::AlphaChannel);
		AlphaAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateSP(this, &SAvaLevelViewportStatusBarButtons::GetPostProcessEnabledMenu);

		EffectsSection.AddEntry(FToolMenuEntry::InitMenuEntry(
			"Alpha",
			LOCTEXT("Alpha", "Alpha"),
			CommandsRef.TogglePostProcessChannelAlpha->GetDescription(),
			AlphaChannelIcon,
			FToolUIActionChoice(AlphaAction),
			EUserInterfaceActionType::Check
		));

		FToolUIAction CheckerboardAction;
		CheckerboardAction.ExecuteAction = FToolMenuExecuteAction::CreateSP(this, &SAvaLevelViewportStatusBarButtons::TogglePostProcessMenu, EAvaViewportPostProcessType::Checkerboard);
		CheckerboardAction.GetActionCheckState = FToolMenuGetActionCheckState::CreateSP(this, &SAvaLevelViewportStatusBarButtons::GetPostProcessActiveMenu, EAvaViewportPostProcessType::Checkerboard);
		CheckerboardAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateSP(this, &SAvaLevelViewportStatusBarButtons::GetPostProcessEnabledMenu);

		EffectsSection.AddEntry(FToolMenuEntry::InitMenuEntry(
			"Checkerboard",
			LOCTEXT("Checkerboard", "Checkerboard"),
			CommandsRef.TogglePostProcessCheckerboard->GetDescription(),
			CheckerboardIcon,
			FToolUIActionChoice(CheckerboardAction),
			EUserInterfaceActionType::Check
		));

		FToolMenuSection& OptionsSection = ContextMenu->AddSection("Options", LOCTEXT("Options", "Options"));

		if (PostProcessOpacitySlider.IsValid())
		{
			OptionsSection.AddEntry(FToolMenuEntry::InitWidget(
				"PostProcessOpacity",
				PostProcessOpacitySlider.ToSharedRef(),
				LOCTEXT("PostProcessOpacity", "Opacity"),
				true
			));
		}

		if (BackgroundTextureSelector.IsValid())
		{
			OptionsSection.AddEntry(FToolMenuEntry::InitWidget(
				"PostProcessTexture",
				BackgroundTextureSelector.ToSharedRef(),
				LOCTEXT("PostProcessTexture", "Texture"),
				true
			));
		}
	}

	if (!ContextMenu)
	{
		return SNullWidget::NullWidget;
	}

	return Menus->GenerateWidget(ContextMenu);
}

bool SAvaLevelViewportStatusBarButtons::GetPostProcessEnabledMenu(const FToolMenuContext& InContext) const
{
	return GetPostProcessEnabled();
}

ECheckBoxState SAvaLevelViewportStatusBarButtons::GetPostProcessActiveMenu(const FToolMenuContext& InContext, EAvaViewportPostProcessType InPostProcessType) const
{
	const FAvaLevelViewportGuideFrameAndClient FrameAndClient(ViewportFrameWeak);

	if (FrameAndClient.IsValid())
	{
		if (const TSharedPtr<FAvaViewportPostProcessManager> PostProcessManager = FrameAndClient.ViewportClient->GetPostProcessManager())
		{
			if (PostProcessManager->GetType() == InPostProcessType)
			{
				return ECheckBoxState::Checked;
			}
		}
	}

	return ECheckBoxState::Unchecked;
}

void SAvaLevelViewportStatusBarButtons::TogglePostProcessMenu(const FToolMenuContext& InContext, EAvaViewportPostProcessType InPostProcessType)
{
	UE::Ava::LevelViewportStatusBarButtons::Private::TogglePostProcess(ViewportFrameWeak, InPostProcessType);
}

FSlateColor SAvaLevelViewportStatusBarButtons::GetHighResScreenshotColor() const
{
	return FSlateColor(EStyleColor::Foreground);
}

bool SAvaLevelViewportStatusBarButtons::GetHighResScreenshotEnabled() const
{
	if (GIsHighResScreenshot)
	{
		return false;
	}

	const FAvaLevelViewportGuideFrameAndClient FrameAndClient(ViewportFrameWeak);

	if (FrameAndClient.IsValid())
	{
		return !!FrameAndClient.ViewportClient->Viewport;
	}

	return false;
}

FReply SAvaLevelViewportStatusBarButtons::HighResScreenshot()
{
	const FAvaLevelViewportGuideFrameAndClient FrameAndClient(ViewportFrameWeak);

	if (FrameAndClient.IsValid() && !!FrameAndClient.ViewportClient->Viewport)
	{
		GScreenshotResolutionX = 0;
		GScreenshotResolutionY = 0;

		FrameAndClient.ViewportClient->Viewport->TakeHighResScreenShot();

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FSlateColor SAvaLevelViewportStatusBarButtons::GetActorAlignmentColor() const
{
	static const FSlateColor Active = FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
	static const FSlateColor Inactive = FSlateColor(FLinearColor(0.3f, 0.3f, 0.3f, 1.0f));

	if (GetActorAlignmentEnabled())
	{
		return Active;
	}

	return Inactive;
}

bool SAvaLevelViewportStatusBarButtons::GetActorAlignmentEnabled() const
{
	const FAvaLevelViewportGuideFrameAndClient FrameAndClient(ViewportFrameWeak);

	if (FrameAndClient.IsValid())
	{
		if (FEditorModeTools* ModeTools = FrameAndClient.ViewportClient->GetModeTools())
		{
			if (USelection* ActorSelection = ModeTools->GetSelectedActors())
			{
				return ActorSelection->Num() > 0;
			}
		}
	}

	return false;
}

TSharedRef<SWidget> SAvaLevelViewportStatusBarButtons::GetActorAlignmentMenuContent() const
{
	if (!GetActorAlignmentEnabled())
	{
		return SNullWidget::NullWidget;
	}

	const FAvaLevelViewportGuideFrameAndWidget FrameAndWidget(ViewportFrameWeak);

	if (!FrameAndWidget.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	TSharedPtr<ILevelEditor> LevelEditor = FrameAndWidget.ViewportWidget->GetParentLevelEditor().Pin();

	if (!LevelEditor.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	return SAvaLevelViewportActorAlignmentMenu::CreateMenu(LevelEditor.ToSharedRef());
}

TSharedRef<SWidget> SAvaLevelViewportStatusBarButtons::GetActorColorMenuContent() const
{
	const FAvaLevelViewportGuideFrameAndWidget FrameAndWidget(ViewportFrameWeak);

	if (!FrameAndWidget.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	TSharedPtr<ILevelEditor> LevelEditor = FrameAndWidget.ViewportWidget->GetParentLevelEditor().Pin();

	if (!LevelEditor.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	return SAvaLevelViewportActorColorMenu::CreateMenu(LevelEditor.ToSharedRef());
}

FSlateColor SAvaLevelViewportStatusBarButtons::GetToggleSnapColor() const
{
	using namespace UE::AvaLevelViewport::Private;

	const FAvaLevelViewportGuideFrameAndWidget FrameAndWidget(ViewportFrameWeak);

	if (FrameAndWidget.IsValid() && FrameAndWidget.ViewportWidget->CanToggleSnapping())
	{
		if (const UAvaViewportSettings* AvaViewportSettings = GetDefault<UAvaViewportSettings>())
		{
			return EnumHasAllFlags(AvaViewportSettings->GetSnapState(), EAvaViewportSnapState::Global)
				? ViewportStatusBarButton::ActiveColor
				: ViewportStatusBarButton::EnabledColor;
		}
	}

	return ViewportStatusBarButton::DisabledColor;
}

bool SAvaLevelViewportStatusBarButtons::GetToggleSnapEnabled() const
{
	const FAvaLevelViewportGuideFrameAndWidget FrameAndWidget(ViewportFrameWeak);

	if (FrameAndWidget.IsValid())
	{
		return FrameAndWidget.ViewportWidget->CanToggleSnapping();
	}

	return false;
}

FReply SAvaLevelViewportStatusBarButtons::ToggleSnap()
{
	const FAvaLevelViewportGuideFrameAndWidget FrameAndWidget(ViewportFrameWeak);

	if (FrameAndWidget.IsValid() && FrameAndWidget.ViewportWidget->CanToggleSnapping())
	{
		FrameAndWidget.ViewportWidget->ExecuteToggleSnapping();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

TSharedRef<SWidget> SAvaLevelViewportStatusBarButtons::GetSnappingMenuContent() const
{
	UToolMenus* Menus = UToolMenus::Get();

	check(Menus);

	static const FName SnapMenuName = TEXT("AvaLevelViewport.StatusBar.Snapping");

	UToolMenu* ContextMenu = Menus->FindMenu(SnapMenuName);

	if (!ContextMenu)
	{
		const FAvaLevelViewportGuideFrameAndWidget FrameAndWidget(ViewportFrameWeak);

		if (!FrameAndWidget.IsValid())
		{
			return SNullWidget::NullWidget;
		}

		ContextMenu = Menus->RegisterMenu(SnapMenuName, NAME_None, EMultiBoxType::Menu);

		FToolMenuSection& Section = ContextMenu->AddSection("SnapTo", LOCTEXT("SnapTo", "Snap To"));

		const FAvaLevelViewportCommands& CommandsRef = FAvaLevelViewportCommands::Get();

		Section.AddEntry(FToolMenuEntry::InitMenuEntryWithCommandList(
			CommandsRef.ToggleGridSnapping,
			FrameAndWidget.ViewportWidget->GetCommandList(),
			LOCTEXT("GridSnapping", "Grid")
		));

		Section.AddEntry(FToolMenuEntry::InitMenuEntryWithCommandList(
			CommandsRef.ToggleScreenSnapping,
			FrameAndWidget.ViewportWidget->GetCommandList(),
			LOCTEXT("ScreenSnapping", "Screen & Guide")
		));

		Section.AddEntry(FToolMenuEntry::InitMenuEntryWithCommandList(
			CommandsRef.ToggleActorSnapping,
			FrameAndWidget.ViewportWidget->GetCommandList(),
			LOCTEXT("ActorSnapping", "Actor")
		));
	}

	if (!ContextMenu)
	{
		return SNullWidget::NullWidget;
	}

	return Menus->GenerateWidget(ContextMenu);
}

FSlateColor SAvaLevelViewportStatusBarButtons::GetToggleShapeEditorOverlayColor() const
{
	using namespace UE::AvaLevelViewport::Private;

	if (const UAvaViewportSettings* AvaViewportSettings = GetDefault<UAvaViewportSettings>())
	{
		return AvaViewportSettings->bEnableShapesEditorOverlay ? ViewportStatusBarButton::ActiveColor : ViewportStatusBarButton::EnabledColor;
	}

	return ViewportStatusBarButton::DisabledColor;
}

bool SAvaLevelViewportStatusBarButtons::GetToggleShapeEditorOverlayEnabled() const
{
	if (const UAvaViewportSettings* AvaViewportSettings = GetDefault<UAvaViewportSettings>())
	{
		return AvaViewportSettings->bEnableViewportOverlay;
	}

	return false;
}

FReply SAvaLevelViewportStatusBarButtons::ToggleShapeEditorOverlay()
{
	using namespace UE::AvaLevelViewport::Private;

	if (UAvaViewportSettings* AvaViewportSettings = GetMutableDefault<UAvaViewportSettings>())
	{
		AvaViewportSettings->bEnableShapesEditorOverlay = !AvaViewportSettings->bEnableShapesEditorOverlay;
		AvaViewportSettings->SaveConfig();
		AvaViewportSettings->OnChange.Broadcast(AvaViewportSettings, GET_MEMBER_NAME_CHECKED(UAvaViewportSettings, bEnableShapesEditorOverlay));

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FSlateColor SAvaLevelViewportStatusBarButtons::GetToggleGuidesColor() const
{
	using namespace UE::AvaLevelViewport::Private;

	const FAvaLevelViewportGuideFrameAndWidget FrameAndWidget(ViewportFrameWeak);

	if (FrameAndWidget.IsValid() && FrameAndWidget.ViewportWidget->CanToggleGuides())
	{
		if (const UAvaViewportSettings* AvaViewportSettings = GetDefault<UAvaViewportSettings>())
		{
			return AvaViewportSettings->bGuidesEnabled ? ViewportStatusBarButton::ActiveColor : ViewportStatusBarButton::EnabledColor;
		}
	}

	return ViewportStatusBarButton::DisabledColor;
}

bool SAvaLevelViewportStatusBarButtons::GetToggleGuidesEnabled() const
{
	const FAvaLevelViewportGuideFrameAndWidget FrameAndWidget(ViewportFrameWeak);

	if (FrameAndWidget.IsValid())
	{
		return FrameAndWidget.ViewportWidget->CanToggleGuides();
	}

	return false;
}

FReply SAvaLevelViewportStatusBarButtons::ToggleGuides()
{
	const FAvaLevelViewportGuideFrameAndWidget FrameAndWidget(ViewportFrameWeak);

	if (FrameAndWidget.IsValid() && FrameAndWidget.ViewportWidget->CanToggleGuides())
	{
		FrameAndWidget.ViewportWidget->ExecuteToggleGuides();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FSlateColor SAvaLevelViewportStatusBarButtons::GetToggleGridColor() const
{
	using namespace UE::AvaLevelViewport::Private;

	const FAvaLevelViewportGuideFrameAndWidget FrameAndWidget(ViewportFrameWeak);

	if (FrameAndWidget.IsValid() && FrameAndWidget.ViewportWidget->CanToggleGrid())
	{
		if (const UAvaViewportSettings* AvaViewportSettings = GetDefault<UAvaViewportSettings>())
		{
			return AvaViewportSettings->bGridEnabled ? ViewportStatusBarButton::ActiveColor : ViewportStatusBarButton::EnabledColor;
		}
	}

	return ViewportStatusBarButton::DisabledColor;
}

bool SAvaLevelViewportStatusBarButtons::GetToggleGridEnabled() const
{
	const FAvaLevelViewportGuideFrameAndWidget FrameAndWidget(ViewportFrameWeak);

	if (FrameAndWidget.IsValid())
	{
		return FrameAndWidget.ViewportWidget->CanToggleGrid();
	}

	return false;
}

FReply SAvaLevelViewportStatusBarButtons::ToggleGrid()
{
	const FAvaLevelViewportGuideFrameAndWidget FrameAndWidget(ViewportFrameWeak);

	if (FrameAndWidget.IsValid() && FrameAndWidget.ViewportWidget->CanToggleGrid())
	{
		FrameAndWidget.ViewportWidget->ExecuteToggleGrid();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

TSharedRef<SWidget> SAvaLevelViewportStatusBarButtons::GetGridMenuContent() const
{
	UToolMenus* Menus = UToolMenus::Get();

	check(Menus);

	static const FName GridMenuName = TEXT("AvaLevelViewport.StatusBar.Grid");

	UToolMenu* ContextMenu = Menus->FindMenu(GridMenuName);

	if (!ContextMenu)
	{
		ContextMenu = Menus->RegisterMenu(GridMenuName, NAME_None, EMultiBoxType::Menu);

		FToolMenuSection& Section = ContextMenu->AddSection("Grid", LOCTEXT("Grid", "Grid"));

		const FAvaLevelViewportCommands& CommandsRef = FAvaLevelViewportCommands::Get();

		const FAvaLevelViewportGuideFrameAndWidget FrameAndWidget(ViewportFrameWeak);

		if (FrameAndWidget.IsValid())
		{
			Section.AddEntry(FToolMenuEntry::InitMenuEntryWithCommandList(
				CommandsRef.ToggleGridAlwaysVisible,
				FrameAndWidget.ViewportWidget->GetCommandList(),
				LOCTEXT("AlwaysShowGrid", "Always On")
			));
		}

		if (GridSizeSlider.IsValid())
		{
			Section.AddEntry(FToolMenuEntry::InitWidget(
				"GridSize",
				GridSizeSlider.ToSharedRef(),
				LOCTEXT("GridSize", "Size"),
				true
			));
		}
	}

	if (!ContextMenu)
	{
		return SNullWidget::NullWidget;
	}

	return Menus->GenerateWidget(ContextMenu);
}

void SAvaLevelViewportStatusBarButtons::OnGridSizeChanged(int32 InNewValue) const
{
	const FAvaLevelViewportGuideFrameAndWidget FrameAndWidget(ViewportFrameWeak);

	if (FrameAndWidget.IsValid() && FrameAndWidget.ViewportWidget->CanChangeGridSize())
	{
		FrameAndWidget.ViewportWidget->ExecuteSetGridSize(InNewValue, false);
	}
}

void SAvaLevelViewportStatusBarButtons::OnGridSizeCommitted(int32 InNewValue, ETextCommit::Type InCommitType) const
{
	const FAvaLevelViewportGuideFrameAndWidget FrameAndWidget(ViewportFrameWeak);

	if (FrameAndWidget.IsValid() && FrameAndWidget.ViewportWidget->CanChangeGridSize())
	{
		FrameAndWidget.ViewportWidget->ExecuteSetGridSize(InNewValue, true);
	}
}

bool SAvaLevelViewportStatusBarButtons::GetViewportInfoEnabled() const
{
	using namespace UE::AvaLevelViewport::Private;

	const FAvaLevelViewportGuideFrameAndClient FrameAndClient(ViewportFrameWeak);

	if (FrameAndClient.IsValid())
	{
		if (FEditorModeTools* ModeTools = FrameAndClient.ViewportClient->GetModeTools())
		{
			return ModeTools->GetToolkitHost().IsValid();
		}
	}

	return false;
}

FSlateColor SAvaLevelViewportStatusBarButtons::GetViewportInfoColor() const
{
	using namespace UE::AvaLevelViewport::Private;

	const FAvaLevelViewportGuideFrameAndClient FrameAndClient(ViewportFrameWeak);

	if (FrameAndClient.IsValid())
	{
		if (FEditorModeTools* ModeTools = FrameAndClient.ViewportClient->GetModeTools())
		{
			if (ModeTools->GetToolkitHost().IsValid())
			{
				return ViewportStatusBarButton::EnabledColor;
			}
		}
	}

	return ViewportStatusBarButton::DisabledColor;
}

TSharedRef<SWidget> SAvaLevelViewportStatusBarButtons::GetViewportInfoWidget() const
{
	using namespace UE::AvaLevelViewport::Private;

	const FAvaLevelViewportGuideFrameAndClient FrameAndClient(ViewportFrameWeak);

	if (FrameAndClient.IsValid())
	{
		if (FEditorModeTools* ModeTools = FrameAndClient.ViewportClient->GetModeTools())
		{
			if (TSharedPtr<IToolkitHost> ToolkitHost = ModeTools->GetToolkitHost())
			{
				return SAvaViewportInfo::CreateInstance(ToolkitHost.ToSharedRef());
			}
		}
	}

	return SNullWidget::NullWidget;
}

bool SAvaLevelViewportStatusBarButtons::CanChangeGridSize() const
{
	const FAvaLevelViewportGuideFrameAndWidget FrameAndWidget(ViewportFrameWeak);

	if (FrameAndWidget.IsValid())
	{
		return FrameAndWidget.ViewportWidget->CanChangeGridSize();
	}

	return false;
}

FSlateColor SAvaLevelViewportStatusBarButtons::GetToggleOverlayColor() const
{
	using namespace UE::AvaLevelViewport::Private;

	const FAvaLevelViewportGuideFrameAndWidget FrameAndWidget(ViewportFrameWeak);

	if (FrameAndWidget.IsValid() && FrameAndWidget.ViewportWidget->CanToggleOverlay())
	{
		if (const UAvaViewportSettings* AvaViewportSettings = GetDefault<UAvaViewportSettings>())
		{
			return AvaViewportSettings->bEnableViewportOverlay ? ViewportStatusBarButton::ActiveColor : ViewportStatusBarButton::EnabledColor;
		}
	}

	return ViewportStatusBarButton::DisabledColor;
}

bool SAvaLevelViewportStatusBarButtons::GetToggleOverlayEnabled() const
{
	const FAvaLevelViewportGuideFrameAndWidget FrameAndWidget(ViewportFrameWeak);

	if (FrameAndWidget.IsValid())
	{
		return FrameAndWidget.ViewportWidget->CanToggleOverlay();
	}

	return false;
}

FReply SAvaLevelViewportStatusBarButtons::ToggleOverlay()
{
	const FAvaLevelViewportGuideFrameAndWidget FrameAndWidget(ViewportFrameWeak);

	if (FrameAndWidget.IsValid() && FrameAndWidget.ViewportWidget->CanToggleOverlay())
	{
		FrameAndWidget.ViewportWidget->ExecuteToggleOverlay();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FSlateColor SAvaLevelViewportStatusBarButtons::GetToggleBoundingBoxesColor() const
{
	using namespace UE::AvaLevelViewport::Private;

	const FAvaLevelViewportGuideFrameAndWidget FrameAndWidget(ViewportFrameWeak);

	if (FrameAndWidget.IsValid() && FrameAndWidget.ViewportWidget->CanToggleBoundingBox())
	{
		if (const UAvaViewportSettings* AvaViewportSettings = GetDefault<UAvaViewportSettings>())
		{
			return AvaViewportSettings->bEnableBoundingBoxes ? ViewportStatusBarButton::ActiveColor : ViewportStatusBarButton::EnabledColor;
		}
	}

	return ViewportStatusBarButton::DisabledColor;
}

bool SAvaLevelViewportStatusBarButtons::GetToggleBoundingBoxesEnabled() const
{
	const FAvaLevelViewportGuideFrameAndWidget FrameAndWidget(ViewportFrameWeak);

	if (FrameAndWidget.IsValid())
	{
		return FrameAndWidget.ViewportWidget->CanToggleBoundingBox();
	}

	return false;
}

FReply SAvaLevelViewportStatusBarButtons::ToggleBoundingBoxes()
{
	const FAvaLevelViewportGuideFrameAndWidget FrameAndWidget(ViewportFrameWeak);

	if (FrameAndWidget.IsValid())
	{
		FrameAndWidget.ViewportWidget->ExecuteToggleBoundingBox();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FSlateColor SAvaLevelViewportStatusBarButtons::GetToggleIsolateActorsColor() const
{
	using namespace UE::AvaLevelViewport::Private;

	const FAvaLevelViewportGuideFrameAndClient FrameAndClient(ViewportFrameWeak);

	if (FrameAndClient.IsValid() && FrameAndClient.ViewportClient->GetIsolateActorsOperation()->CanToggleIsolateActors())
	{
		return FrameAndClient.ViewportClient->GetIsolateActorsOperation()->IsIsolatingActors() ? ViewportStatusBarButton::ActiveColor : ViewportStatusBarButton::EnabledColor;
	}

	return ViewportStatusBarButton::DisabledColor;
}

bool SAvaLevelViewportStatusBarButtons::GetToggleIsolateActorsEnabled() const
{
	using namespace UE::AvaLevelViewport::Private;

	const FAvaLevelViewportGuideFrameAndClient FrameAndClient(ViewportFrameWeak);

	if (FrameAndClient.IsValid())
	{
		return FrameAndClient.ViewportClient->GetIsolateActorsOperation()->CanToggleIsolateActors();
	}

	return false;
}

FReply SAvaLevelViewportStatusBarButtons::ToggleIsolateActors()
{
	using namespace UE::AvaLevelViewport::Private;

	const FAvaLevelViewportGuideFrameAndClient FrameAndClient(ViewportFrameWeak);

	if (FrameAndClient.IsValid())
	{
		FrameAndClient.ViewportClient->GetIsolateActorsOperation()->ToggleIsolateActors();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FSlateColor SAvaLevelViewportStatusBarButtons::GetToggleSafeFramesColor() const
{
	using namespace UE::AvaLevelViewport::Private;

	const FAvaLevelViewportGuideFrameAndWidget FrameAndWidget(ViewportFrameWeak);

	if (FrameAndWidget.IsValid() && FrameAndWidget.ViewportWidget->CanToggleSafeFrames())
	{
		if (!FrameAndWidget.ViewportWidget->CanToggleSafeFrames())
		{
			return ViewportStatusBarButton::DisabledColor;
		}

		if (const UAvaViewportSettings* AvaViewportSettings = GetDefault<UAvaViewportSettings>())
		{
			return AvaViewportSettings->bSafeFramesEnabled ? ViewportStatusBarButton::ActiveColor : ViewportStatusBarButton::EnabledColor;
		}
	}

	return ViewportStatusBarButton::DisabledColor;
}

bool SAvaLevelViewportStatusBarButtons::GetToggleSafeFramesEnabled() const
{
	const FAvaLevelViewportGuideFrameAndWidget FrameAndWidget(ViewportFrameWeak);

	if (FrameAndWidget.IsValid())
	{
		return FrameAndWidget.ViewportWidget->CanToggleSafeFrames();
	}

	return false;
}

FReply SAvaLevelViewportStatusBarButtons::ToggleSafeFrames()
{
	const FAvaLevelViewportGuideFrameAndWidget FrameAndWidget(ViewportFrameWeak);

	if (FrameAndWidget.IsValid() && FrameAndWidget.ViewportWidget->CanToggleSafeFrames())
	{
		FrameAndWidget.ViewportWidget->ExecuteToggleSafeFrames();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FSlateColor SAvaLevelViewportStatusBarButtons::GetToggleGameViewColor() const
{
	using namespace UE::AvaLevelViewport::Private;

	const FAvaLevelViewportGuideFrameAndClient FrameAndClient(ViewportFrameWeak);

	if (FrameAndClient.IsValid())
	{
		return FrameAndClient.ViewportClient->IsInGameView() ? ViewportStatusBarButton::ActiveColor : ViewportStatusBarButton::EnabledColor;
	}

	return ViewportStatusBarButton::DisabledColor;
}

bool SAvaLevelViewportStatusBarButtons::GetToggleGameViewEnabled() const
{
	const FAvaLevelViewportGuideFrameAndClient FrameAndClient(ViewportFrameWeak);

	return FrameAndClient.IsValid();
}

FReply SAvaLevelViewportStatusBarButtons::ToggleGameView()
{
	const FAvaLevelViewportGuideFrameAndClient FrameAndClient(ViewportFrameWeak);

	if (FrameAndClient.IsValid())
	{
		const bool bNewGameModeValue = !FrameAndClient.ViewportClient->IsInGameView();

		FrameAndClient.ViewportClient->SetGameView(bNewGameModeValue);

		if (!bNewGameModeValue)
		{
			FrameAndClient.ViewportClient->ShowWidget(true);
		}

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE

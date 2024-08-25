// Copyright Epic Games, Inc. All Rights Reserved.

#include "SLiveLinkTimecode.h"

#include "UObject/NameTypes.h"

#include "EditorFontGlyphs.h"
#include "Features/IModularFeatures.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Internationalization/Text.h"
#include "Misc/App.h"
#include "UI/Widgets/SLiveLinkTimecode.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/STimecode.h"

#include "Clients/LiveLinkHubProvider.h"
#include "LiveLinkClient.h"
#include "LiveLinkHub.h"
#include "LiveLinkHubCommands.h"
#include "LiveLinkHubMessages.h"
#include "LiveLinkHubModule.h"

#define LOCTEXT_NAMESPACE "LiveLinkHub"

namespace UE::LiveLinkTimecode::Private
{
	static FName EnableTimecodeSourceId = TEXT("EnableTimeCodeSource");
	/** We only support a preset list of timecode values + named subjects. */
	static FName System24fps = TEXT("SystemTime24fps");
	static FName System30fps = TEXT("SystemTime30fps");
	static FName System60fps = TEXT("SystemTime60fps");
}

FSlateColor SLiveLinkTimecode::GetTimecodeStatusColor() const
{
	return bIsTimecodeSource ? FSlateColor(FColor::Green) : FSlateColor(FColor::Yellow);
}

FText SLiveLinkTimecode::GetTimecodeTooltip() const
{
	if (bIsTimecodeSource)
	{
		return LOCTEXT("LiveLinkTimeCode_IsConnected", "Sending timecode data to connected editors.");
	}
	return LOCTEXT("LiveLinkTimeCode_NotConnected", "No timecode data shared with connected editors.");
}

void SLiveLinkTimecode::OnEnableTimecodeToggled()
{
	bIsTimecodeSource = !bIsTimecodeSource;
	SendUpdatedTimecodeToEditor();
}

void SLiveLinkTimecode::SendUpdatedTimecodeToEditor()
{
	FLiveLinkHubTimecodeSettings Settings;
	using namespace UE::LiveLinkTimecode::Private;

	if (!bIsTimecodeSource)
	{
		Settings.Source = ELiveLinkHubTimecodeSource::NotDefined;
	}
	else if (ActiveTimecodeSource == System24fps)
	{
		Settings.Source = ELiveLinkHubTimecodeSource::SystemTimeEditor;
		Settings.DesiredFrameRate = FFrameRate(24,1);
	}
	else if (ActiveTimecodeSource == System30fps)
	{
		Settings.Source = ELiveLinkHubTimecodeSource::SystemTimeEditor;
		Settings.DesiredFrameRate = FFrameRate(30, 1);
	}
	else if (ActiveTimecodeSource == System60fps)
	{
		Settings.Source = ELiveLinkHubTimecodeSource::SystemTimeEditor;
		Settings.DesiredFrameRate = FFrameRate(60, 1);
	}
	else
	{
		Settings.Source = ELiveLinkHubTimecodeSource::UseSubjectName;
	}
	Settings.SubjectName = ActiveTimecodeSource;

	// Apply the timecode settings to the local instance too.
	Settings.AssignTimecodeSettingsAsProviderToEngine();

	const TSharedPtr<FLiveLinkHub> LiveLinkHub = FModuleManager::Get().GetModuleChecked<FLiveLinkHubModule>("LiveLinkHub").GetLiveLinkHub();
	if (LiveLinkHub.IsValid())
	{
		TSharedPtr<FLiveLinkHubProvider> Provider = LiveLinkHub->GetLiveLinkProvider();
		if (Provider.IsValid())
		{
			Provider->SetTimecodeSettings(MoveTemp(Settings));
		}
	}
}

void SLiveLinkTimecode::SetTimecodeSource(const FName SourceId)
{
	if (SourceId == ActiveTimecodeSource)
	{
		return;
	}

	ActiveTimecodeSource = SourceId;
	SendUpdatedTimecodeToEditor();
}

TSharedRef<SWidget> SLiveLinkTimecode::MakeMenu()
{
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, nullptr);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("LiveLinkHubTimecodeSource", "Enable Timecode Source"),
		LOCTEXT("LiveLinkHubTimecodeSource_Tooltip", "Make this Live Link Hub a time code source for connected editors."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SLiveLinkTimecode::OnEnableTimecodeToggled),
			FCanExecuteAction::CreateLambda([] { return true; }),
			FIsActionChecked::CreateLambda([this] { return bIsTimecodeSource; })),
		NAME_None,
		EUserInterfaceActionType::ToggleButton);


	MenuBuilder.BeginSection("LiveLinkHub.Timecode.TimecodeProvider", LOCTEXT("TimecodeProviderSection", "Timecode Provider"));

	auto GenerateUIAction = [this](const FName Id) -> FUIAction
	{
		// Generate a UI Action
		return FUIAction(
			FExecuteAction::CreateSP(this, &SLiveLinkTimecode::SetTimecodeSource, Id),
			FCanExecuteAction::CreateLambda([] { return true; }),
			FIsActionChecked::CreateLambda([this, Id] { return Id == ActiveTimecodeSource; }));
	};
	MenuBuilder.AddMenuEntry(
		LOCTEXT("LiveLinkHubTimecodeSource24fps", "System Time (24 fps)"),
		LOCTEXT("LiveLinkHubTimecodeSource24fps_Tooltip", "Use a 24 FPS time code based on system time."),
		FSlateIcon(),
		GenerateUIAction(UE::LiveLinkTimecode::Private::System24fps),
		NAME_None,
		EUserInterfaceActionType::Check);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("LiveLinkHubTimecodeSource30fps", "System Time (30 fps)"),
		LOCTEXT("LiveLinkHubTimecodeSource30fps_Tooltip", "Use a 30 FPS time code based on system time."),
		FSlateIcon(),
		GenerateUIAction(UE::LiveLinkTimecode::Private::System30fps),
		NAME_None,
		EUserInterfaceActionType::Check);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("LiveLinkHubTimecodeSource60fps", "System Time (60 fps)"),
		LOCTEXT("LiveLinkHubTimecodeSource60fps_Tooltip", "Use a 60 FPS time code based on system time."),
		FSlateIcon(),
		GenerateUIAction(UE::LiveLinkTimecode::Private::System60fps),
		NAME_None,
		EUserInterfaceActionType::Check);

	TArray<FName> Subjects;
	WorkingClient->GetSubjectNames(Subjects);
	for (FName Subject : Subjects)
	{
		FTextBuilder SubjectText;
		SubjectText.AppendLineFormat(LOCTEXT("LiveLinkHubTimecodeSourceSubject_Tooltip", "{0}'s timecode"), FText::FromName(Subject));
		MenuBuilder.AddMenuEntry(
			FText::FromName(Subject),
			SubjectText.ToText(),
			FSlateIcon(),
			GenerateUIAction(Subject),
			NAME_None,
			EUserInterfaceActionType::Check);
	}
	MenuBuilder.EndSection();
	return MenuBuilder.MakeWidget();
}

void SLiveLinkTimecode::Construct(const FArguments& InArgs)
{
	ActiveTimecodeSource = UE::LiveLinkTimecode::Private::System24fps;
	WorkingClient = (FLiveLinkClient*)&IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);

	check(WorkingClient);
	ChildSlot
	[
		SNew(SComboButton)
		.ContentPadding(FMargin(6.0f, 0.0f))
		.MenuPlacement(MenuPlacement_AboveAnchor)
		.OnGetMenuContent(this, &SLiveLinkTimecode::MakeMenu)
		.ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("SimpleComboButton"))
		.HasDownArrow(true)
		.ToolTipText(this, &SLiveLinkTimecode::GetTimecodeTooltip)
		.ButtonContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 3, 0)
			[
				SNew(STextBlock)
					.Font(FAppStyle::Get().GetFontStyle("FontAwesome.8"))
					.ColorAndOpacity(this, &SLiveLinkTimecode::GetTimecodeStatusColor)
					.Text(FEditorFontGlyphs::Circle)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2, 0, 10, 0)
			[
				SNew(STimecode)
				.DisplayLabel(false)
				.TimecodeFont(FCoreStyle::Get().GetFontStyle(TEXT("NormalText")))
				.Timecode(MakeAttributeLambda([]
				{
					return FApp::GetTimecode();
				}))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2, 0, 10, 0)
			[
				SNew(STextBlock)
				.Font(FCoreStyle::Get().GetFontStyle(TEXT("NormalText")))
				.Text(MakeAttributeLambda([] {return FApp::GetTimecodeFrameRate().ToPrettyText();}))
			]
    	]
	];
}

#undef LOCTEXT_NAMESPACE

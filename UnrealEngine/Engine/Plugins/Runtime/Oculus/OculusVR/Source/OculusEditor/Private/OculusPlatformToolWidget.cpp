// Copyright Epic Games, Inc. All Rights Reserved.
#include "OculusPlatformToolWidget.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "DesktopPlatformModule.h"
#include "Editor.h"
#include "Styling/AppStyle.h"
#include "Misc/FileHelper.h"
#include "Internationalization/Regex.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Images/SImage.h"
#include "HAL/FileManagerGeneric.h"
#include "DOM/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "OculusHMDModule.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Interfaces/IPluginManager.h"
#include "SHyperlinkLaunchURL.h"

#define LOCTEXT_NAMESPACE "OculusPlatformToolWidget"
#define TEXT_INDENT_OFFSET 20.0f

const FString UrlPlatformUtil = "https://www.oculus.com/download_app/?id=1076686279105243";
const FString ProjectPlatformUtilPath = "Oculus/Tools/ovr-platform-util.exe";

FText OculusPlatformDialogTitle = LOCTEXT("DownloadOculusPlatformUtility", "Download Oculus Platform Utility");
FText OculusPlatformDialogMessage = LOCTEXT("DownloadOculusPlatformUtilityMessage",
	"Oculus Platform Window would like to download the latest version of the Oculus Platform Utility."
	" Oculus Platform Utility is a command-line tool that enables the uploading of builds to your release channels on the Oculus Developer Dashboard."
	"\n\nYou can learn more about the Oculus Platform Utility at https://developer.oculus.com/distribute/publish-reference-platform-command-line-utility/"
	"\n\nCanceling will prevent the download and the UPLOAD button will be unfunctional. Would you like the tool to download the Oculus Platform Utility to your project?"
);

static bool bShowUploadDebugSymbols = false;

FString SOculusPlatformToolWidget::LogText;

PRAGMA_DISABLE_DEPRECATION_WARNINGS

SOculusPlatformToolWidget::SOculusPlatformToolWidget()
{
	LogTextUpdated = false;
	ActiveUploadButton = true;
	Options2DCollapsed = true;
	RequestUploadButtonActive = true;
	OptionsRedistPackagesCollapsed = true;

	EnableUploadButtonDel.BindRaw(this, &SOculusPlatformToolWidget::EnableUploadButton);
	UpdateLogTextDel.BindRaw(this, &SOculusPlatformToolWidget::UpdateLogText);
	SetProcessDel.BindRaw(this, &SOculusPlatformToolWidget::SetPlatformProcess);

	LoadConfigSettings();

	FOculusHMDModule::GetPluginWrapper().SendEvent2("oculus_platform_tool", "show_window", "integration");
}

void SOculusPlatformToolWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	// Update log text if it changes, otherwise constant updating will yeild the field unselectable.
	if (LogTextUpdated)
	{
		ToolConsoleLog->SetText(FText::FromString(LogText));
		LogTextUpdated = false;
	}

	if (RequestUploadButtonActive != ActiveUploadButton)
	{
		ActiveUploadButton = RequestUploadButtonActive;
		BuildButtonToolbar(ButtonToolbar);
	}
}

void SOculusPlatformToolWidget::Construct(const FArguments& InArgs)
{
	auto logTextBox = SNew(SMultiLineEditableTextBox).IsReadOnly(true);
	ToolConsoleLog = logTextBox;

	auto mainVerticalBox = SNew(SVerticalBox);
	GeneralSettingsBox = mainVerticalBox;

	auto buttonToolbarBox = SNew(SHorizontalBox);
	ButtonToolbar = buttonToolbarBox;

	auto optionalSettings = SNew(SVerticalBox);
	OptionalSettings = optionalSettings;

	auto expansionFilesSettings = SNew(SVerticalBox);
	ExpansionFilesSettings = expansionFilesSettings;

	BuildGeneralSettingsBox(GeneralSettingsBox);
	BuildButtonToolbar(ButtonToolbar);
	BuildExpansionFileBox(ExpansionFilesSettings);

	if (PlatformSettings != NULL)
	{
		if (PlatformSettings->GetTargetPlatform() == (uint8)EOculusPlatformTarget::Rift)
		{
			BuildRiftOptionalFields(OptionalSettings);
		}
		else
		{
			OptionalSettings.Get()->ClearChildren();
		}
	}

	FString ODHIconPath = IPluginManager::Get().FindPlugin(TEXT("OculusVR"))->GetBaseDir() / TEXT("Resources/odhIcon128.png");
	const FName BrushName(*ODHIconPath);
	FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(BrushName);
	ODHIconDynamicImageBrush = MakeShareable(new FSlateDynamicImageBrush(BrushName, FVector2D(60.0f, 60.0f)));

#if PLATFORM_MAC
	FString odhLink = "https://developer.oculus.com/downloads/package/oculus-developer-hub-mac/?source=unreal";
#else
	FString odhLink = "https://developer.oculus.com/downloads/package/oculus-developer-hub-win/?source=unreal";
#endif

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.LightGroupBorder"))
		.Padding(2)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().Padding(0, 0).FillHeight(1.f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SNew(SExpandableArea)
					.HeaderPadding(5)
					.Padding(5)
					.BorderBackgroundColor(FLinearColor(0.4f, 0.4f, 0.4f, 1.0f))
					.BodyBorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					.BodyBorderBackgroundColor(FLinearColor::White)
					.InitiallyCollapsed(false)
					.HeaderContent()
					[
						SNew(SRichTextBlock)
						.TextStyle(FAppStyle::Get(), "ToolBar.Heading")
						.DecoratorStyleSet(&FAppStyle::Get()).AutoWrapText(true)
						.Text(LOCTEXT("GeneralSettings", "<RichTextBlock.Bold>General Settings</>"))
					]
					.BodyContent()
					[
						mainVerticalBox
					]
				]
				+ SScrollBox::Slot()
				[
					SNew(SExpandableArea)
					.HeaderPadding(5)
					.Padding(5)
					.BorderBackgroundColor(FLinearColor(0.4f, 0.4f, 0.4f, 1.0f))
					.BodyBorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					.BodyBorderBackgroundColor(FLinearColor::White)
					.InitiallyCollapsed(true)
					.HeaderContent()
					[
						SNew(SRichTextBlock)
						.TextStyle(FAppStyle::Get(), "ToolBar.Heading")
						.DecoratorStyleSet(&FAppStyle::Get()).AutoWrapText(true)
						.Text(LOCTEXT("OptionalSettings", "<RichTextBlock.Bold>Optional Settings</>"))
					]
					.BodyContent()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							optionalSettings
						]
					]
				]
				+ SScrollBox::Slot()
				[
					SNew(SExpandableArea)
					.HeaderPadding(5)
					.Padding(5)
					.BorderBackgroundColor(FLinearColor(0.4f, 0.4f, 0.4f, 1.0f))
					.BodyBorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					.BodyBorderBackgroundColor(FLinearColor::White)
					.InitiallyCollapsed(true)
					.HeaderContent()
					[
						SNew(SRichTextBlock)
						.TextStyle(FAppStyle::Get(), "ToolBar.Heading")
						.DecoratorStyleSet(&FAppStyle::Get()).AutoWrapText(true)
						.Text(LOCTEXT("ExpansionFileSettings", "<RichTextBlock.Bold>Expansion Files</>"))
					]
					.BodyContent()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							expansionFilesSettings
						]
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				buttonToolbarBox
			]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					logTextBox
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SBox)
						.WidthOverride(60.0f)
						.HeightOverride(60.0f)
						[
							SNew(SImage)
							.Image(ODHIconDynamicImageBrush.IsValid() ? ODHIconDynamicImageBrush.Get() : nullptr)
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)
						[
							SNew(SRichTextBlock)
							.Text(LOCTEXT("ODHCallout",
								"<RichTextBlock.Bold>Oculus Developer Hub</> is a desktop companion tool that can upload builds, manage apps and reduce friction in daily Quest development."))
							.DecoratorStyleSet(&FAppStyle::Get())
							.AutoWrapText(true)
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SBox)
							.HAlign(HAlign_Left)
							[
								SNew(SHyperlinkLaunchURL, odhLink)
								.Text(LOCTEXT("ODHDownloadPage", "Download Oculus Developer Hub"))
								.ToolTipText(LOCTEXT("ODHDownloadPageTooltip", "Opens a page that provides the download link for Oculus Developer Hub"))
							]
						]
					]
				]
			]
		]
	];
}

void SOculusPlatformToolWidget::BuildGeneralSettingsBox(TSharedPtr<SVerticalBox> box)
{
	if (PlatformSettings == NULL)
	{
		return;
	}

	box.Get()->ClearChildren();

	BuildTextComboBoxField(GeneralSettingsBox, LOCTEXT("TargetPlatform", "Target Platform"),
		&OculusPlatforms, OculusPlatforms[PlatformSettings->GetTargetPlatform()],
		&SOculusPlatformToolWidget::OnPlatformSettingChanged);

	// Build field for Oculus Application ID.
	BuildTextField(box, LOCTEXT("AppID", "Oculus Application ID"), FText::FromString(PlatformSettings->GetApplicationID()), 
		LOCTEXT("AppIDTT", "Specifies the ID of your app. Obtained from the API tab of your app in the Oculus Dashboard."),
		&SOculusPlatformToolWidget::OnApplicationIDChanged);

	// Build field for Oculus Application Token.
	BuildTextField(box, LOCTEXT("AppToken", "Oculus Application Token"), FText::FromString(PlatformSettings->GetApplicationToken()),
		LOCTEXT("AppTokenTT", "Specifies the app secret token. Obtained from the API tab of your app in the Oculus Dashboard."),
		&SOculusPlatformToolWidget::OnApplicationTokenChanged, true);

	// Build field for Release Channel.
	BuildTextField(box, LOCTEXT("ReleaseChannel", "Release Channel"), FText::FromString(PlatformSettings->GetReleaseChannel()),
		LOCTEXT("ReleaseChannelTT", "Specifies the release channel for uploading the build. Release channel names are not case-sensitive."),
		&SOculusPlatformToolWidget::OnReleaseChannelChanged);

	// Build field for Release Notes.
	BuildTextField(box, LOCTEXT("ReleaseNote", "Release Note"), FText::FromString(PlatformSettings->GetReleaseNote()),
		LOCTEXT("ReleaseNoteTT", "Specifies the release note text shown to users."),
		&SOculusPlatformToolWidget::OnReleaseNoteChanged);

	// Platform specific fields.
	if (PlatformSettings->GetTargetPlatform() == (uint8)EOculusPlatformTarget::Rift)
	{
		// Build field for Rift Build Directory.
		BuildFileDirectoryField(box, LOCTEXT("BuildPath", "Rift Build Directory"), FText::FromString(PlatformSettings->OculusRiftBuildDirectory),
			LOCTEXT("BuildPathTT", "Specifies the full path to the directory containing your build files."),
			&SOculusPlatformToolWidget::OnSelectRiftBuildDirectory, &SOculusPlatformToolWidget::OnClearRiftBuildDirectory);

		// Build field for Build Version.
		BuildTextField(box, LOCTEXT("BuildVersion", "Build Version"), FText::FromString(PlatformSettings->OculusRiftBuildVersion),
			LOCTEXT("BuildVersionTT", "Specifies the version number shown to users."),
			&SOculusPlatformToolWidget::OnRiftBuildVersionChanged);

		// Build field for Launch File Path.
		BuildFileDirectoryField(box, LOCTEXT("LaunchPath", "Launch File Path"), FText::FromString(PlatformSettings->GetLaunchFilePath()),
			LOCTEXT("LaunchPathTT", " Specifies the path to the executable that launches your app."),
			&SOculusPlatformToolWidget::OnSelectLaunchFilePath, &SOculusPlatformToolWidget::OnClearLaunchFilePath);
	}
	else
	{
		// Build field for APK File Path.
		BuildFileDirectoryField(box, LOCTEXT("APKLaunchPath", "APK File Path"), FText::FromString(PlatformSettings->GetLaunchFilePath()),
			LOCTEXT("APKLaunchPathTT", " Specifies the path to the APK that launches your app."),
			&SOculusPlatformToolWidget::OnSelectLaunchFilePath, &SOculusPlatformToolWidget::OnClearLaunchFilePath);

		BuildCheckBoxField(box, LOCTEXT("UploadDebugSymbols", "Upload Debug Symbols"), PlatformSettings->UploadDebugSymbols,
			LOCTEXT("UploadDebugSymbolsTT", "If checked, debug symbols will be uploaded along with the application."),
			&SOculusPlatformToolWidget::OnUploadDebugSymbolsChanged);

		if (PlatformSettings->UploadDebugSymbols)
		{
			if (bShowUploadDebugSymbols != PlatformSettings->UploadDebugSymbols)
			{
				if (PlatformSettings->GetSymbolDirPath().IsEmpty())
				{
					FString defaultPath = GenerateSymbolPath();
					PlatformSettings->SetSymbolDirPath(FPaths::ConvertRelativePathToFull(defaultPath));
					PlatformSettings->SaveConfig();
				}
			}

			// Build field for Debug symbol directory path.
			BuildFileDirectoryField(box, LOCTEXT("SymbolPath", "Symbol Directory Path"), FText::FromString(PlatformSettings->GetSymbolDirPath()),
				LOCTEXT("SymbolPathTT", "Specifies the path to the directory containing the app symbols (libUE4.so)."),
				&SOculusPlatformToolWidget::OnSelectSymbolDirPath, &SOculusPlatformToolWidget::OnClearSymbolDirPath, 1);

			BuildCheckBoxField(box, LOCTEXT("DebugSymbolsOnly", "Upload Debug Symbols Only"), PlatformSettings->DebugSymbolsOnly,
				LOCTEXT("DebugSymbolsOnlyTT", "If checked, the tool will upload onyl debug symbols to an existing build. Requires Build ID, App ID, App Token, and Debug Symbols Directory."),
				&SOculusPlatformToolWidget::OnDebugSymbolsOnlyChanged, 1);

			if (PlatformSettings->DebugSymbolsOnly)
			{
				BuildTextField(box, LOCTEXT("BuildID", "Build ID"), FText::FromString(PlatformSettings->BuildID),
					LOCTEXT("BuildIDTT", "Specifies the Build ID to upload debug symbols to."),
					&SOculusPlatformToolWidget::OnBuildIDChanged, false, 1);
			}
		}
		bShowUploadDebugSymbols = PlatformSettings->UploadDebugSymbols;
	}
}

void SOculusPlatformToolWidget::BuildTextField(TSharedPtr<SVerticalBox> box, FText name, FText text, FText tooltip, 
	PTextComittedDel deleg, bool isPassword, int32 indentAmount)
{
	FMargin textMargin = FMargin(TEXT_INDENT_OFFSET * indentAmount, 1.0f, 1.0f, 1.0f);

	box.Get()->AddSlot()
	.Padding(1)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().Padding(1).AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(250.f)
			.Padding(textMargin)
			[
				SNew(STextBlock)
				.Text(name)
				.ToolTipText(tooltip)
			]
		]
		+ SHorizontalBox::Slot().Padding(1).FillWidth(1.f)
		[
			SNew(SEditableTextBox)
			.Text(text)
			.IsPassword(isPassword)
			.OnTextCommitted(this, deleg)
		]
	];
}

void SOculusPlatformToolWidget::BuildTextComboBoxField(TSharedPtr<SVerticalBox> box, FText name, 
	TArray<TSharedPtr<FString>>* options, TSharedPtr<FString> current, PTextComboBoxDel deleg, int32 indentAmount)
{
	FMargin textMargin = FMargin(TEXT_INDENT_OFFSET * indentAmount, 1.0f, 1.0f, 1.0f);

	box.Get()->AddSlot()
	.Padding(1)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().Padding(1).AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(250.f).Padding(textMargin)
			[
				SNew(SRichTextBlock)
				.DecoratorStyleSet(&FAppStyle::Get())
				.Text(name)
			]
		]
		+ SHorizontalBox::Slot().Padding(1).FillWidth(1.f)
		[
			SNew(STextComboBox)
			.OptionsSource(options)
			.InitiallySelectedItem(current)
			.OnSelectionChanged(this, deleg)
		]
	];
}

void SOculusPlatformToolWidget::BuildCheckBoxField(TSharedPtr<SVerticalBox> box, FText name, bool check, 
	FText tooltip, PCheckBoxChangedDel deleg, int32 indentAmount)
{
	FMargin textMargin = FMargin(TEXT_INDENT_OFFSET * indentAmount, 1.0f, 1.0f, 1.0f);

	box.Get()->AddSlot()
	.Padding(1)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().Padding(1).AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(250.f).Padding(textMargin)
			[
				SNew(SRichTextBlock)
				.DecoratorStyleSet(&FAppStyle::Get())
				.Text(name)
			]
		]
		+ SHorizontalBox::Slot().Padding(1).FillWidth(1.f)
		[
			SNew(SCheckBox)
			.OnCheckStateChanged(this, deleg)
			.IsChecked(check ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
		]
	];
}

void SOculusPlatformToolWidget::BuildFileDirectoryField(TSharedPtr<SVerticalBox> box, FText name, FText path, FText tooltip, 
	PButtonClickedDel deleg, PButtonClickedDel clearDeleg, int32 indentAmount)
{
	EVisibility cancelButtonVisibility = path.IsEmpty() ? EVisibility::Hidden : EVisibility::Visible;
	FMargin textMargin = FMargin(TEXT_INDENT_OFFSET * indentAmount, 1.0f, 1.0f, 1.0f);

	box.Get()->AddSlot()
	.Padding(1)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().Padding(1).AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(250.f).Padding(textMargin)
			[
				SNew(STextBlock)
				.Text(name)
				.ToolTipText(tooltip)
			]
		]
		+ SHorizontalBox::Slot().Padding(1).FillWidth(1.f)
		[
			SNew(SEditableText)
			.Text(path)
			.IsReadOnly(true)
			.Justification(ETextJustify::Left)
		]
		+ SHorizontalBox::Slot().Padding(1).AutoWidth().HAlign(EHorizontalAlignment::HAlign_Right)
		[
			SNew(SButton)
			.Text(FText::FromString("X"))
			.Visibility(cancelButtonVisibility)
			.OnClicked(this, clearDeleg)
			.ButtonColorAndOpacity(FLinearColor(0.36f, 0.1f, 0.05f))
		]
		+ SHorizontalBox::Slot().Padding(1).AutoWidth().HAlign(EHorizontalAlignment::HAlign_Right)
		[
			SNew(SButton)
			.Text((LOCTEXT("Choose", "Choose...")))
			.OnClicked(this, deleg)
		]
	];
}

void SOculusPlatformToolWidget::BuildButtonToolbar(TSharedPtr<SHorizontalBox> box)
{
	box.Get()->ClearChildren();

	box.Get()->AddSlot().FillWidth(1.f);
	box.Get()->AddSlot().AutoWidth().Padding(2.f)
	[
		SNew(SButton)
		.Text((LOCTEXT("Upload", "Upload")))
		.OnClicked(this, &SOculusPlatformToolWidget::OnStartPlatformUpload)
		.IsEnabled(ActiveUploadButton)
	];
	box.Get()->AddSlot().AutoWidth().Padding(2.f)
	[
		SNew(SButton)
		.Text((LOCTEXT("Cancel", "Cancel")))
		.OnClicked(this, &SOculusPlatformToolWidget::OnCancelUpload)
		.IsEnabled(!ActiveUploadButton)
	];
	box.Get()->AddSlot().FillWidth(1.f);
}

void SOculusPlatformToolWidget::BuildRiftOptionalFields(TSharedPtr<SVerticalBox> box)
{
	if (PlatformSettings == NULL)
	{
		return;
	}

	box.Get()->ClearChildren();

	// Add Launch Parameter Field
	BuildTextField(box, LOCTEXT("LaunchParams", "Launch Parameters"), FText::FromString(PlatformSettings->OculusRiftLaunchParams),
		LOCTEXT("LaunchParamsTT", ""),
		&SOculusPlatformToolWidget::OnRiftLaunchParamsChanged);

	// Add Firewall Exception Toggle
	BuildCheckBoxField(box, LOCTEXT("Firewall", "Firewall Exception"), PlatformSettings->OculusRiftFireWallException,
		LOCTEXT("FirewallTT", ""),
		&SOculusPlatformToolWidget::OnRiftFirewallChanged);

	// Add Gamepad Emulation Dropdown
	BuildTextComboBoxField(box, LOCTEXT("GamepadEmu", "Gamepad Emulation"),
		&RiftGamepadEmulation, RiftGamepadEmulation[(uint8)PlatformSettings->GetRiftGamepadEmulation()],
		&SOculusPlatformToolWidget::OnRiftGamepadEmulationChanged);
	
	// Generate 2D Settings Expandable Area
	TSharedRef<SVerticalBox> settings2DBox = SNew(SVerticalBox);

	// Add 2D Launch File Field
	BuildFileDirectoryField(settings2DBox, LOCTEXT("2DLaunch", "2D Launch File"), FText::FromString(PlatformSettings->OculusRift2DLaunchPath),
		LOCTEXT("2DLaunchPathTT", ""),
		&SOculusPlatformToolWidget::OnSelect2DLaunchPath, &SOculusPlatformToolWidget::OnClear2DLaunchPath);

	// Add 2D Launch Parameter Field
	BuildTextField(settings2DBox, LOCTEXT("2DLaunchParams", "2D Launch Parameters"), FText::FromString(PlatformSettings->OculusRift2DLaunchParams),
		LOCTEXT("2DLaunchParamsTT", ""),
		&SOculusPlatformToolWidget::On2DLaunchParamsChanged);

	box.Get()->AddSlot().AutoHeight().Padding(1)
	[
		SNew(SExpandableArea)
		.HeaderPadding(5)
		.Padding(5)
		.BorderBackgroundColor(FLinearColor(0.4f, 0.4f, 0.4f, 1.0f))
		.BodyBorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.BodyBorderBackgroundColor(FLinearColor::White)
		.InitiallyCollapsed(Options2DCollapsed)
		.OnAreaExpansionChanged(this, &SOculusPlatformToolWidget::On2DOptionsExpanded)
		.HeaderContent()
		[
			SNew(SRichTextBlock)
			.TextStyle(FAppStyle::Get(), "ToolBar.Heading")
			.DecoratorStyleSet(&FAppStyle::Get()).AutoWrapText(true)
			.Text(LOCTEXT("2DSettings", "<RichTextBlock.Bold>2D Settings</>"))
		]
		.BodyContent()
		[
			settings2DBox
		]
	];

	BuildRedistPackagesBox(box);
}

void SOculusPlatformToolWidget::BuildRedistPackagesBox(TSharedPtr<SVerticalBox> box)
{
	// Create check box toggle for each redistributable package we loaded
	TSharedRef<SVerticalBox> redistBox = SNew(SVerticalBox);
	for (int i = 0; i < PlatformSettings->OculusRedistPackages.Num(); i++)
	{
		FRedistPackage* Package = &PlatformSettings->OculusRedistPackages[i];
		redistBox->AddSlot()
		.Padding(1)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().Padding(1).AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(250.f)
				[
					SNew(SRichTextBlock)
					.DecoratorStyleSet(&FAppStyle::Get())
					.Text(FText::FromString(Package->Name))
				]
			]
			+ SHorizontalBox::Slot().Padding(1).FillWidth(1.f)
			[
				SNew(SCheckBox)
				.OnCheckStateChanged(this, &SOculusPlatformToolWidget::OnRedistPackageStateChanged, Package)
				.IsChecked(Package->Included ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			]
		];
	}

	box.Get()->AddSlot().AutoHeight().Padding(1)
	[
		SNew(SExpandableArea)
		.HeaderPadding(5)
		.Padding(5)
		.BorderBackgroundColor(FLinearColor(0.4f, 0.4f, 0.4f, 1.0f))
		.BodyBorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.BodyBorderBackgroundColor(FLinearColor::White)
		.InitiallyCollapsed(OptionsRedistPackagesCollapsed)
		.OnAreaExpansionChanged(this, &SOculusPlatformToolWidget::OnRedistPackagesExpanded)
		.HeaderContent()
		[
			SNew(SRichTextBlock)
			.TextStyle(FAppStyle::Get(), "ToolBar.Heading")
			.DecoratorStyleSet(&FAppStyle::Get()).AutoWrapText(true)
			.Text(LOCTEXT("RedistPack", "<RichTextBlock.Bold>Redistributable Packages</>"))
		]
		.BodyContent()
		[
			redistBox
		]
	];
}

void SOculusPlatformToolWidget::BuildExpansionFileBox(TSharedPtr<SVerticalBox> box)
{
	if (PlatformSettings == NULL)
	{
		return;
	}

	ExpansionFilesSettings.Get()->ClearChildren();

	if (PlatformSettings->GetTargetPlatform() == (uint8)EOculusPlatformTarget::Rift)
	{
		BuildFileDirectoryField(box, LOCTEXT("LanguagePacks", "Language Packs Directory"), FText::FromString(PlatformSettings->GetLanguagePacksPath()),
			LOCTEXT("LanguagePacksTT", ""), &SOculusPlatformToolWidget::OnSelectLanguagePacksPath, &SOculusPlatformToolWidget::OnClearLanguagePacksPath);
	}

	BuildFileDirectoryField(box, LOCTEXT("ExpansionFilesDirectory", "Expansion Files Directory"), FText::FromString(PlatformSettings->GetExpansionFilesPath()),
		LOCTEXT("ExpansionFilesTT", ""), &SOculusPlatformToolWidget::OnSelectExpansionFilesPath, &SOculusPlatformToolWidget::OnClearExpansionFilesPath);

	TArray<FAssetConfig>* AssetConfigs = PlatformSettings->GetAssetConfigs();
	if (AssetConfigs)
	{
		for (int i = 0; i < AssetConfigs->Num(); i++)
		{
			auto AssetConfigBox = SNew(SVerticalBox);
			BuildAssetConfigBox(AssetConfigBox, (*AssetConfigs)[i], i);

			box.Get()->AddSlot().AutoHeight().Padding(1)
			[
				SNew(SExpandableArea)
				.HeaderPadding(5)
				.Padding(5)
				.BorderBackgroundColor(FLinearColor(0.4f, 0.4f, 0.4f, 1.0f))
				.BodyBorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.BodyBorderBackgroundColor(FLinearColor::White)
				.HeaderContent()
				[
					SNew(SRichTextBlock)
					.TextStyle(FAppStyle::Get(), "ToolBar.Heading")
					.DecoratorStyleSet(&FAppStyle::Get()).AutoWrapText(true)
					.Text(FText::FromString((*AssetConfigs)[i].Name))
				]
				.BodyContent()
				[
					AssetConfigBox
				]
			];
		}
	}
}

void SOculusPlatformToolWidget::BuildAssetConfigBox(TSharedPtr<SVerticalBox> box, FAssetConfig config, int index)
{
	box.Get()->AddSlot()
	.Padding(1)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().Padding(1).AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(250.f)
			[
				SNew(SRichTextBlock)
				.DecoratorStyleSet(&FAppStyle::Get())
				.Text(LOCTEXT("AssetType", "Asset Type"))
			]
		]
		+ SHorizontalBox::Slot().Padding(1).FillWidth(1.f)
		[
			SNew(STextComboBox)
			.OptionsSource(&AssetType)
			.InitiallySelectedItem(AssetType[(uint8)config.AssetType])
			.OnSelectionChanged(this, &SOculusPlatformToolWidget::OnAssetConfigTypeChanged, index)
		]
	];

	box.Get()->AddSlot()
	.Padding(1)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().Padding(1).AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(250.f)
			[
				SNew(SRichTextBlock)
				.DecoratorStyleSet(&FAppStyle::Get())
				.Text(LOCTEXT("AssetRequired", "Required"))
			]
		]
		+ SHorizontalBox::Slot().Padding(1).FillWidth(1.f)
		[
			SNew(SCheckBox)
			.OnCheckStateChanged(this, &SOculusPlatformToolWidget::OnAssetConfigRequiredChanged, index)
			.IsChecked(config.Required ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
		]
	];

	box.Get()->AddSlot()
	.Padding(1)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().Padding(1).AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(250.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("SKU", "SKU"))
			]
		]
		+ SHorizontalBox::Slot().Padding(1).FillWidth(1.f)
		[
			SNew(SEditableTextBox)
			.Text(FText::FromString(config.Sku))
			.OnTextCommitted(this, &SOculusPlatformToolWidget::OnAssetConfigSKUChanged, index)
		]
	];
}

bool SOculusPlatformToolWidget::ConstructArguments(FString& args)
{
	if (PlatformSettings == NULL)
	{
		return false;
	}

	if (PlatformSettings->UploadDebugSymbols && PlatformSettings->DebugSymbolsOnly)
	{
		return ConstructDebugSymbolArguments(args);
	}

	// Build the args string that will be passed to the CLI. Print all errors that occur to the log.
	bool success = true;

	switch (PlatformSettings->GetTargetPlatform())
	{
		case (uint8)EOculusPlatformTarget::Rift:
			args = "upload-rift-build";
			break;
		case (uint8)EOculusPlatformTarget::Quest:
			args = "upload-quest-build";
			break;
		default:
			UpdateLogText(LogText + "ERROR: Invalid target platform selected");
			success = false;
			break;
	}

	// Oculus Application ID check and command.
	ValidateTextField(&SOculusPlatformToolWidget::IDFieldValidator, PlatformSettings->GetApplicationID(),
		LOCTEXT("ApplicationID", "Application ID").ToString(), success);
	args += " --app_id \"" + PlatformSettings->GetApplicationID() + "\"";

	// Oculus Application Token check and command.
	ValidateTextField(&SOculusPlatformToolWidget::GenericFieldValidator, PlatformSettings->GetApplicationToken(),
		LOCTEXT("ApplicationToken", "Application Token").ToString(), success);
	args += " --app_secret \"" + PlatformSettings->GetApplicationToken() + "\"";

	// Release Channel check and command.
	ValidateTextField(&SOculusPlatformToolWidget::GenericFieldValidator, PlatformSettings->GetReleaseChannel(),
		LOCTEXT("ReleaseChannel", "Release Channel").ToString(), success);
	args += " --channel \"" + PlatformSettings->GetReleaseChannel() + "\"";

	// Release Note check and command. Not a required command.
	if (!PlatformSettings->GetReleaseNote().IsEmpty())
	{
		FString SanatizedReleaseNote = PlatformSettings->GetReleaseNote();
		SanatizedReleaseNote = SanatizedReleaseNote.Replace(TEXT("\""), TEXT("\"\""));
		args += " --notes \"" + SanatizedReleaseNote + "\"";
	}

	// Platform specific commands
	if (PlatformSettings->GetTargetPlatform() == (uint8)EOculusPlatformTarget::Rift)
	{
		// Launch File Path check and command.
		ValidateTextField(&SOculusPlatformToolWidget::FileFieldValidator, PlatformSettings->GetLaunchFilePath(), 
			LOCTEXT("LaunchFile", "Launch File Path").ToString(), success);
		args += " --launch-file \"" + PlatformSettings->GetLaunchFilePath() + "\"";

		// Rift Build Directory check and command.
		ValidateTextField(&SOculusPlatformToolWidget::DirectoryFieldValidator, PlatformSettings->OculusRiftBuildDirectory,
			LOCTEXT("RiftBuildDir", "Rift Build Directory").ToString(), success);
		args += " --build_dir \"" + PlatformSettings->OculusRiftBuildDirectory + "\"";

		// Rift Build Version check and command.
		ValidateTextField(&SOculusPlatformToolWidget::GenericFieldValidator, PlatformSettings->OculusRiftBuildVersion,
			LOCTEXT("BuildVersion", "Build Version").ToString(), success);
		args += " --version \"" + PlatformSettings->OculusRiftBuildVersion + "\"";

		// Rift Launch Parameters check and command
		if (!PlatformSettings->OculusRiftLaunchParams.IsEmpty())
		{
			ValidateTextField(&SOculusPlatformToolWidget::LaunchParamValidator, PlatformSettings->OculusRiftLaunchParams,
				LOCTEXT("LaunchParam", "Launch Parameters").ToString(), success);
			args += " --launch_params \"" + PlatformSettings->OculusRiftLaunchParams + "\"";
		}

		// Rift 2D Options checks and commands
		if (!PlatformSettings->OculusRift2DLaunchPath.IsEmpty())
		{
			ValidateTextField(&SOculusPlatformToolWidget::FileFieldValidator, PlatformSettings->OculusRift2DLaunchPath,
				LOCTEXT("2DLaunchFile", "2D Launch File Path").ToString(), success);
			args += " --launch_file_2d \"" + PlatformSettings->OculusRift2DLaunchPath + "\"";

			if (!PlatformSettings->OculusRift2DLaunchParams.IsEmpty())
			{
				ValidateTextField(&SOculusPlatformToolWidget::LaunchParamValidator, PlatformSettings->OculusRift2DLaunchParams,
					LOCTEXT("2DLaunchParams", "2D Launch Parameters").ToString(), success);
				args += " --launch_params_2d \"" + PlatformSettings->OculusRift2DLaunchParams + "\"";
			}
		}

		// Rift Firewall Exception command
		if (PlatformSettings->OculusRiftFireWallException)
		{
			args += " --firewall_exceptions true";
		}

		// Rift Gamepad Emulation command
		if (PlatformSettings->GetRiftGamepadEmulation() > EOculusGamepadEmulation::Off &&
			PlatformSettings->GetRiftGamepadEmulation() < EOculusGamepadEmulation::Length)
		{
			args += " --gamepad-emulation ";
			switch (PlatformSettings->GetRiftGamepadEmulation())
			{
				case EOculusGamepadEmulation::Twinstick: args += "TWINSTICK";   break;
				case EOculusGamepadEmulation::RightDPad: args += "RIGHT_D_PAD"; break;
				case EOculusGamepadEmulation::LeftDPad:  args += "LEFT_D_PAD";  break;
				default:								 args += "OFF";			break;
			}
		}

		// Rift Redistributable Packages commands
		TArray<FString> IncludedPackages;
		for (int i = 0; i < PlatformSettings->OculusRedistPackages.Num(); i++)
		{
			FRedistPackage Package = PlatformSettings->OculusRedistPackages[i];
			if (Package.Included)
			{
				IncludedPackages.Add(Package.Id);
			}
		}
		if (IncludedPackages.Num() > 0)
		{
			args += " --redistributables \"" + FString::Join(IncludedPackages, TEXT(",")) + "\"";
		}
	}
	else
	{
		// APK File Path check and command.
		ValidateTextField(&SOculusPlatformToolWidget::FileFieldValidator, PlatformSettings->GetLaunchFilePath(),
			LOCTEXT("APKLaunchFile", "APK File Path").ToString(), success);
		args += " --apk \"" + PlatformSettings->GetLaunchFilePath() + "\"";

		if (PlatformSettings->UploadDebugSymbols)
		{
			ValidateTextField(&SOculusPlatformToolWidget::DirectoryFieldValidator, PlatformSettings->GetSymbolDirPath(),
				LOCTEXT("SymbolDirPath", "Symbol Directory Path").ToString(), success);
			if (success)
			{
				args += " --debug-symbols-dir \"" + PlatformSettings->GetSymbolDirPath() + "\"";
			}
		}
	}

	if (!PlatformSettings->GetExpansionFilesPath().IsEmpty())
	{
		ValidateTextField(&SOculusPlatformToolWidget::DirectoryFieldValidator, PlatformSettings->GetExpansionFilesPath(),
			LOCTEXT("ExpansionFilesPath", "Expansion Files Path").ToString(), success);
		args += " --assets-dir \"" + PlatformSettings->GetExpansionFilesPath() + "\"";

		TArray<FAssetConfig>* AssetConfigs = PlatformSettings->GetAssetConfigs();
		if (AssetConfigs->Num() > 0)
		{
			TArray<FString> AssetConfig;
			for (int i = 0; i < AssetConfigs->Num(); i++)
			{
				TArray<FString> ConfigParams;
				FAssetConfig Config = (*AssetConfigs)[i];

				if (Config.Required)
				{
					ConfigParams.Add("\\\"required\\\":true");
				}
				if (Config.AssetType > EOculusAssetType::Default && Config.AssetType < EOculusAssetType::Length)
				{
					FString command = "\\\"type\\\":";
					switch (Config.AssetType)
					{
						case EOculusAssetType::Store:
							ConfigParams.Add(command + "\\\"STORE\\\"");
							break;
						case EOculusAssetType::Language_Pack:
							ConfigParams.Add(command + "\\\"LANGUAGE_PACK\\\"");
							break;
						default:
							ConfigParams.Add(command + "\\\"DEFAULT\\\"");
							break;
					}
				}
				if (!Config.Sku.IsEmpty())
				{
					ConfigParams.Add("\\\"sku\\\":\\\"" + Config.Sku + "\\\"");
				}

				if (ConfigParams.Num() > 0)
				{
					FString ConfigCommand = "\\\"" + Config.Name + "\\\":{" + FString::Join(ConfigParams, TEXT(",")) + "}";
					AssetConfig.Add(ConfigCommand);
				}
			}

			if (AssetConfig.Num())
			{
				args += " --asset_files_config {" + FString::Join(AssetConfig, TEXT(",")) + "}";
			}
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("%s"), *args);
	return success;
}

bool SOculusPlatformToolWidget::ConstructDebugSymbolArguments(FString& args)
{
	bool success = true;
	args = "upload-debug-symbols";

	ValidateTextField(&SOculusPlatformToolWidget::IDFieldValidator, PlatformSettings->BuildID,
		LOCTEXT("BuildID", "Build ID").ToString(), success);
	args += " --parent \"" + PlatformSettings->BuildID + "\"";

	// Oculus Application ID check and command.
	ValidateTextField(&SOculusPlatformToolWidget::IDFieldValidator, PlatformSettings->GetApplicationID(),
		LOCTEXT("ApplicationID", "Application ID").ToString(), success);
	args += " --app_id \"" + PlatformSettings->GetApplicationID() + "\"";

	// Oculus Application Token check and command.
	ValidateTextField(&SOculusPlatformToolWidget::GenericFieldValidator, PlatformSettings->GetApplicationToken(),
		LOCTEXT("ApplicationToken", "Application Token").ToString(), success);
	args += " --app_secret \"" + PlatformSettings->GetApplicationToken() + "\"";

	ValidateTextField(&SOculusPlatformToolWidget::DirectoryFieldValidator, PlatformSettings->GetSymbolDirPath(),
		LOCTEXT("SymbolDirPath", "Symbol Directory Path").ToString(), success);
	args += " --debug-symbols-dir \"" + PlatformSettings->GetSymbolDirPath() + "\"";
	args += " --debug-symbols-pattern \"*.so\"";

	return success;
}

void SOculusPlatformToolWidget::EnableUploadButton(bool enabled)
{
	RequestUploadButtonActive = enabled;
}

void SOculusPlatformToolWidget::LoadConfigSettings()
{
	PlatformSettings = GetMutableDefault<UDEPRECATED_UOculusPlatformToolSettings>();
	PlatformEnum = StaticEnum<EOculusPlatformTarget>();
	GamepadEmulationEnum = StaticEnum<EOculusGamepadEmulation>();
	AssetTypeEnum = StaticEnum<EOculusAssetType>();

	RiftGamepadEmulation.Empty();
	OculusPlatforms.Empty();
	for (uint8 i = 0; i < (uint8)EOculusPlatformTarget::Length; i++)
	{
		OculusPlatforms.Add(MakeShareable(new FString(PlatformEnum->GetDisplayNameTextByIndex((int64)i).ToString())));
	}
	for (uint8 i = 0; i < (uint8)EOculusGamepadEmulation::Length; i++)
	{
		RiftGamepadEmulation.Add(MakeShareable(new FString(GamepadEmulationEnum->GetDisplayNameTextByIndex((int64)i).ToString())));
	}
	for (uint8 i = 0; i < (uint8)EOculusAssetType::Length; i++)
	{
		AssetType.Add(MakeShareable(new FString(AssetTypeEnum->GetDisplayNameTextByIndex((int64)i).ToString())));
	}

	LoadRedistPackages();
}

void SOculusPlatformToolWidget::LoadRedistPackages()
{
	(new FAsyncTask<FPlatformLoadRedistPackagesTask>(UpdateLogTextDel))->StartBackgroundTask();
}

FReply SOculusPlatformToolWidget::OnStartPlatformUpload()
{
	FString launchArgs;

	UpdateLogText("");
	FOculusHMDModule::GetPluginWrapper().SendEvent2("oculus_platform_tool", "upload", "integration");
	if (ConstructArguments(launchArgs))
	{
		UpdateLogText(LogText + LOCTEXT("StartUpload", "Starting Platform Tool Upload Process . . .\n").ToString());
		(new FAsyncTask<FPlatformUploadTask>(launchArgs, EnableUploadButtonDel, UpdateLogTextDel, SetProcessDel))->StartBackgroundTask();
	}
	return FReply::Handled();
}

void SOculusPlatformToolWidget::OnPlatformSettingChanged(TSharedPtr<FString> ItemSelected, ESelectInfo::Type SelectInfo)
{
	if (!ItemSelected.IsValid())
	{
		return;
	}

	for (uint8 i = 0; i < (uint8)EOculusPlatformTarget::Length; i++)
	{
		if (PlatformEnum->GetDisplayNameTextByIndex(i).EqualTo(FText::FromString(*ItemSelected)))
		{
			if (PlatformSettings != NULL)
			{
				PlatformSettings->SetTargetPlatform(i);
				PlatformSettings->SaveConfig();

				LoadConfigSettings();
				BuildGeneralSettingsBox(GeneralSettingsBox);
				BuildExpansionFileBox(ExpansionFilesSettings);

				OptionalSettings.Get()->ClearChildren();
				if (i == (uint8)EOculusPlatformTarget::Rift)
				{
					BuildRiftOptionalFields(OptionalSettings);
				}
				break;
			}
		}
	}
}

void SOculusPlatformToolWidget::OnApplicationIDChanged(const FText& InText, ETextCommit::Type InCommitType)
{
	if (PlatformSettings != NULL)
	{
		PlatformSettings->SetApplicationID(InText.ToString());
		PlatformSettings->SaveConfig();
	}
}

void SOculusPlatformToolWidget::OnApplicationTokenChanged(const FText& InText, ETextCommit::Type InCommitType)
{
	if (PlatformSettings != NULL)
	{
		PlatformSettings->SetApplicationToken(InText.ToString());
		PlatformSettings->SaveConfig();
	}
}

void SOculusPlatformToolWidget::OnReleaseChannelChanged(const FText& InText, ETextCommit::Type InCommitType)
{
	if (PlatformSettings != NULL)
	{
		PlatformSettings->SetReleaseChannel(InText.ToString());
		PlatformSettings->SaveConfig();
	}
}

void SOculusPlatformToolWidget::OnReleaseNoteChanged(const FText& InText, ETextCommit::Type InCommitType)
{
	if (PlatformSettings != NULL)
	{
		PlatformSettings->SetReleaseNote(InText.ToString());
		PlatformSettings->SaveConfig();
	}
}

void SOculusPlatformToolWidget::OnRiftBuildVersionChanged(const FText& InText, ETextCommit::Type InCommitType)
{
	if (PlatformSettings != NULL)
	{
		PlatformSettings->OculusRiftBuildVersion = InText.ToString();
		PlatformSettings->SaveConfig();
	}
}

void SOculusPlatformToolWidget::OnRiftLaunchParamsChanged(const FText& InText, ETextCommit::Type InCommitType)
{
	if (PlatformSettings != NULL)
	{
		PlatformSettings->OculusRiftLaunchParams = InText.ToString();
		PlatformSettings->SaveConfig();
	}
}

void SOculusPlatformToolWidget::On2DLaunchParamsChanged(const FText& InText, ETextCommit::Type InCommitType)
{
	if (PlatformSettings != NULL)
	{
		PlatformSettings->OculusRift2DLaunchParams = InText.ToString();
		PlatformSettings->SaveConfig();
	}
}

void SOculusPlatformToolWidget::OnRiftFirewallChanged(ECheckBoxState CheckState)
{
	if (PlatformSettings != NULL)
	{
		PlatformSettings->OculusRiftFireWallException = CheckState == ECheckBoxState::Checked ? true : false;
		PlatformSettings->SaveConfig();
	}
}

void SOculusPlatformToolWidget::OnRedistPackageStateChanged(ECheckBoxState CheckState, FRedistPackage* Package)
{
	if (PlatformSettings != NULL)
	{
		Package->Included = CheckState == ECheckBoxState::Checked;
		PlatformSettings->SaveConfig();
		BuildRiftOptionalFields(OptionalSettings);
	}
}

void SOculusPlatformToolWidget::OnAssetConfigTypeChanged(TSharedPtr<FString> ItemSelected, ESelectInfo::Type SelectInfo, int i)
{
	if (PlatformSettings != NULL)
	{
		TArray<FAssetConfig>* AssetConfigs = PlatformSettings->GetAssetConfigs();
		for (int e = 0; e < (uint8)EOculusAssetType::Length; e++)
		{
			if (AssetTypeEnum->GetDisplayNameTextByIndex(e).ToString().Equals(*ItemSelected.Get()))
			{
				(*AssetConfigs)[i].AssetType = (EOculusAssetType)e;
				break;
			}
		}

		PlatformSettings->SaveConfig();
		BuildExpansionFileBox(ExpansionFilesSettings);
	}
}

void SOculusPlatformToolWidget::OnAssetConfigRequiredChanged(ECheckBoxState CheckState, int i)
{
	if (PlatformSettings != NULL)
	{
		TArray<FAssetConfig>* AssetConfigs = PlatformSettings->GetAssetConfigs();
		(*AssetConfigs)[i].Required = CheckState == ECheckBoxState::Checked;

		PlatformSettings->SaveConfig();
		BuildExpansionFileBox(ExpansionFilesSettings);
	}
}

void SOculusPlatformToolWidget::OnAssetConfigSKUChanged(const FText& InText, ETextCommit::Type InCommitType, int i)
{
	if (PlatformSettings != NULL)
	{
		TArray<FAssetConfig>* AssetConfigs = PlatformSettings->GetAssetConfigs();
		(*AssetConfigs)[i].Sku = InText.ToString();

		PlatformSettings->SaveConfig();
		BuildExpansionFileBox(ExpansionFilesSettings);
	}
}

void SOculusPlatformToolWidget::OnUploadDebugSymbolsChanged(ECheckBoxState CheckState)
{
	if (PlatformSettings != NULL)
	{
		PlatformSettings->UploadDebugSymbols = CheckState == ECheckBoxState::Checked ? true : false;
		PlatformSettings->SaveConfig();

		BuildGeneralSettingsBox(GeneralSettingsBox);
	}
}

void SOculusPlatformToolWidget::OnDebugSymbolsOnlyChanged(ECheckBoxState CheckState)
{
	if (PlatformSettings != NULL)
	{
		PlatformSettings->DebugSymbolsOnly = CheckState == ECheckBoxState::Checked ? true : false;
		PlatformSettings->SaveConfig();

		BuildGeneralSettingsBox(GeneralSettingsBox);
	}
}

void SOculusPlatformToolWidget::OnBuildIDChanged(const FText& InText, ETextCommit::Type InCommitType)
{
	if (PlatformSettings != NULL)
	{
		PlatformSettings->BuildID = InText.ToString();
		PlatformSettings->SaveConfig();
	}
}

void SOculusPlatformToolWidget::OnRiftGamepadEmulationChanged(TSharedPtr<FString> ItemSelected, ESelectInfo::Type SelectInfo)
{
	if (!ItemSelected.IsValid())
	{
		return;
	}

	for (uint8 i = 0; i < (uint8)EOculusGamepadEmulation::Length; i++)
	{
		if (GamepadEmulationEnum->GetDisplayNameTextByIndex(i).EqualTo(FText::FromString(*ItemSelected)))
		{
			if (PlatformSettings != NULL)
			{
				PlatformSettings->SetRiftGamepadEmulation(i);
				PlatformSettings->SaveConfig();
				break;
			}
		}
	}
}

FReply SOculusPlatformToolWidget::OnSelectRiftBuildDirectory()
{
	TSharedPtr<SWindow> parentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
	const void* parentWindowHandle = (parentWindow.IsValid() && parentWindow->GetNativeWindow().IsValid()) ? parentWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr;

	if (PlatformSettings != NULL)
	{
		FString path;
		FString defaultPath = PlatformSettings->OculusRiftBuildDirectory.IsEmpty() ? FPaths::ProjectContentDir() : PlatformSettings->OculusRiftBuildDirectory;
		if (FDesktopPlatformModule::Get()->OpenDirectoryDialog(parentWindowHandle, "Choose Rift Build Directory", defaultPath, path))
		{
			PlatformSettings->OculusRiftBuildDirectory = path;
			PlatformSettings->SaveConfig();
			BuildGeneralSettingsBox(GeneralSettingsBox);
		}
	}
	return FReply::Handled();
}

FReply SOculusPlatformToolWidget::OnClearRiftBuildDirectory()
{
	if (PlatformSettings != NULL)
	{
		PlatformSettings->OculusRiftBuildDirectory.Empty();
		PlatformSettings->SaveConfig();
		BuildGeneralSettingsBox(GeneralSettingsBox);
	}
	return FReply::Handled();
}

FReply SOculusPlatformToolWidget::OnSelectLaunchFilePath()
{
	TSharedPtr<SWindow> parentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
	const void* parentWindowHandle = (parentWindow.IsValid() && parentWindow->GetNativeWindow().IsValid()) ? parentWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr;

	if (PlatformSettings != NULL)
	{
		TArray<FString> path;
		FString defaultPath = PlatformSettings->GetLaunchFilePath().IsEmpty() ? FPaths::ProjectContentDir() : PlatformSettings->GetLaunchFilePath();
		FString fileType = PlatformSettings->GetTargetPlatform() == (uint8)EOculusPlatformTarget::Rift ? "Executables (*.exe)|*.exe" : "APKs (*.apk)|*.apk";
		if (FDesktopPlatformModule::Get()->OpenFileDialog(parentWindowHandle, "Choose Launch File", defaultPath, defaultPath, fileType, EFileDialogFlags::None, path))
		{
			if (path.Num() > 0)
			{
				PlatformSettings->SetLaunchFilePath(FPaths::ConvertRelativePathToFull(path[0]));
			}
			PlatformSettings->SaveConfig();
			BuildGeneralSettingsBox(GeneralSettingsBox);
		}
	}
	return FReply::Handled();
}

FReply SOculusPlatformToolWidget::OnClearLaunchFilePath()
{
	if (PlatformSettings != NULL)
	{
		PlatformSettings->SetLaunchFilePath("");
		PlatformSettings->SaveConfig();
		BuildGeneralSettingsBox(GeneralSettingsBox);
	}
	return FReply::Handled();
}

FString SOculusPlatformToolWidget::GenerateSymbolPath()
{
	return FPaths::ProjectDir() + TEXT("Binaries/Android/") + FApp::GetProjectName() + TEXT("_Symbols_v1/") + FApp::GetProjectName() + TEXT("-arm64");
}


FReply SOculusPlatformToolWidget::OnSelectSymbolDirPath()
{
	TSharedPtr<SWindow> parentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
	const void* parentWindowHandle = (parentWindow.IsValid() && parentWindow->GetNativeWindow().IsValid()) ? parentWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr;

	if (PlatformSettings != NULL)
	{
		FString dirPath;
		FString defaultPath = PlatformSettings->GetSymbolDirPath().IsEmpty() ? GenerateSymbolPath() : PlatformSettings->GetSymbolDirPath();
		if (FDesktopPlatformModule::Get()->OpenDirectoryDialog(parentWindowHandle, "Choose Launch File", defaultPath, dirPath))
		{
			PlatformSettings->SetSymbolDirPath(FPaths::ConvertRelativePathToFull(dirPath));
			PlatformSettings->SaveConfig();
			BuildGeneralSettingsBox(GeneralSettingsBox);
		}
	}
	return FReply::Handled();
}

FReply SOculusPlatformToolWidget::OnClearSymbolDirPath()
{
	if (PlatformSettings != NULL)
	{
		PlatformSettings->SetSymbolDirPath("");
		PlatformSettings->SaveConfig();
		BuildGeneralSettingsBox(GeneralSettingsBox);
	}
	return FReply::Handled();
}

FReply SOculusPlatformToolWidget::OnSelect2DLaunchPath()
{

	if (PlatformSettings != NULL)
	{
		TSharedPtr<SWindow> parentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
		const void* parentWindowHandle = (parentWindow.IsValid() && parentWindow->GetNativeWindow().IsValid()) ? parentWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr;
		TArray<FString> path;
		FString defaultPath = PlatformSettings->OculusRift2DLaunchPath.IsEmpty() ? FPaths::ProjectContentDir() : PlatformSettings->OculusRift2DLaunchPath;
		if (FDesktopPlatformModule::Get()->OpenFileDialog(parentWindowHandle, "Choose 2D Launch File", defaultPath, defaultPath, "Executables (*.exe)|*.exe", EFileDialogFlags::None, path))
		{
			if (path.Num() > 0)
			{
				PlatformSettings->OculusRift2DLaunchPath = FPaths::ConvertRelativePathToFull(path[0]);
			}
			PlatformSettings->SaveConfig();
			BuildRiftOptionalFields(OptionalSettings);
		}
	}
	return FReply::Handled();
}

FReply SOculusPlatformToolWidget::OnClear2DLaunchPath()
{
	if (PlatformSettings != NULL)
	{
		PlatformSettings->OculusRift2DLaunchPath.Empty();
		PlatformSettings->SaveConfig();
		BuildRiftOptionalFields(OptionalSettings);
	}
	return FReply::Handled();
}

FReply SOculusPlatformToolWidget::OnCancelUpload()
{
	if (FMessageDialog::Open(EAppMsgType::OkCancel, LOCTEXT("CancelUploadWarning", "Are you sure you want to cancel the upload process?")) == EAppReturnType::Ok)
	{
		if (PlatformProcess.IsValid())
		{
			FPlatformProcess::TerminateProc(PlatformProcess);
			UpdateLogText(LogText + LOCTEXT("UploadCancel", "Upload process was canceled.").ToString());
		}
	}
	return FReply::Handled();
}

FReply SOculusPlatformToolWidget::OnSelectLanguagePacksPath()
{

	if (PlatformSettings != NULL)
	{
		TSharedPtr<SWindow> parentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
		const void* parentWindowHandle = (parentWindow.IsValid() && parentWindow->GetNativeWindow().IsValid()) ? parentWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr;
		FString path;
		FString defaultPath = PlatformSettings->GetLanguagePacksPath().IsEmpty() ? FPaths::ProjectContentDir() : PlatformSettings->GetLanguagePacksPath();
		if (FDesktopPlatformModule::Get()->OpenDirectoryDialog(parentWindowHandle, "Choose Language Packs Directory", defaultPath, path))
		{
			PlatformSettings->SetLanguagePacksPath(path);
			PlatformSettings->SaveConfig();
			BuildExpansionFileBox(ExpansionFilesSettings);
		}
	}
	return FReply::Handled();
}

FReply SOculusPlatformToolWidget::OnClearLanguagePacksPath()
{
	if (PlatformSettings != NULL)
	{
		PlatformSettings->SetLanguagePacksPath("");
		PlatformSettings->SaveConfig();
		BuildExpansionFileBox(ExpansionFilesSettings);
	}
	return FReply::Handled();
}

FReply SOculusPlatformToolWidget::OnSelectExpansionFilesPath()
{

	if (PlatformSettings != NULL)
	{
		TSharedPtr<SWindow> parentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
		const void* parentWindowHandle = (parentWindow.IsValid() && parentWindow->GetNativeWindow().IsValid()) ? parentWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr;
		FString path;
		FString defaultPath = PlatformSettings->GetExpansionFilesPath().IsEmpty() ? FPaths::ProjectContentDir() : PlatformSettings->GetExpansionFilesPath();
		if (FDesktopPlatformModule::Get()->OpenDirectoryDialog(parentWindowHandle, "Choose Expansion Files Directory", defaultPath, path))
		{
			if (!path.Equals(PlatformSettings->GetExpansionFilesPath()))
			{
				if (!path.IsEmpty() && FPaths::DirectoryExists(path))
				{
					TArray<FString> Files;
					//FFileManagerGeneric::Get().FindFilesRecursive(Files, *path, TEXT("*.*"), true, false, false);
					IFileManager::Get().FindFiles(Files, *path);

					TArray<FAssetConfig>* AssetConfigs = PlatformSettings->GetAssetConfigs();
					for (int i = 0; i < Files.Num(); i++)
					{
						FAssetConfig AssetConfig;
						AssetConfig.Name = Files[i];
						AssetConfigs->Push(AssetConfig);
					}

					PlatformSettings->SetExpansionFilesPath(path);
					PlatformSettings->SaveConfig();
					BuildExpansionFileBox(ExpansionFilesSettings);
				}
			}
		}
	}
	return FReply::Handled();
}

FReply SOculusPlatformToolWidget::OnClearExpansionFilesPath()
{
	if (PlatformSettings != NULL)
	{
		PlatformSettings->SetExpansionFilesPath("");
		PlatformSettings->GetAssetConfigs()->Empty();
		PlatformSettings->SaveConfig();
		BuildExpansionFileBox(ExpansionFilesSettings);
	}
	return FReply::Handled();
}

void SOculusPlatformToolWidget::ValidateTextField(PFieldValidatorDel del, FString text, FString name, bool& success)
{
	FString error = "";
	FFieldValidatorDel fieldValidator;

	// Check the given field with the given field validator and print the error if it fails.
	fieldValidator.BindSP(this, del);
	if (!fieldValidator.Execute(text, error))
	{
		FString errorMessage = LOCTEXT("Error", "ERROR: Please verify that the {0} is correct. ").ToString();
		errorMessage = FString::Format(*errorMessage, { name });
		UpdateLogText(LogText + errorMessage + (error.IsEmpty() ? "\n" : error + "\n"));
		success = false;
	}
}

bool SOculusPlatformToolWidget::GenericFieldValidator(FString text, FString& error)
{
	if (text.IsEmpty())
	{
		error = LOCTEXT("FieldEmpty", "The field is empty.").ToString();
		return false;
	}
	return true;
}

bool SOculusPlatformToolWidget::IDFieldValidator(FString text, FString& error)
{
	const FRegexPattern RegExPat(TEXT("^[0-9]+$"));
	FRegexMatcher RegMatcher(RegExPat, text);

	if (!GenericFieldValidator(text, error))
	{
		return false;
	}
	else if (!RegMatcher.FindNext())
	{
		error = LOCTEXT("InvalidChar", "The field contains invalid characters.").ToString();
		return false;
	}
	return true;
}

bool SOculusPlatformToolWidget::DirectoryFieldValidator(FString text, FString& error)
{
	if (!GenericFieldValidator(text, error))
	{
		return false;
	}
	if (!FPaths::DirectoryExists(text))
	{
		error = LOCTEXT("DirectoryNull", "The directory does not exist.").ToString();
		return false;
	}
	return true;
}

bool SOculusPlatformToolWidget::FileFieldValidator(FString text, FString& error)
{
	if (!GenericFieldValidator(text, error))
	{
		return false;
	}
	if (!FPaths::FileExists(text))
	{
		error = LOCTEXT("FileNull", "The file does not exist.").ToString();
		return false;
	}
	return true;
}

bool SOculusPlatformToolWidget::LaunchParamValidator(FString text, FString& error)
{
	if (text.Contains("\""))
	{
		error = LOCTEXT("LaunchParamError", "The field contains illegal characters.").ToString();
		return false;
	}
	return true;
}

void SOculusPlatformToolWidget::On2DOptionsExpanded(bool bExpanded)
{
	Options2DCollapsed = !bExpanded;
}

void SOculusPlatformToolWidget::OnRedistPackagesExpanded(bool bExpanded)
{
	OptionsRedistPackagesCollapsed = !bExpanded;
}

void SOculusPlatformToolWidget::UpdateLogText(FString text)
{
	// Make sure that log text updating happens on the right thread.
	LogText = text;
	LogTextUpdated = true;
}

void SOculusPlatformToolWidget::SetPlatformProcess(FProcHandle proc)
{
	PlatformProcess = proc;
}

//=======================================================================================
//FPlatformDownloadTask

FPlatformDownloadTask::FPlatformDownloadTask(FUpdateLogTextDel textDel, FEvent* saveEvent)
{
	UpdateLogText = textDel;
	SaveCompleteEvent = saveEvent;

	FOculusHMDModule::GetPluginWrapper().SendEvent2("oculus_platform_tool", "provision_util", "integration");
}

void FPlatformDownloadTask::DoWork()
{
	// Create HTTP request for downloading oculus platform tool
	downloadCompleteEvent = FGenericPlatformProcess::GetSynchEventFromPool(false);
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> httpRequest = FHttpModule::Get().CreateRequest();

	httpRequest->OnProcessRequestComplete().BindRaw(this, &FPlatformDownloadTask::OnDownloadRequestComplete);
	httpRequest->OnRequestProgress().BindRaw(this, &FPlatformDownloadTask::OnRequestDownloadProgress);
	httpRequest->SetURL(UrlPlatformUtil);
	httpRequest->SetVerb("GET");

	httpRequest->ProcessRequest();

	UpdateLogText.Execute(SOculusPlatformToolWidget::LogText + LOCTEXT("DownloadProgress", "Downloading Platform Tool: {0}%\n").ToString());
	ToolConsoleLog = SOculusPlatformToolWidget::LogText;
	UpdateProgressLog(0);
	
	// Wait for download to complete
	downloadCompleteEvent->Wait();

	// Save HTTP data
	FString fullPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()) + ProjectPlatformUtilPath;
	if (FFileHelper::SaveArrayToFile(httpData, *fullPath))
	{
		UpdateLogText.Execute(SOculusPlatformToolWidget::LogText + LOCTEXT("DownloadSuccess", "Platform tool successfully downloaded.\n").ToString());
	}
	else
	{
		UpdateLogText.Execute(SOculusPlatformToolWidget::LogText + LOCTEXT("DownloadError", "An error has occured with downloading the platform tool.\n").ToString());
	}

	if (SaveCompleteEvent != NULL)
	{
		SaveCompleteEvent->Trigger();
	}
}

void FPlatformDownloadTask::UpdateProgressLog(int progress)
{
	UpdateLogText.Execute(FString::Format(*ToolConsoleLog, { progress }));
}

void FPlatformDownloadTask::OnRequestDownloadProgress(FHttpRequestPtr HttpRequest, int32 BytesSend, int32 InBytesReceived)
{
	// Update progress on download in tool console log
	FHttpResponsePtr httpResponse = HttpRequest->GetResponse();
	if (httpResponse.IsValid())
	{
		int progress = ((float)InBytesReceived / (float)httpResponse->GetContentLength()) * 100;
		UpdateProgressLog(progress);
	}
}

void FPlatformDownloadTask::OnDownloadRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	// Extract data from HTTP response and trigger download complete event
	if (bSucceeded && HttpResponse.IsValid())
	{
		httpData = HttpResponse->GetContent();
		downloadCompleteEvent->Trigger();
	}
}

//=======================================================================================
//FPlatformUploadTask

FPlatformUploadTask::FPlatformUploadTask(FString args, FEnableUploadButtonDel del, FUpdateLogTextDel textDel, FSetProcessDel procDel)
{
	LaunchArgs = args;
	EnableUploadButton = del;
	UpdateLogText = textDel;
	SetProcess = procDel;

	EnableUploadButton.Execute(false);
}

void FPlatformUploadTask::DoWork()
{
	// Check if the platform tool exists in the project directory. If not, start process to download it.
	if (!FPaths::FileExists(FPaths::ProjectContentDir() + ProjectPlatformUtilPath))
	{
		FEvent* PlatformToolCreatedEvent = FGenericPlatformProcess::GetSynchEventFromPool(false);

		UpdateLogText.Execute(SOculusPlatformToolWidget::LogText + LOCTEXT("NoCLI", "Unable to find Oculus Platform Utility.\n").ToString());
		EAppReturnType::Type dialogChoice = FMessageDialog::Open(EAppMsgType::OkCancel, OculusPlatformDialogMessage, &OculusPlatformDialogTitle);
		if (dialogChoice == EAppReturnType::Ok)
		{
			UpdateLogText.Execute(SOculusPlatformToolWidget::LogText + LOCTEXT("DownloadCLI", "Downloading Oculus Platform Utility . . .\n").ToString());
			(new FAsyncTask<FPlatformDownloadTask>(UpdateLogText, PlatformToolCreatedEvent))->StartBackgroundTask();
			PlatformToolCreatedEvent->Wait();
		}
		else
		{
			return;
		}

		UpdateLogText.Execute(SOculusPlatformToolWidget::LogText + LOCTEXT("StartUploadAfterDownload", "Starting upload . . .\n").ToString());
	}
	 
	// Start up the CLI and pass in arguments.
	FPlatformProcess::CreatePipe(ReadPipe, WritePipe);
	FProcHandle PlatformProcess = FPlatformProcess::CreateProc(*(FPaths::ProjectContentDir() + ProjectPlatformUtilPath), *LaunchArgs, false, true, true, nullptr, 0, nullptr, WritePipe, ReadPipe);
	SetProcess.Execute(PlatformProcess);

	// Redirect CLI output to the tool's log.
	while (FPlatformProcess::IsProcRunning(PlatformProcess))
	{
		FString log = FPlatformProcess::ReadPipe(ReadPipe);
		if (!log.IsEmpty())
		{
			// Remove parts of the log that contain escape character codes
			int32 escapeIndex = log.Find("\u001b");
			while (escapeIndex >= 0)
			{
				int32 lineEndIndex = log.Find("\n", ESearchCase::IgnoreCase, ESearchDir::FromStart, escapeIndex);
				if (lineEndIndex < 0) // If an escape character code exists without a new line end, just remove the escape character
				{
					lineEndIndex = escapeIndex + 1;
				}
				log.RemoveAt(escapeIndex, lineEndIndex - escapeIndex);
				escapeIndex = log.Find("\u001b");
			}
			UpdateLogText.Execute(SOculusPlatformToolWidget::LogText + log);
		}
	}
	EnableUploadButton.Execute(true);
}

//=======================================================================================
//FPlatformLoadRedistPackagesTask

FPlatformLoadRedistPackagesTask::FPlatformLoadRedistPackagesTask(FUpdateLogTextDel textDel)
{
	UpdateLogText = textDel;
}

void FPlatformLoadRedistPackagesTask::DoWork()
{
	UDEPRECATED_UOculusPlatformToolSettings* PlatformSettings = GetMutableDefault<UDEPRECATED_UOculusPlatformToolSettings>();

	// Check to see if the CLI exists, we need this to load avalible redist packages
	if (!FPaths::FileExists(FPaths::ProjectContentDir() + ProjectPlatformUtilPath))
	{
		UpdateLogText.Execute(SOculusPlatformToolWidget::LogText + LOCTEXT("LoadRedist", "Loading redistributable packages . . .\n").ToString());

		FEvent* PlatformToolCreatedEvent = FGenericPlatformProcess::GetSynchEventFromPool(false);

		UpdateLogText.Execute(SOculusPlatformToolWidget::LogText + LOCTEXT("NoCLI", "Unable to find Oculus Platform Utility.\n").ToString());
		EAppReturnType::Type dialogChoice = FMessageDialog::Open(EAppMsgType::OkCancel, OculusPlatformDialogMessage, &OculusPlatformDialogTitle);
		if (dialogChoice == EAppReturnType::Ok)
		{
			UpdateLogText.Execute(SOculusPlatformToolWidget::LogText + LOCTEXT("DownloadCLI", "Downloading Oculus Platform Utility . . .\n").ToString());
			(new FAsyncTask<FPlatformDownloadTask>(UpdateLogText, PlatformToolCreatedEvent))->StartBackgroundTask();
			PlatformToolCreatedEvent->Wait();
		}
		else
		{
			return;
		}
	}

	// Launch CLI and pass command to list out redist packages currently avalible
	TArray<FRedistPackage> LoadedPackages;
	FString Args = "list-redists";
	FPlatformProcess::CreatePipe(ReadPipe, WritePipe);
	FProcHandle PlatformProcess = FPlatformProcess::CreateProc(*(FPaths::ProjectContentDir() + ProjectPlatformUtilPath), *Args, false, true, true, nullptr, 0, nullptr, WritePipe, ReadPipe);

	// Load redist packages
	while (FPlatformProcess::IsProcRunning(PlatformProcess))
	{
		FString log = FPlatformProcess::ReadPipe(ReadPipe);
		if (!log.IsEmpty() && !log.Contains("\u001b") && !log.Contains("ID"))
		{
			TArray<FString> Packages;
			log.ParseIntoArrayLines(Packages);
			if (Packages.Num() > 0)
			{
				for (int i = 0; i < Packages.Num(); i++)
				{
					FString id, name;
					Packages[i].Split("|", &id, &name);

					if (!id.IsEmpty() && !name.IsEmpty())
					{
						FRedistPackage newPackage;
						newPackage.Name = name;
						newPackage.Id = id;

						LoadedPackages.Add(newPackage);
					}
				}
			}
		}
	}

	// Check to see if our stored copy of redist packages is outdated
	if (PlatformSettings != NULL)
	{
		if (LoadedPackages.Num() > PlatformSettings->OculusRedistPackages.Num())
		{
			PlatformSettings->OculusRedistPackages = LoadedPackages;
			PlatformSettings->SaveConfig();
			UpdateLogText.Execute(SOculusPlatformToolWidget::LogText + LOCTEXT("FinishRedistLoad", "Finished updating redistributable packages.\n").ToString());
		}
	}
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

#undef LOCTEXT_NAMESPACE
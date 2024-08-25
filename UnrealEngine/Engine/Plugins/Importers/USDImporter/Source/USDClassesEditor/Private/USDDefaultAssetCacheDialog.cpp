// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDDefaultAssetCacheDialog.h"

#include "USDAssetCache2.h"
#include "USDAssetCacheFactory.h"
#include "USDProjectSettings.h"

#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "CoreGlobals.h"
#include "CoreMinimal.h"
#include "Editor.h"
#include "EditorDirectories.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IContentBrowserSingleton.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "UObject/GCObjectScopeGuard.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "USDDefaultAssetCacheDialog"

namespace UE::DefaultCacheDialog::Private
{
	UUsdAssetCache2* CreateNewAssetCacheWithDialog()
	{
		UUsdAssetCache2* Result = nullptr;

		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		UUsdAssetCacheFactory* Factory = NewObject<UUsdAssetCacheFactory>();

		// Copied from UAssetToolsImpl::CreateAssetWithDialog, but modified so that we can set our own dialog title
		if (Factory)
		{
			// Determine the starting path. Try to use the most recently used directory
			FString AssetPath;

			const FString DefaultFilesystemDirectory = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::NEW_ASSET);
			if (DefaultFilesystemDirectory.IsEmpty() || !FPackageName::TryConvertFilenameToLongPackageName(DefaultFilesystemDirectory, AssetPath))
			{
				// No saved path, just use the game content root
				AssetPath = TEXT("/Game");
			}

			FString PackageName;
			FString AssetName;
			AssetTools.CreateUniqueAssetName(AssetPath / Factory->GetDefaultNewAssetName(), TEXT(""), PackageName, AssetName);

			FGCObjectScopeGuard DontGCFactory(Factory);

			FSaveAssetDialogConfig SaveAssetDialogConfig;
			SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("CreateAssetCacheDialogTitle", "Save a new USD Asset Cache");
			SaveAssetDialogConfig.DefaultPath = AssetPath;
			SaveAssetDialogConfig.DefaultAssetName = AssetName;
			SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;

			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);
			if (!SaveObjectPath.IsEmpty())
			{
				FEditorDelegates::OnConfigureNewAssetProperties.Broadcast(Factory);
				bool bCreateAsset = Factory->ConfigureProperties();

				if (bCreateAsset)
				{
					const FString SavePackageName = FPackageName::ObjectPathToPackageName(SaveObjectPath);
					const FString SavePackagePath = FPaths::GetPath(SavePackageName);
					const FString SaveAssetName = FPaths::GetBaseFilename(SavePackageName);
					FEditorDirectories::Get().SetLastDirectory(ELastDirectory::NEW_ASSET, SavePackagePath);

					const FName CallingContext = NAME_None;
					Result = Cast<UUsdAssetCache2>(
						AssetTools.CreateAsset(SaveAssetName, SavePackagePath, UUsdAssetCache2::StaticClass(), Factory, CallingContext)
					);
				}
			}
		}

		return Result;
	}
}	 // namespace UE::DefaultCacheDialog::Private

UUsdAssetCache2* SUsdDefaultAssetCacheDialog::GetCreatedCache()
{
	return ChosenCache;
}

void SUsdDefaultAssetCacheDialog::Construct(const FArguments& InArgs)
{
	ChosenCache = nullptr;
	Window = InArgs._WidgetWindow;
	DialogOutcome = EDefaultAssetCacheDialogOption::Cancel;

	FSlateFontInfo MessageFont(FAppStyle::GetFontStyle("StandardDialog.LargeFont"));

	// clang-format off
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(16.f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.FillHeight(1.0f)
			.MaxHeight(550)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Left)
				[
					SNew(SImage)
					.DesiredSizeOverride(FVector2D(24.f, 24.f))
					.Image(FAppStyle::Get().GetBrush("Icons.WarningWithColor.Large"))
				]

				+SHorizontalBox::Slot()
				.Padding(16.f, 0.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("DefaultAssetCacheDialogText", "Opening a USD Stage with a Stage Actor requires a default USD Asset Cache.\n\nThe cache will be used to store and share the generated assets across all USD Stage Actors.\n\nHover on the buttons for more info about each of the proposed options."))
					.Font(MessageFont)
					.WrapTextAt(650.0f)
				]
			]

			+SVerticalBox::Slot()
			.Padding(0.f, 32.f, 0.f, 0.f)
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(0)
				[
					SNew(SCheckBox)
					.IsChecked(ECheckBoxState::Unchecked)
					.OnCheckStateChanged_Lambda([](ECheckBoxState NewState)
					{
						if(UUsdProjectSettings* ProjectSettings = GetMutableDefault<UUsdProjectSettings>())
						{
							ProjectSettings->bShowCreateDefaultAssetCacheDialog = NewState != ECheckBoxState::Checked;
							ProjectSettings->SaveConfig();
						}
					})
					[
						SNew(STextBlock)
						.WrapTextAt(615.0f)
						.Text(LOCTEXT("DontPromptAgainText", "Don't prompt again"))
					]
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.Padding(16.f, 0.f, 0.f, 0.f)
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FAppStyle::Get().GetMargin("StandardDialog.SlotPadding"))
					.MinDesiredSlotWidth(FAppStyle::Get().GetFloat("StandardDialog.MinDesiredSlotWidth"))
					.MinDesiredSlotHeight(FAppStyle::Get().GetFloat("StandardDialog.MinDesiredSlotHeight"))

					+SUniformGridPanel::Slot(0, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(LOCTEXT("UseExistingButtonText", "Pick an existing cache"))
						.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
						.ToolTipText(LOCTEXT("UseExistingButtonToolTip", "Opens a Content Browser window to pick an asset cache to use as the default.\nThe default cache can be later changed on the project settings."))
						.OnClicked(this, &SUsdDefaultAssetCacheDialog::OnUseExisting)
						.ButtonStyle(&FAppStyle::Get(), "PrimaryButton")
					]

					+SUniformGridPanel::Slot(1, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(LOCTEXT("CreateNewButtonText", "Create new cache"))
						.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
						.ToolTipText(LOCTEXT("CreateNewButtonToolTip", "Creates a brand new asset cache to be used as the default right now, and pick where to save it.\nThe default cache can be changed on the project settings."))
						.OnClicked(this, &SUsdDefaultAssetCacheDialog::OnCreateNew)
						.ButtonStyle(&FAppStyle::Get(), "Button")
					]

					+SUniformGridPanel::Slot(2, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(LOCTEXT("DontCreateButtonText", "Don't use a default cache"))
						.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
						.ToolTipText(LOCTEXT("DontCreateButtonToolTip", "Will use a temporary cache individual to this USD Stage Actor."))
						.OnClicked(this, &SUsdDefaultAssetCacheDialog::OnDontCreate)
						.ButtonStyle(&FAppStyle::Get(), "Button")
					]
				]
			]
		]
	];
	// clang-format on
}

bool SUsdDefaultAssetCacheDialog::SupportsKeyboardFocus() const
{
	return true;
}

FReply SUsdDefaultAssetCacheDialog::OnUseExisting()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	FOpenAssetDialogConfig Config;
	Config.DialogTitleOverride = LOCTEXT("ExistingCachePickerDialogTitle", "Pick USD Asset Cache to use as default");
	Config.bAllowMultipleSelection = false;
	Config.AssetClassNames = {UUsdAssetCache2::StaticClass()->GetClassPathName()};

	TArray<FAssetData> PickedAssets = ContentBrowserModule.Get().CreateModalOpenAssetDialog(Config);
	if (PickedAssets.Num() == 1)
	{
		ChosenCache = Cast<UUsdAssetCache2>(PickedAssets[0].GetAsset());
	}

	DialogOutcome = EDefaultAssetCacheDialogOption::PickExisting;
	if (Window.IsValid())
	{
		Window.Pin()->RequestDestroyWindow();
	}
	return FReply::Handled();
}

FReply SUsdDefaultAssetCacheDialog::OnCreateNew()
{
	ChosenCache = UE::DefaultCacheDialog::Private::CreateNewAssetCacheWithDialog();

	DialogOutcome = EDefaultAssetCacheDialogOption::CreateNew;
	if (Window.IsValid())
	{
		Window.Pin()->RequestDestroyWindow();
	}
	return FReply::Handled();
}

FReply SUsdDefaultAssetCacheDialog::OnDontCreate()
{
	DialogOutcome = EDefaultAssetCacheDialogOption::DontUseDefault;
	if (Window.IsValid())
	{
		Window.Pin()->RequestDestroyWindow();
	}
	return FReply::Handled();
}

FReply SUsdDefaultAssetCacheDialog::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		return OnDontCreate();
	}
	return FReply::Unhandled();
}

EDefaultAssetCacheDialogOption SUsdDefaultAssetCacheDialog::GetDialogOutcome() const
{
	return DialogOutcome;
}

#undef LOCTEXT_NAMESPACE

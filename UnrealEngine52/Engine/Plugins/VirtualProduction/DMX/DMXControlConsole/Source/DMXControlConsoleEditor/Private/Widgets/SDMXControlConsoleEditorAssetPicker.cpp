// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorAssetPicker.h"

#include "DMXControlConsole.h"
#include "Models/DMXControlConsoleEditorModel.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Commands/DMXControlConsoleEditorCommands.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorAssetPicker"

void SDMXControlConsoleEditorAssetPicker::Construct(const FArguments& InArgs)
{
	RegisterCommands();

	ChildSlot
		[
			SNew(SBox)
			.MinDesiredWidth(150.f)
			.Padding(2.0f)
			[
				SAssignNew(AssetComboButton, SComboButton)
				.OnGetMenuContent(this, &SDMXControlConsoleEditorAssetPicker::CreateMenu)
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text(this, &SDMXControlConsoleEditorAssetPicker::GetEditorConsoleName)
				]
			]
		];
}

void SDMXControlConsoleEditorAssetPicker::RegisterCommands()
{
	CommandList = MakeShared<FUICommandList>();

	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	CommandList->MapAction
	(
		FDMXControlConsoleEditorCommands::Get().SaveConsole,
		FExecuteAction::CreateUObject(EditorConsoleModel, &UDMXControlConsoleEditorModel::SaveConsole)
	);

	CommandList->MapAction
	(
		FDMXControlConsoleEditorCommands::Get().SaveConsoleAs,
		FExecuteAction::CreateUObject(EditorConsoleModel, &UDMXControlConsoleEditorModel::SaveConsoleAs)
	);
}

TSharedRef<SWidget> SDMXControlConsoleEditorAssetPicker::CreateMenu()
{
	ensureMsgf(CommandList.IsValid(), TEXT("Invalid command list for control console asset picker."));

	FMenuBuilder MenuBuilder(true, CommandList);

	// Display the loaded console name
	{
		const TSharedRef<SWidget> LoadedConsoleNameWidget =
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("NoBorder"))
			.Padding(20.f, 2.f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
					{
						return FText::Format(LOCTEXT("CurrentConsoleLabel", "Current Console: {0}"), GetEditorConsoleName());
					})
			];

		MenuBuilder.AddWidget(LoadedConsoleNameWidget, FText::GetEmpty());
	}

	// Separator
	{
		MenuBuilder.AddMenuSeparator();
	}

	// Save Console button
	{
		MenuBuilder.AddMenuEntry(FDMXControlConsoleEditorCommands::Get().SaveConsole,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FAppStyle::Get().GetStyleSetName(), "AssetEditor.SaveAsset")
		);
	}

	// Save Console As button
	{
		MenuBuilder.AddMenuEntry(FDMXControlConsoleEditorCommands::Get().SaveConsoleAs,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FAppStyle::Get().GetStyleSetName(), "AssetEditor.SaveAssetAs")
		);
	}

	// Asset picker
	MenuBuilder.BeginSection(NAME_None, LOCTEXT("LoadConsoleMenuSection", "Load Console"));
	{
		IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();

		FAssetPickerConfig AssetPickerConfig;
		{
			AssetPickerConfig.SelectionMode = ESelectionMode::Single;
			AssetPickerConfig.InitialAssetViewType = EAssetViewType::Column;
			AssetPickerConfig.bFocusSearchBoxWhenOpened = true;
			AssetPickerConfig.bAllowNullSelection = false;
			AssetPickerConfig.bShowBottomToolbar = true;
			AssetPickerConfig.bAutohideSearchBar = false;
			AssetPickerConfig.bAllowDragging = false;
			AssetPickerConfig.bCanShowClasses = false;
			AssetPickerConfig.bShowPathInColumnView = true;
			AssetPickerConfig.bShowTypeInColumnView = false;
			AssetPickerConfig.bSortByPathInColumnView = false;
			AssetPickerConfig.bForceShowEngineContent = false;
			AssetPickerConfig.bForceShowPluginContent = false;
			AssetPickerConfig.HiddenColumnNames.Add(TEXT("ItemDiskSize"));
			AssetPickerConfig.HiddenColumnNames.Add(TEXT("HasVirtualizedData"));

			AssetPickerConfig.AssetShowWarningText = LOCTEXT("NoControlConsoleAssetsFoundMessage", "No Control Console assets found");
			AssetPickerConfig.Filter.ClassPaths.Add(UDMXControlConsole::StaticClass()->GetClassPathName());
			AssetPickerConfig.Filter.bRecursiveClasses = false;
			AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SDMXControlConsoleEditorAssetPicker::OnAssetSelected);
			AssetPickerConfig.OnAssetEnterPressed = FOnAssetEnterPressed::CreateSP(this, &SDMXControlConsoleEditorAssetPicker::OnAssetEnterPressed);
		}

		const TSharedRef<SWidget> ConsolePicker = SNew(SBox)
			.MinDesiredWidth(650.f)
			.MinDesiredHeight(400.f)
			[
				ContentBrowser.CreateAssetPicker(AssetPickerConfig)
			];

		constexpr bool bNoIndent = true;
		constexpr bool bShowsSearch = false;
		MenuBuilder.AddWidget(ConsolePicker, FText(), bNoIndent, bShowsSearch);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

FText SDMXControlConsoleEditorAssetPicker::GetEditorConsoleName() const
{
	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsole* EditorConsole = EditorConsoleModel->GetEditorConsole();
	if (ensureMsgf(EditorConsole, TEXT("No Console present in Control Console Editor, cannot display console name.")))
	{
		if (EditorConsole->IsAsset())
		{
			const bool bDirty = EditorConsole->GetOutermost()->IsDirty();
			return bDirty ?
				FText::FromString(EditorConsole->GetName() + TEXT(" *")) :
				FText::FromString(EditorConsole->GetName());
		}
		else
		{
			return LOCTEXT("UnsavedConsoleName", "Unsaved Console *");
		}
	}
	return FText::GetEmpty();
}

void SDMXControlConsoleEditorAssetPicker::OnAssetSelected(const FAssetData& AssetData)
{
	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	EditorConsoleModel->LoadConsole(AssetData);

	AssetComboButton->SetIsOpen(false);
}

void SDMXControlConsoleEditorAssetPicker::OnAssetEnterPressed(const TArray<FAssetData>& SelectedAssets)
{
	if (SelectedAssets.IsEmpty())
	{
		return;
	}

	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	EditorConsoleModel->LoadConsole(SelectedAssets[0]);
}

#undef LOCTEXT_NAMESPACE

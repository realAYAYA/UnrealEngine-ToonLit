// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanImportUI.h"
#include "MetaHumanTypes.h"
#include "MetaHumanVersionService.h"
#include "Editor.h"
#include "SPrimaryButton.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "MetaHumanImportUI"

namespace UE::MetaHumanImportUI::Private
{
	using namespace MetaHumanVersionService;
	static const FText VersionUnavailable = LOCTEXT("VersionUnavailable", "Not Available");

	// TODO: padding needs revisiting to make sure it works OK with High DPI scaling etc.
	static constexpr float DefaultPadding = 8.0f;

	// Column names used in the Overwrite Dialog
	namespace OverwriteDialogColumns
	{
		static const FName IconColumnName(TEXT("Icon"));
		static const FName FileNameColumnName(TEXT("FileName"));
		static const FName OldVersionColumnName(TEXT("OldVersion"));
		static const FName NewVersionColumnName(TEXT("NewVersion"));
		static const FName MetaHumanColumnName(TEXT("MetaHuman"));
		static const FName VersionColumnName(TEXT("Version"));
		static const FName ReleaseNoteTitleColumnName(TEXT("Update"));
		static const FName ReleaseNoteColumnName(TEXT("ItemsAffected"));
	}


	// Data models for use by the UI

	// Represents a file that is being changed as part of the update process
	struct FFileChangeData
	{
		FText Filename;
		FAssetUpdateReason UpgradeReason;
	};

	struct FMetaHumanData
	{
		FText Name;
		FText Version;
		FText OldVersion;
	};

	FMetaHumanData FromMetaHuman(const FInstalledMetaHuman& MetaHuman)
	{
		return {FText::FromString(MetaHuman.GetName()), FText::FromString(UEVersionFromMhVersion(MetaHuman.GetVersion()))};
	}

	FMetaHumanData FromMetaHuman(const FSourceMetaHuman& MetaHuman)
	{
		return {FText::FromString(MetaHuman.GetName()), FText::FromString(UEVersionFromMhVersion(MetaHuman.GetVersion()))};
	}

	FMetaHumanData FromMetaHumanUpgrade(const FInstalledMetaHuman& Old, bool bIsUpgradeable, FString TargetUpgradeVersion)
	{
		return {FText::FromString(Old.GetName()), bIsUpgradeable ? FText::FromString(TargetUpgradeVersion) : VersionUnavailable, FText::FromString(UEVersionFromMhVersion(Old.GetVersion()))};
	}

	static void OnBrowserLinkClicked(const FSlateHyperlinkRun::FMetadata& Metadata)
	{
		const FString* URL = Metadata.Find(TEXT("href"));

		if(URL)
		{
			FPlatformProcess::LaunchURL(**URL, nullptr, nullptr);
		}
	}


	// Representation of a Release Note in the list of Release Notes
	class SReleaseNoteDataRow : public SMultiColumnTableRow<TSharedRef<FReleaseNoteData>>
	{
	public:
		SLATE_BEGIN_ARGS(SReleaseNoteDataRow)
			{
			}
			SLATE_ARGUMENT(TSharedPtr<FReleaseNoteData>, Item)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView)
		{
			RowData = Args._Item;

			FSuperRowType::Construct(
				FSuperRowType::FArguments()
				.Padding(5.0f),
				OwnerTableView
			);
		}

		/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the list view. */
		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			TSharedRef<SRichTextBlock> TextBlock = SNew(SRichTextBlock)
			+ SRichTextBlock::HyperlinkDecorator(TEXT("browser"), FSlateHyperlinkRun::FOnClick::CreateStatic(&OnBrowserLinkClicked));
			if (ColumnName == OverwriteDialogColumns::ReleaseNoteTitleColumnName)
			{
				TextBlock->SetText(RowData->Title);
				return TextBlock;
			}
			if (ColumnName == OverwriteDialogColumns::ReleaseNoteColumnName)
			{
				TextBlock->SetText(RowData->Note);
				TextBlock->SetAutoWrapText(true);
				return TextBlock;
			}
			if (ColumnName == OverwriteDialogColumns::VersionColumnName)
			{
				TextBlock->SetText(FText::FromString(UEVersionFromMhVersion(RowData->Version)));
				return TextBlock;
			}

			return SNullWidget::NullWidget;
		}

	private:
		TSharedPtr<FReleaseNoteData> RowData;
	};

	// Representation of a MetaHuman in one of the list of MetaHumans
	class SMetaHumanDataRow : public SMultiColumnTableRow<TSharedRef<FMetaHumanData>>
	{
	public:
		SLATE_BEGIN_ARGS(SMetaHumanDataRow)
			{
			}
			SLATE_ARGUMENT(TSharedPtr<FMetaHumanData>, Item)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView)
		{
			RowData = Args._Item;

			FSuperRowType::Construct(
				FSuperRowType::FArguments()
				.Padding(1.0f),
				OwnerTableView
			);
		}

		/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the list view. */
		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			TSharedRef<STextBlock> TextBlock = SNew(STextBlock);
			if (RowData->Version.IdenticalTo(VersionUnavailable))
			{
				TextBlock->SetColorAndOpacity(FLinearColor(1.0f, 0.2f, 0.2f));
			}
			if (ColumnName == OverwriteDialogColumns::MetaHumanColumnName)
			{
				TextBlock->SetText(RowData->Name);
				return TextBlock;
			}
			if (ColumnName == OverwriteDialogColumns::VersionColumnName || ColumnName == OverwriteDialogColumns::NewVersionColumnName)
			{
				TextBlock->SetText(RowData->Version);
				return TextBlock;
			}
			if (ColumnName == OverwriteDialogColumns::OldVersionColumnName)
			{
				TextBlock->SetText(RowData->OldVersion);
				return TextBlock;
			}

			return SNullWidget::NullWidget;
		}

	private:
		TSharedPtr<FMetaHumanData> RowData;
	};

	// Representation of a row in the list of files that have been changed
	class SFileChangeDataRow : public SMultiColumnTableRow<TSharedRef<FFileChangeData>>
	{
	public:
		SLATE_BEGIN_ARGS(SFileChangeDataRow)
			{
			}
			SLATE_ARGUMENT(TSharedPtr<FFileChangeData>, Item)
		SLATE_END_ARGS()

		void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView)
		{
			RowData = Args._Item;

			FSuperRowType::Construct(
				FSuperRowType::FArguments()
				.Padding(1.0f),
				OwnerTableView
			);
		}

		/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the list view. */
		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			if (ColumnName == OverwriteDialogColumns::IconColumnName)
			{
				TSharedRef<SImage> Icon = SNew(SImage).Image(FAppStyle::GetBrush("Icons.WarningWithColor"));
				if (RowData->UpgradeReason.NewVersion.Major && !RowData->UpgradeReason.IsBreakingChange())
				{
					Icon = SNew(SImage).Image(FAppStyle::GetBrush("Icons.InfoWithColor"));
				}
				return Icon;
			}

			TSharedRef<STextBlock> TextBlock = SNew(STextBlock);
			if (ColumnName == OverwriteDialogColumns::FileNameColumnName)
			{
				TextBlock->SetText(RowData->Filename);
				return TextBlock;
			}
			if (ColumnName == OverwriteDialogColumns::OldVersionColumnName)
			{
				TextBlock->SetText(FText::FromString(RowData->UpgradeReason.OldVersion.AsString()));
				return TextBlock;
			}
			if (ColumnName == OverwriteDialogColumns::NewVersionColumnName)
			{
				TextBlock->SetText(FText::FromString(RowData->UpgradeReason.NewVersion.AsString()));
				return TextBlock;
			}

			return SNullWidget::NullWidget;
		}

	private:
		TSharedPtr<FFileChangeData> RowData;
	};

	//Dialog implementation
	class SBatchImportDialog : public SCompoundWidget
	{
		TSharedRef<ITableRow> OnGenerateWidgetForMetaHumanList(TSharedRef<FMetaHumanData> InItem, const TSharedRef<STableViewBase>& OwnerTable) const
		{
			return SNew(SMetaHumanDataRow, OwnerTable)
				.Item(InItem);
		}

	public:
		SLATE_BEGIN_ARGS(SBatchImportDialog)
			{
			}
			/** Window in which this widget resides */
			SLATE_ARGUMENT(TSharedPtr<SWindow>, ParentWindow)
			SLATE_ARGUMENT(TArray<TSharedRef<FMetaHumanData>>, MetaHumanUpgrades)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			ParentWindow = InArgs._ParentWindow;
			MetaHumanUpgrades = InArgs._MetaHumanUpgrades;

			const TSharedRef<SHeaderRow> MetaHumanHeader = SNew(SHeaderRow)
				+ SHeaderRow::Column(OverwriteDialogColumns::MetaHumanColumnName).DefaultLabel(LOCTEXT("MetaHumanName", "MetaHuman Name")).FillWidth(1.0f)
				+ SHeaderRow::Column(OverwriteDialogColumns::OldVersionColumnName).DefaultLabel(LOCTEXT("CurrentVersion", "Current Version")).FixedWidth(120.0f).HAlignCell(HAlign_Right)
				+ SHeaderRow::Column(OverwriteDialogColumns::NewVersionColumnName).DefaultLabel(LOCTEXT("EngineVersion", "Engine Version")).FixedWidth(120.0f).HAlignCell(HAlign_Right);

			bool bHasMissingMetaHumans = false;
			for (const auto& MetaHuman : MetaHumanUpgrades)
			{
				if (MetaHuman->Version.IdenticalTo(VersionUnavailable))
				{
					bHasMissingMetaHumans = true;
					break;
				}
			}

			ChildSlot
			[
				SNew(SVerticalBox)
				// Upgradeable MetaHumans List
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(DefaultPadding)
				[
					SNew(SListView< TSharedRef<FMetaHumanData> >)
					.ItemHeight(24)
					.ListItemsSource(&MetaHumanUpgrades)
					.OnGenerateRow(this, &SBatchImportDialog::OnGenerateWidgetForMetaHumanList)
					.HeaderRow(MetaHumanHeader)
					.SelectionMode(ESelectionMode::None)
				]
				// Main body text
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(DefaultPadding)
				[
					SAssignNew(MissingItemWarning, SHorizontalBox)
					.Visibility(bHasMissingMetaHumans ? EVisibility::Visible : EVisibility::Collapsed)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.Padding(DefaultPadding)
					[
						SNew(SImage)
						.Image(FAppStyle::Get().GetBrush("Icons.WarningWithColor.Large"))
					]
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Fill)
					.Padding(DefaultPadding)
					[
						SNew(STextBlock)
						.AutoWrapText(true)
						.Text(LOCTEXT("UpgradeWarning", "WARNING: Only previously downloaded MetaHumans with the correct version will be updated as part of this process. MetaHumans that are the correct version will be automatically downloaded. Any MetaHumans showing as not available in the above table will either need to be upgraded in MetaHuman Creator, or are not linked to your current Epic account and will not be updated."))
					]
				]
				// Export all Button
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(DefaultPadding)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Fill)
					.AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("Continue", "Continue"))
						.HAlign(HAlign_Center)
						.OnClicked(this, &SBatchImportDialog::HandleOk)
					]
				]
			];
		}

		bool GetShouldContinue() const
		{
			return bShouldContinue;
		}

	private:
		TSharedPtr<SWindow> ParentWindow;
		TSharedPtr<SHorizontalBox> MissingItemWarning;
		TArray<TSharedRef<FMetaHumanData>> MetaHumanUpgrades;
		bool bShouldContinue = false;

		FReply HandleOk()
		{
			const FText Title = LOCTEXT("BulkImportWarningTitle", "Proceed with import?");
			const FText Message = LOCTEXT("BulkImportWarningBody", "Batch updating all MetaHumans in your project will take some time, and will also overwrite any local files that you have changed.\n\n"
				"Clicking OK will import the current MetaHuman and then select and download the remaining MetaHumans. Once the download has completed, add the selected MetaHumans to your project by clicking the \"Add\" Button. Please do not close Bridge until this process has completed.");

			const EAppReturnType::Type UpdateAssetsDialog = FMessageDialog::Open(EAppMsgType::OkCancel, Message, Title);
			bShouldContinue = UpdateAssetsDialog == EAppReturnType::Ok;
			ParentWindow->RequestDestroyWindow();
			return FReply::Handled();
		}
	};

	//Dialog implementation
	class SOverwriteDialog : public SCompoundWidget
	{
		TSharedRef<ITableRow> OnGenerateWidgetForFileList(TSharedRef<FFileChangeData> InItem, const TSharedRef<STableViewBase>& OwnerTable) const
		{
			return SNew(SFileChangeDataRow, OwnerTable)
			.Item(InItem)
			.ToolTipText(InItem->Filename);
		}

		TSharedRef<ITableRow> OnGenerateWidgetForReleaseNoteList(TSharedRef<FReleaseNoteData> InItem, const TSharedRef<STableViewBase>& OwnerTable) const
		{
			return SNew(SReleaseNoteDataRow, OwnerTable)
			.Item(InItem)
			.ToolTipText(InItem->Detail);
		}

		TSharedRef<ITableRow> OnGenerateWidgetForMetaHumanList(TSharedRef<FMetaHumanData> InItem, const TSharedRef<STableViewBase>& OwnerTable) const
		{
			return SNew(SMetaHumanDataRow, OwnerTable)
				.Item(InItem);
		}

	public:
		SLATE_BEGIN_ARGS(SOverwriteDialog)
			{
			}
			/** Window in which this widget resides */
			SLATE_ARGUMENT(TSharedPtr<SWindow>, ParentWindow)
			SLATE_ARGUMENT(FText, Header)
			SLATE_ARGUMENT(TArray<TSharedRef<FReleaseNoteData>>, ReleaseNotes)
			SLATE_ARGUMENT(TArray<TSharedRef<FMetaHumanData>>, IncomingMetaHumans)
			SLATE_ARGUMENT(TArray<TSharedRef<FMetaHumanData>>, ExistingMetaHumans)
			SLATE_ARGUMENT(TArray<TSharedRef<FFileChangeData>>, UpdatedFiles)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			ParentWindow = InArgs._ParentWindow;
			UpdatedFiles = InArgs._UpdatedFiles;
			IncomingMetaHumans = InArgs._IncomingMetaHumans;
			ExistingMetaHumans = InArgs._ExistingMetaHumans;
			ReleaseNotes = InArgs._ReleaseNotes;

			const TSharedRef<SHeaderRow> ReleaseNotesHeader = SNew(SHeaderRow)
				+ SHeaderRow::Column(OverwriteDialogColumns::ReleaseNoteTitleColumnName)
				.DefaultLabel(LOCTEXT("ReleaseNoteTitleHeader", "Update"))
				.FillWidth(0.3f)
				+ SHeaderRow::Column(OverwriteDialogColumns::ReleaseNoteColumnName)
				.DefaultLabel(LOCTEXT("ReleaseNoteHeader", "Items Affected"))
				.FillWidth(0.7f)
				+ SHeaderRow::Column(OverwriteDialogColumns::VersionColumnName)
				.DefaultLabel(LOCTEXT("VersionHeader", "Version"))
				.FixedWidth(120.0f)
				.HAlignCell(HAlign_Right);

			const TSharedRef<SHeaderRow> IncomingMetaHumanHeader = SNew(SHeaderRow)
				+ SHeaderRow::Column(OverwriteDialogColumns::MetaHumanColumnName)
				.DefaultLabel(LOCTEXT("MetaHumanImportHeader", "MetaHuman Selected for Import"))
				.DefaultTooltip(LOCTEXT("MetaHumanImportHeaderTooltip", "The MetaHuman currently selected for import"))
				.FillWidth(1.0f)
				+ SHeaderRow::Column(OverwriteDialogColumns::VersionColumnName)
				.DefaultLabel(LOCTEXT("VersionHeader", "Version"))
				.FixedWidth(120.0f)
				.HAlignCell(HAlign_Right);

			const TSharedRef<SHeaderRow> ExistingMetaHumanHeader = SNew(SHeaderRow)
				+ SHeaderRow::Column(OverwriteDialogColumns::MetaHumanColumnName)
				.DefaultLabel(LOCTEXT("MetaHumanUpdateHeader", "MetaHumans That Should be Updated"))
				.DefaultTooltip(LOCTEXT("MetaHumanUpdateHeaderTooltip", "These MetaHumans will potentially be broken by this import operation and should also be updated"))
				.FillWidth(1.0f)
				+ SHeaderRow::Column(OverwriteDialogColumns::VersionColumnName)
				.DefaultLabel(LOCTEXT("VersionHeader", "Version"))
				.FixedWidth(120.0f)
				.HAlignCell(HAlign_Right);

			const TSharedRef<SHeaderRow> UpdateHeader = SNew(SHeaderRow)
				+ SHeaderRow::Column(OverwriteDialogColumns::IconColumnName)
				.DefaultLabel(LOCTEXT("IconHeader", ""))
				.FixedWidth(20.0f)
				+ SHeaderRow::Column(OverwriteDialogColumns::FileNameColumnName)
				.DefaultLabel(LOCTEXT("FileNameHeader", "File Name"))
				.FillWidth(1.0f)
				+ SHeaderRow::Column(OverwriteDialogColumns::OldVersionColumnName)
				.DefaultLabel(LOCTEXT("OldVersionHeader", "Previous Version"))
				.FixedWidth(120.0f)
				.HAlignCell(HAlign_Right)
				+ SHeaderRow::Column(OverwriteDialogColumns::NewVersionColumnName)
				.DefaultLabel(LOCTEXT("NewVersionHeader", "New Version"))
				.FixedWidth(120.0f)
				.HAlignCell(HAlign_Right);

			const FText ContinueButtonLabel = ExistingMetaHumans.Num() == 1 ? LOCTEXT("ContinueButtonSingleUpgrade", "Continue With Import") : LOCTEXT("ContinueButton", "Continue With Single Import");
			const FButtonStyle* ContinueButtonStyle = &FAppStyle::Get().GetWidgetStyle<FButtonStyle>(ExistingMetaHumans.Num() == 1 ? "PrimaryButton" : "Button");
			const EVisibility UpdateButtonVisibility = ExistingMetaHumans.Num() == 1 ? EVisibility::Collapsed : EVisibility::Visible;

			ChildSlot
			[
				SNew(SVerticalBox)
				// Main body text
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(DefaultPadding)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(DefaultPadding)
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(SImage)
						.Image(FAppStyle::Get().GetBrush("Icons.WarningWithColor.Large"))
					]
					+ SHorizontalBox::Slot()
					.Padding(DefaultPadding)
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Fill)
					[
						SNew(STextBlock)
						.AutoWrapText(true)
						.Text(InArgs._Header)
					]
				]
				// Export all Button
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(DefaultPadding)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					.AutoWidth()
					[
						SNew(SPrimaryButton)
						.Text(LOCTEXT("UpdateButton", "Update All MetaHumans in Project..."))
						.OnClicked(this, &SOverwriteDialog::HandleExportAll)
						.Visibility(UpdateButtonVisibility)
					]
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					.AutoWidth()
					.Padding(DefaultPadding, 0.0f)
					[
						SNew(SButton)
						.ButtonStyle(ContinueButtonStyle)
						.ForegroundColor(FSlateColor::UseStyle())
						.Text(ContinueButtonLabel)
						.HAlign(HAlign_Center)
						.OnClicked(this, &SOverwriteDialog::HandleYes)
					]
				]
				// Release Note List
				+ SVerticalBox::Slot()
				.AutoHeight()
				.MaxHeight(200.0f)
				.Padding(DefaultPadding)
				[
					// Release Note Collapse UI
					SNew(SExpandableArea)
					.AreaTitle(LOCTEXT("ConflictDetails", "Conflict information details"))
					.InitiallyCollapsed(true)
					.BodyContent()
					[
						SNew(SListView< TSharedRef<FReleaseNoteData> >)
						.ItemHeight(24)
						.ListItemsSource(&ReleaseNotes)
						.OnGenerateRow(this, &SOverwriteDialog::OnGenerateWidgetForReleaseNoteList)
						.HeaderRow(ReleaseNotesHeader)
						.SelectionMode(ESelectionMode::None)
					]
				]
				// Incoming MetaHumans List
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(DefaultPadding)
				[
					SNew(SListView< TSharedRef<FMetaHumanData> >)
					.ItemHeight(24)
					.ListItemsSource(&IncomingMetaHumans)
					.OnGenerateRow(this, &SOverwriteDialog::OnGenerateWidgetForMetaHumanList)
					.HeaderRow(IncomingMetaHumanHeader)
					.SelectionMode(ESelectionMode::None)
				]
				// Existing MetaHumans List
				+ SVerticalBox::Slot()
				.AutoHeight()
				.MaxHeight(140.f)
				.Padding(DefaultPadding)
				[
					SNew(SListView< TSharedRef<FMetaHumanData> >)
					.ItemHeight(24)
					.ListItemsSource(&ExistingMetaHumans)
					.OnGenerateRow(this, &SOverwriteDialog::OnGenerateWidgetForMetaHumanList)
					.HeaderRow(ExistingMetaHumanHeader)
					.SelectionMode(ESelectionMode::None)
				]
				// File Details
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(DefaultPadding)
				[
					// Release Note Collapse UI
					SNew(SExpandableArea)
					.AreaTitle(LOCTEXT("DetailsTitle", "Detailed asset overwrite info"))
					.InitiallyCollapsed(true)
					.BodyContent()
					[
						SNew(SListView< TSharedRef<FFileChangeData> >)
						.ItemHeight(24)
						.ListItemsSource(&UpdatedFiles)
						.OnGenerateRow(this, &SOverwriteDialog::OnGenerateWidgetForFileList)
						.HeaderRow(UpdateHeader)
						.SelectionMode(ESelectionMode::None)
					]
				]
				//Padding
				+ SVerticalBox::Slot()
				.FillHeight(0.01f)
				//Yes, No Buttons
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(DefaultPadding)
				.HAlign(HAlign_Right)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					.AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("CancelButton", "Cancel"))
						.HAlign(HAlign_Center)
						.OnClicked(this, &SOverwriteDialog::HandleNo)
					]
				]
			];
		}

		bool GetShouldContinue() const
		{
			return bShouldContinue;
		}

		bool GetShouldImportAll() const
		{
			return bShouldImportAll;
		}

	private:
		TSharedPtr<SWindow> ParentWindow;
		TArray<TSharedRef<FMetaHumanData>> IncomingMetaHumans;
		TArray<TSharedRef<FMetaHumanData>> ExistingMetaHumans;
		TArray<TSharedRef<FFileChangeData>> UpdatedFiles;
		TArray<TSharedRef<FReleaseNoteData>> ReleaseNotes;
		bool bShouldContinue = false;
		bool bShouldImportAll = false;

		FReply HandleYes()
		{
			const FText Title = LOCTEXT("ImportWarningTitle", "Proceed with import?");
			const FText Message = LOCTEXT("ImportWarningBody", "Importing this MetaHuman will overwrite the common assets listed in the Asset overwrite details rollout. \n\n Any local changes you have made to these assets will be lost.");

			const EAppReturnType::Type UpdateAssetsDialog = FMessageDialog::Open(EAppMsgType::OkCancel, Message, Title);
			bShouldContinue = UpdateAssetsDialog == EAppReturnType::Ok;
			ParentWindow->RequestDestroyWindow();
			return FReply::Handled();
		}

		FReply HandleNo() const
		{
			ParentWindow->RequestDestroyWindow();
			return FReply::Handled();
		}

		FReply HandleExportAll()
		{
			bShouldImportAll = true;
			bShouldContinue = true;
			ParentWindow->RequestDestroyWindow();
			return FReply::Handled();
		}
	};
}

EImportOperationUserResponse DisplayUpgradeWarning(const FSourceMetaHuman& SourceMetaHuman, const TSet<FString>& IncompatibleCharacters, const TArray<FInstalledMetaHuman>& InstalledMetaHumans, const TSet<FString>& AvailableMetaHumans, const FAssetOperationPaths& AssetOperations)
{
	using namespace UE::MetaHumanImportUI::Private;
	using namespace UE::MetaHumanVersionService;


	FMetaHumanVersion TargetMetaHumanCurrentVersion{0, 5, 0};
	TArray<TSharedRef<FMetaHumanData>> IncomingMetaHumans = {MakeShared<FMetaHumanData>(FromMetaHuman(SourceMetaHuman))};
	TArray<TSharedRef<FMetaHumanData>> ExistingMetaHumans;
	for (const FInstalledMetaHuman& MetaHuman : InstalledMetaHumans)
	{
		if (MetaHuman.GetName() == SourceMetaHuman.GetName())
		{
			TargetMetaHumanCurrentVersion = MetaHuman.GetVersion();
		}
		if (IncompatibleCharacters.Contains(MetaHuman.GetName()))
		{
			ExistingMetaHumans.Add(MakeShared<FMetaHumanData>(FromMetaHuman(MetaHuman)));
		}
	}

	// Release notes
	TArray<TSharedRef<FReleaseNoteData>> ReleaseNotes = GetReleaseNotesForVersionUpgrade(TargetMetaHumanCurrentVersion, SourceMetaHuman.GetVersion());

	TArray<TSharedRef<FMetaHumanData>> MetaHumanUpgrades;
	for (const FInstalledMetaHuman& MetaHuman : InstalledMetaHumans)
	{
		MetaHumanUpgrades.Add(MakeShared<FMetaHumanData>(FromMetaHumanUpgrade(MetaHuman, AvailableMetaHumans.Contains(MetaHuman.GetName()), UEVersionFromMhVersion(SourceMetaHuman.GetVersion()))));
	}

	// Build the text and data structures representing the updated assets
	const FText Header = IncompatibleCharacters.Num() == 1 ?
		LOCTEXT("MainDialogTextSingleUpgrade", "You are importing a MetaHuman that has a Major version mismatch with the existing MetaHuman in your project."):
		LOCTEXT("MainDialogText", "You are importing a MetaHuman that has a Major version mismatch with existing MetaHumans in your project. Continuing import will break functionality on these existing MetaHumans unless you update all MetaHumans in the project.");
	TArray<TSharedRef<FFileChangeData>> UpdateList;
	for (int i = 0; i < AssetOperations.Update.Num(); i++)
	{
		UpdateList.Add(MakeShared<FFileChangeData>(FFileChangeData{
			FText::FromString(AssetOperations.Update[i]),
			AssetOperations.UpdateReasons[i]
		}));
	}

	// Display the dialog as a modal window
	const TSharedRef<SWindow> ModalWindow = SNew(SWindow)
		.Title(LOCTEXT("ImportWarningWindowTitle", "MetaHuman Import Warning"))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(700, 700))
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.HasCloseButton(true)
		.SupportsMinimize(false)
		.SupportsMaximize(false);

	const TSharedRef<SOverwriteDialog> OverwriteDialog = SNew(SOverwriteDialog)
		.ParentWindow(ModalWindow)
		.Header(Header)
		.ReleaseNotes(ReleaseNotes)
		.IncomingMetaHumans(IncomingMetaHumans)
		.ExistingMetaHumans(ExistingMetaHumans)
		.UpdatedFiles(UpdateList);

	ModalWindow->SetContent(OverwriteDialog);

	GEditor->EditorAddModalWindow(ModalWindow);

	if (OverwriteDialog->GetShouldContinue())
	{
		if (OverwriteDialog->GetShouldImportAll())
		{
			// Display the dialog as a modal window
			const TSharedRef<SWindow> InnerModalWindow = SNew(SWindow)
				.Title(LOCTEXT("MainDialogTitle", "Quixel MetaHuman Update Dialog"))
				.SizingRule(ESizingRule::UserSized)
				.ClientSize(FVector2D(600, 250))
				.AutoCenter(EAutoCenter::PreferredWorkArea)
				.HasCloseButton(true)
				.SupportsMinimize(false)
				.SupportsMaximize(false);

			const TSharedRef<SBatchImportDialog> BatchImportDialog = SNew(SBatchImportDialog)
				.ParentWindow(InnerModalWindow)
				.MetaHumanUpgrades(MetaHumanUpgrades);

			InnerModalWindow->SetContent(BatchImportDialog);

			GEditor->EditorAddModalWindow(InnerModalWindow);

			return BatchImportDialog->GetShouldContinue() ? EImportOperationUserResponse::BulkImport : EImportOperationUserResponse::Cancel;
		}
		return EImportOperationUserResponse::OK;
	}
	return EImportOperationUserResponse::Cancel;
}

FText GetValueAsText(EQualityLevel Level)
{
	if (Level == EQualityLevel::High)
	{
		return LOCTEXT("EQualityLevel:High", "High");
	}
	if (Level == EQualityLevel::Medium)
	{
		return LOCTEXT("EQualityLevel:Medium", "Medium");
	}
	return LOCTEXT("EQualityLevel:Low", "Low");
}

bool DisplayQualityLevelChangeWarning(EQualityLevel Source, EQualityLevel Target)
{
	const FText Title = LOCTEXT("QualityLevelWarningTitle", "Proceed with import?");
	const FText Message = FText::Format(LOCTEXT("QualityLevelWarningBody", "You are about to import a MetaHuman at the \"{0}\" quality level, over an existing MetaHuman at the \"{1}\" quality level."), GetValueAsText(Source), GetValueAsText(Target));

	const EAppReturnType::Type UpdateAssetsDialog = FMessageDialog::Open(EAppMsgType::OkCancel, Message, Title);
	return UpdateAssetsDialog == EAppReturnType::Ok;
	
}

#undef LOCTEXT_NAMESPACE

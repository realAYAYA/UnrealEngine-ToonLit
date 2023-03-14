// Copyright Epic Games, Inc. All Rights Reserved.

#include "RetargetEditor/SIKRetargetAssetBrowser.h"

#include "SPositiveActionButton.h"
#include "AnimPreviewInstance.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/PoseAsset.h"

#include "IKRigEditor.h"
#include "RetargetEditor/IKRetargetBatchOperation.h"
#include "RetargetEditor/IKRetargetEditorController.h"
#include "RetargetEditor/SRetargetAnimAssetsWindow.h"
#include "Retargeter/IKRetargeter.h"
#include "Retargeter/IKRetargetProcessor.h"
#include "Widgets/Layout/SUniformGridPanel.h"

#define LOCTEXT_NAMESPACE "IKRetargeterAssetBrowser"

void SIKRetargetAssetBrowser::Construct(
	const FArguments& InArgs,
	TSharedRef<FIKRetargetEditorController> InEditorController)
{
	EditorController = InEditorController;
	EditorController.Pin()->SetAssetBrowserView(SharedThis(this));
	
	ChildSlot
    [
        SNew(SVerticalBox)
        
        + SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5)
		[
			SNew(SPositiveActionButton)
			.IsEnabled(this, &SIKRetargetAssetBrowser::IsExportButtonEnabled)
			.Icon(FAppStyle::Get().GetBrush("Icons.Save"))
			.Text(LOCTEXT("ExportButtonLabel", "Export Selected Animations"))
			.ToolTipText(LOCTEXT("ExportButtonToolTip", "Generate new retargeted sequence assets on target skeletal mesh (uses current retargeting configuration)."))
			.OnClicked(this, &SIKRetargetAssetBrowser::OnExportButtonClicked)
		]

		+SVerticalBox::Slot()
		[
			SAssignNew(AssetBrowserBox, SBox)
		]
    ];

	RefreshView();
}

void SIKRetargetAssetBrowser::RefreshView()
{
	FAssetPickerConfig AssetPickerConfig;

	// setup filtering
	AssetPickerConfig.Filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
	AssetPickerConfig.Filter.ClassPaths.Add(UAnimMontage::StaticClass()->GetClassPathName());
	AssetPickerConfig.Filter.ClassPaths.Add(UPoseAsset::StaticClass()->GetClassPathName());
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::Column;
	AssetPickerConfig.bAddFilterUI = true;
	AssetPickerConfig.bShowPathInColumnView = true;
	AssetPickerConfig.bShowTypeInColumnView = true;
	AssetPickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateSP(this, &SIKRetargetAssetBrowser::OnShouldFilterAsset);
	AssetPickerConfig.DefaultFilterMenuExpansion = EAssetTypeCategories::Animation;
	AssetPickerConfig.OnAssetDoubleClicked = FOnAssetSelected::CreateSP(this, &SIKRetargetAssetBrowser::OnAssetDoubleClicked);
	AssetPickerConfig.OnGetAssetContextMenu = FOnGetAssetContextMenu::CreateSP(this, &SIKRetargetAssetBrowser::OnGetAssetContextMenu);
	AssetPickerConfig.GetCurrentSelectionDelegates.Add(&GetCurrentSelectionDelegate);
	AssetPickerConfig.bAllowNullSelection = false;

	// hide all asset registry columns by default (we only really want the name and path)
	TArray<UObject::FAssetRegistryTag> AssetRegistryTags;
	UAnimSequence::StaticClass()->GetDefaultObject()->GetAssetRegistryTags(AssetRegistryTags);
	for(UObject::FAssetRegistryTag& AssetRegistryTag : AssetRegistryTags)
	{
		AssetPickerConfig.HiddenColumnNames.Add(AssetRegistryTag.Name.ToString());
	}

	// Also hide the type column by default (but allow users to enable it, so don't use bShowTypeInColumnView)
	AssetPickerConfig.HiddenColumnNames.Add(TEXT("Class"));
	AssetPickerConfig.HiddenColumnNames.Add(TEXT("Has Virtualized Data"));

	const FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	AssetBrowserBox->SetContent(ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig));
}

TSharedPtr<SWidget> SIKRetargetAssetBrowser::OnGetAssetContextMenu(const TArray<FAssetData>& SelectedAssets) const
{
	if (SelectedAssets.Num() <= 0)
	{
		return nullptr;
	}

	UObject* SelectedAsset = SelectedAssets[0].GetAsset();
	if (SelectedAsset == nullptr)
	{
		return nullptr;
	}
	
	FMenuBuilder MenuBuilder(true, MakeShared<FUICommandList>());

	MenuBuilder.BeginSection(TEXT("Asset"), LOCTEXT("AssetSectionLabel", "Asset"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("Browse", "Browse to Asset"),
			LOCTEXT("BrowseTooltip", "Browses to the associated asset and selects it in the most recently used Content Browser (summoning one if necessary)"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "SystemWideCommands.FindInContentBrowser.Small"),
			FUIAction(
				FExecuteAction::CreateLambda([SelectedAsset] ()
				{
					if (SelectedAsset)
					{
						const TArray<FAssetData>& Assets = { SelectedAsset };
						const FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
						ContentBrowserModule.Get().SyncBrowserToAssets(Assets);
					}
				}),
				FCanExecuteAction::CreateLambda([] () { return true; })
			)
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

FReply SIKRetargetAssetBrowser::OnExportButtonClicked()
{
	FIKRetargetEditorController* Controller = EditorController.Pin().Get();
	if (!Controller)
	{
		checkNoEntry();
		return FReply::Handled();
	}
	
	// prompt user for path to export animations to
	TSharedRef<SBatchExportDialog> Dialog = SNew(SBatchExportDialog).DefaultAssetPath(FText::FromString(PrevBatchOutputPath));
	if(Dialog->ShowModal() == EAppReturnType::Cancel)
	{
		return FReply::Handled();
	}

	// store path for next time
	PrevBatchOutputPath = Dialog->GetAssetPath();

	// assemble the data for the assets we want to batch duplicate/retarget
	FIKRetargetBatchOperationContext BatchContext = Dialog.Get().BatchContext;
	BatchContext.NameRule.FolderPath = PrevBatchOutputPath;
	BatchContext.SourceMesh = Controller->GetSkeletalMesh(ERetargetSourceOrTarget::Source);
	BatchContext.TargetMesh = Controller->GetSkeletalMesh(ERetargetSourceOrTarget::Target);
	BatchContext.IKRetargetAsset = Controller->AssetController->GetAsset();
	BatchContext.bRemapReferencedAssets = false;

	// add selected assets to dup/retarget
	TArray<FAssetData> SelectedAssets = GetCurrentSelectionDelegate.Execute();
	for (const FAssetData& Asset : SelectedAssets)
	{
		UE_LOG(LogIKRigEditor, Display, TEXT("Duplicating and Retargeting: %s"), *Asset.GetFullName());
		BatchContext.AssetsToRetarget.Add(Asset.GetAsset());
	}

	// actually run the retarget
	FIKRetargetBatchOperation BatchOperation;
	BatchOperation.RunRetarget(BatchContext);

	return FReply::Handled();
}

bool SIKRetargetAssetBrowser::IsExportButtonEnabled() const
{
	if (!EditorController.Pin().IsValid())
	{
		return false; // editor in bad state
	}

	const UIKRetargetProcessor* Processor = EditorController.Pin()->GetRetargetProcessor();
	if (!Processor)
	{
		return false; // no retargeter running
	}

	if (!Processor->IsInitialized())
	{
		return false; // retargeter not loaded and valid
	}

	TArray<FAssetData> SelectedAssets = GetCurrentSelectionDelegate.Execute();
	if (SelectedAssets.IsEmpty())
	{
		return false; // nothing selected
	}

	return true;
}

void SIKRetargetAssetBrowser::OnAssetDoubleClicked(const FAssetData& AssetData)
{
	if (!AssetData.GetAsset())
	{
		return;
	}
	
	UAnimationAsset* NewAnimationAsset = Cast<UAnimationAsset>(AssetData.GetAsset());
	if (NewAnimationAsset && EditorController.Pin().IsValid())
	{
		EditorController.Pin()->PlayAnimationAsset(NewAnimationAsset);
	}
}

bool SIKRetargetAssetBrowser::OnShouldFilterAsset(const struct FAssetData& AssetData)
{
	// is this an animation asset?
	if (!AssetData.IsInstanceOf(UAnimationAsset::StaticClass()))
	{
		return true;
	}
	
	// controller setup
	const FIKRetargetEditorController* Controller = EditorController.Pin().Get();
	if (!Controller)
	{
		return true;
	}

	// get source skeleton
	const USkeleton* DesiredSkeleton = Controller->GetSkeleton(ERetargetSourceOrTarget::Source);
	if (!DesiredSkeleton)
	{
		return true;
	}

	return !DesiredSkeleton->IsCompatibleSkeletonByAssetData(AssetData);
}

// ------------------------------------------BEGIN  SBatchExportDialog ----------------------------

void SBatchExportDialog::Construct(const FArguments& InArgs)
{
	AssetPath = FText::FromString(FPackageName::GetLongPackagePath(InArgs._DefaultAssetPath.ToString()));

	if(AssetPath.IsEmpty())
	{
		AssetPath = FText::FromString(TEXT("/Game"));
	}

	FPathPickerConfig PathPickerConfig;
	PathPickerConfig.DefaultPath = AssetPath.ToString();
	PathPickerConfig.OnPathSelected = FOnPathSelected::CreateSP(this, &SBatchExportDialog::OnPathChange);
	PathPickerConfig.bAddDefaultPath = true;

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	constexpr int32 TextInputWidths = 200;

	SWindow::Construct(
		SWindow::FArguments()
		.Title(LOCTEXT("SBatchExportDialog_Title", "Batch Export Retargeted Animations"))
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		.IsTopmostWindow(true)
		.ClientSize(FVector2D(350, 600))
		[
			SNew(SVerticalBox)
			
			+ SVerticalBox::Slot()
			.Padding(2)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					SNew(SVerticalBox)

					+SVerticalBox::Slot()
					.Padding(2, 3)
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SelectPath", "Select Path"))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
					]

					+SVerticalBox::Slot()
					.FillHeight(1)
					.Padding(3)
					[
						ContentBrowserModule.Get().CreatePathPicker(PathPickerConfig)
					]
				]
			]

			+ SVerticalBox::Slot()
			.Padding(2)
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					SNew(SVerticalBox)

					+SVerticalBox::Slot()
					.Padding(2, 3)
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("DuplicateAndRetarget_RenameLabel", "Rename New Assets"))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
					]

					+SVerticalBox::Slot()
					.AutoHeight()
					.Padding(2, 1)
					.HAlign(HAlign_Right)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(2, 1)
						[
							SNew(STextBlock).Text(LOCTEXT("DuplicateAndRetarget_Prefix", "Add Prefix:"))
						]

						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SEditableTextBox)
							.Text_Lambda([this]
							{
								return FText::FromString(BatchContext.NameRule.Prefix);
							})
							.MinDesiredWidth(TextInputWidths)
							.OnTextChanged_Lambda([this](const FText& InText)
							{
								BatchContext.NameRule.Prefix = InText.ToString();
								UpdateExampleText();
							})
							.IsReadOnly(false)
							.RevertTextOnEscape(true)
						]
					]

					+SVerticalBox::Slot()
					.AutoHeight()
					.Padding(2, 1)
					.HAlign(HAlign_Right)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(2, 1)
						[
							SNew(STextBlock).Text(LOCTEXT("DuplicateAndRetarget_Suffix", "Add Suffix:"))
						]

						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SEditableTextBox)
							.Text_Lambda([this]
							{
								return FText::FromString(BatchContext.NameRule.Suffix);
							})
							.MinDesiredWidth(TextInputWidths)
							.OnTextChanged_Lambda([this](const FText& InText)
							{
								BatchContext.NameRule.Suffix = InText.ToString();
								UpdateExampleText();
							})
							.IsReadOnly(false)
							.RevertTextOnEscape(true)
						]
					]

					+SVerticalBox::Slot()
					.AutoHeight()
					.Padding(2, 1)
					.HAlign(HAlign_Right)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.HAlign(HAlign_Right)
						.VAlign(VAlign_Center)
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.Padding(2, 1)
							[
								SNew(STextBlock).Text(LOCTEXT("DuplicateAndRetarget_Search", "Search for:"))
							]

							+SHorizontalBox::Slot()
							[
								SNew(SEditableTextBox)
								.Text_Lambda([this]
								{
									return FText::FromString(BatchContext.NameRule.ReplaceFrom);
								})
								.MinDesiredWidth(TextInputWidths)
								.OnTextChanged_Lambda([this](const FText& InText)
								{
									BatchContext.NameRule.ReplaceFrom = InText.ToString();
									UpdateExampleText();
								})
								.IsReadOnly(false)
								.RevertTextOnEscape(true)
							]
						]
						
						+SHorizontalBox::Slot()
						.HAlign(HAlign_Right)
						.VAlign(VAlign_Center)
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.Padding(2, 1)
							[
								SNew(STextBlock).Text(LOCTEXT("DuplicateAndRetarget_Replace", "Replace with:"))
							]

							+SHorizontalBox::Slot()
							[
								SNew(SEditableTextBox)
								.Text_Lambda([this]
								{
									return FText::FromString(BatchContext.NameRule.ReplaceTo);
								})
								.MinDesiredWidth(TextInputWidths)
								.OnTextChanged_Lambda([this](const FText& InText)
								{
									BatchContext.NameRule.ReplaceTo = InText.ToString();
									UpdateExampleText();
								})
								.IsReadOnly(false)
								.RevertTextOnEscape(true)
							]
						]
					]

					+SVerticalBox::Slot()
					.AutoHeight()
					.Padding(2, 3)
					.HAlign(HAlign_Right)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.Padding(5, 5)
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text_Lambda([this]{ return ExampleText; })
							.Font(FAppStyle::GetFontStyle("Persona.RetargetManager.ItalicFont"))
						]
					]
				]
			]
			
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(5)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FAppStyle::GetMargin("StandardDialog.SlotPadding"))
				.MinDesiredSlotWidth(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
				+SUniformGridPanel::Slot(0, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
					.Text(LOCTEXT("Export", "Export"))
					.OnClicked(this, &SBatchExportDialog::OnButtonClick, EAppReturnType::Ok)
				]
				+SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
					.Text(LOCTEXT("Cancel", "Cancel"))
					.OnClicked(this, &SBatchExportDialog::OnButtonClick, EAppReturnType::Cancel)
				]
			]
		]);
}

void SBatchExportDialog::UpdateExampleText()
{
	const FString ReplaceFrom = FString::Printf(TEXT("Old Name : ###%s###"), *BatchContext.NameRule.ReplaceFrom);
	const FString ReplaceTo = FString::Printf(TEXT("New Name : %s###%s###%s"), *BatchContext.NameRule.Prefix, *BatchContext.NameRule.ReplaceTo, *BatchContext.NameRule.Suffix);

	ExampleText = FText::FromString(FString::Printf(TEXT("%s\n%s"), *ReplaceFrom, *ReplaceTo));
}

void SBatchExportDialog::OnPathChange(const FString& NewPath)
{
	AssetPath = FText::FromString(NewPath);
}

FReply SBatchExportDialog::OnButtonClick(EAppReturnType::Type ButtonID)
{
	UserResponse = ButtonID;

	RequestDestroyWindow();

	return FReply::Handled();
}

EAppReturnType::Type SBatchExportDialog::ShowModal()
{
	GEditor->EditorAddModalWindow(SharedThis(this));
	return UserResponse;
}

FString SBatchExportDialog::GetAssetPath()
{
	return AssetPath.ToString();
}


#undef LOCTEXT_NAMESPACE

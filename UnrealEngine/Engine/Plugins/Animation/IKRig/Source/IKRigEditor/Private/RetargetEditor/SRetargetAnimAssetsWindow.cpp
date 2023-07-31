// Copyright Epic Games, Inc. All Rights Reserved.

#include "RetargetEditor/SRetargetAnimAssetsWindow.h"

#include "AnimPose.h"
#include "AnimPreviewInstance.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "ObjectEditorUtils.h"
#include "SSkeletonWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SSeparator.h"
#include "PropertyCustomizationHelpers.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Misc/ScopedSlowTask.h"
#include "RetargetEditor/IKRetargeterController.h"
#include "Retargeter/IKRetargeter.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "RetargetAnimAssetWindow"

void SSelectExportPathDialog::Construct(const FArguments& InArgs)
{
	AssetPath = FText::FromString(FPackageName::GetLongPackagePath(InArgs._DefaultAssetPath.ToString()));

	if(AssetPath.IsEmpty())
	{
		AssetPath = FText::FromString(TEXT("/Game"));
	}

	FPathPickerConfig PathPickerConfig;
	PathPickerConfig.DefaultPath = AssetPath.ToString();
	PathPickerConfig.OnPathSelected = FOnPathSelected::CreateSP(this, &SSelectExportPathDialog::OnPathChange);
	PathPickerConfig.bAddDefaultPath = true;

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	SWindow::Construct(SWindow::FArguments()
		.Title(LOCTEXT("SSelectExportPathDialog_Title", "Select Export Path"))
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		.IsTopmostWindow(true)
		.ClientSize(FVector2D(450, 450))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot() // Add user input block
			.Padding(2)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					SNew(SVerticalBox)

					+SVerticalBox::Slot()
					.AutoHeight()
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
					.Text(LOCTEXT("OK", "OK"))
					.OnClicked(this, &SSelectExportPathDialog::OnButtonClick, EAppReturnType::Ok)
				]
				+SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
					.Text(LOCTEXT("Cancel", "Cancel"))
					.OnClicked(this, &SSelectExportPathDialog::OnButtonClick, EAppReturnType::Cancel)
				]
			]
		]);
}

void SSelectExportPathDialog::OnPathChange(const FString& NewPath)
{
	AssetPath = FText::FromString(NewPath);
}

FReply SSelectExportPathDialog::OnButtonClick(EAppReturnType::Type ButtonID)
{
	UserResponse = ButtonID;

	RequestDestroyWindow();

	return FReply::Handled();
}

EAppReturnType::Type SSelectExportPathDialog::ShowModal()
{
	GEditor->EditorAddModalWindow(SharedThis(this));
	return UserResponse;
}

FString SSelectExportPathDialog::GetAssetPath()
{
	return AssetPath.ToString();
}

void SRetargetPoseViewport::Construct(const FArguments& InArgs)
{
	SEditorViewport::Construct(SEditorViewport::FArguments());

	PreviewComponent = NewObject<UDebugSkelMeshComponent>();
	PreviewComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	PreviewScene.AddComponent(PreviewComponent, FTransform::Identity);

	SetSkeletalMesh(InArgs._SkeletalMesh);
}

void SRetargetPoseViewport::SetSkeletalMesh(USkeletalMesh* InSkeltalMesh)
{
	if(InSkeltalMesh == Mesh)
	{
		return;
	}
	
	Mesh = InSkeltalMesh;

	if(Mesh)
	{
		PreviewComponent->SetSkeletalMesh(Mesh);
		PreviewComponent->EnablePreview(true, nullptr);
		// todo add IK retargeter and set it to output the retarget pose
		PreviewComponent->PreviewInstance->SetForceRetargetBasePose(true);
		PreviewComponent->RefreshBoneTransforms(nullptr);

		//Place the camera at a good viewer position
		FBoxSphereBounds Bounds = Mesh->GetBounds();
		Client->FocusViewportOnBox(Bounds.GetBox(), true);
	}
	else
	{
		PreviewComponent->SetSkeletalMesh(nullptr);
	}

	Client->Invalidate();
}

SRetargetPoseViewport::SRetargetPoseViewport()
: PreviewScene(FPreviewScene::ConstructionValues())
{
}

bool SRetargetPoseViewport::IsVisible() const
{
	return true;
}

TSharedRef<FEditorViewportClient> SRetargetPoseViewport::MakeEditorViewportClient()
{
	TSharedPtr<FEditorViewportClient> EditorViewportClient = MakeShareable(new FRetargetPoseViewportClient(PreviewScene, SharedThis(this)));

	EditorViewportClient->ViewportType = LVT_Perspective;
	EditorViewportClient->bSetListenerPosition = false;
	EditorViewportClient->SetViewLocation(EditorViewportDefs::DefaultPerspectiveViewLocation);
	EditorViewportClient->SetViewRotation(EditorViewportDefs::DefaultPerspectiveViewRotation);

	EditorViewportClient->SetRealtime(false);
	EditorViewportClient->VisibilityDelegate.BindSP(this, &SRetargetPoseViewport::IsVisible);
	EditorViewportClient->SetViewMode(VMI_Lit);

	return EditorViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SRetargetPoseViewport::MakeViewportToolbar()
{
	return nullptr;
}


TSharedPtr<SWindow> SRetargetAnimAssetsWindow::DialogWindow;

void SRetargetAnimAssetsWindow::Construct(const FArguments& InArgs)
{
	AssetThumbnailPool = MakeShareable( new FAssetThumbnailPool(1024) );
	
	this->ChildSlot
	[
		SNew (SHorizontalBox)
		
		+SHorizontalBox::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		.AutoWidth()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			.Padding(0, 5)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("DuplicateAndRetarget_SourceTitle", "Source Skeletal Mesh"))
						.Font(FAppStyle::GetFontStyle("Persona.RetargetManager.BoldFont"))
						.AutoWrapText(true)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(5, 5)
					[
						SAssignNew(SourceViewport, SRetargetPoseViewport)
						.SkeletalMesh(BatchContext.SourceMesh)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(5, 5)
					[
						SNew(SObjectPropertyEntryBox)
						.AllowedClass(USkeletalMesh::StaticClass())
						.AllowClear(true)
						.DisplayUseSelected(true)
						.DisplayBrowse(true)
						.DisplayThumbnail(true)
						.ThumbnailPool(AssetThumbnailPool)
						.IsEnabled_Lambda([this]()
						{
							if (!BatchContext.IKRetargetAsset)
							{
								return false;
							}
							
							return BatchContext.IKRetargetAsset->GetSourceIKRig() != nullptr;
						})
						.ObjectPath(this, &SRetargetAnimAssetsWindow::GetCurrentSourceMeshPath)
						.OnObjectChanged(this, &SRetargetAnimAssetsWindow::SourceMeshAssigned)
					]
				]

				+SHorizontalBox::Slot()
				.Padding(5)
				.AutoWidth()
				[
					SNew(SSeparator)
					.Orientation(Orient_Vertical)
				]

				+SHorizontalBox::Slot()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("DuplicateAndRetarget_TargetTitle", "Target Skeletal Mesh"))
						.Font(FAppStyle::GetFontStyle("Persona.RetargetManager.BoldFont"))
						.AutoWrapText(true)
					]
				
					+SVerticalBox::Slot()
					.AutoHeight()
					.Padding(5, 5)
					[
						SAssignNew(TargetViewport, SRetargetPoseViewport)
						.SkeletalMesh(nullptr)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(5, 5)
					[
						SNew(SObjectPropertyEntryBox)
						.AllowedClass(USkeletalMesh::StaticClass())
						.AllowClear(true)
						.DisplayUseSelected(true)
						.DisplayBrowse(true)
						.DisplayThumbnail(true)
						.ThumbnailPool(AssetThumbnailPool)
						.IsEnabled_Lambda([this]()
						{
							if (!BatchContext.IKRetargetAsset)
							{
								return false;
							}
							
							return BatchContext.IKRetargetAsset->GetTargetIKRig() != nullptr;
						})
						.ObjectPath(this, &SRetargetAnimAssetsWindow::GetCurrentTargetMeshPath)
						.OnObjectChanged(this, &SRetargetAnimAssetsWindow::TargetMeshAssigned)
					]
				]
			]
		]

		+SHorizontalBox::Slot()
		.Padding(5)
		.AutoWidth()
		[
			SNew(SSeparator)
			.Orientation(Orient_Vertical)
		]
			
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0, 5)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("DuplicateAndRetarget_RetargetAsset", "IK Retargeter"))
				.Font(FAppStyle::GetFontStyle("Persona.RetargetManager.BoldFont"))
				.AutoWrapText(true)
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.Padding(2)
			[
				SNew(SObjectPropertyEntryBox)
				.AllowedClass(UIKRetargeter::StaticClass())
				.AllowClear(true)
				.DisplayUseSelected(true)
				.DisplayBrowse(true)
				.DisplayThumbnail(true)
				.ThumbnailPool(AssetThumbnailPool)
				.ObjectPath(this, &SRetargetAnimAssetsWindow::GetCurrentRetargeterPath)
				.OnObjectChanged(this, &SRetargetAnimAssetsWindow::RetargeterAssigned)
			]

			+SVerticalBox::Slot()
			.Padding(5)
			.AutoHeight()
			[
				SNew(SSeparator)
				.Orientation(Orient_Horizontal)
			]
			
			+SVerticalBox::Slot()
			[	
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(2, 3)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Font(FAppStyle::GetFontStyle("Persona.RetargetManager.SmallBoldFont"))
					.Text(LOCTEXT("DuplicateAndRetarget_RenameLabel", "Rename New Assets"))
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2, 1)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					[
						SNew(STextBlock).Text(LOCTEXT("DuplicateAndRetarget_Prefix", "Prefix"))
					]

					+SHorizontalBox::Slot()
					[
						SNew(SEditableTextBox)
							.Text(this, &SRetargetAnimAssetsWindow::GetPrefixName)
							.MinDesiredWidth(100)
							.OnTextChanged(this, &SRetargetAnimAssetsWindow::SetPrefixName)
							.IsReadOnly(false)
							.RevertTextOnEscape(true)
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2, 1)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					[
						SNew(STextBlock).Text(LOCTEXT("DuplicateAndRetarget_Suffix", "Suffix"))
					]

					+SHorizontalBox::Slot()
					[
						SNew(SEditableTextBox)
							.Text(this, &SRetargetAnimAssetsWindow::GetSuffixName)
							.MinDesiredWidth(100)
							.OnTextChanged(this, &SRetargetAnimAssetsWindow::SetSuffixName)
							.IsReadOnly(false)
							.RevertTextOnEscape(true)
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2, 1)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					[
						SNew(STextBlock).Text(LOCTEXT("DuplicateAndRetarget_Search", "Search "))
					]

					+SHorizontalBox::Slot()
					[
						SNew(SEditableTextBox)
							.Text(this, &SRetargetAnimAssetsWindow::GetReplaceFrom)
							.MinDesiredWidth(100)
							.OnTextChanged(this, &SRetargetAnimAssetsWindow::SetReplaceFrom)
							.IsReadOnly(false)
							.RevertTextOnEscape(true)
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2, 1)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					[
						SNew(STextBlock).Text(LOCTEXT("DuplicateAndRetarget_Replace", "Replace "))
					]

					+SHorizontalBox::Slot()
					[
						SNew(SEditableTextBox)
							.Text(this, &SRetargetAnimAssetsWindow::GetReplaceTo)
							.MinDesiredWidth(100)
							.OnTextChanged(this, &SRetargetAnimAssetsWindow::SetReplaceTo)
							.IsReadOnly(false)
							.RevertTextOnEscape(true)
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2, 3)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.Padding(5, 5)
					[
						SNew(STextBlock)
						.Text(this,  &SRetargetAnimAssetsWindow::GetExampleText)
						.Font(FAppStyle::GetFontStyle("Persona.RetargetManager.ItalicFont"))
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2, 3)
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("DuplicateAndRetarget_Folder", "Folder "))
						.Font(FAppStyle::GetFontStyle("Persona.RetargetManager.SmallBoldFont"))
					]

					+SHorizontalBox::Slot()
					.FillWidth(1)
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(this, &SRetargetAnimAssetsWindow::GetFolderPath)
					]

					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(LOCTEXT("DuplicateAndRetarget_ChangeFolder", "Change..."))
						.OnClicked(this, &SRetargetAnimAssetsWindow::GetExportFolder)
					]
				]

				+SVerticalBox::Slot()
				.Padding(5)
				.AutoHeight()
				[
					SNew(SSeparator)
					.Orientation(Orient_Horizontal)
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Fill)
				.Padding(2)
				[
					SNew(SCheckBox)
					.IsChecked(this, &SRetargetAnimAssetsWindow::IsRemappingReferencedAssets)
					.OnCheckStateChanged(this, &SRetargetAnimAssetsWindow::OnRemappingReferencedAssetsChanged)
					[
						SNew(STextBlock).Text(LOCTEXT("DuplicateAndRetarget_AllowRemap", "Remap Referenced Assets"))
					]
				]
			]

			+SVerticalBox::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			.Padding(2)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FAppStyle::GetMargin("StandardDialog.SlotPadding"))
				.MinDesiredSlotWidth(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
				+SUniformGridPanel::Slot(0, 0)
				[
					SNew(SButton).HAlign(HAlign_Center)
					.Text(LOCTEXT("RetargetOptions_Cancel", "Cancel"))
					.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
					.OnClicked(this, &SRetargetAnimAssetsWindow::OnCancel)
				]
				+SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton).HAlign(HAlign_Center)
					.Text(LOCTEXT("RetargetOptions_Apply", "Retarget"))
					.IsEnabled(this, &SRetargetAnimAssetsWindow::CanApply)
					.OnClicked(this, &SRetargetAnimAssetsWindow::OnApply)
					.HAlign(HAlign_Center)
					.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
				]
				
			]
		]
	];

	UpdateExampleText();
}

bool SRetargetAnimAssetsWindow::CanApply() const
{
	return BatchContext.IsValid();
}

FReply SRetargetAnimAssetsWindow::OnApply()
{
	CloseWindow();
	FIKRetargetBatchOperation BatchOperation;
	BatchOperation.RunRetarget(BatchContext);
	return FReply::Handled();
}

FReply SRetargetAnimAssetsWindow::OnCancel()
{
	CloseWindow();
	return FReply::Handled();
}

void SRetargetAnimAssetsWindow::CloseWindow()
{
	if ( DialogWindow.IsValid() )
	{
		DialogWindow->RequestDestroyWindow();
	}
}

void SRetargetAnimAssetsWindow::ShowWindow(TArray<UObject*> InSelectedAssets)
{	
	if(DialogWindow.IsValid())
	{
		FSlateApplication::Get().DestroyWindowImmediately(DialogWindow.ToSharedRef());
	}
	
	DialogWindow = SNew(SWindow)
		.Title(LOCTEXT("RetargetAssets", "Duplicate and Retarget Animation Assets"))
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		.HasCloseButton(true)
		.IsTopmostWindow(true)
		.SizingRule(ESizingRule::Autosized);
	
	TSharedPtr<class SRetargetAnimAssetsWindow> DialogWidget;
	TSharedPtr<SBorder> DialogWrapper =
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(4.0f)
		[
			SAssignNew(DialogWidget, SRetargetAnimAssetsWindow)
		];

	DialogWidget->BatchContext.AssetsToRetarget = FObjectEditorUtils::GetTypedWeakObjectPtrs<UObject>(InSelectedAssets);
	DialogWindow->SetOnWindowClosed(FRequestDestroyWindowOverride::CreateSP(DialogWidget.Get(), &SRetargetAnimAssetsWindow::OnDialogClosed));
	DialogWindow->SetContent(DialogWrapper.ToSharedRef());
	
	FSlateApplication::Get().AddWindow(DialogWindow.ToSharedRef());
}

void SRetargetAnimAssetsWindow::OnDialogClosed(const TSharedRef<SWindow>& Window)
{
	DialogWindow = nullptr;
}

void SRetargetAnimAssetsWindow::SourceMeshAssigned(const FAssetData& InAssetData)
{
	USkeletalMesh* Mesh = Cast<USkeletalMesh>(InAssetData.GetAsset());
	BatchContext.SourceMesh = Mesh;
	SourceViewport->SetSkeletalMesh(BatchContext.SourceMesh);
}

void SRetargetAnimAssetsWindow::TargetMeshAssigned(const FAssetData& InAssetData)
{
	USkeletalMesh* Mesh = Cast<USkeletalMesh>(InAssetData.GetAsset());
	BatchContext.TargetMesh = Mesh;
	TargetViewport->SetSkeletalMesh(BatchContext.TargetMesh);
}

FString SRetargetAnimAssetsWindow::GetCurrentSourceMeshPath() const
{
	return BatchContext.SourceMesh ? BatchContext.SourceMesh->GetPathName() : FString("");
}

FString SRetargetAnimAssetsWindow::GetCurrentTargetMeshPath() const
{
	return BatchContext.TargetMesh ? BatchContext.TargetMesh->GetPathName() : FString("");
}

FString SRetargetAnimAssetsWindow::GetCurrentRetargeterPath() const
{
	return BatchContext.IKRetargetAsset ? BatchContext.IKRetargetAsset->GetPathName() : FString("");
}

void SRetargetAnimAssetsWindow::RetargeterAssigned(const FAssetData& InAssetData)
{
	UIKRetargeter* InRetargeter = Cast<UIKRetargeter>(InAssetData.GetAsset());
	BatchContext.IKRetargetAsset = InRetargeter;
	const UIKRetargeterController* Controller = UIKRetargeterController::GetController(InRetargeter);
	SourceMeshAssigned(FAssetData(Controller->GetPreviewMesh(ERetargetSourceOrTarget::Source)));
	TargetMeshAssigned(FAssetData(Controller->GetPreviewMesh(ERetargetSourceOrTarget::Target)));
}

ECheckBoxState SRetargetAnimAssetsWindow::IsRemappingReferencedAssets() const
{
	return BatchContext.bRemapReferencedAssets ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SRetargetAnimAssetsWindow::OnRemappingReferencedAssetsChanged(ECheckBoxState InNewRadioState)
{
	BatchContext.bRemapReferencedAssets = (InNewRadioState == ECheckBoxState::Checked);
}

FText SRetargetAnimAssetsWindow::GetPrefixName() const
{
	return FText::FromString(BatchContext.NameRule.Prefix);
}

void SRetargetAnimAssetsWindow::SetPrefixName(const FText &InText)
{
	BatchContext.NameRule.Prefix = InText.ToString();
	UpdateExampleText();
}

FText SRetargetAnimAssetsWindow::GetSuffixName() const
{
	return FText::FromString(BatchContext.NameRule.Suffix);
}

void SRetargetAnimAssetsWindow::SetSuffixName(const FText &InText)
{
	BatchContext.NameRule.Suffix = InText.ToString();
	UpdateExampleText();
}

FText SRetargetAnimAssetsWindow::GetReplaceFrom() const
{
	return FText::FromString(BatchContext.NameRule.ReplaceFrom);
}

void SRetargetAnimAssetsWindow::SetReplaceFrom(const FText &InText)
{
	BatchContext.NameRule.ReplaceFrom = InText.ToString();
	UpdateExampleText();
}

FText SRetargetAnimAssetsWindow::GetReplaceTo() const
{
	return FText::FromString(BatchContext.NameRule.ReplaceTo);
}

void SRetargetAnimAssetsWindow::SetReplaceTo(const FText &InText)
{
	BatchContext.NameRule.ReplaceTo = InText.ToString();
	UpdateExampleText();
}

FText SRetargetAnimAssetsWindow::GetExampleText() const
{
	return ExampleText;
}

void SRetargetAnimAssetsWindow::UpdateExampleText()
{
	const FString ReplaceFrom = FString::Printf(TEXT("Old Name : ###%s###"), *BatchContext.NameRule.ReplaceFrom);
	const FString ReplaceTo = FString::Printf(TEXT("New Name : %s###%s###%s"), *BatchContext.NameRule.Prefix, *BatchContext.NameRule.ReplaceTo, *BatchContext.NameRule.Suffix);

	ExampleText = FText::FromString(FString::Printf(TEXT("%s\n%s"), *ReplaceFrom, *ReplaceTo));
}

FText SRetargetAnimAssetsWindow::GetFolderPath() const
{
	return FText::FromString(BatchContext.NameRule.FolderPath);
}

FReply SRetargetAnimAssetsWindow::GetExportFolder()
{
	TSharedRef<SSelectExportPathDialog> Dialog = SNew(SSelectExportPathDialog)
	.DefaultAssetPath(FText::FromString(BatchContext.NameRule.FolderPath));
	
	if(Dialog->ShowModal() != EAppReturnType::Cancel)
	{
		BatchContext.NameRule.FolderPath = Dialog->GetAssetPath();
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE

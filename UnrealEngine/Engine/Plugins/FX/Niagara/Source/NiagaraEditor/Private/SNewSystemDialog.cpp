// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNewSystemDialog.h"
#include "NiagaraEditorModule.h"
#include "NiagaraEditorStyle.h"
#include "NiagaraSystem.h"
#include "SNiagaraAssetPickerList.h"

#include "AssetRegistry/AssetData.h"

#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Styling/AppStyle.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"

typedef SItemSelector<FText, FAssetData> SNiagaraAssetItemSelector;

#define LOCTEXT_NAMESPACE "SNewSystemDialog"

void SNewSystemDialog::Construct(const FArguments& InArgs)
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	FNiagaraAssetPickerListViewOptions DisplayAllViewOptions;
	DisplayAllViewOptions.SetExpandTemplateAndLibraryAssets(true);
	DisplayAllViewOptions.SetCategorizeLibraryAssets(true);
	DisplayAllViewOptions.SetCategorizeUserDefinedCategory(true);
	DisplayAllViewOptions.SetAddLibraryOnlyCheckbox(true);
	
	SNiagaraTemplateTabBox::FNiagaraTemplateTabOptions AllTabOptions;
	AllTabOptions.ChangeTabState(ENiagaraScriptTemplateSpecification::Template, true);
	AllTabOptions.ChangeTabState(ENiagaraScriptTemplateSpecification::None, true);
	AllTabOptions.ChangeTabState(ENiagaraScriptTemplateSpecification::Behavior, true);

	SNiagaraTemplateTabBox::FNiagaraTemplateTabOptions TemplateAndBehaviorsOnlyTabOptions;
	TemplateAndBehaviorsOnlyTabOptions.ChangeTabState(ENiagaraScriptTemplateSpecification::Template, true);
	TemplateAndBehaviorsOnlyTabOptions.ChangeTabState(ENiagaraScriptTemplateSpecification::Behavior, true);

	FNiagaraEditorModule::Get().PreloadSelectablePluginAssetsByClass(UNiagaraEmitter::StaticClass());
	SAssignNew(EmitterAssetPicker, SNiagaraAssetPickerList, UNiagaraEmitter::StaticClass())
		.OnTemplateAssetActivated(this, &SNewSystemDialog::OnEmitterAssetsActivated)
		.ViewOptions(DisplayAllViewOptions)
		.TabOptions(AllTabOptions)
		.bAllowMultiSelect(true);

	FNiagaraEditorModule::Get().PreloadSelectablePluginAssetsByClass(UNiagaraSystem::StaticClass());
	SAssignNew(TemplateBehaviorAssetPicker, SNiagaraAssetPickerList, UNiagaraSystem::StaticClass())
		.OnTemplateAssetActivated(this, &SNewSystemDialog::ConfirmSelection)
		.ViewOptions(DisplayAllViewOptions)
		.TabOptions(TemplateAndBehaviorsOnlyTabOptions);

	SAssignNew(CopySystemAssetPicker, SNiagaraAssetPickerList, UNiagaraSystem::StaticClass())
		.OnTemplateAssetActivated(this, &SNewSystemDialog::ConfirmSelection)
		.ViewOptions(DisplayAllViewOptions)
		.TabOptions(AllTabOptions);
	
	SNiagaraNewAssetDialog::Construct(SNiagaraNewAssetDialog::FArguments(), UNiagaraSystem::StaticClass()->GetFName(), LOCTEXT("AssetTypeName", "system"),
		{
			SNiagaraNewAssetDialog::FNiagaraNewAssetDialogOption(
				LOCTEXT("CreateFromSelectedEmittersLabel", "New system from selected emitter(s)"),
				LOCTEXT("CreateFromSelectedEmittersDescription", "Choose a mix of emitters (inherited) and emitter templates/behavior examples (no inheritance)"),
				LOCTEXT("ProjectEmittersLabel", "Select Emitters to Add"),
				SNiagaraNewAssetDialog::FOnGetSelectedAssetsFromPicker::CreateSP(this, &SNewSystemDialog::GetSelectedProjectEmiterAssets),
				SNiagaraNewAssetDialog::FOnSelectionConfirmed(),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding(0, 0, 0, 10)
				[
					EmitterAssetPicker.ToSharedRef()
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.NewAssetDialog.SubHeaderText")
						.Text(LOCTEXT("SelectedEmittersLabel", "Emitters to Add:"))
					]
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "FlatButton.Success")
						.IsEnabled(this, &SNewSystemDialog::IsAddEmittersToSelectionButtonEnabled)
						.OnClicked(this, &SNewSystemDialog::AddEmittersToSelectionButtonClicked)
						.ToolTipText(LOCTEXT("AddSelectedEmitterToolTip", "Add the selected emitter to the collection\n of emitters to be added to the new system."))
						.Content()
						[
							SNew(SBox)
							.WidthOverride(32.0f)
							.HeightOverride(16.0f)
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.TextStyle(FAppStyle::Get(), "NormalText.Important")
								.Font(FAppStyle::Get().GetFontStyle("FontAwesome.10"))
								.Text(FText::FromString(FString(TEXT("\xf067"))) /*fa-plus*/)
							]
						]
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(SelectedEmitterBox, SWrapBox)
					.UseAllottedSize(true)
				],
				EmitterAssetPicker->GetSearchBox()
				),
			SNiagaraNewAssetDialog::FNiagaraNewAssetDialogOption(
				LOCTEXT("CreateFromTemplateLabel", "New system from a template or behavior example"),
				LOCTEXT("CreateFromTemplateDescription", "The new system will be derived from a system template or behavior example"),
				LOCTEXT("TemplateLabel", "Select a System Template"),
				SNiagaraNewAssetDialog::FOnGetSelectedAssetsFromPicker::CreateSP(this, &SNewSystemDialog::GetSelectedSystemTemplateAssets),
				SNiagaraNewAssetDialog::FOnSelectionConfirmed(),
				TemplateBehaviorAssetPicker.ToSharedRef(), TemplateBehaviorAssetPicker->GetSearchBox()
				),
			SNiagaraNewAssetDialog::FNiagaraNewAssetDialogOption(
				LOCTEXT("CreateFromOtherSystemLabel", "Copy existing system"),
				LOCTEXT("CreateFromOtherSystemDescription", "Copies an existing system from your project content and maintains any inheritance of the included emitters"),
				LOCTEXT("ProjectSystemsLabel", "Select a Project System"),
				SNiagaraNewAssetDialog::FOnGetSelectedAssetsFromPicker::CreateSP(this, &SNewSystemDialog::GetSelectedProjectSystemAssets),
				SNiagaraNewAssetDialog::FOnSelectionConfirmed(),
				CopySystemAssetPicker.ToSharedRef(), CopySystemAssetPicker->GetSearchBox()
				),
			SNiagaraNewAssetDialog::FNiagaraNewAssetDialogOption(
				LOCTEXT("CreateEmptyLabel", "Create empty system"),
				LOCTEXT("CreateEmptyDescription", "Create an empty system with no emitters or emitter templates"),
				LOCTEXT("EmptyLabel", "Empty System"),
				SNiagaraNewAssetDialog::FOnGetSelectedAssetsFromPicker(),
				SNiagaraNewAssetDialog::FOnSelectionConfirmed(),
				SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("NoOptionsLabel", "No Options"))
				])
		});
}

TOptional<FAssetData> SNewSystemDialog::GetSelectedSystemAsset() const
{
	const TArray<FAssetData>& AllSelectedAssets = GetSelectedAssets();
	TArray<FAssetData> SelectedSystemAssets;
	for (const FAssetData& SelectedAsset : AllSelectedAssets)
	{
		if (SelectedAsset.AssetClassPath == UNiagaraSystem::StaticClass()->GetClassPathName())
		{
			SelectedSystemAssets.Add(SelectedAsset);
		}
	}
	if (SelectedSystemAssets.Num() == 1)
	{
		return TOptional<FAssetData>(SelectedSystemAssets[0]);
	}
	return TOptional<FAssetData>();
}

TArray<FAssetData> SNewSystemDialog::GetSelectedEmitterAssets() const
{
	const TArray<FAssetData>& AllSelectedAssets = GetSelectedAssets();
	TArray<FAssetData> ConfirmedSelectedEmitterAssets;
	for (const FAssetData& SelectedAsset : AllSelectedAssets)
	{
		if (SelectedAsset.AssetClassPath == UNiagaraEmitter::StaticClass()->GetClassPathName())
		{
			ConfirmedSelectedEmitterAssets.Add(SelectedAsset);
		}
	}
	return ConfirmedSelectedEmitterAssets;
}

void SNewSystemDialog::OnEmitterAssetsActivated(const FAssetData& ActivatedTemplateAsset)
{
	TArray<FAssetData> ActivatedTemplateAssets;
	ActivatedTemplateAssets.Add(ActivatedTemplateAsset);
	AddEmitterAssetsToSelection(ActivatedTemplateAssets);
}

void SNewSystemDialog::GetSelectedSystemTemplateAssets(TArray<FAssetData>& OutSelectedAssets)
{
	OutSelectedAssets.Append(TemplateBehaviorAssetPicker->GetSelectedAssets());
}

void SNewSystemDialog::GetSelectedProjectSystemAssets(TArray<FAssetData>& OutSelectedAssets)
{
	OutSelectedAssets.Append(CopySystemAssetPicker->GetSelectedAssets());
}

void SNewSystemDialog::GetSelectedProjectEmiterAssets(TArray<FAssetData>& OutSelectedAssets)
{
	OutSelectedAssets.Append(SelectedEmitterAssets);
}

bool SNewSystemDialog::IsAddEmittersToSelectionButtonEnabled() const
{
	return EmitterAssetPicker->GetSelectedAssets().Num() > 0;
}

FReply SNewSystemDialog::AddEmittersToSelectionButtonClicked()
{
	TArray<FAssetData> SelectedEmitterAssetsFromPicker = EmitterAssetPicker->GetSelectedAssets();
	AddEmitterAssetsToSelection(SelectedEmitterAssetsFromPicker);
	return FReply::Handled();
}

void SNewSystemDialog::AddEmitterAssetsToSelection(const TArray<FAssetData>& EmitterAssets)
{
	for (const FAssetData& SelectedEmitterAsset : EmitterAssets)
	{
		TSharedPtr<SWidget> SelectedEmitterWidget;
		SelectedEmitterBox->AddSlot()
			.Padding(FMargin(0, 0, 5, 0))
			[
				SAssignNew(SelectedEmitterWidget, SBorder)
				.BorderImage(FNiagaraEditorStyle::Get().GetBrush("NiagaraEditor.NewAssetDialog.SubBorder"))
				.BorderBackgroundColor(FNiagaraEditorStyle::Get().GetColor("NiagaraEditor.NewAssetDialog.SubBorderColor"))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(5, 0, 0, 0)
					[
						SNew(STextBlock)
						.Text(FText::FromName(SelectedEmitterAsset.AssetName))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(2, 0, 0, 0)
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
						.OnClicked(this, &SNewSystemDialog::RemoveEmitterFromSelectionButtonClicked, SelectedEmitterAsset)
						.ToolTipText(LOCTEXT("RemoveSelectedEmitterToolTip", "Remove the selected emitter from the collection\n of emitters to be added to the new system."))
						[
							SNew(STextBlock)
							//.TextStyle(FAppStyle::Get(), "NormalText.Important")
							.Font(FAppStyle::Get().GetFontStyle("FontAwesome.10"))
							.Text(FText::FromString(FString(TEXT("\xf057"))) /*times-circle*/)
							.ColorAndOpacity(FLinearColor(.8f, .2f, .2f, 1.0f))
						]
					]
				]
			];
		SelectedEmitterAssets.Add(SelectedEmitterAsset);
		SelectedEmitterAssetWidgets.Add(SelectedEmitterWidget);
	}
}

FReply SNewSystemDialog::RemoveEmitterFromSelectionButtonClicked(FAssetData EmitterAsset)
{
	int32 SelectionIndex = SelectedEmitterAssets.IndexOfByKey(EmitterAsset);
	if (SelectionIndex != INDEX_NONE)
	{
		SelectedEmitterAssets.RemoveAt(SelectionIndex);
		SelectedEmitterBox->RemoveSlot(SelectedEmitterAssetWidgets[SelectionIndex].ToSharedRef());
		SelectedEmitterAssetWidgets.RemoveAt(SelectionIndex);
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE

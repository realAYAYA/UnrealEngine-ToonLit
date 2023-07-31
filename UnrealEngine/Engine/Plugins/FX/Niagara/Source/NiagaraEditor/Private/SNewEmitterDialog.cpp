// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNewEmitterDialog.h"
#include "NiagaraEditorStyle.h"
#include "NiagaraEmitter.h"
#include "SNiagaraAssetPickerList.h"
#include "SNiagaraNewAssetDialog.h"

#include "AssetRegistry/AssetData.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"

typedef SItemSelector<FText, FAssetData> SNiagaraAssetItemSelector;

#define LOCTEXT_NAMESPACE "SNewEmitterDialog"

void SNewEmitterDialog::Construct(const FArguments& InArgs)
{
	FNiagaraAssetPickerListViewOptions DisplayAllViewOptions;
	DisplayAllViewOptions.SetExpandTemplateAndLibraryAssets(true);
	DisplayAllViewOptions.SetCategorizeLibraryAssets(true);
	DisplayAllViewOptions.SetCategorizeUserDefinedCategory(true);
	DisplayAllViewOptions.SetAddLibraryOnlyCheckbox(true);

	SNiagaraTemplateTabBox::FNiagaraTemplateTabOptions TabOptions;
	TabOptions.ChangeTabState(ENiagaraScriptTemplateSpecification::Template, true);
	TabOptions.ChangeTabState(ENiagaraScriptTemplateSpecification::None, true);
	TabOptions.ChangeTabState(ENiagaraScriptTemplateSpecification::Behavior, true);

	SAssignNew(NewAssetPicker, SNiagaraAssetPickerList, UNiagaraEmitter::StaticClass())
		.OnTemplateAssetActivated(this, &SNewEmitterDialog::ConfirmSelection)
		.ViewOptions(DisplayAllViewOptions)
		.TabOptions(TabOptions);

	SAssignNew(CopyAssetPicker, SNiagaraAssetPickerList, UNiagaraEmitter::StaticClass())
		.OnTemplateAssetActivated(this, &SNewEmitterDialog::ConfirmSelection)
		.ViewOptions(DisplayAllViewOptions)
		.TabOptions(TabOptions);
	
	SNiagaraNewAssetDialog::Construct(SNiagaraNewAssetDialog::FArguments(), UNiagaraEmitter::StaticClass()->GetFName(), LOCTEXT("AssetTypeName", "emitter"),
		{
			SNiagaraNewAssetDialog::FNiagaraNewAssetDialogOption(
				LOCTEXT("CreateFromEmitterLabel", "New emitter"),
				LOCTEXT("CreateFromEmitterDescription", "Create a new emitter from a template or behavior emitter (no inheritance) or from a parent (inheritance)"),
				LOCTEXT("EmitterPickerHeader", "Select an Emitter"),
				SNiagaraNewAssetDialog::FOnGetSelectedAssetsFromPicker::CreateSP(this, &SNewEmitterDialog::GetSelectedEmitterAssets_NewAssets),
				SNiagaraNewAssetDialog::FOnSelectionConfirmed::CreateSP(this, &SNewEmitterDialog::CheckUseInheritance),
				NewAssetPicker.ToSharedRef(), NewAssetPicker->GetSearchBox() 
				),
			SNiagaraNewAssetDialog::FNiagaraNewAssetDialogOption(
				LOCTEXT("CreateFromOtherEmitterLabel", "Copy existing emitter"),
				LOCTEXT("CreateFromOtherEmitterDescription", "Copies an existing emitter from your project content"),
				LOCTEXT("ProjectEmitterPickerHeader", "Select a Project Emitter"),
				SNiagaraNewAssetDialog::FOnGetSelectedAssetsFromPicker::CreateSP(this, &SNewEmitterDialog::GetSelectedEmitterAssets_CopyAssets),
				SNiagaraNewAssetDialog::FOnSelectionConfirmed::CreateLambda([&]()
				{
					bUseInheritance = false;
				}),
				CopyAssetPicker.ToSharedRef(), CopyAssetPicker->GetSearchBox()
				),
			SNiagaraNewAssetDialog::FNiagaraNewAssetDialogOption(
				LOCTEXT("CreateEmptyLabel", "Create an empty emitter"),
				LOCTEXT("CreateEmptyDescription", "Create an empty emitter with no modules or renderers"),
				LOCTEXT("EmptyLabel", "Empty Emitter"),
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

TOptional<FAssetData> SNewEmitterDialog::GetSelectedEmitterAsset() const
{
	const TArray<FAssetData>& SelectedEmitterAssets = GetSelectedAssets();
	if (SelectedEmitterAssets.Num() > 0)
	{
		return TOptional<FAssetData>(SelectedEmitterAssets[0]);
	}
	return TOptional<FAssetData>();
}

bool SNewEmitterDialog::GetUseInheritance() const
{
	return bUseInheritance;
}

void SNewEmitterDialog::GetSelectedEmitterAssets_NewAssets(TArray<FAssetData>& OutSelectedAssets) const
{
	OutSelectedAssets.Append(NewAssetPicker->GetSelectedAssets());
}

void SNewEmitterDialog::GetSelectedEmitterAssets_CopyAssets(TArray<FAssetData>& OutSelectedAssets) const
{
	OutSelectedAssets.Append(CopyAssetPicker->GetSelectedAssets());
}

void SNewEmitterDialog::InheritanceOptionConfirmed()
{
	bUseInheritance = true;
}

void SNewEmitterDialog::CheckUseInheritance()
{
	TOptional<FAssetData> EmitterAssetData = GetSelectedEmitterAsset();

	if(EmitterAssetData.IsSet())
	{
		UNiagaraEmitter* Emitter = Cast<UNiagaraEmitter>(EmitterAssetData.GetValue().GetAsset());
		bUseInheritance = Emitter->TemplateSpecification == ENiagaraScriptTemplateSpecification::None;
	}
}

#undef LOCTEXT_NAMESPACE

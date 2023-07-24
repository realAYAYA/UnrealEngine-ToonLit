// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stack/SNiagaraStackFunctionInputCollection.h"

#include "NiagaraEditorWidgetsStyle.h"
#include "ViewModels/Stack/NiagaraStackFunctionInputCollection.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SWrapBox.h"

void SNiagaraStackFunctionInputCollection::Construct(const FArguments& InArgs, UNiagaraStackFunctionInputCollectionBase* InInputCollection)
{
	InputCollection = InInputCollection;
	InputCollection->OnStructureChanged().AddSP(this, &SNiagaraStackFunctionInputCollection::InputCollectionStructureChanged);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 2)
		[
			SNew(STextBlock)
			.TextStyle(FNiagaraEditorWidgetsStyle::Get(), "NiagaraEditor.Stack.ItemText")
			.ToolTipText_UObject(InputCollection, &UNiagaraStackEntry::GetTooltipText)
			.Text_UObject(InputCollection, &UNiagaraStackEntry::GetDisplayName)
			.IsEnabled_UObject(InputCollection, &UNiagaraStackEntry::GetOwnerIsEnabled)
			.Visibility(this, &SNiagaraStackFunctionInputCollection::GetLabelVisibility)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4, 2)
		[
			SAssignNew(SectionSelectorBox, SWrapBox)
			.UseAllottedSize(true)
			.InnerSlotPadding(FVector2D(4, 4))
		]
	];

	ConstructSectionButtons();
}

EVisibility SNiagaraStackFunctionInputCollection::GetLabelVisibility() const
{
	return InputCollection->GetShouldDisplayLabel() ? EVisibility::Visible : EVisibility::Collapsed;
}

void SNiagaraStackFunctionInputCollection::ConstructSectionButtons()
{
	SectionSelectorBox->ClearChildren();
	for (FText Section : InputCollection->GetSections())
	{
		SectionSelectorBox->AddSlot()
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "DetailsView.SectionButton")
			.OnCheckStateChanged(this, &SNiagaraStackFunctionInputCollection::OnSectionChecked, Section)
			.IsChecked(this, &SNiagaraStackFunctionInputCollection::GetSectionCheckState, Section)
			[
				SNew(STextBlock)
				.TextStyle(FAppStyle::Get(), "SmallText")
				.Text(Section)
			]
		];
	}
}

void SNiagaraStackFunctionInputCollection::InputCollectionStructureChanged(ENiagaraStructureChangedFlags StructureChangedFlags)
{
	ConstructSectionButtons();
}

ECheckBoxState SNiagaraStackFunctionInputCollection::GetSectionCheckState(FText Section) const
{
	return Section.IdenticalTo(InputCollection->GetActiveSection()) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SNiagaraStackFunctionInputCollection::OnSectionChecked(ECheckBoxState CheckState, FText Section)
{
	if (CheckState == ECheckBoxState::Checked)
	{
		InputCollection->SetActiveSection(Section);
	}
}
// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEEditorRuntimesWidget.h"

#include "NNE.h"
#include "SlateOptMacros.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SNNEEditorRuntimesWidget"

namespace UE::NNEEditor::Private
{

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SRuntimesWidget::Construct(const FArguments& InArgs)
{
	TargetRuntimes = InArgs._TargetRuntimes;
	OnTargetRuntimesChanged = InArgs._OnTargetRuntimesChanged;

	TSharedPtr<SWrapBox> WrapBox;

	ChildSlot
	[
		SNew(SBox)
		[
			SNew(SVerticalBox)
			+
			SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(4.0)
			[
				SNew(STextBlock).Text(FText::FromString("Target Runtimes"))
				.Font(FAppStyle::GetFontStyle("PropertyWindow.BoldFont"))
			]
			+
			SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(4.0)
			[
				SAssignNew(WrapBox, SWrapBox)
				.PreferredWidth(300.0f)
			]
		]
	];

	InitCheckBoxNames();

	for (int32 i = 0; i < CheckBoxNames.Num(); ++i)
	{
		WrapBox->AddSlot()
		.Padding(5)
		.VAlign(VAlign_Top)
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), i==0?"RadioButton":"Checkbox")//"All" in first position as a RadioButton.
			.IsChecked(this, &SRuntimesWidget::IsEntryChecked, i)
			.OnCheckStateChanged(this, &SRuntimesWidget::OnEntryChanged, i)
			[
				SNew(STextBlock).Text(FText::FromString(CheckBoxNames[i]))
				.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
			]
		];
	}
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SRuntimesWidget::OnEntryChanged(ECheckBoxState CheckType, int32 Index)
{
	check(Index < CheckBoxNames.Num());
	const FString& RuntimeName = CheckBoxNames[Index];

	if (!OnTargetRuntimesChanged.IsBound())
		return;

	if (RuntimeName == TEXT("All"))
	{
		if (CheckType == ECheckBoxState::Checked)
		{
			TArray<FString> EmptyTargetRuntimes;
			OnTargetRuntimesChanged.Execute(EmptyTargetRuntimes);
		}
	}
	else
	{
		TArray<FString, TInlineAllocator<10>> NewTargetRuntimes;
		NewTargetRuntimes = TargetRuntimes.Get();
		if (CheckType == ECheckBoxState::Checked)
		{
			NewTargetRuntimes.AddUnique(RuntimeName);
		}
		else
		{
			NewTargetRuntimes.Remove(RuntimeName);
		}
		OnTargetRuntimesChanged.Execute(NewTargetRuntimes);
	}
}

ECheckBoxState	SRuntimesWidget::IsEntryChecked(int32 Index) const
{
	check(Index < CheckBoxNames.Num());
	const FString& CheckBoxName = CheckBoxNames[Index];

	if (CheckBoxName == TEXT("All"))
	{
		return TargetRuntimes.Get().Num() == 0 ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	
	return TargetRuntimes.Get().Contains(CheckBoxName) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SRuntimesWidget::InitCheckBoxNames()
{
	CheckBoxNames.Reset();

	CheckBoxNames.Add(TEXT("All"));
	CheckBoxNames.Append(UE::NNE::GetAllRuntimeNames());

	//A runtime could possibly be in the target list but not registered to the editor
	for (const FString& EnabledRuntimeName : TargetRuntimes.Get())
	{
		if (CheckBoxNames.Contains(EnabledRuntimeName))
			continue;
		CheckBoxNames.Add(EnabledRuntimeName);
	}
}

} //UE::NNEEditor::Private

#undef LOCTEXT_NAMESPACE
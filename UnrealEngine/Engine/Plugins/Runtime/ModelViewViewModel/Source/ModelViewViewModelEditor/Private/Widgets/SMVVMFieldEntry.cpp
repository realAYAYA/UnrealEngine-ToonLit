// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMVVMFieldEntry.h"
#include "SMVVMFieldIcon.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"

#define LOCTEXT_NAMESPACE "MVVMFieldEntry"

namespace UE::MVVM
{

namespace Private
{

FText GetFieldDisplayName(const FMVVMConstFieldVariant& Field)
{
	if (!Field.IsEmpty())
	{
		if (Field.IsProperty())
		{
			return Field.GetProperty()->GetDisplayNameText();
		}
		if (Field.IsFunction())
		{
			return Field.GetFunction()->GetDisplayNameText();
		}
	}
	return LOCTEXT("None", "<None>");
}

FText GetFieldToolTip(const FMVVMConstFieldVariant& Field)
{
	if (!Field.IsEmpty())
	{
		if (Field.IsFunction() && Field.GetFunction() != nullptr)
		{
			return Field.GetFunction()->GetToolTipText();
		}
		if (Field.IsProperty() && Field.GetProperty() != nullptr)
		{
			return FText::Join(FText::FromString(TEXT("\n")), 
				Field.GetProperty()->GetToolTipText(), 
					FText::FromString(Field.GetProperty()->GetCPPType())
				);
		}
	}

	return FText::GetEmpty();
}

} // namespace Private

void SFieldEntry::Construct(const FArguments& InArgs)
{
	Field = InArgs._Field;
	TextStyle = InArgs._TextStyle;

	ChildSlot
	[
		SAssignNew(FieldBox, SHorizontalBox)
	];

	Refresh();
}

void SFieldEntry::Refresh()
{
	FieldBox->ClearChildren();

	TArray<FMVVMConstFieldVariant> Fields = Field.GetFields();
	for (int32 Index = 0; Index < Fields.Num(); ++Index)
	{
		FieldBox->AddSlot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SFieldIcon)
			.Field(Fields[Index])
		];

		FieldBox->AddSlot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.Padding(4, 0, 0, 0)
		.AutoWidth()
		[
			SNew(STextBlock)
			.TextStyle(TextStyle)
			.Clipping(EWidgetClipping::OnDemand)
			.Text(Private::GetFieldDisplayName(Fields[Index]))
		];

		// if not the last, then we need to add a chevron separator
		if (Index != Fields.Num() - 1)
		{
			FieldBox->AddSlot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(6, 0)
			.AutoWidth()
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("Icons.ChevronRight"))
			];
		}
	}

	if (Fields.Num() > 0)
	{
		SetToolTipText(Private::GetFieldToolTip(Fields.Last()));
	}
}

void SFieldEntry::SetField(const FMVVMBlueprintPropertyPath& InField)
{
	Field = InField;

	Refresh();
} 

} // namespace UE::MVVM

#undef LOCTEXT_NAMESPACE

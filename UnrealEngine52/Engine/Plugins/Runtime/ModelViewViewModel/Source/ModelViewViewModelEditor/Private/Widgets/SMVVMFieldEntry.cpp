// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMVVMFieldEntry.h"

#include "Bindings/MVVMBindingHelper.h"
#include "SMVVMFieldIcon.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"

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

FString GetFunctionSignature(const UFunction* Function)
{
	if (Function == nullptr)
	{
		return FString();
	}

	TStringBuilder<256> Signature;

	const FProperty* ReturnProperty = BindingHelper::GetReturnProperty(Function);
	if (ReturnProperty != nullptr)
	{
		Signature << ReturnProperty->GetCPPType();
	}
	else
	{
		Signature << TEXT("void");
	}

	Signature << TEXT(" ");
	Signature << Function->GetName();
	Signature << TEXT("(");

	TArray<const FProperty*> Arguments = BindingHelper::GetAllArgumentProperties(Function);
	Signature << FString::JoinBy(Arguments, TEXT(", "), 
		[](const FProperty* Property)
		{
			return Property->GetCPPType();
		}
	);

	Signature << TEXT(")");
	return Signature.ToString();
}

FText GetPathToolTip(TConstArrayView<FMVVMConstFieldVariant> Path)
{
	if (Path.IsEmpty())
	{
		return FText::GetEmpty();
	}

	FMVVMConstFieldVariant LastField = Path.Last();
	if (!LastField.IsEmpty())
	{
		FText LastToolTip, LastType;
		if (LastField.IsFunction() && LastField.GetFunction() != nullptr)
		{
			LastToolTip = LastField.GetFunction()->GetToolTipText();
			LastType = FText::FromString(GetFunctionSignature(LastField.GetFunction()));
		}
		else if (LastField.IsProperty() && LastField.GetProperty() != nullptr)
		{
			LastToolTip = LastField.GetProperty()->GetToolTipText();
			LastType = FText::Format(LOCTEXT("PropertyFormat", "Property: {0} {1}"),
				FText::FromString(LastField.GetProperty()->GetCPPType()),
				FText::FromString(LastField.GetProperty()->GetName())
			);
		}

		// only show the joined name if we've got more than 1 part to the path
		if (Path.Num() > 1)
		{
			TArray<FText> DisplayNames;
			for (const FMVVMConstFieldVariant& Field : Path)
			{
				DisplayNames.Add(GetFieldDisplayName(Field));
			}

			const FText JoinedDisplayName = FText::Join(FText::FromString(TEXT(".")), DisplayNames);

			return FText::Join(FText::FromString(TEXT("\n")),
				JoinedDisplayName,
				LastToolTip,
				LastType
			);
		}
		else
		{
			return FText::Join(FText::FromString(TEXT("\n")),
				LastToolTip,
				LastType
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

	int32 Index = 0;
	if (bShowOnlyLast && Fields.Num() > 0)
	{
		Index = Fields.Num() - 1;
	}

	for (; Index < Fields.Num(); ++Index)
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

	SetToolTipText(Private::GetPathToolTip(Fields));
}

void SFieldEntry::SetField(const FMVVMBlueprintPropertyPath& InField)
{
	Field = InField;

	Refresh();
} 

} // namespace UE::MVVM

#undef LOCTEXT_NAMESPACE

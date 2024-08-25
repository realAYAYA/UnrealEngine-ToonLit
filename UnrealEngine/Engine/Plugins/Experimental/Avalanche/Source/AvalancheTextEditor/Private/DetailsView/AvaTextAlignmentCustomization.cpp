// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTextAlignmentCustomization.h"

#include "AvaTextDefs.h"
#include "DetailWidgetRow.h"
#include "Slate/SAvaTextAlignmentWidget.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "FAvaTextAlignmentCustomization"

TSharedRef<IPropertyTypeCustomization> FAvaTextAlignmentCustomization::MakeInstance()
{
	return MakeShared<FAvaTextAlignmentCustomization>();
}

void FAvaTextAlignmentCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> StructPropertyHandle
	, FDetailWidgetRow& HeaderRow
	, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	AlignmentPropertyHandle = StructPropertyHandle;

	HeaderRow.NameContent()[AlignmentPropertyHandle->CreatePropertyNameWidget()]
	.ValueContent()
	[
		SNew(SBox)
		.ToolTipText(LOCTEXT("AlignTextStructTooltip", "Align Text"))
		[
			SNew(SAvaTextAlignmentWidget)				
			.TextAlignmentPropertyHandle(AlignmentPropertyHandle)
			.OnHorizontalAlignmentChanged(this, &FAvaTextAlignmentCustomization::OnHorizontalAlignmentChanged)
			.OnVerticalAlignmentChanged(this, &FAvaTextAlignmentCustomization::OnVerticalAlignmentChanged)
		]
	];
}

void FAvaTextAlignmentCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> StructPropertyHandle
	, IDetailChildrenBuilder& StructBuilder
	, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}

void FAvaTextAlignmentCustomization::OnHorizontalAlignmentChanged(EText3DHorizontalTextAlignment InHorizontalAlignment) const
{
	const TSharedPtr<IPropertyHandle> HorizontalAlignmentPropertyHandle = AlignmentPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAvaTextAlignment, HorizontalAlignment));
	check(HorizontalAlignmentPropertyHandle.IsValid());

	HorizontalAlignmentPropertyHandle->SetValue(static_cast<uint8>(InHorizontalAlignment));
}

void FAvaTextAlignmentCustomization::OnVerticalAlignmentChanged(EText3DVerticalTextAlignment InVerticalAlignment) const
{
	const TSharedPtr<IPropertyHandle> VerticalAlignmentPropertyHandle = AlignmentPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAvaTextAlignment, VerticalAlignment));
	check(VerticalAlignmentPropertyHandle.IsValid());

	VerticalAlignmentPropertyHandle->SetValue(static_cast<uint8>(InVerticalAlignment));
}

#undef LOCTEXT_NAMESPACE

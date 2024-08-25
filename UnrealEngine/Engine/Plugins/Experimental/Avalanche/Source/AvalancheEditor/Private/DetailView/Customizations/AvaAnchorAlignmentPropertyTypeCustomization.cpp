// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaAnchorAlignmentPropertyTypeCustomization.h"
#include "Alignment/SAvaAnchorAlignment.h"
#include "AvaDefs.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "AvaAnchorAlignmentPropertyTypeCustomization"

TSharedRef<IPropertyTypeCustomization> FAvaAnchorAlignmentPropertyTypeCustomization::MakeInstance()
{
	return MakeShared<FAvaAnchorAlignmentPropertyTypeCustomization>();
}

void FAvaAnchorAlignmentPropertyTypeCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InStructPropertyHandle
	, FDetailWidgetRow& HeaderRow
	, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructPropertyHandle = InStructPropertyHandle;

	check(StructPropertyHandle.IsValid());

	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SAvaAnchorAlignment)
			.UniformPadding(FMargin(5.0f, 2.0f))
			.Anchors(this, &FAvaAnchorAlignmentPropertyTypeCustomization::GetAnchors)
			.OnAnchorChanged(this, &FAvaAnchorAlignmentPropertyTypeCustomization::OnAnchorChanged)
		];
}

FAvaAnchorAlignment FAvaAnchorAlignmentPropertyTypeCustomization::GetAnchors() const
{
	if (!StructPropertyHandle.IsValid())
	{
		return FAvaAnchorAlignment();
	}
	
	void* AnchorsPtr = nullptr;
	StructPropertyHandle->GetValueData(AnchorsPtr);
	const FAvaAnchorAlignment* Anchors = static_cast<FAvaAnchorAlignment*>(AnchorsPtr);
	
	if (!Anchors)
	{
		return FAvaAnchorAlignment();
	}
	
	return *Anchors;
}

void FAvaAnchorAlignmentPropertyTypeCustomization::OnAnchorChanged(const FAvaAnchorAlignment NewAnchor) const
{
	if (!StructPropertyHandle.IsValid())
	{
		return;
	}
	
	void* AnchorsPtr = nullptr;
	StructPropertyHandle->GetValueData(AnchorsPtr);
	FAvaAnchorAlignment* Anchors = static_cast<FAvaAnchorAlignment*>(AnchorsPtr);

	if (!Anchors)
	{
		return;
	}

	StructPropertyHandle->NotifyPreChange();

	*Anchors = NewAnchor;

	StructPropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
}

#undef LOCTEXT_NAMESPACE

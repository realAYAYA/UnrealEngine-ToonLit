// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaSequenceTimeCustomization.h"
#include "AvaSequenceShared.h"
#include "DetailWidgetRow.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"

void FAvaSequenceTimeCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	TSharedPtr<IPropertyHandle> PositionTypeHandle = InPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAvaSequenceTime, TimeType));

	TSharedRef<SHorizontalBox> PropertyValueBox = SNew(SHorizontalBox);

	auto AddPropertySlot = [PropertyValueBox, InPropertyHandle](FName InPropertyName)
	{
		TSharedPtr<IPropertyHandle> ChildPropertyHandle = InPropertyHandle->GetChildHandle(InPropertyName);

		TSharedRef<SWidget> PropertyValueWidget = ChildPropertyHandle->CreatePropertyValueWidget();

		TAttribute<EVisibility>::FGetter VisibilityGetter = TAttribute<EVisibility>::FGetter::CreateLambda([ChildPropertyHandle]
			{
				return ChildPropertyHandle->IsEditable()
					? EVisibility::SelfHitTestInvisible
					: EVisibility::Collapsed;
			});

		PropertyValueWidget->SetVisibility(TAttribute<EVisibility>::Create(VisibilityGetter));

		PropertyValueBox->AddSlot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Fill)
			[
				PropertyValueWidget
			];
	};

	AddPropertySlot(GET_MEMBER_NAME_CHECKED(FAvaSequenceTime, Frame));
	AddPropertySlot(GET_MEMBER_NAME_CHECKED(FAvaSequenceTime, Seconds));
	AddPropertySlot(GET_MEMBER_NAME_CHECKED(FAvaSequenceTime, MarkLabel));

	PropertyValueBox->AddSlot()
		.AutoWidth()
		[
			InPropertyHandle->CreateDefaultPropertyButtonWidgets()
		];

	TSharedPtr<IPropertyHandle> HasTimeHandle = InPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAvaSequenceTime, bHasTimeConstraint));

	static const bool bDisplayDefaultPropertyButtons = false;

	InHeaderRow
		.NameContent()
		[
			SNew(SBox)
			.MinDesiredWidth(300.f)
			.MaxDesiredWidth(300.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					HasTimeHandle->CreatePropertyValueWidget(bDisplayDefaultPropertyButtons)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.f, 0.f, 0.f, 0.f)
				[
					InPropertyHandle->CreatePropertyNameWidget()
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.Padding(0.f, 0.f, 5.f, 0.f)
				.HAlign(HAlign_Right)
				[
					PositionTypeHandle->CreatePropertyValueWidget(bDisplayDefaultPropertyButtons)
				]
			]
		]
		.ValueContent()
		[
			PropertyValueBox
		];
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/PropertyAnimatorCoreEditorContextTypeCustomization.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "Subsystems/PropertyAnimatorCoreSubsystem.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "PropertyAnimatorCoreEditorContextTypeCustomization"

void FPropertyAnimatorCoreEditorContextTypeCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& InRow, IPropertyTypeCustomizationUtils& InUtils)
{
	if (!InPropertyHandle->IsValidHandle())
	{
		return;
	}

	if (InPropertyHandle->GetNumOuterObjects() != 1)
	{
		return;
	}

	const UPropertyAnimatorCoreContext* PropertyContext = GetPropertyContextValue(InPropertyHandle);

	if (!PropertyContext)
	{
		return;
	}

	PropertyContextHandle = InPropertyHandle;

	const FName PropertyName = PropertyContext->GetAnimatedProperty().GetPropertyDisplayName();

	InRow
		.NameContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SCheckBox)
				.IsChecked(this, &FPropertyAnimatorCoreEditorContextTypeCustomization::IsPropertyEnabled)
				.OnCheckStateChanged(this, &FPropertyAnimatorCoreEditorContextTypeCustomization::OnPropertyEnabled)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				InPropertyHandle->CreatePropertyNameWidget(FText::FromName(PropertyName))
			]
		]
		.ValueContent()
		.HAlign(HAlign_Left)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.f, 0.f)
			[
				SNew(SButton)
				.ContentPadding(2.f)
				.ToolTipText(LOCTEXT("UnlinkProperty", "Unlink property from animator"))
				.OnClicked(this, &FPropertyAnimatorCoreEditorContextTypeCustomization::UnlinkProperty)
				.Content()
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.Delete"))
					.DesiredSizeOverride(FVector2D(16.f))
				]
			]
		];
}

void FPropertyAnimatorCoreEditorContextTypeCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& InBuilder, IPropertyTypeCustomizationUtils& InUtils)
{
	static const TSet<FName> SkipProperties
	{
		GET_MEMBER_NAME_CHECKED(UPropertyAnimatorCoreContext, bAnimated)
	};

	UObject* PropertyObject;
	InPropertyHandle->GetValue(PropertyObject);

	if (UPropertyAnimatorCoreContext* Options = Cast<UPropertyAnimatorCoreContext>(PropertyObject))
	{
		for (const FProperty* Property : TFieldRange<FProperty>(Options->GetClass()))
		{
			if (Property
				&& Property->HasAnyPropertyFlags(EPropertyFlags::CPF_Edit)
				&& !SkipProperties.Contains(Property->GetFName()))
			{
				IDetailPropertyRow* Row = InBuilder.AddExternalObjectProperty({Options}, Property->GetFName());

				if (Row && Property->GetFName() == GET_MEMBER_NAME_CHECKED(UPropertyAnimatorCoreContext, ConverterRule))
				{
					if (const TSharedPtr<IPropertyHandle> ConverterHandle = Row->GetPropertyHandle())
					{
						PropertyUtilities = InUtils.GetPropertyUtilities();

						// Fix for container properties (array, set, map) in instanced struct not updating details view (when adding/removing entries)
						ConverterHandle->SetOnChildPropertyValueChangedWithData(TDelegate<void(const FPropertyChangedEvent&)>::CreateLambda([this, Row](const FPropertyChangedEvent& InEvent)
						{
							if (PropertyUtilities.IsValid()
								&& InEvent.Property
								&& (InEvent.Property->IsA<FArrayProperty>()
								|| InEvent.Property->IsA<FSetProperty>()
								|| InEvent.Property->IsA<FMapProperty>()))
							{
								PropertyUtilities->RequestRefresh();
							}
						}));

						// Hide customization dropdown to avoid user changing instanced struct type
						PropertyUtilities->EnqueueDeferredAction(FSimpleDelegate::CreateSP(this, &FPropertyAnimatorCoreEditorContextTypeCustomization::HideCustomValueWidget, Row));
					}
				}
			}
		}
	}
}

UPropertyAnimatorCoreContext* FPropertyAnimatorCoreEditorContextTypeCustomization::GetPropertyContextValue(TSharedPtr<IPropertyHandle> InHandle)
{
	if (!InHandle.IsValid() || !InHandle->IsValidHandle())
	{
		return nullptr;
	}

	UObject* ValueData = nullptr;

	if (InHandle->GetValue(ValueData) == FPropertyAccess::Result::Success)
	{
		return Cast<UPropertyAnimatorCoreContext>(ValueData);
	}

	return nullptr;
}

ECheckBoxState FPropertyAnimatorCoreEditorContextTypeCustomization::IsPropertyEnabled() const
{
	if (const UPropertyAnimatorCoreContext* PropertyContext = GetPropertyContextValue(PropertyContextHandle))
	{
		return PropertyContext->IsAnimated() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	return ECheckBoxState::Undetermined;
}

void FPropertyAnimatorCoreEditorContextTypeCustomization::OnPropertyEnabled(ECheckBoxState InNewState) const
{
	if (UPropertyAnimatorCoreContext* PropertyContext = GetPropertyContextValue(PropertyContextHandle))
	{
		PropertyContext->SetAnimated(InNewState == ECheckBoxState::Checked);
	}
}

void FPropertyAnimatorCoreEditorContextTypeCustomization::HideCustomValueWidget(IDetailPropertyRow* InConverterRow)
{
	if (InConverterRow && InConverterRow->CustomValueWidget())
	{
		InConverterRow->CustomValueWidget()->Widget->SetVisibility(EVisibility::Hidden);
	}
}

FReply FPropertyAnimatorCoreEditorContextTypeCustomization::UnlinkProperty() const
{
	if (!PropertyContextHandle.IsValid())
	{
		return FReply::Unhandled();
	}

	const UPropertyAnimatorCoreContext* PropertyContext = GetPropertyContextValue(PropertyContextHandle);
	if (!PropertyContext)
	{
		return FReply::Unhandled();
	}

	TArray<UObject*> Owners;
	PropertyContextHandle->GetOuterObjects(Owners);

	if (UPropertyAnimatorCoreSubsystem* AnimatorSubsystem = UPropertyAnimatorCoreSubsystem::Get())
	{
		FPropertyAnimatorCoreData PropertyData = PropertyContext->GetAnimatedProperty();

		for (UObject* Owner : Owners)
		{
			if (UPropertyAnimatorCoreBase* PropertyAnimator = Cast<UPropertyAnimatorCoreBase>(Owner))
			{
				AnimatorSubsystem->UnlinkAnimatorProperty(PropertyAnimator, PropertyData, true);
			}
		}
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE

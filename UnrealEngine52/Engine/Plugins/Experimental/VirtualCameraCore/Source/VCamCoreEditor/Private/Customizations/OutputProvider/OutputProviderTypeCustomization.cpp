// Copyright Epic Games, Inc. All Rights Reserved.

#include "OutputProviderTypeCustomization.h"

#include "DetailCategoryBuilder.h"
#include "Output/VCamOutputProviderBase.h"
#include "UI/VCamWidget.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyCustomizationHelpers.h"

#define LOCTEXT_NAMESPACE "FOutputProviderCustomization"

namespace UE::VCamCoreEditor::Private
{
	TSharedRef<IPropertyTypeCustomization> FOutputProviderTypeCustomization::MakeInstance()
	{
		return MakeShared<FOutputProviderTypeCustomization>();
	}

	void FOutputProviderTypeCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
	{}

	void FOutputProviderTypeCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
	{
		UObject* CustomizedObject;
		const bool bSuccessGettingValue = PropertyHandle->GetValue(CustomizedObject) == FPropertyAccess::Success;
		UVCamOutputProviderBase* CustomizedOutputProvider = bSuccessGettingValue
			? Cast<UVCamOutputProviderBase>(CustomizedObject)
			: nullptr;
		
		uint32 NumChildren;
		// If user just clicked + Add button or reset the instance class, reference will be null. Let the user select a class.
		if (!CustomizedOutputProvider
			|| PropertyHandle->GetNumChildren(NumChildren) != FPropertyAccess::Success
			|| NumChildren != 1)
		{
			ChildBuilder.AddCustomRow(FText::GetEmpty())
				.NameContent()
				[
					PropertyHandle->CreatePropertyNameWidget()
				]
				.ValueContent()
				[
					PropertyHandle->CreatePropertyValueWidget()
				];

			// Sadly this will be missing the reset to default widget - I could not figure out any way quickly to add it using AddCustomRow
		}
		else
		{
			// AddExternalObjects causes FOutputProviderLayoutCustomization to customize the details.
			IDetailPropertyRow* DetailRow = ChildBuilder.AddExternalObjects({ CustomizedOutputProvider },
				FAddPropertyParams()
					.CreateCategoryNodes(false)		// Avoid creating intermediate group expansion
					.AllowChildren(true)			// Child properties should be shown
					.HideRootObjectNode(false)		// Needed so we can use NameContent() & ValueContent() below
					);
			
			DetailRow->CustomWidget(true)
				.NameContent()
				[
					PropertyHandle->CreatePropertyNameWidget()
				]
				.ValueContent()
				[
					PropertyHandle->CreatePropertyValueWidget()
				];

			// Because AddExternalObjects was used the property system will not add a reset to default widget by default 
			DetailRow->OverrideResetToDefault(
				FResetToDefaultOverride::Create(
					FIsResetToDefaultVisible::CreateLambda([PropertyHandle](TSharedPtr<IPropertyHandle> Handle)
					{
						return PropertyHandle->CanResetToDefault();
					}),
					FResetToDefaultHandler::CreateLambda([PropertyHandle](TSharedPtr<IPropertyHandle> Handle)
					{
						return PropertyHandle->ResetToDefault();
					})
				)
			);
		}
		
	}
}

#undef LOCTEXT_NAMESPACE
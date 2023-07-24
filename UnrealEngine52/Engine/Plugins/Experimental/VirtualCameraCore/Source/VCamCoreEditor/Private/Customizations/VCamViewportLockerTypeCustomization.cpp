// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamViewportLockerTypeCustomization.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "Util/VCamViewportLocker.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FVCamViewportLockerTypeCustomization"

namespace UE::VCamCoreEditor::Private
{
	static FText GetViewportDisplayNameByIndex(int32 ViewportIndex)
	{
		switch (ViewportIndex)
		{
		case 0: return LOCTEXT("Viewport1", "Viewport 1");
		case 1: return LOCTEXT("Viewport2", "Viewport 2");
		case 2: return LOCTEXT("Viewport3", "Viewport 3");
		case 3: return LOCTEXT("Viewport4", "Viewport 4");

		default:
			checkNoEntry();
			return FText::GetEmpty();
		}
	}
	
	TSharedRef<IPropertyTypeCustomization> FVCamViewportLockerTypeCustomization::MakeInstance()
	{
		return MakeShared<FVCamViewportLockerTypeCustomization>();
	}

	void FVCamViewportLockerTypeCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
	{
		HeaderRow.
			NameContent()
			[
				PropertyHandle->CreatePropertyNameWidget()
			]
			.ValueContent()
			[
				PropertyHandle->CreatePropertyValueWidget()
			];
	}

	void FVCamViewportLockerTypeCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
	{
		const FName LocksProperty = GET_MEMBER_NAME_CHECKED(FVCamViewportLocker, Locks);
		const TSharedPtr<IPropertyHandle> LocksMap = PropertyHandle->GetChildHandle(LocksProperty);

		uint32 NumElements = 0;
		if (LocksMap->AsMap()->GetNumElements(NumElements) != FPropertyAccess::Success)
		{
			return;
		}
		
		for (uint32 ChildIndex = 0; ChildIndex < NumElements; ++ChildIndex)
		{
			const TSharedPtr<IPropertyHandle> ViewportLockStateProperty = LocksMap->GetChildHandle(ChildIndex);
			const TSharedPtr<IPropertyHandle> LockViewportToCameraProperty = ViewportLockStateProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FVCamViewportLockState, bLockViewportToCamera));
			ChildBuilder.AddProperty(LockViewportToCameraProperty.ToSharedRef())
				.CustomWidget()
				.NameContent()
				[
					SNew(STextBlock)
					.Text(GetViewportDisplayNameByIndex(ChildIndex))
					.Font(CustomizationUtils.GetRegularFont())
				]
				.ValueContent()
				[
					LockViewportToCameraProperty->CreatePropertyValueWidget()
				];
		}
	}
}

#undef LOCTEXT_NAMESPACE
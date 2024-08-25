// Copyright Epic Games, Inc. All Rights Reserved.
#include "FusionPatchImportOptionsCustomization.h"
#include "FusionPatchImportOptions.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyCustomizationHelpers.h"

#include "HarmonixDsp/FusionSampler/Settings/KeyzoneSettings.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Text/STextBlock.h"

void FFusionPatchCreateOptionsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	static const int32 MinDesiredSlotWidth = 80;
	TSharedPtr<IPropertyHandle> KeyzonesPropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UFusionPatchCreateOptions, Keyzones));
	TSharedPtr<IPropertyHandle> SortOptionHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UFusionPatchCreateOptions, SortOption));
	check(KeyzonesPropertyHandle);
	TSharedPtr<IPropertyHandleArray> KeyzonesArrayHandle = KeyzonesPropertyHandle->AsArray();
	check(KeyzonesArrayHandle);
	
	IDetailCategoryBuilder& KeyzonesCategory = DetailLayout.EditCategory(TEXT("Keyzones"), FText::GetEmpty(), ECategoryPriority::Uncommon);
	KeyzonesCategory.InitiallyCollapsed(true);
	
	uint32 NumElements = 0;
	KeyzonesArrayHandle->GetNumElements(NumElements);
	if (NumElements > 0)
	{
		TSharedRef<IPropertyHandle> Element = KeyzonesArrayHandle->GetElement(0);
		TSharedPtr<IPropertyHandle> SoundWaveHandle = Element->GetChildHandle(GET_MEMBER_NAME_CHECKED(FKeyzoneSettings, SoundWave));
		TSharedPtr<IPropertyHandle> MinNoteHandle = Element->GetChildHandle(GET_MEMBER_NAME_CHECKED(FKeyzoneSettings, MinNote));
		TSharedPtr<IPropertyHandle> MaxNoteHandle = Element->GetChildHandle(GET_MEMBER_NAME_CHECKED(FKeyzoneSettings, MaxNote));
		TSharedPtr<IPropertyHandle> RootNoteHandle = Element->GetChildHandle(GET_MEMBER_NAME_CHECKED(FKeyzoneSettings, RootNote));
		TSharedPtr<IPropertyHandle> MinVelocityHandle = Element->GetChildHandle(GET_MEMBER_NAME_CHECKED(FKeyzoneSettings, MinVelocity));
		TSharedPtr<IPropertyHandle> MaxVelocityHandle = Element->GetChildHandle(GET_MEMBER_NAME_CHECKED(FKeyzoneSettings, MaxVelocity));

		KeyzonesCategory.AddCustomRow(NSLOCTEXT("FusionoPatch_Details", "KeyzonesHeader", "Keyzones"))
		.NameContent()
		[
			SoundWaveHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
		SNew(SUniformGridPanel)
			.MinDesiredSlotWidth(MinDesiredSlotWidth)
		   +SUniformGridPanel::Slot(0, 0)
		   [
				MinNoteHandle->CreatePropertyNameWidget()
		   ]
		   +SUniformGridPanel::Slot(1, 0)
		   [
			   RootNoteHandle->CreatePropertyNameWidget()
		   ]
		   +SUniformGridPanel::Slot(2, 0)
		   [
			   MaxNoteHandle->CreatePropertyNameWidget()
		   ]
		   +SUniformGridPanel::Slot(3, 0)
		   [
				MinVelocityHandle->CreatePropertyNameWidget()
		   ]
		   +SUniformGridPanel::Slot(4, 0)
		   [
		   		MaxVelocityHandle->CreatePropertyNameWidget()
		   ]
		];
	}
	TSharedPtr<FDetailArrayBuilder> ArrayBuilder = MakeShared<FDetailArrayBuilder>(KeyzonesPropertyHandle.ToSharedRef(), false, false, false);
	
	ArrayBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateLambda([](TSharedRef<IPropertyHandle> Element, int32 Index, IDetailChildrenBuilder& ChildrenBuilder)
	{
		TSharedPtr<IPropertyHandle> SoundWaveHandle = Element->GetChildHandle(GET_MEMBER_NAME_CHECKED(FKeyzoneSettings, SoundWave));
		TSharedPtr<IPropertyHandle> MinNoteHandle = Element->GetChildHandle(GET_MEMBER_NAME_CHECKED(FKeyzoneSettings, MinNote));
		TSharedPtr<IPropertyHandle> MaxNoteHandle = Element->GetChildHandle(GET_MEMBER_NAME_CHECKED(FKeyzoneSettings, MaxNote));
		TSharedPtr<IPropertyHandle> RootNoteHandle = Element->GetChildHandle(GET_MEMBER_NAME_CHECKED(FKeyzoneSettings, RootNote));
		TSharedPtr<IPropertyHandle> MinVelocityHandle = Element->GetChildHandle(GET_MEMBER_NAME_CHECKED(FKeyzoneSettings, MinVelocity));
		TSharedPtr<IPropertyHandle> MaxVelocityHandle = Element->GetChildHandle(GET_MEMBER_NAME_CHECKED(FKeyzoneSettings, MaxVelocity));

		UObject* SoundWave = nullptr;
		SoundWaveHandle->GetValue(SoundWave);
		if (!SoundWave)
		{
			return;
		}
				
		FText SoundWaveName = FText::FromString(SoundWave->GetPackage()->GetName());

		ChildrenBuilder.AddCustomRow(NSLOCTEXT("FusionPatch_Details", "KeyzoneProperty", "Keyzone"))
		.EditCondition(TAttribute<bool>::CreateLambda([]() { return false; }), nullptr)
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(SoundWaveName)
		]
		.ValueContent()
		[
		 SNew(SUniformGridPanel)
			.MinDesiredSlotWidth(MinDesiredSlotWidth)
			+SUniformGridPanel::Slot(0, 0)
			[
				MinNoteHandle->CreatePropertyValueWidget()
			]
			+SUniformGridPanel::Slot(1, 0)
			[
				RootNoteHandle->CreatePropertyValueWidget()
			]
			+SUniformGridPanel::Slot(2, 0)
			[
				MaxNoteHandle->CreatePropertyValueWidget()
			]
			+SUniformGridPanel::Slot(3, 0)
			[
				MinVelocityHandle->CreatePropertyValueWidget()
			]
			+SUniformGridPanel::Slot(4, 0)
			[
				MaxVelocityHandle->CreatePropertyValueWidget()
			]
		];
	}));

	SortOptionHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([ArrayBuilder]()
	{
		ArrayBuilder->RefreshChildren();
	}));
	KeyzonesCategory.AddCustomBuilder(ArrayBuilder.ToSharedRef());

	DetailLayout.HideProperty(KeyzonesPropertyHandle);
}

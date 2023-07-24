// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerMorphModelQualityLevelCustomization.h"
#include "MLDeformerMorphModelQualityLevel.h"

#include "DetailLayoutBuilder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "MLDeformerMorphModelQualityLevelCustomization"

namespace UE::MLDeformer
{
	TSharedRef<IPropertyTypeCustomization> FMLDeformerMorphModelQualityLevelCustomization::MakeInstance()
	{
		return MakeShareable(new FMLDeformerMorphModelQualityLevelCustomization());
	}

	void FMLDeformerMorphModelQualityLevelCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
	{
		uint32 NumChildren;
		StructPropertyHandle->GetNumChildren(NumChildren);
		TMap<FName, TSharedPtr<IPropertyHandle>> PropertyHandles;
		PropertyHandles.Reserve(NumChildren);
		for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
		{
			TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();
			const FName PropertyName = ChildHandle->GetProperty()->GetFName();
			PropertyHandles.Add(PropertyName, ChildHandle);
		}

		TSharedPtr<IPropertyHandle> MaxMorphsHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FMLDeformerMorphModelQualityLevel, MaxActiveMorphs));
		check(MaxMorphsHandle.IsValid());

		const int32 QualityLevel = StructPropertyHandle->GetIndexInArray();
		check(QualityLevel != INDEX_NONE);

		HeaderRow
		.NameContent()
		[
			SNew(STextBlock)
			.Margin(FMargin(5, 0, 0, 0))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(FText::Format(LOCTEXT("QualityLevelFormat", "Level {0}"), QualityLevel))
		]
		.ValueContent()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			[
				MaxMorphsHandle->CreatePropertyValueWidget()
			]
			+SHorizontalBox::Slot()
			.AutoWidth()			
			.HAlign(EHorizontalAlignment::HAlign_Left)
			.VAlign(EVerticalAlignment::VAlign_Center)
			[
				SNew(STextBlock)
				.Margin(FMargin(5, 0, 0, 0))
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(LOCTEXT("ActiveMorphs", "Morphs"))
			]
		];
	}
}	// namespace UE::MLDeformer

#undef LOCTEXT_NAMESPACE

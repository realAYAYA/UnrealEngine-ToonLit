// Copyright Epic Games, Inc. All Rights Reserved.

#include "DistanceDatumStructCustomization.h"

#include "Containers/Array.h"
#include "DetailWidgetRow.h"
#include "HAL/Platform.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Misc/AssertionMacros.h"
#include "PropertyHandle.h"
#include "Sound/SoundNodeDistanceCrossFade.h"
#include "Sound/SoundNodeParamCrossFade.h"
#include "Templates/Casts.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/UnrealNames.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "DistanceDatumStructCustomization"

TSharedRef<IPropertyTypeCustomization> FDistanceDatumStructCustomization::MakeInstance()
{
	return MakeShareable(new FDistanceDatumStructCustomization);
}

FDistanceDatumStructCustomization::~FDistanceDatumStructCustomization()
{
}

FDistanceDatumStructCustomization::FDistanceDatumStructCustomization()
{
}

void FDistanceDatumStructCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	HeaderRow
		.NameContent()
		[
			InStructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			InStructPropertyHandle->CreatePropertyValueWidget()
		];
}

void FDistanceDatumStructCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	uint32 NumChildren;
	InStructPropertyHandle->GetNumChildren(NumChildren);

	TArray<UObject*> OuterObjects;
	InStructPropertyHandle->GetOuterObjects(OuterObjects);

	// Check whether we are dealing with param cross fade nodes
	bool bAllParamNodes = true;
	for (UObject* OuterObject : OuterObjects)
	{
		if (!OuterObject->IsA(USoundNodeParamCrossFade::StaticClass()))
		{
			bAllParamNodes = false;
		}
	}

	// How to describe the parameter depending on how many are selected
	FText ParamDesc = LOCTEXT("Param", "Param");
	if (bAllParamNodes)
	{
		if (OuterObjects.Num() == 1)
		{
			FName ParamName = CastChecked<USoundNodeParamCrossFade>(OuterObjects[0])->ParamName;
			if (ParamName != NAME_None)
			{
				ParamDesc = FText::FromName(ParamName);
			}
		}
	}

	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		TSharedRef<IPropertyHandle> Child = InStructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();
		IDetailPropertyRow& ChildRow = StructBuilder.AddProperty(Child);

		if (bAllParamNodes)
		{
			if (Child->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FDistanceDatum, FadeInDistanceStart))
			{
				ChildRow.DisplayName(FText::Format(LOCTEXT("FadeInStart", "Fade In {0} Value Start"), ParamDesc));
			}
			else if (Child->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FDistanceDatum, FadeInDistanceEnd))
			{
				ChildRow.DisplayName(FText::Format(LOCTEXT("FadeInEnd", "Fade In {0} Value End"), ParamDesc));
			}
			else if (Child->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FDistanceDatum, FadeOutDistanceStart))
			{
				ChildRow.DisplayName(FText::Format(LOCTEXT("FadeOutStart", "Fade Out {0} Value Start"), ParamDesc));
			}
			else if (Child->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FDistanceDatum, FadeOutDistanceEnd))
			{
				ChildRow.DisplayName(FText::Format(LOCTEXT("FadeOutEnd", "Fade Out {0} Value End"), ParamDesc));
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE

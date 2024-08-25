// Copyright Epic Games, Inc. All Rights Reserved.

#include "DetailsCustomizations/ChaosVDQueryDataWrappersCustomizationDetails.h"

#include "DataWrappers/ChaosVDQueryDataWrappers.h"
#include "DetailsCustomizations/ChaosVDDetailsCustomizationUtils.h"
#include "DetailLayoutBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

TSharedRef<IPropertyTypeCustomization> FChaosVDQueryDataWrappersCustomizationDetails::MakeInstance()
{
	return MakeShareable(new FChaosVDQueryDataWrappersCustomizationDetails());

}

void FChaosVDQueryDataWrappersCustomizationDetails::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren(NumChildren);

	if (NumChildren == 0)
	{
		return;
	}

	TArray<TSharedPtr<IPropertyHandle>> Handles;
	Handles.Reserve(NumChildren);

	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		TSharedRef<IPropertyHandle> Handle = StructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();
		Handles.Add(Handle);
	}

	FChaosVDDetailsCustomizationUtils::HideInvalidCVDDataWrapperProperties(Handles);
}

TSharedRef<IDetailCustomization> FChaosVDQueryVisitDataCustomization::MakeInstance()
{
	return MakeShareable( new FChaosVDQueryVisitDataCustomization );
}

void FChaosVDQueryVisitDataCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TSharedRef<IPropertyHandle>> PotentialPropertiesToHide;
	
	constexpr int32 PropertiesToEvaluateNum = 2;
	PotentialPropertiesToHide.Reset(PropertiesToEvaluateNum);

	PotentialPropertiesToHide.Add(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(FChaosVDQueryVisitStep, QueryFastData)));
	PotentialPropertiesToHide.Add(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(FChaosVDQueryVisitStep, HitData)));

	FChaosVDDetailsCustomizationUtils::HideInvalidCVDDataWrapperProperties(PotentialPropertiesToHide, DetailBuilder);
}

TSharedRef<IDetailCustomization> FChaosVDQueryDataWrapperCustomization::MakeInstance()
{
	return MakeShareable( new FChaosVDQueryVisitDataCustomization );
}

void FChaosVDQueryDataWrapperCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TSharedRef<IPropertyHandle>> PotentialPropertiesToHide;

	constexpr int32 PropertiesToEvaluateNum = 3;
	PotentialPropertiesToHide.Reset(PropertiesToEvaluateNum);

	PotentialPropertiesToHide.Add(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(FChaosVDQueryDataWrapper, CollisionQueryParams)));
	PotentialPropertiesToHide.Add(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(FChaosVDQueryDataWrapper, CollisionResponseParams)));
	PotentialPropertiesToHide.Add(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(FChaosVDQueryDataWrapper, CollisionObjectQueryParams)));

	FChaosVDDetailsCustomizationUtils::HideInvalidCVDDataWrapperProperties(PotentialPropertiesToHide, DetailBuilder);
}

#undef LOCTEXT_NAMESPACE

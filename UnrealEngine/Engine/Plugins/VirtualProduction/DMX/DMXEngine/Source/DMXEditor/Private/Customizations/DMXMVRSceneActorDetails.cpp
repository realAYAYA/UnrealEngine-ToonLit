// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/DMXMVRSceneActorDetails.h"

#include "MVR/DMXMVRSceneActor.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityFixtureType.h"
#include "Library/DMXImportGDTF.h"
#include "Library/DMXLibrary.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IPropertyUtilities.h"


#define LOCTEXT_NAMESPACE "DMXMVRSceneActorDetails"

TSharedRef<IDetailCustomization> FDMXMVRSceneActorDetails::MakeInstance()
{
	return MakeShared<FDMXMVRSceneActorDetails>();
}

void FDMXMVRSceneActorDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	PropertyUtilities = DetailBuilder.GetPropertyUtilities();

	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	for (TWeakObjectPtr<UObject> Object : ObjectsBeingCustomized)
	{
		if (ADMXMVRSceneActor* MVRSceneActor = Cast<ADMXMVRSceneActor>(Object))
		{
			OuterSceneActors.Add(MVRSceneActor);
		}
	}

	IDetailCategoryBuilder& ActorTypeForGDTFCategory = DetailBuilder.EditCategory("GDTF to Spawned Actor");
	ActorTypeForGDTFCategory.InitiallyCollapsed(false);

	const TSharedRef<IPropertyHandle> GDTFToDefaultActorClassesHandle = DetailBuilder.GetProperty(ADMXMVRSceneActor::GetGDTFToDefaultActorClassesPropertyNameChecked());
	GDTFToDefaultActorClassesHandle->MarkHiddenByCustomization();

	const TSharedPtr<IPropertyHandleArray> GDTFToDefaultActorClassHandleArray = GDTFToDefaultActorClassesHandle->AsArray();
	FSimpleDelegate GDTFToDefaultActorClassArrayChangedDelegate = FSimpleDelegate::CreateSP(this, &FDMXMVRSceneActorDetails::ForceRefresh);
	GDTFToDefaultActorClassHandleArray->SetOnNumElementsChanged(GDTFToDefaultActorClassArrayChangedDelegate);

	uint32 NumGDTFToDefaultActorClassElements;
	if (!ensure(GDTFToDefaultActorClassHandleArray->GetNumElements(NumGDTFToDefaultActorClassElements) == FPropertyAccess::Success))
	{
		return;
	}

	for (uint32 GDTFToDefaultActorClassElementIndex = 0; GDTFToDefaultActorClassElementIndex < NumGDTFToDefaultActorClassElements; GDTFToDefaultActorClassElementIndex++)
	{
		const TSharedPtr<IPropertyHandle> GDTFToDefaultActorClassHandle = GDTFToDefaultActorClassHandleArray->GetElement(GDTFToDefaultActorClassElementIndex);
		const TSharedPtr<IPropertyHandle> GDTFHandle = GDTFToDefaultActorClassHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDMXMVRSceneGDTFToActorClassPair, GDTF));
		DefaultActorClassHandle = GDTFToDefaultActorClassHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDMXMVRSceneGDTFToActorClassPair, ActorClass));
		DefaultActorClassHandle->SetOnChildPropertyValuePreChange(FSimpleDelegate::CreateSP(this, &FDMXMVRSceneActorDetails::OnPreEditChangeActorClassInGDTFToActorClasses));
		DefaultActorClassHandle->SetOnChildPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDMXMVRSceneActorDetails::OnPostEditChangeActorClassInGDTFToActorClasses));

		ActorTypeForGDTFCategory.AddCustomRow(LOCTEXT("GDTFToDefaultActorClassFilter", "GDTF"))
			.NameContent()
			[
				GDTFHandle->CreatePropertyValueWidget()
			]
			.ValueContent()
			[
				DefaultActorClassHandle->CreatePropertyValueWidget()
			];
	}
}

void FDMXMVRSceneActorDetails::OnFixturePatchChanged(const UDMXEntityFixturePatch* FixturePatch)
{
	ForceRefresh();
}

void FDMXMVRSceneActorDetails::OnFixtureTypeChanged(const UDMXEntityFixtureType* FixtureType)
{
	ForceRefresh();
}

void FDMXMVRSceneActorDetails::OnPreEditChangeActorClassInGDTFToActorClasses()
{
	for (TWeakObjectPtr<ADMXMVRSceneActor> WeakMVRSceneActor : OuterSceneActors)
	{
		if (ADMXMVRSceneActor* MVRSceneActor = WeakMVRSceneActor.Get())
		{
			MVRSceneActor->PreEditChange(FDMXMVRSceneGDTFToActorClassPair::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FDMXMVRSceneGDTFToActorClassPair, ActorClass)));
		}
	}
}

void FDMXMVRSceneActorDetails::OnPostEditChangeActorClassInGDTFToActorClasses()
{
	for (TWeakObjectPtr<ADMXMVRSceneActor> WeakMVRSceneActor : OuterSceneActors)
	{
		if (ADMXMVRSceneActor* MVRSceneActor = WeakMVRSceneActor.Get())
		{
			MVRSceneActor->PostEditChange();
		}
	}
}

void FDMXMVRSceneActorDetails::ForceRefresh()
{
	PropertyUtilities->ForceRefresh();
}

#undef LOCTEXT_NAMESPACE

// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextInterfacePropertyTypeCustomization.h"

#include "AnimNextInterfaceGraph.h"
#include "AnimNextInterfaceWidgetFactories.h"
#include "DetailWidgetRow.h"
#include "Editor/PropertyEditor/Private/ObjectPropertyNode.h"
#include "Editor/PropertyEditor/Private/PropertyNode.h"
#include "IAnimNextInterface.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "SClassViewer.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "AnimNextInterfacePropertyTypeCustomization"

namespace UE::AnimNext::InterfaceGraphEditor
{

bool FPropertyTypeIdentifier::IsPropertyTypeCustomized(const IPropertyHandle& PropertyHandle) const
{
	return (PropertyHandle.GetMetaData("AnimNextInterfaceType") != "");
}

void FAnimNextInterfacePropertyTypeCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	FString TypeName = PropertyHandle->GetMetaData("AnimNextInterfaceType");
	FName AnimNextInterfaceTypeName(TypeName);

	void* ValuePtr;
	PropertyHandle->GetValueData(ValuePtr);
	FScriptInterface* PropertyValue = reinterpret_cast<FScriptInterface*>(ValuePtr);
	
	TSharedPtr<SWidget> Widget = FAnimNextInterfaceWidgetFactories::CreateAnimNextInterfaceWidget(AnimNextInterfaceTypeName, PropertyValue->GetObject(),
			FOnClassPicked::CreateLambda([this, PropertyHandle](UClass* ChosenClass)
			{
				TArray<void*> RawData;
				PropertyHandle->AccessRawData(RawData);
				TArray<UPackage*> OuterObjects;
				PropertyHandle->GetOuterPackages(OuterObjects);

				for(int i=0;i<RawData.Num();i++)
				{
					UObject* NewValue = NewObject<UObject>(OuterObjects[i], ChosenClass);
				
					PropertyHandle->NotifyPreChange();
					PropertyHandle->SetValue(NewValue);
					PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
					PropertyHandle->GetPropertyNode()->GetParentNode()->RequestRebuildChildren();
				}
			}),
			nullptr
	);

	HeaderRow
	.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		Widget.ToSharedRef()
	];
}
	
void FAnimNextInterfacePropertyTypeCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// todo: make this work correctly for multiselect
	void* ValuePtr;
	PropertyHandle->GetValueData(ValuePtr);
	FScriptInterface* PropertyValue = reinterpret_cast<FScriptInterface*>(ValuePtr);
	if (PropertyValue->GetObject())
	{
		TSharedPtr<FStructOnScope> StructData = MakeShareable(new FStructOnScope(PropertyValue->GetObject()->GetClass(), (uint8*)PropertyValue->GetObject()));
		StructData->SetPackage(PropertyValue->GetObject()->GetPackage());
		TArray<TSharedPtr<IPropertyHandle>> NewHandles = ChildBuilder.AddAllExternalStructureProperties(StructData.ToSharedRef());
		for (auto& NewHandle : NewHandles)
		{
			ChildBuilder.AddProperty(NewHandle.ToSharedRef());
		}
	}
}

}

#undef LOCTEXT_NAMESPACE
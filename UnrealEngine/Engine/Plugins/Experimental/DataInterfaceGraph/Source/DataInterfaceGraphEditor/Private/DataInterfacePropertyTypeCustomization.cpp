// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataInterfacePropertyTypeCustomization.h"

#include "DataInterfaceGraph.h"
#include "PropertyHandle.h"
#include "IDataInterface.h"
#include "DetailWidgetRow.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Text/STextBlock.h"
#include "SClassViewer.h"
#include "DataInterfaceWidgetFactories.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyEditor/Private/ObjectPropertyNode.h"
#include "PropertyEditor/Private/PropertyNode.h"

#define LOCTEXT_NAMESPACE "DataInterfacePropertyTypeCustomization"

namespace UE::DataInterfaceGraphEditor
{

bool FPropertyTypeIdentifier::IsPropertyTypeCustomized(const IPropertyHandle& PropertyHandle) const
{
	return (PropertyHandle.GetMetaData("DataInterfaceType") != "");
}

void FDataInterfacePropertyTypeCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	FString TypeName = PropertyHandle->GetMetaData("DataInterfaceType");
	FName DataInterfaceTypeName(TypeName);

	void* ValuePtr;
	PropertyHandle->GetValueData(ValuePtr);
	FScriptInterface* PropertyValue = reinterpret_cast<FScriptInterface*>(ValuePtr);
	
	TSharedPtr<SWidget> Widget = FDataInterfaceWidgetFactories::CreateDataInterfaceWidget(DataInterfaceTypeName, PropertyValue->GetObject(),
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
	
void FDataInterfacePropertyTypeCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
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
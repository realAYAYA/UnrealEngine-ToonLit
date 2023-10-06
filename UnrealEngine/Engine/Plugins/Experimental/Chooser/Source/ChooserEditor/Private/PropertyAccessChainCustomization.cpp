// Copyright Epic Games, Inc. All Rights Reserved.

#include "PropertyAccessChainCustomization.h"

#include "Chooser.h"
#include "ChooserPropertyAccess.h"
#include "PropertyHandle.h"
#include "DetailWidgetRow.h"
#include "GraphEditorSettings.h"
#include "SClassViewer.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyAccessEditor.h"
#include "ScopedTransaction.h"
#include "SPropertyAccessChainWidget.h"

#define LOCTEXT_NAMESPACE "PropertyAccessChainCustomization"

namespace UE::ChooserEditor
{
	
void FPropertyAccessChainCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	FProperty* Property = PropertyHandle->GetProperty();

	UClass* ContextClass = nullptr;
	static FName TypeMetaData = "BindingType";
	static FName ColorMetaData = "BindingColor";
	static FName AllowFunctionsMetaData = "BindingAllowFunctions";

	FString TypeFilter = PropertyHandle->GetMetaData(TypeMetaData);
	FString BindingColor = PropertyHandle->GetMetaData(ColorMetaData);
	FString BindingAllowFunctions = PropertyHandle->GetMetaData(AllowFunctionsMetaData);
	bool AllowFunctions = BindingAllowFunctions.ToBool();
		

	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);
	UObject* OuterObject = OuterObjects[0];
	
	while (OuterObject && !OuterObject->Implements<UHasContextClass>())
	{
		OuterObject = OuterObject->GetOuter();
	}
	
	IHasContextClass* HasContext = nullptr;
	if (OuterObject)
	{
		HasContext = static_cast<IHasContextClass*>(OuterObject->GetInterfaceAddress(UHasContextClass::StaticClass()));
	}

	HeaderRow
	.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SPropertyAccessChainWidget).ContextClassOwner(HasContext).TypeFilter(TypeFilter).BindingColor(BindingColor).AllowFunctions(AllowFunctions)
		.PropertyBindingValue_Lambda([PropertyHandle]()
		{
			void* data;
			PropertyHandle->GetValueData(data);
			return reinterpret_cast<const FChooserPropertyBinding*>(data);
		})
		.OnAddBinding_Lambda([PropertyHandle, TypeFilter](FName InPropertyName, const TArray<FBindingChainElement>& InBindingChain)
         	 	{
         	 		TArray<UObject*> OuterObjects;
         	 		PropertyHandle->GetOuterObjects(OuterObjects);
         	 		const FScopedTransaction Transaction(NSLOCTEXT("ChooserPropertyBinding", "Change Property Binding", "Change Property Binding"));
         	 	
         			for (uint32 i=0; i<PropertyHandle->GetNumOuterObjects(); i++)
         	 		{
         	 			void* ValuePtr = nullptr;
         	 			PropertyHandle->GetValueData(ValuePtr);// todo get per object value data?
         	 			FChooserPropertyBinding* PropertyValue = reinterpret_cast<FChooserPropertyBinding*>(ValuePtr);
         	 		
         	 			if (PropertyValue != nullptr)
         	 			{
         	 				PropertyHandle->NotifyPreChange();
         	 				
         	 				OuterObjects[i]->Modify(true);
         	 				Chooser::CopyPropertyChain(InBindingChain, *PropertyValue);
         	
         	 				if (TypeFilter == "enum")
         	 				{
         	 					FField* Property = InBindingChain.Last().Field.ToField();
         	 					FChooserEnumPropertyBinding* EnumPropertyValue = static_cast<FChooserEnumPropertyBinding*>(PropertyValue);
         	 					
         	 					if (const FEnumProperty* EnumProperty = CastField<const FEnumProperty>(Property))
         	 					{
         	 						EnumPropertyValue->Enum = EnumProperty->GetEnum();
         	 					}
         	 					else if (const FByteProperty* ByteProperty = CastField<const FByteProperty>(Property))
         	 					{
         	 						EnumPropertyValue->Enum = ByteProperty->Enum;
         	 					}
         	 				}
							if (TypeFilter == "object")
							{
								FField* Property = InBindingChain.Last().Field.ToField();
								FChooserObjectPropertyBinding* ObjectPropertyBinding = static_cast<FChooserObjectPropertyBinding*>(PropertyValue);
								
								if (const FObjectPropertyBase* ObjectProperty = CastField<const FObjectPropertyBase>(Property))
								{
									ObjectPropertyBinding->AllowedClass = ObjectProperty->PropertyClass;
								}
							}
         	 				if (TypeFilter == "struct")
							{
								FField* Property = InBindingChain.Last().Field.ToField();
								FChooserStructPropertyBinding* StructPropertyBinding = static_cast<FChooserStructPropertyBinding*>(PropertyValue);
								
								if (const FStructProperty* StructProperty = CastField<const FStructProperty>(Property))
								{
									StructPropertyBinding->StructType = StructProperty->Struct;
								}
							}
         	
         	 				PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
         	 			}
         	 		}
         	 	})
	];
}
	
void FPropertyAccessChainCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

}

#undef LOCTEXT_NAMESPACE
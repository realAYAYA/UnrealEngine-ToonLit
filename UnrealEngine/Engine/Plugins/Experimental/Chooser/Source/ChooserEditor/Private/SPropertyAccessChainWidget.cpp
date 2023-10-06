// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPropertyAccessChainWidget.h"

#include "Chooser.h"
#include "ChooserPropertyAccess.h"
#include "PropertyHandle.h"
#include "DetailWidgetRow.h"
#include "GraphEditorSettings.h"
#include "SClassViewer.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyAccessEditor.h"
#include "ScopedTransaction.h"
#include "Features/IModularFeatures.h"

#define LOCTEXT_NAMESPACE "PropertyAccessChainWidget"

namespace UE::ChooserEditor
{
	
TSharedRef<SWidget> SPropertyAccessChainWidget::CreatePropertyAccessWidget()
{
	FPropertyBindingWidgetArgs Args;
	Args.bAllowPropertyBindings = true;

	TConstArrayView<FInstancedStruct> ContextData;
	if (ContextClassOwner)
	{
		ContextData = ContextClassOwner->GetContextData();
	}
	
	Args.bAllowUObjectFunctions = true;
	Args.bAllowOnlyThreadSafeFunctions = true;

	auto CanBindProperty = [this](FProperty* Property)
	{
		if (TypeFilter == "" || Property == nullptr)
		{
			return true;
		}
		if (TypeFilter == "struct")
		{
			// special case for struct of any type
			return CastField<FStructProperty>(Property) != nullptr;
		}
		if (TypeFilter == "object")
		{
			// special case for objects references of any type
			return CastField<FObjectProperty>(Property) != nullptr;
		}
		if (TypeFilter == "double")
		{
			// special case for doubles to bind to either floats or doubles
			return Property->GetCPPType() == "float" || Property->GetCPPType() == "double";
		}
		else if (TypeFilter == "enum")
		{
			// special case for enums, to find properties of type EnumProperty or ByteProperty which have an Enum
	
			if (const FEnumProperty* EnumProperty = CastField<const FEnumProperty>(Property))
			{
				return true;
			}
			else if (const FByteProperty* ByteProperty = CastField<const FByteProperty>(Property))
			{
				return ByteProperty->Enum != nullptr;
			}
			return false;
		}
		
		return Property->GetCPPType() == TypeFilter;
	};

	// allow struct bindings to bind context structs directly
	Args.OnCanBindToContextStruct = FOnCanBindToContextStruct::CreateLambda([this](UStruct* StructType)
	{
		if (TypeFilter == "struct" && !StructType->IsChildOf(UObject::StaticClass()))
		{
			// struct bindings can bind any type of struct
			return true;
		}
		else
		{
			return StructType->GetName()  == TypeFilter;
		}
	});

	Args.OnCanBindProperty = FOnCanBindProperty::CreateLambda(CanBindProperty);

	Args.OnCanBindFunction = FOnCanBindFunction::CreateLambda([CanBindProperty](UFunction* Function)
	{
		if (Function->NumParms !=1)
		{
			// only allow binding object member functions which have no parameters
			return false;
		}

		if (FProperty* ReturnProperty = Function->GetReturnProperty())
		{
			return CanBindProperty(ReturnProperty);
		}
	
		return false;
	});

	Args.OnCanBindToClass = FOnCanBindToClass::CreateLambda([](UClass* InClass)
	{
		return true;
	});

	FLinearColor BindingColorValue = FLinearColor::Gray;
	if (BindingColor != "")
	{
		const UGraphEditorSettings* GraphEditorSettings = GetDefault<UGraphEditorSettings>();
		if (const FStructProperty* ColorProperty = FindFProperty<FStructProperty>(GraphEditorSettings->GetClass(), FName(BindingColor)))
		{
			BindingColorValue = *ColorProperty->ContainerPtrToValuePtr<FLinearColor>(GraphEditorSettings);
		}
	}

	Args.CurrentBindingColor = MakeAttributeLambda([BindingColorValue]() {
		return BindingColorValue;
	});
	
	Args.OnCanBindToSubObjectClass = FOnCanBindToSubObjectClass::CreateLambda([](UClass* InClass)
		{
			// CanBindToSubObjectClass does the opposite of what it's name says.  True means don't allow bindings
			// don't allow binding to any object propertoes (forcing use of thread safe functions to access objects)
			return true;
		});

	Args.OnCanAcceptPropertyOrChildren = FOnCanBindProperty::CreateLambda([](FProperty* InProperty)
		{
			// Make only blueprint visible properties visible for binding.
			return InProperty->HasAnyPropertyFlags(CPF_BlueprintVisible);
		});	

	Args.OnAddBinding = OnAddBinding;


	Args.CurrentBindingToolTipText = MakeAttributeLambda([this]()
			{
				const FText Bind = NSLOCTEXT("ContextPropertyWidget", "Bind", "Bind");
				FText CurrentValue = Bind;
				
				const FChooserPropertyBinding* PropertyValue = PropertyBindingValue.Get();

				if (PropertyValue != nullptr && PropertyValue->PropertyBindingChain.Num()>0)
				{
					TArray<FText> BindingChainText;
					BindingChainText.Reserve(PropertyValue->PropertyBindingChain.Num());
				 
					for (const FName& Name : PropertyValue->PropertyBindingChain)
					{
						BindingChainText.Add(FText::FromName(Name));
					}
					
					CurrentValue = FText::Join(NSLOCTEXT("ContextPropertyWidget", "PropertyPathSeparator","."), BindingChainText);
				}
	
				return CurrentValue;	
			});
	
	Args.CurrentBindingText = MakeAttributeLambda([this]()
			{
				const FText Bind = NSLOCTEXT("ContextPropertyWidget", "Bind", "Bind");
				FText CurrentValue = Bind;
		
				const FChooserPropertyBinding* PropertyValue = PropertyBindingValue.Get();
	
				int BindingChainLength = PropertyValue->PropertyBindingChain.Num();
				if (BindingChainLength == 0)
				{
					if (PropertyValue->ContextIndex >= 0)
					{
						// direct binding to a context struct
						if (ContextClassOwner)
						{
							TConstArrayView<FInstancedStruct> ContextData = ContextClassOwner->GetContextData();
							if (ContextData.IsValidIndex(PropertyValue->ContextIndex))
							{
								if (const FContextObjectTypeStruct* StructType = ContextData[PropertyValue->ContextIndex].GetPtr<FContextObjectTypeStruct>())
								{
									CurrentValue = FText::FromString(StructType->Struct->GetAuthoredName());
								}
							}
						}
					}
				}
				else
				{
					if (!PropertyValue->DisplayName.IsEmpty())
					{
						CurrentValue = FText::FromString(PropertyValue->DisplayName);
					}
					else if (BindingChainLength == 1)
					{
						// single property, just use the property name
						CurrentValue = FText::FromName(PropertyValue->PropertyBindingChain.Last());
					}
					else
					{
						// for longer chains always show the last struct/object name, and the final property name (full path in tooltip)
						CurrentValue = FText::Join(NSLOCTEXT("ContextPropertyWidget", "PropertyPathSeparator","."),
							TArray<FText>({
								FText::FromName(PropertyValue->PropertyBindingChain[BindingChainLength-2]),
								FText::FromName(PropertyValue->PropertyBindingChain[BindingChainLength-1])
							}));
					}
				}
	
				return CurrentValue;
			});

	Args.CurrentBindingImage = MakeAttributeLambda([]() -> const FSlateBrush*
		{
			static FName PropertyIcon(TEXT("Kismet.Tabs.Variables"));
			return FAppStyle::GetBrush(PropertyIcon);
		});
	
	TArray<FBindingContextStruct> ContextStructs;
	for (const FInstancedStruct& ContextStruct : ContextData)
	{
		if (const FContextObjectTypeClass* ClassType = ContextStruct.GetPtr<FContextObjectTypeClass>())
		{
			ContextStructs.SetNum(ContextStructs.Num()+1);
			ContextStructs.Last().Struct = ClassType->Class;
		}
		else if (const FContextObjectTypeStruct* StructType = ContextStruct.GetPtr<FContextObjectTypeStruct>())
		{
			ContextStructs.SetNum(ContextStructs.Num()+1);
			ContextStructs.Last().Struct = StructType->Struct;
		}
	}	

	const IPropertyAccessEditor& PropertyAccessEditor = IModularFeatures::Get().GetModularFeature<IPropertyAccessEditor>("PropertyAccessEditor");
	return  PropertyAccessEditor.MakePropertyBindingWidget(ContextStructs, Args);
}

void SPropertyAccessChainWidget::UpdateWidget()
{
	ChildSlot[ CreatePropertyAccessWidget() ];
}

void SPropertyAccessChainWidget::ContextClassChanged()
{
	UpdateWidget();
}

void SPropertyAccessChainWidget::Construct( const FArguments& InArgs)
{
	TypeFilter = InArgs._TypeFilter;
	BindingColor = InArgs._BindingColor;
	ContextClassOwner = InArgs._ContextClassOwner;
	bAllowFunctions = InArgs._AllowFunctions;
	OnAddBinding = InArgs._OnAddBinding;
	PropertyBindingValue = InArgs._PropertyBindingValue;
	UpdateWidget();

	if (ContextClassOwner)
	{
		ContextClassOwner->OnContextClassChanged.AddSP(this, &SPropertyAccessChainWidget::ContextClassChanged);
	}
}

SPropertyAccessChainWidget::~SPropertyAccessChainWidget()
{
}

}

#undef LOCTEXT_NAMESPACE
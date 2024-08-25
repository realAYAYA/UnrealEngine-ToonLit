// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPropertyAccessChainWidget.h"

#include "Chooser.h"
#include "ChooserPropertyAccess.h"
#include "DetailWidgetRow.h"
#include "GraphEditorSettings.h"
#include "SClassViewer.h"
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
			return CastField<FObjectPropertyBase>(Property) != nullptr;
		}
		if (TypeFilter == "double")
		{
			// special case for doubles to bind to either floats or doubles
			return Property->GetCPPType() == "float" || Property->GetCPPType() == "double" || Property->GetCPPType() == "int32";
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
		else if (TypeFilter == "bool")
		{
			// special case for bools, because CPPType == "bool" doesn't catch: uint8 bBool : 1
	
			if (const FBoolProperty* BoolProperty = CastField<const FBoolProperty>(Property))
			{
				return true;
			}
			return false;
		}

		const FString CPPType = Property->GetCPPType();

		return CPPType == TypeFilter || CPPType == AlternateTypeFilter;
	};

	// allow struct bindings to bind context structs directly
	Args.OnCanBindToContextStruct = FOnCanBindToContextStruct::CreateLambda([this](UStruct* StructType)
	{
		if (StructType)
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
		}
		return false;
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

	Args.OnCanAcceptPropertyOrChildrenWithBindingChain = FOnCanAcceptPropertyOrChildrenWithBindingChain::CreateLambda([](FProperty* InProperty, TConstArrayView<FBindingChainElement> BindingChain)
		{
			// Make only blueprint visible properties visible for binding.
			return InProperty->HasAnyPropertyFlags(CPF_BlueprintVisible);
		});	

	if (OnAddBinding.IsBound())
	{
		Args.OnAddBinding = OnAddBinding; 
	}
	else
	{
		Args.OnAddBinding = FOnAddBinding::CreateLambda( 
			[this](FName InPropertyName, const TArray<FBindingChainElement>& InBindingChain)
			{
				if (PropertyBindingValue.IsSet())
				{
					UObject* TransactionObject = Cast<UObject>(ContextClassOwner);
					FChooserPropertyBinding* ContextProperty = PropertyBindingValue.Get();
					const FScopedTransaction Transaction(NSLOCTEXT("ContextPropertyWidget", "Change Property Binding", "Change Property Binding"));
					TransactionObject->Modify(true);

					// todo: move these to a virtual function on FChooserPropertyBinding
					Chooser::CopyPropertyChain(InBindingChain, *PropertyBindingValue.Get());
					
					FField* Property = InBindingChain.Last().Field.ToField();
					
					ContextProperty->DisplayName = "";
					if (Property)
					{
						ContextProperty->DisplayName = Property->GetDisplayNameText().ToString();
						static const int ShortNameLength = 5;
						if (ContextProperty->DisplayName.Len() < ShortNameLength && InBindingChain.Num() > 2)
						{
							FField* ParentProperty = InBindingChain[InBindingChain.Num() - 2].Field.ToField();
							ContextProperty->DisplayName = ParentProperty->GetDisplayNameText().ToString() + "." + ContextProperty->DisplayName;
						}
					}
					
					if (TypeFilter == "enum")
					{
						FChooserEnumPropertyBinding* EnumPropertyValue = static_cast<FChooserEnumPropertyBinding*>(ContextProperty);
						
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
						FChooserObjectPropertyBinding* ObjectPropertyBinding = static_cast<FChooserObjectPropertyBinding*>(ContextProperty);
						
						if (const FObjectPropertyBase* ObjectProperty = CastField<const FObjectPropertyBase>(Property))
						{
							ObjectPropertyBinding->AllowedClass = ObjectProperty->PropertyClass;
						}
					}
					if (TypeFilter == "struct")
					{
						FChooserStructPropertyBinding* StructPropertyBinding = static_cast<FChooserStructPropertyBinding*>(ContextProperty);

						StructPropertyBinding->StructType = nullptr;

						if (const FStructProperty* StructProperty = CastField<const FStructProperty>(Property))
						{
							StructPropertyBinding->StructType = StructProperty->Struct;
						}
						else if (InBindingChain.Num() == 1)
						{
							// direct binding to a context struct
							if (ContextClassOwner)
							{
								TConstArrayView<FInstancedStruct> ContextData = ContextClassOwner->GetContextData();
								if (ContextData.IsValidIndex(InBindingChain[0].ArrayIndex))
								{
									if (const FContextObjectTypeStruct* StructContext = ContextData[InBindingChain[0].ArrayIndex].GetPtr<FContextObjectTypeStruct>())
									{
										StructPropertyBinding->StructType = StructContext->Struct;
									}
								}
							}
						}
					}

					ContextProperty->Compile(ContextClassOwner);
					OnValueChanged.ExecuteIfBound();
				}
			});
	}

	Args.CurrentBindingToolTipText = MakeAttributeLambda([this]()
	{
		const FText Bind = NSLOCTEXT("ContextPropertyWidget", "Bind", "Bind");
		FText CurrentValue = Bind;
		
		const FChooserPropertyBinding* PropertyValue = PropertyBindingValue.Get();
		if (PropertyValue != nullptr)
		{
			if (!PropertyValue->CompileMessage.IsEmpty())
			{
				CurrentValue = PropertyValue->CompileMessage;
			}
			else
			{
				if (PropertyValue->PropertyBindingChain.Num()>0)
				{
					TArray<FText> BindingChainText;
					BindingChainText.Reserve(PropertyValue->PropertyBindingChain.Num());
			 
					for (const FName& Name : PropertyValue->PropertyBindingChain)
					{
						BindingChainText.Add(FText::FromName(Name));
					}
				
					CurrentValue = FText::Join(NSLOCTEXT("ContextPropertyWidget", "PropertyPathSeparator","."), BindingChainText);
				}
			}
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
									if (StructType->Struct)
									{
										CurrentValue = FText::FromString(StructType->Struct->GetAuthoredName());
									}
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

	Args.CurrentBindingImage = MakeAttributeLambda([this]() -> const FSlateBrush*
		{
			static FName PropertyIcon(TEXT("Kismet.Tabs.Variables"));
			static FName WarningIcon(TEXT("Icons.WarningWithColor"));
			bool bHasWarning = false;
		
			const FChooserPropertyBinding* PropertyValue = PropertyBindingValue.Get();
			if (PropertyValue != nullptr)
			{
				bHasWarning = !PropertyValue->CompileMessage.IsEmpty();
			}
		
			return FAppStyle::GetBrush(bHasWarning ? WarningIcon : PropertyIcon);
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
	OnValueChanged = InArgs._OnValueChanged;
	PropertyBindingValue = InArgs._PropertyBindingValue;
	OnAddBinding = InArgs._OnAddBinding;
	UpdateWidget();


	if (TypeFilter[TypeFilter.Len() - 1] == '*')
	{
		FString Trimmed = TypeFilter.TrimChar('*');
		AlternateTypeFilter = "TObjectPtr<" + Trimmed + ">";
	}


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
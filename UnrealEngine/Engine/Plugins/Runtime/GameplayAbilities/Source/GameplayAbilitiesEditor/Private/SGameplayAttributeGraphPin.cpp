// Copyright Epic Games, Inc. All Rights Reserved.

#include "SGameplayAttributeGraphPin.h"
#include "Widgets/SBoxPanel.h"
#include "AttributeSet.h"
#include "SGameplayAttributeWidget.h"
#include "ScopedTransaction.h"
#include "UObject/CoreRedirects.h"
#include "UObject/UObjectIterator.h"
#include "AbilitySystemComponent.h"

#define LOCTEXT_NAMESPACE "K2Node"

void SGameplayAttributeGraphPin::Construct( const FArguments& InArgs, UEdGraphPin* InGraphPinObj )
{
	SGraphPin::Construct( SGraphPin::FArguments(), InGraphPinObj );
	LastSelectedProperty = nullptr;

}

TSharedRef<SWidget>	SGameplayAttributeGraphPin::GetDefaultValueWidget()
{
	// Parse out current default value	
	FString DefaultString = GraphPinObj->GetDefaultAsString();
	FGameplayAttribute DefaultAttribute;

	// check for redirectors
	FString TempString;
	FString WorkingString;
	if (DefaultString.Split(TEXT(","), nullptr, &WorkingString, ESearchCase::CaseSensitive))
	{
		if (WorkingString.Split(TEXT(","), &TempString, nullptr, ESearchCase::CaseSensitive))
		{
			WorkingString = MoveTemp(TempString);
			FString AttributeNameString;
			WorkingString.Split(TEXT(":"), &TempString, &AttributeNameString, ESearchCase::CaseSensitive);

			WorkingString = MoveTemp(TempString);
			FString ClassNameString;
			WorkingString.Split(TEXT("."), &TempString, &ClassNameString, ESearchCase::CaseSensitive);

			WorkingString = MoveTemp(TempString);
			FString PackageNameString;
			WorkingString.Split(TEXT("Attribute="), nullptr, &PackageNameString);

			// class redirector check
			const FCoreRedirect* ClassValueRedirect = nullptr;
			FCoreRedirectObjectName OldClassName(FName(*ClassNameString, FNAME_Find), NAME_None, FName(PackageNameString, FNAME_Find));
			FCoreRedirectObjectName NewClassName;
			bool bFoundClassRedirector = FCoreRedirects::RedirectNameAndValues(ECoreRedirectFlags::Type_Class, OldClassName, NewClassName, &ClassValueRedirect);

			// property redirector check
			const FCoreRedirect* PropertyRedirect = nullptr;
			FCoreRedirectObjectName OldPropertyName(FName(*AttributeNameString, FNAME_Find), FName(*ClassNameString, FNAME_Find), FName(PackageNameString, FNAME_Find));
			FCoreRedirectObjectName NewPropertyName;
			bool bFoundPropertyRedirector = FCoreRedirects::RedirectNameAndValues(ECoreRedirectFlags::Type_Property, OldPropertyName, NewPropertyName, &PropertyRedirect);

			if (bFoundClassRedirector || bFoundPropertyRedirector)
			{
				// we found a redirector
				// now we need to find the matching property for the new attribute

				bool bFoundMatch = false;

				for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
				{
					UClass* Class = *ClassIt;
					if ((Class->IsChildOf(UAttributeSet::StaticClass()) || Class->IsChildOf(UAbilitySystemComponent::StaticClass())) && !Class->ClassGeneratedBy)
					{
						if (Class->GetFName() == NewClassName.ObjectName)
						{
							FName PropertyNameToFind(AttributeNameString, FNAME_Find);

							if (bFoundPropertyRedirector)
							{
								PropertyNameToFind = NewPropertyName.ObjectName;
							}

							for (TFieldIterator<FProperty> PropertyIt(Class, EFieldIteratorFlags::ExcludeSuper); PropertyIt; ++PropertyIt)
							{
								FProperty* Property = *PropertyIt;
								if (Property->GetFName() == PropertyNameToFind)
								{
									FGameplayAttribute Attribute;
									Attribute.SetUProperty(Property);
									DefaultString.Reset();
									FGameplayAttribute::StaticStruct()->ExportText(DefaultString, &Attribute, &Attribute, nullptr, EPropertyPortFlags::PPF_SerializedAsImportText, nullptr);
									bFoundMatch = true;
									break;
								}
							}
						}
					}

					if (bFoundMatch)
					{
						break;
					}
				}
			}
		}
	}

	UScriptStruct* PinLiteralStructType = FGameplayAttribute::StaticStruct();
	if (!DefaultString.IsEmpty())
	{
		PinLiteralStructType->ImportText(*DefaultString, &DefaultAttribute, nullptr, EPropertyPortFlags::PPF_SerializedAsImportText, GError, PinLiteralStructType->GetName(), true);
	}

	//Create widget
	return SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SGameplayAttributeWidget)
			.OnAttributeChanged(this, &SGameplayAttributeGraphPin::OnAttributeChanged)
			.DefaultProperty(DefaultAttribute.GetUProperty())
			.Visibility(this, &SGraphPin::GetDefaultValueVisibility)
			.IsEnabled(this, &SGameplayAttributeGraphPin::GetDefaultValueIsEnabled)
		];
}

void SGameplayAttributeGraphPin::OnAttributeChanged(FProperty* SelectedAttribute)
{
	FString FinalValue;
	FGameplayAttribute NewAttributeStruct;
	NewAttributeStruct.SetUProperty(SelectedAttribute);

	FGameplayAttribute::StaticStruct()->ExportText(FinalValue, &NewAttributeStruct, &NewAttributeStruct, nullptr, EPropertyPortFlags::PPF_SerializedAsImportText, nullptr);

	if (FinalValue != GraphPinObj->GetDefaultAsString())
	{
		const FScopedTransaction Transaction(NSLOCTEXT("GraphEditor", "ChangePinValue", "Change Pin Value"));
		GraphPinObj->Modify();
		GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, FinalValue);
	}

	LastSelectedProperty = SelectedAttribute;
}

#undef LOCTEXT_NAMESPACE

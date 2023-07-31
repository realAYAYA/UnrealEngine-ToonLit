// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableObjectFactory.h"

#include "MuCO/CustomizableObject.h"
#include "MuCOE/CustomizableObjectGraph.h"
#include "Templates/SubclassOf.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UnrealNames.h"

class FFeedbackContext;
class UClass;
class UObject;

#define LOCTEXT_NAMESPACE "CustomizableObjectFactory"


UCustomizableObjectFactory::UCustomizableObjectFactory()
	: Super()
{
	// Property initialization
	bCreateNew = true;
	SupportedClass = UCustomizableObject::StaticClass();
	bEditAfterNew = true;
}


bool UCustomizableObjectFactory::DoesSupportClass(UClass * Class)
{
	return ( Class == UCustomizableObject::StaticClass() );
}


UClass* UCustomizableObjectFactory::ResolveSupportedClass()
{
	return UCustomizableObject::StaticClass();
}


bool UCustomizableObjectFactory::ConfigureProperties()
{
	return true;
}


UObject* UCustomizableObjectFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UCustomizableObject* NewObj = NewObject<UCustomizableObject>(InParent, Class, Name, Flags);

	NewObj->Source = NewObject<UCustomizableObjectGraph>(NewObj, NAME_None, RF_Transactional);

	return NewObj;
}

#undef LOCTEXT_NAMESPACE

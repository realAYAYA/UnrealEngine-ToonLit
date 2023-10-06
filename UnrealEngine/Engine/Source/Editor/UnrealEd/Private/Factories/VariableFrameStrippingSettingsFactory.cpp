// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factories/VariableFrameStrippingSettingsFactory.h"
#include "Animation/VariableFrameStrippingSettings.h"
#include "Templates/SubclassOf.h"

class FFeedbackContext;
class UClass;
class UObject;

UVariableFrameStrippingSettingsFactory::UVariableFrameStrippingSettingsFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Provide the factory with information about how to handle our asset
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UVariableFrameStrippingSettings::StaticClass();

}

UObject* UVariableFrameStrippingSettingsFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	// Create and return a new instance of VariableFrameStrippingSettings
	return NewObject<UVariableFrameStrippingSettings>(InParent, Class, Name, Flags);
}
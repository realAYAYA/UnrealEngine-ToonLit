// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
AnimCurveCompressionSettingsFactory.cpp: Factory for animation curve compression settings assets
=============================================================================*/

#include "Factories/AnimCurveCompressionSettingsFactory.h"
#include "Animation/AnimCurveCompressionSettings.h"

UAnimCurveCompressionSettingsFactory::UAnimCurveCompressionSettingsFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	SupportedClass = UAnimCurveCompressionSettings::StaticClass();
}

UObject* UAnimCurveCompressionSettingsFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UAnimCurveCompressionSettings>(InParent, Class, Name, Flags);
}

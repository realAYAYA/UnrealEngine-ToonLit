// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeLayerInfoObjectFactory.h"

#include "LandscapeLayerInfoObject.h"
#include "Templates/SubclassOf.h"

class FFeedbackContext;
class UClass;
class UObject;

ULandscapeLayerInfoObjectFactory::ULandscapeLayerInfoObjectFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = ULandscapeLayerInfoObject::StaticClass();
}

UObject* ULandscapeLayerInfoObjectFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	ULandscapeLayerInfoObject* NewLayerInfoObject = NewObject<ULandscapeLayerInfoObject>(InParent, Class, Name, Flags | RF_Public | RF_Standalone | RF_Transactional);

	return NewLayerInfoObject;
}

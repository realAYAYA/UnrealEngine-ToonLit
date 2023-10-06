// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeGrassTypeFactory.h"

#include "LandscapeGrassType.h"
#include "Templates/SubclassOf.h"

class FFeedbackContext;
class UClass;
class UObject;

ULandscapeGrassTypeFactory::ULandscapeGrassTypeFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = ULandscapeGrassType::StaticClass();
}

UObject* ULandscapeGrassTypeFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	auto NewGrassType = NewObject<ULandscapeGrassType>(InParent, Class, Name, Flags | RF_Transactional);

	return NewGrassType;
}

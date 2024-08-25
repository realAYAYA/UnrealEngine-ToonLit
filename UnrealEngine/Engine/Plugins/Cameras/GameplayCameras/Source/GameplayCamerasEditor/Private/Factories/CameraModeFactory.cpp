// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factories/CameraModeFactory.h"

#include "Core/CameraMode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CameraModeFactory)

#define LOCTEXT_NAMESPACE "CameraModeFactory"

UCameraModeFactory::UCameraModeFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UCameraMode::StaticClass();
}

UObject* UCameraModeFactory::FactoryCreateNew(UClass* Class, UObject* Parent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UCameraMode* NewCameraMode = NewObject<UCameraMode>(Parent, Class, Name, Flags | RF_Transactional);
	return NewCameraMode;
}

bool UCameraModeFactory::ConfigureProperties()
{
	return true;
}

bool UCameraModeFactory::ShouldShowInNewMenu() const
{
	return true;
}

#undef LOCTEXT_NAMESPACE



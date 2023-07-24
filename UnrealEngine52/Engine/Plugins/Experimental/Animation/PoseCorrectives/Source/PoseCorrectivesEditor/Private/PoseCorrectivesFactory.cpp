// Copyright Epic Games, Inc. All Rights Reserved.s

#include "PoseCorrectivesFactory.h"

#include "PoseCorrectivesAsset.h"

UPoseCorrectivesFactory::UPoseCorrectivesFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UPoseCorrectivesAsset::StaticClass();
}


UObject* UPoseCorrectivesFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InContext, FFeedbackContext* InWarn)
{
	return NewObject<UPoseCorrectivesAsset>(InParent, InClass, InName, InFlags);
}

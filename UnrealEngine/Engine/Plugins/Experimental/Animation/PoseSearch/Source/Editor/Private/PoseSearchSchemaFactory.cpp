// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchSchemaFactory.h"
#include "PoseSearch/PoseSearch.h"

#define LOCTEXT_NAMESPACE "PoseSearchEditor"

UPoseSearchSchemaFactory::UPoseSearchSchemaFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UPoseSearchSchema::StaticClass();
}

UObject* UPoseSearchSchemaFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UPoseSearchSchema>(InParent, Class, Name, Flags);
}

FString UPoseSearchSchemaFactory::GetDefaultNewAssetName() const
{
	return FString(TEXT("NewMotionDatabaseConfig"));
}

#undef LOCTEXT_NAMESPACE
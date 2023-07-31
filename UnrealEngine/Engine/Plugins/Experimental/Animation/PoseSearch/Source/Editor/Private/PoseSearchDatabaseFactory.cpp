// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchDatabaseFactory.h"
#include "PoseSearch/PoseSearch.h"

#define LOCTEXT_NAMESPACE "PoseSearchEditor"

UPoseSearchDatabaseFactory::UPoseSearchDatabaseFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UPoseSearchDatabase::StaticClass();
}

UObject* UPoseSearchDatabaseFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UPoseSearchDatabase>(InParent, Class, Name, Flags);
}

FString UPoseSearchDatabaseFactory::GetDefaultNewAssetName() const
{
	return FString(TEXT("NewMotionDatabase"));
}

#undef LOCTEXT_NAMESPACE
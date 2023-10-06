// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchSchemaFactory.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PoseSearchFeatureChannel_Pose.h"
#include "PoseSearchFeatureChannel_Trajectory.h"

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
	UPoseSearchSchema* Schema = NewObject<UPoseSearchSchema>(InParent, Class, Name, Flags);

	// defaulting UPoseSearchSchema for a meaningful locomotion setup
	Schema->AddChannel(NewObject<UPoseSearchFeatureChannel_Trajectory>(Schema, NAME_None, RF_Transactional));
	Schema->AddChannel(NewObject<UPoseSearchFeatureChannel_Pose>(Schema, NAME_None, RF_Transactional));

	return Schema;
}

FString UPoseSearchSchemaFactory::GetDefaultNewAssetName() const
{
	return FString(TEXT("NewMotionDatabaseConfig"));
}

#undef LOCTEXT_NAMESPACE
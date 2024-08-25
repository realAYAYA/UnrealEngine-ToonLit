// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scene/InterchangeCameraActorFactory.h"

#include "InterchangeCameraFactoryNode.h"
#include "Scene/InterchangeActorFactory.h"
#include "Scene/InterchangeActorHelper.h"

#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeCameraActorFactory)

UClass* UInterchangeCineCameraActorFactory::GetFactoryClass() const
{
	return ACineCameraActor::StaticClass();
}

UObject* UInterchangeCineCameraActorFactory::ProcessActor(AActor& SpawnedActor, const UInterchangeActorFactoryNode& /*FactoryNode*/, const UInterchangeBaseNodeContainer& /*NodeContainer*/, const FImportSceneObjectsParams& /*Params*/)
{
	ACineCameraActor* CineCameraActor = Cast<ACineCameraActor>(&SpawnedActor);

	return CineCameraActor ? CineCameraActor->GetCineCameraComponent() : nullptr;
}


UClass* UInterchangeCameraActorFactory::GetFactoryClass() const
{
	return ACameraActor::StaticClass();
}

UObject* UInterchangeCameraActorFactory::ProcessActor(AActor& SpawnedActor, const UInterchangeActorFactoryNode& /*FactoryNode*/, const UInterchangeBaseNodeContainer& /*NodeContainer*/, const FImportSceneObjectsParams& /*Params*/)
{
	ACameraActor* CameraActor = Cast<ACameraActor>(&SpawnedActor);

	return CameraActor ? CameraActor->GetCameraComponent() : nullptr;
}
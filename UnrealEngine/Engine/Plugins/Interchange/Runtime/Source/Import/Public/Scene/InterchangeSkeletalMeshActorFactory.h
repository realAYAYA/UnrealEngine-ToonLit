// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangeActorFactory.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "InterchangeSkeletalMeshActorFactory.generated.h"

class AActor;
class ASkeletalMeshActor;
class UInterchangeActorFactoryNode;

UCLASS(BlueprintType, Experimental)
class INTERCHANGEIMPORT_API UInterchangeSkeletalMeshActorFactory : public UInterchangeActorFactory
{
	GENERATED_BODY()
public:

	//////////////////////////////////////////////////////////////////////////
	// Interchange factory base interface begin

	virtual UClass* GetFactoryClass() const override;

	virtual UObject* CreateSceneObject(const UInterchangeFactoryBase::FCreateSceneObjectsParams& CreateSceneObjectsParams) override;

	virtual bool CanExecuteOnAnyThread() const override
	{
		return false;
	}

	virtual void PostImportPreCompletedCallback(const FImportPreCompletedCallbackParams& Arguments) override;

	// Interchange factory base interface end
	//////////////////////////////////////////////////////////////////////////

private:

	void SetupSkeletalMeshActor(const UInterchangeBaseNodeContainer* NodeContainer, const UInterchangeFactoryBaseNode* ActorFactoryNode, ASkeletalMeshActor* SkeletalMeshActor);
};



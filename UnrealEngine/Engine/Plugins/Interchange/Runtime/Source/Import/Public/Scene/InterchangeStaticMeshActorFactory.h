// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangeActorFactory.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "InterchangeStaticMeshActorFactory.generated.h"

class AActor;
class AStaticMeshActor;
class UInterchangeActorFactoryNode;

UCLASS(BlueprintType, Experimental)
class INTERCHANGEIMPORT_API UInterchangeStaticMeshActorFactory : public UInterchangeFactoryBase
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

	// Interchange factory base interface end
	//////////////////////////////////////////////////////////////////////////

private:

	void SetupStaticMeshActor(const UInterchangeBaseNodeContainer* NodeContainer, const UInterchangeFactoryBaseNode* ActorFactoryNode, AStaticMeshActor* StaticMeshActor);
};



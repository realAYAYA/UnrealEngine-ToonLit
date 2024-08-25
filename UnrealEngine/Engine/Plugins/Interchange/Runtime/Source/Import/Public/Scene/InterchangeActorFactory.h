// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangeFactoryBase.h"
#include "InterchangeSourceData.h"
//#include "InterchangeResult.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "InterchangeActorFactory.generated.h"

class AActor;
class UInterchangeActorFactoryNode;
class USceneComponent;

UCLASS(BlueprintType)
class INTERCHANGEIMPORT_API UInterchangeActorFactory : public UInterchangeFactoryBase
{
	GENERATED_BODY()
public:

	//////////////////////////////////////////////////////////////////////////
	// Interchange factory base interface begin

	virtual UClass* GetFactoryClass() const override;

private:
	virtual UObject* ImportSceneObject_GameThread(const UInterchangeFactoryBase::FImportSceneObjectsParams& CreateSceneObjectsParams) override;

	// Interchange factory base interface end
	//////////////////////////////////////////////////////////////////////////

protected:
	/**
	 * Method called in UInterchangeActorFactory::ImportSceneObject_GameThread to allow
	 * child class to complete the creation of the actor.
	 * This method is expected to return the UOBject to apply the factory node's custom attributes to.
	 */
	virtual UObject* ProcessActor(AActor& SpawnedActor, const UInterchangeActorFactoryNode& FactoryNode, const UInterchangeBaseNodeContainer& NodeContainer, const FImportSceneObjectsParams& Params);

	template <typename T>
	void LogMessage(const FImportSceneObjectsParams& Params, const FText& Message, const FString& ActorLabel)
	{
		T* Item = /*Super::*/AddMessage<T>();
		FillMessage(Params, Message, ActorLabel, *Item);
	}

	template <typename T>
	void FillMessage(const FImportSceneObjectsParams& Params, const FText& Message, const FString& ActorLabel, T& Result)
	{
		Result.SourceAssetName = Params.SourceData ? Params.SourceData->GetFilename() : TEXT("Unknown file");
		Result.DestinationAssetName = Params.ObjectName;
		Result.AssetType = GetFactoryClass();
		Result.Text = Message;

		if (!ActorLabel.IsEmpty())
		{
			Result.AssetFriendlyName = ActorLabel;
		}
	}
};



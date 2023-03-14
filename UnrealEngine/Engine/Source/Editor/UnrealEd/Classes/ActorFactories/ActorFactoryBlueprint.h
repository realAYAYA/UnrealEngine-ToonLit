// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ActorFactories/ActorFactory.h"
#include "ActorFactoryBlueprint.generated.h"

class AActor;
struct FAssetData;

UCLASS(config=Editor, collapsecategories, hidecategories=Object)
class UNREALED_API UActorFactoryBlueprint : public UActorFactory
{
	GENERATED_UCLASS_BODY()

	//~ Begin UActorFactory Interface
	virtual bool PreSpawnActor( UObject* Asset, FTransform& InOutLocation ) override;
	virtual bool CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg ) override;
	virtual AActor* GetDefaultActor( const FAssetData& AssetData ) override;
	virtual UClass* GetDefaultActorClass( const FAssetData& AssetData ) override;
	//~ End UActorFactory Interface	
};




// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "Dataflow/DataflowComponent.h"

#include "DataflowActor.generated.h"


UCLASS()
class DATAFLOWENGINEPLUGIN_API ADataflowActor : public AActor
{
	GENERATED_UCLASS_BODY()

public:

	/* DataflowComponent */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Destruction, meta = (ExposeFunctionCategories = "Components|Dataflow", AllowPrivateAccess = "true"))
	TObjectPtr<UDataflowComponent> DataflowComponent;
	UDataflowComponent* GetDataflowComponent() const { return DataflowComponent; }


#if WITH_EDITOR
	virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override;
#endif
};

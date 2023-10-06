// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/** This class represents an FieldSystem Actor. */

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Field/FieldSystem.h"
#include "Field/FieldSystemNodes.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Field/FieldSystemComponent.h"
#endif
#include "UObject/ObjectMacros.h"

#include "FieldSystemActor.generated.h"

class UFieldSystemComponent;

UCLASS(meta=(ChildCanTick), MinimalAPI)
class AFieldSystemActor: public AActor
{
	GENERATED_UCLASS_BODY()

public:

	/* FieldSystemComponent */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Field, meta = (ExposeFunctionCategories = "Components|FieldSystem", AllowPrivateAccess = "true"))
	TObjectPtr<UFieldSystemComponent> FieldSystemComponent;
	UFieldSystemComponent* GetFieldSystemComponent() const { return FieldSystemComponent; }
	FIELDSYSTEMENGINE_API virtual void OnConstruction(const FTransform& Transform) override;
};

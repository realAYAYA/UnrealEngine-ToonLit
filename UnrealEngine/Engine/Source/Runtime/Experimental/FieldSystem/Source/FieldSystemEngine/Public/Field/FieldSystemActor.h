// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/** This class represents an FieldSystem Actor. */

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Field/FieldSystem.h"
#include "Field/FieldSystemNodes.h"
#include "Field/FieldSystemComponent.h"
#include "UObject/ObjectMacros.h"

#include "FieldSystemActor.generated.h"


UCLASS(meta=(ChildCanTick))
class FIELDSYSTEMENGINE_API AFieldSystemActor: public AActor
{
	GENERATED_UCLASS_BODY()

public:

	/* FieldSystemComponent */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Field, meta = (ExposeFunctionCategories = "Components|FieldSystem", AllowPrivateAccess = "true"))
	TObjectPtr<UFieldSystemComponent> FieldSystemComponent;
	UFieldSystemComponent* GetFieldSystemComponent() const { return FieldSystemComponent; }
	virtual void OnConstruction(const FTransform& Transform) override;
};

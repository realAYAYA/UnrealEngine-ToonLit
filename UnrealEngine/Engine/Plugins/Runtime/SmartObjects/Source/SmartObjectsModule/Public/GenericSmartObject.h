// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "GenericSmartObject.generated.h"

class USmartObjectComponent;
class USmartObjectRenderingComponent;
struct FGameplayTagContainer;

UCLASS(hidecategories = (Input))
class SMARTOBJECTSMODULE_API AGenericSmartObject : public AActor
{
	GENERATED_BODY()
public:
	AGenericSmartObject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

protected:
	UPROPERTY(EditAnywhere, Category = SmartObject, NoClear)
	TObjectPtr<USmartObjectComponent> SOComponent;

#if WITH_EDITORONLY_DATA
	UPROPERTY(NoClear)
	TObjectPtr<USmartObjectRenderingComponent> RenderingComponent;
#endif // WITH_EDITORONLY_DATA
};

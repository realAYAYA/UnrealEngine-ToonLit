// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "ImagePlate.generated.h"

class UImagePlateComponent;

UCLASS(ClassGroup=Rendering, hidecategories=(Object,Activation,Physics,Collision,Input,PhysicsVolume))
class IMAGEPLATE_API AImagePlate : public AActor
{
public:
	GENERATED_BODY()

	AImagePlate(const FObjectInitializer& Init);

	UImagePlateComponent* GetImagePlateComponent() const
	{
		return ImagePlate;
	}

protected:

	UPROPERTY(Category=ImagePlate, VisibleAnywhere, BlueprintReadOnly, meta=(ExposeFunctionCategories="Mesh,Rendering,Physics,Components|StaticMesh"))
	TObjectPtr<UImagePlateComponent> ImagePlate;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif

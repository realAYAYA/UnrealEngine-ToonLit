// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "ControlPointMeshActor.generated.h"

/**
 * ControlPointMeshActor is an actor with a ControlPointMeshComponent.
 *
 * @see UControlPointMeshComponent
 */
UCLASS(NotPlaceable, hideCategories = (Input), showCategories = ("Input|MouseInput", "Input|TouchInput"), ConversionRoot, ComponentWrapperClass, meta = (ChildCanTick))
class LANDSCAPE_API AControlPointMeshActor : public AActor
{
	GENERATED_UCLASS_BODY()

private:
	UPROPERTY(Category = ControlPointMeshActor, VisibleAnywhere, BlueprintReadOnly, meta = (ExposeFunctionCategories = "Mesh,Rendering,Physics,Components|StaticMesh,Components|ControlPointMesh", AllowPrivateAccess = "true"))
	TObjectPtr<class UControlPointMeshComponent> ControlPointMeshComponent;

public:

	/** Function to change mobility type */
	void SetMobility(EComponentMobility::Type InMobility);

#if WITH_EDITOR
	//~ Begin AActor Interface
	virtual void CheckForErrors() override;
	virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override;
	//~ End AActor Interface
#endif // WITH_EDITOR	

protected:
	//~ Begin UObject Interface.
	virtual FString GetDetailedInfoInternal() const override;
	//~ End UObject Interface.

public:
	/** Returns ControlPointMeshComponent subobject **/
	class UControlPointMeshComponent* GetControlPointMeshComponent() const;
};




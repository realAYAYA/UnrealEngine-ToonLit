// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "IAvaInteractiveToolsModeDetailsObjectProvider.h"
#include "Viewport/Interaction/IAvaSnapPointGenerator.h"
#include "AvaShapeActor.generated.h"

class UAvaShapeDynamicMeshBase;
class UDynamicMeshComponent;

UCLASS()
class AVALANCHESHAPES_API AAvaShapeActor
	: public AActor
	, public IAvaSnapPointGenerator
	, public IAvaInteractiveToolsModeDetailsObjectProvider
{
	GENERATED_BODY()

public:
	AAvaShapeActor();

	bool HasFinishedCreation() const { return bFinishedCreation; }
	void FinishCreation() { bFinishedCreation = true; }

	UAvaShapeDynamicMeshBase* GetDynamicMesh() const { return DynamicMesh; }
	void SetDynamicMesh(UAvaShapeDynamicMeshBase* NewDynamicMesh);

	UDynamicMeshComponent* GetShapeMeshComponent() const { return ShapeMeshComponent; }

#if WITH_EDITOR
	virtual FString GetDefaultActorLabel() const override;
	virtual void PostEditUndo() override;
#endif

	FAvaColorChangeData GetColorData() const;
	void SetColorData(const FAvaColorChangeData& NewColorData);

	//~ Begin IAvaSnapPointGenerator
	virtual TArray<FAvaSnapPoint> GetLocalSnapPoints() const override;
	//~ End IAvaSnapPointGenerator

	//~ Begin IAvaInteractiveToolsModeDetailsObjectProvider
	virtual UObject* GetModeDetailsObject_Implementation() const override;
	//~ End IAvaInteractiveToolsModeDetailsObjectProvider

protected:
	static const FName ShapeComponentName;

	UPROPERTY()
	bool bFinishedCreation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Motion Design")
	TObjectPtr<UAvaShapeDynamicMeshBase> DynamicMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Motion Design")
	TObjectPtr<UDynamicMeshComponent> ShapeMeshComponent = nullptr;
};

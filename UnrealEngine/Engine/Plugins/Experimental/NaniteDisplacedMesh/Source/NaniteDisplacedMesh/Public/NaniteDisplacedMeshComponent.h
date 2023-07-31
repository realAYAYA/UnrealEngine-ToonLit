// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/StaticMeshComponent.h"
#include "NaniteDisplacedMesh.h"
#include "NaniteDisplacedMeshComponent.generated.h"

class FPrimitiveSceneProxy;
class UStaticMeshComponent;
class UMaterialInterface;
class UTexture;

UCLASS(ClassGroup=Rendering, hidecategories=(Object,Activation,Collision,"Components|Activation",Physics), editinlinenew, meta=(BlueprintSpawnableComponent))
class NANITEDISPLACEDMESH_API UNaniteDisplacedMeshComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	virtual void PostLoad() override;

	virtual void BeginDestroy() override;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual void OnRegister() override;

	virtual const Nanite::FResources* GetNaniteResources() const;

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;

	void OnRebuild();
#endif

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Displacement)
	TObjectPtr<class UNaniteDisplacedMesh> DisplacedMesh;

private:
	void UnbindCallback();
	void BindCallback();
};

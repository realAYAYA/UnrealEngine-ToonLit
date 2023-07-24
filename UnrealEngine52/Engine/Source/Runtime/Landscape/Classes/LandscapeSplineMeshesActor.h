// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ActorPartition/PartitionActor.h"
#include "LandscapeSplineMeshesActor.generated.h"

//
// This class is only intended to be used by UWorldPartitionLandscapeSplineMeshesBuilder which extracts and clones
// landscape spline and control point static meshes into partitioned actors
//
// This serves as an optimization as these actors will be streamed at runtime
//
UCLASS(NotPlaceable, NotBlueprintable, hideCategories = (Input), showCategories = ("Input|MouseInput", "Input|TouchInput"), ConversionRoot, ComponentWrapperClass, meta = (ChildCanTick))
class LANDSCAPE_API ALandscapeSplineMeshesActor : public APartitionActor
{
	GENERATED_UCLASS_BODY()

private:
	UPROPERTY(Category = PartitionSplineMeshActor, VisibleAnywhere, BlueprintReadOnly, meta = (ExposeFunctionCategories = "Mesh,Rendering,Physics,Components|StaticMesh,Components|SplineMesh", AllowPrivateAccess = "true"))
	TArray<TObjectPtr<class UStaticMeshComponent>> StaticMeshComponents;

public:

#if WITH_EDITOR
	class UStaticMeshComponent* CreateStaticMeshComponent(const TSubclassOf<class UStaticMeshComponent>& InComponentClass);
	void ClearStaticMeshComponents();

	//~ Begin AActor Interface
	virtual void CheckForErrors() override;
	virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override;
	virtual bool IsRuntimeOnly() const override { return true; }
	//~ End AActor Interface

	//~ Begin APartitionActor Interface
	virtual uint32 GetDefaultGridSize(UWorld* InWorld) const override;
	virtual FGuid GetGridGuid() const override { return GridGuid; }
	//~ End APartitionActor Interface

	void SetGridGuid(const FGuid& InGuid) { GridGuid = InGuid; }
#endif // WITH_EDITOR

	const TArray<class UStaticMeshComponent*>& GetStaticMeshComponents() const;

protected:
	//~ Begin UObject Interface.
	virtual FString GetDetailedInfoInternal() const override;
	//~ End UObject Interface.

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FGuid GridGuid;
#endif
};
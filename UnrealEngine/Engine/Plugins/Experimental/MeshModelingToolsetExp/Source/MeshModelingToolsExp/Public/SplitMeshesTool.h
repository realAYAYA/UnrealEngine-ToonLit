// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseTools/MultiSelectionMeshEditingTool.h"
#include "InteractiveToolBuilder.h"
#include "PropertySets/CreateMeshObjectTypeProperties.h"
#include "SplitMeshesTool.generated.h"

class UMaterialInterface;


UCLASS()
class MESHMODELINGTOOLSEXP_API USplitMeshesToolBuilder : public UMultiSelectionMeshEditingToolBuilder
{
	GENERATED_BODY()
public:
	virtual UMultiSelectionMeshEditingTool* CreateNewTool(const FToolBuilderState& SceneState) const override;

protected:
	virtual const FToolTargetTypeRequirements& GetTargetRequirements() const override;
};



UCLASS()
class MESHMODELINGTOOLSEXP_API USplitMeshesToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = Options)
	bool bTransferMaterials = true;
};



UCLASS()
class MESHMODELINGTOOLSEXP_API USplitMeshesTool : public UMultiSelectionMeshEditingTool
{
	GENERATED_BODY()

public:
	virtual void Setup() override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override;

	UPROPERTY()
	TObjectPtr<USplitMeshesToolProperties> BasicProperties;

	UPROPERTY()
	TObjectPtr<UCreateMeshObjectTypeProperties> OutputTypeProperties;

protected:
	struct FSourceMeshInfo
	{
		UE::Geometry::FDynamicMesh3 Mesh;
		TArray<UMaterialInterface*> Materials;
	};
	TArray<FSourceMeshInfo> SourceMeshes;


	struct FComponentsInfo
	{
		bool bNoComponents;
		TArray<UE::Geometry::FDynamicMesh3> Meshes;
		TArray<TArray<UMaterialInterface*>> Materials;
		TArray<FVector3d> Origins;
	};
	TArray<FComponentsInfo> SplitMeshes;

	int32 NoSplitCount = 0;

	void UpdateSplitMeshes();
};

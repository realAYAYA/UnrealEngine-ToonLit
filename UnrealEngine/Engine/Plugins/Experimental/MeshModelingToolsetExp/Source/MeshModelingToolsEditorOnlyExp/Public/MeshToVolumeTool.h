// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BaseTools/SingleSelectionMeshEditingTool.h"
#include "ConversionUtils/DynamicMeshToVolume.h"
#include "Drawing/LineSetComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Engine/BlockingVolume.h"
#include "GameFramework/Volume.h"
#include "InteractiveToolBuilder.h"
#include "PreviewMesh.h"
#include "PropertySets/OnAcceptProperties.h"
#include "SingleSelectionTool.h"
#include "MeshToVolumeTool.generated.h"

/**
 *
 */
UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API UMeshToVolumeToolBuilder : public USingleSelectionMeshEditingToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState & SceneState) const override;
	virtual USingleSelectionMeshEditingTool* CreateNewTool(const FToolBuilderState & SceneState) const override;
};




UENUM()
enum class EMeshToVolumeMode
{
	/** Create a separate Volume Face for each Triangle */
	TriangulatePolygons,
	/** Create Volume Faces based on Planar Polygons */
	MinimalPolygons
};



UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API UMeshToVolumeToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	/** Method for converting the input mesh to a set of Planar Polygonal Faces in the output Volume. */
	UPROPERTY(EditAnywhere, Category = ConversionOptions)
	EMeshToVolumeMode ConversionMode = EMeshToVolumeMode::MinimalPolygons;

	/** Type of new Volume to create on Accept */
	UPROPERTY(EditAnywhere, Category = NewVolume, meta = (EditCondition = "TargetVolume == nullptr") )
	TSubclassOf<class AVolume> NewVolumeType = ABlockingVolume::StaticClass();

	/** If set, the target Volume will be updated, rather than creating a new Volume. */
	UPROPERTY(EditAnywhere, Category = UpdateExisting)
	TLazyObjectPtr<AVolume> TargetVolume;
};


/**
 * Converts a mesh to a volume.
 *
 * Note: If ConversionUtils/DynamicMeshToVolume is rewritten to be safe for runtime, this
 * tool can be moved out of the editor-only section and put with VolumeToMeshTool.
 */
UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API UMeshToVolumeTool : public USingleSelectionMeshEditingTool
{
	GENERATED_BODY()

public:
	UMeshToVolumeTool();

	virtual void Setup() override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;

	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void OnTick(float DeltaTime) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }

protected:
	UPROPERTY()
	TObjectPtr<UMeshToVolumeToolProperties> Settings;

	UPROPERTY()
	TObjectPtr<UOnAcceptHandleSourcesProperties> HandleSourcesProperties;

	UPROPERTY()
	TObjectPtr<UPreviewMesh> PreviewMesh;

	UPROPERTY()
	TObjectPtr<ULineSetComponent> VolumeEdgesSet;

protected:
	UE::Geometry::FDynamicMesh3 InputMesh;

	void RecalculateVolume();
	void UpdateLineSet();

	bool bVolumeValid = false;
	TArray<UE::Conversion::FDynamicMeshFace> Faces;

};

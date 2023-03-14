// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "BaseTools/MultiSelectionMeshEditingTool.h"
#include "GeometryBase.h"
#include "InteractiveToolBuilder.h"
#include "MeshOpPreviewHelpers.h"
#include "ToolDataVisualizer.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Properties/MeshMaterialProperties.h"
#include "Properties/MeshUVChannelProperties.h"
#include "Drawing/UVLayoutPreview.h"

#include "UVLayoutTool.generated.h"


// Forward declarations
struct FMeshDescription;
class UDynamicMeshComponent;
class UUVLayoutProperties;
class UUVLayoutOperatorFactory;
PREDECLARE_GEOMETRY(class FDynamicMesh3);

/**
 *
 */
UCLASS()
class MESHMODELINGTOOLS_API UUVLayoutToolBuilder : public UMultiSelectionMeshEditingToolBuilder
{
	GENERATED_BODY()

public:
	virtual UMultiSelectionMeshEditingTool* CreateNewTool(const FToolBuilderState& SceneState) const override;
};


/**
 * The level editor version of the UV layout tool.
 */
UCLASS()
class MESHMODELINGTOOLS_API UUVLayoutTool : public UMultiSelectionMeshEditingTool
{
	GENERATED_BODY()

public:

	UUVLayoutTool();

	virtual void Setup() override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;

	virtual void OnTick(float DeltaTime) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override;

	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

	int32 GetSelectedUVChannel() const;

protected:

	UPROPERTY()
	TObjectPtr<UMeshUVChannelProperties> UVChannelProperties = nullptr;

	UPROPERTY()
	TObjectPtr<UUVLayoutProperties> BasicProperties = nullptr;

	UPROPERTY()
	TObjectPtr<UExistingMeshMaterialProperties> MaterialSettings = nullptr;

	UPROPERTY()
	TArray<TObjectPtr<UMeshOpPreviewWithBackgroundCompute>> Previews;

	UPROPERTY()
	TArray<TObjectPtr<UUVLayoutOperatorFactory>> Factories;

	TArray<TSharedPtr<UE::Geometry::FDynamicMesh3, ESPMode::ThreadSafe>> OriginalDynamicMeshes;

	FViewCameraState CameraState;

	void UpdateNumPreviews();

	void UpdateVisualization();
	
	void UpdatePreviewMaterial();

	void OnPreviewMeshUpdated(UMeshOpPreviewWithBackgroundCompute* Compute);

	void GenerateAsset(const TArray<FDynamicMeshOpResult>& Results);

	UPROPERTY()
	TObjectPtr<UUVLayoutPreview> UVLayoutView = nullptr;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseTools/SingleSelectionMeshEditingTool.h"
#include "PreviewMesh.h"
#include "ModelingOperators.h"
#include "MeshOpPreviewHelpers.h"
#include "PropertySets/PolygroupLayersProperties.h"
#include "ConvertToPolygonsTool.generated.h"

// predeclaration
class UConvertToPolygonsTool;
class FConvertToPolygonsOp;
class UPreviewGeometry;
PREDECLARE_GEOMETRY(class FDynamicMesh3);

/**
 *
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UConvertToPolygonsToolBuilder : public USingleSelectionMeshEditingToolBuilder
{
	GENERATED_BODY()
public:
	virtual USingleSelectionMeshEditingTool* CreateNewTool(const FToolBuilderState & SceneState) const override;
};




UENUM()
enum class EConvertToPolygonsMode
{
	/** Convert based on Angle Tolerance between Face Normals */
	FaceNormalDeviation UMETA(DisplayName = "Face Normal Deviation"),
	/** Create Polygroups by merging triangle pairs into Quads */
	FindPolygons UMETA(DisplayName = "Find Quads"),
	/** Create PolyGroups based on UV Islands */
	FromUVIslands  UMETA(DisplayName = "From UV Islands"),
	/** Create PolyGroups based on Hard Normal Seams */
	FromNormalSeams  UMETA(DisplayName = "From Hard Normal Seams"),
	/** Create Polygroups based on Connected Triangles */
	FromConnectedTris UMETA(DisplayName = "From Connected Tris"),
	/** Create Polygroups centered on well-spaced sample points, approximating a surface Voronoi diagram */
	FromFurthestPointSampling UMETA(DisplayName = "Furthest Point Sampling"),
	/** Copy from existing Polygroup Layer */
	CopyFromLayer UMETA(DisplayName = "Copy From Layer"),
};



UCLASS()
class MESHMODELINGTOOLSEXP_API UConvertToPolygonsToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	/** Strategy to use to group triangles */
	UPROPERTY(EditAnywhere, Category = PolyGroups)
	EConvertToPolygonsMode ConversionMode = EConvertToPolygonsMode::FaceNormalDeviation;

	/** Tolerance for planarity */
	UPROPERTY(EditAnywhere, Category = NormalDeviation, meta = (UIMin = "0.001", UIMax = "60.0", ClampMin = "0.0", ClampMax = "90.0", EditCondition = "ConversionMode == EConvertToPolygonsMode::FaceNormalDeviation", EditConditionHides))
	float AngleTolerance = 0.1f;

	/** Furthest-Point Sample count, approximately this number of polygroups will be generated */
	UPROPERTY(EditAnywhere, Category = FurthestPoint, meta = (UIMin = "1", UIMax = "100", ClampMin = "1", ClampMax = "10000", EditCondition = "ConversionMode == EConvertToPolygonsMode::FromFurthestPointSampling", EditConditionHides))
	int32 NumPoints = 100;

	/** If enabled, then furthest-point sampling happens with respect to existing Polygroups, ie the existing groups are further subdivided */
	UPROPERTY(EditAnywhere, Category = FurthestPoint, meta = (EditCondition = "ConversionMode == EConvertToPolygonsMode::FromFurthestPointSampling", EditConditionHides))
	bool bSplitExisting = false;

	/** If true, region-growing in Sampling modes will be controlled by face normals, resulting in regions with borders that are more-aligned with curvature ridges */
	UPROPERTY(EditAnywhere, Category = FurthestPoint, meta = (EditCondition = "ConversionMode == EConvertToPolygonsMode::FromFurthestPointSampling", EditConditionHides))
	bool bNormalWeighted = true;

	/** This parameter modulates the effect of normal weighting during region-growing */
	UPROPERTY(EditAnywhere, Category = FurthestPoint, meta = (UIMin = "0.1", UIMax = "2.0", ClampMin = "0.01", ClampMax = "100.0", EditCondition = "ConversionMode == EConvertToPolygonsMode::FromFurthestPointSampling", EditConditionHides))
	float NormalWeighting = 1.0f;


	/** Bias for Quads that are adjacent to already-discovered Quads. Set to 0 to disable.  */
	UPROPERTY(EditAnywhere, Category = FindQuads, meta = (UIMin = 0, UIMax = 5, EditCondition = "ConversionMode == EConvertToPolygonsMode::FindPolygons", EditConditionHides))
	float QuadAdjacencyWeight = 1.0;

	/** Set to values below 1 to ignore less-likely triangle pairings */
	UPROPERTY(EditAnywhere, Category = FindQuads, meta = (AdvancedDisplay, UIMin = 0, UIMax = 1, EditCondition = "ConversionMode == EConvertToPolygonsMode::FindPolygons", EditConditionHides))
	float QuadMetricClamp = 1.0;

	/** Iteratively repeat quad-searching in uncertain areas, to try to slightly improve results */
	UPROPERTY(EditAnywhere, Category = FindQuads, meta = (AdvancedDisplay, UIMin = 1, UIMax = 5, EditCondition = "ConversionMode == EConvertToPolygonsMode::FindPolygons", EditConditionHides))
	int QuadSearchRounds = 1;

	/** If true, polygroup borders will not cross existing UV seams */
	UPROPERTY(EditAnywhere, Category = Topology, meta = (EditCondition = "ConversionMode == EConvertToPolygonsMode::FaceNormalDeviation || ConversionMode == EConvertToPolygonsMode::FindPolygons", EditConditionHides))
	bool bRespectUVSeams = false;

	/** If true, polygroup borders will not cross existing hard normal seams */
	UPROPERTY(EditAnywhere, Category = Topology, meta = (EditCondition = "ConversionMode == EConvertToPolygonsMode::FaceNormalDeviation || ConversionMode == EConvertToPolygonsMode::FindPolygons", EditConditionHides))
	bool bRespectHardNormals = false;


	/** group filtering */
	UPROPERTY(EditAnywhere, Category = Filtering, meta = (UIMin = "1", UIMax = "100", ClampMin = "1", ClampMax = "10000", EditCondition = "ConversionMode != EConvertToPolygonsMode::CopyFromLayer"))
	int32 MinGroupSize = 2;


	/** If true, normals are recomputed per-group, with hard edges at group boundaries */
	UPROPERTY(EditAnywhere, Category = Output, meta=(EditCondition = "ConversionMode != EConvertToPolygonsMode::CopyFromLayer") )
	bool bCalculateNormals = false;
	
	/** Display each group with a different auto-generated color */
	UPROPERTY(EditAnywhere, Category = Display)
	bool bShowGroupColors = true;
};







UCLASS()
class MESHMODELINGTOOLSEXP_API UOutputPolygroupLayerProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:

	/** Select PolyGroup layer to use. */
	UPROPERTY(EditAnywhere, Category = "Output", meta = (DisplayName = "Output Layer", GetOptions = GetGroupOptionsList, NoResetToDefault))
	FName GroupLayer = "Default";

	// Provides set of available group layers
	UFUNCTION()
	TArray<FString> GetGroupOptionsList() { return OptionsList; }

	// internal list used to implement above
	UPROPERTY(meta = (TransientToolProperty))
	TArray<FString> OptionsList;

	UPROPERTY(meta = (TransientToolProperty))
	bool bShowNewLayerName = false;

	/** Name of the new Group Layer */
	UPROPERTY(EditAnywhere, Category = "Output", meta = (TransientToolProperty, DisplayName = "New Layer Name",
		EditCondition = "bShowNewLayerName", HideEditConditionToggle, NoResetToDefault))
	FString NewLayerName = TEXT("polygroups");
};




UCLASS()
class MESHMODELINGTOOLSEXP_API UConvertToPolygonsOperatorFactory : public UObject, public UE::Geometry::IDynamicMeshOperatorFactory
{
	GENERATED_BODY()

public:
	// IDynamicMeshOperatorFactory API
	virtual TUniquePtr<UE::Geometry::FDynamicMeshOperator> MakeNewOperator() override;

	UPROPERTY()
	TObjectPtr<UConvertToPolygonsTool> ConvertToPolygonsTool;  // back pointer
};

/**
 *
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UConvertToPolygonsTool : public USingleSelectionMeshEditingTool
{
	GENERATED_BODY()

public:
	UConvertToPolygonsTool();

	virtual void Setup() override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;

	virtual void OnTick(float DeltaTime) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override;

	// update parameters in ConvertToPolygonsOp based on current Settings
	void UpdateOpParameters(FConvertToPolygonsOp& ConvertToPolygonsOp) const;

protected:
	
	void OnSettingsModified();

protected:
	UPROPERTY()
	TObjectPtr<UConvertToPolygonsToolProperties> Settings;

	UPROPERTY()
	TObjectPtr<UPolygroupLayersProperties> CopyFromLayerProperties = nullptr;

	UPROPERTY()
	TObjectPtr<UOutputPolygroupLayerProperties> OutputProperties = nullptr;


	UPROPERTY()
	TObjectPtr<UMeshOpPreviewWithBackgroundCompute> PreviewCompute = nullptr;

	UPROPERTY()
	TObjectPtr<UPreviewGeometry> PreviewGeometry = nullptr;

protected:
	TSharedPtr<UE::Geometry::FDynamicMesh3, ESPMode::ThreadSafe> OriginalDynamicMesh;

	// for visualization
	TArray<int> PolygonEdges;
	
	void UpdateVisualization();


	TSharedPtr<UE::Geometry::FPolygroupSet, ESPMode::ThreadSafe> ActiveFromGroupSet;
	void OnSelectedFromGroupLayerChanged();
	void UpdateFromGroupLayer();

};

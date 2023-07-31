// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseTools/MultiSelectionMeshEditingTool.h"
#include "InteractiveToolBuilder.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "SphereTypes.h"
#include "OrientedBoxTypes.h"
#include "CapsuleTypes.h"
#include "Physics/CollisionPropertySets.h"
#include "Physics/PhysicsDataCollection.h"
#include "PropertySets/PolygroupLayersProperties.h"
#include "Polygroups/PolygroupSet.h"
#include "ModelingOperators.h"
#include "MeshOpPreviewHelpers.h"
#include "SetCollisionGeometryTool.generated.h"

class UPreviewGeometry;
PREDECLARE_GEOMETRY(class FMeshSimpleShapeApproximation)
PREDECLARE_USE_GEOMETRY_CLASS(FDynamicMesh3);

UCLASS()
class MESHMODELINGTOOLSEXP_API USetCollisionGeometryToolBuilder : public UMultiSelectionMeshEditingToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UMultiSelectionMeshEditingTool* CreateNewTool(const FToolBuilderState& SceneState) const override;

protected:
	virtual const FToolTargetTypeRequirements& GetTargetRequirements() const override;
};




UENUM()
enum class ESetCollisionGeometryInputMode
{
	CombineAll = 0,
	PerInputObject = 1,
	PerMeshComponent = 2,
	PerMeshGroup = 3
};


UENUM()
enum class ECollisionGeometryType
{
	KeepExisting = 0,
	AlignedBoxes = 1,
	OrientedBoxes = 2,
	MinimalSpheres = 3,
	Capsules = 4,
	ConvexHulls = 5,
	SweptHulls = 6,
	LevelSets = 7,
	MinVolume = 10,

	None = 11
};



UENUM()
enum class EProjectedHullAxis
{
	X = 0,
	Y = 1,
	Z = 2,
	SmallestBoxDimension = 3,
	SmallestVolume = 4
};


UCLASS()
class MESHMODELINGTOOLSEXP_API USetCollisionGeometryToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Category = Options)
	ECollisionGeometryType GeometryType = ECollisionGeometryType::AlignedBoxes;

	UPROPERTY(EditAnywhere, Category = Options)
	ESetCollisionGeometryInputMode InputMode = ESetCollisionGeometryInputMode::PerInputObject;

	UPROPERTY(EditAnywhere, Category = Options)
	bool bUseWorldSpace = false;

	UPROPERTY(EditAnywhere, Category = Options)
	bool bRemoveContained = true;

	UPROPERTY(EditAnywhere, Category = Options)
	bool bEnableMaxCount = true;

	UPROPERTY(EditAnywhere, Category = Options, meta = (UIMin = "1", UIMax = "100", ClampMin = "1", ClampMax = "9999999", EditCondition = "bEnableMaxCount"))
	int32 MaxCount = 50;

	UPROPERTY(EditAnywhere, Category = Options, AdvancedDisplay)
	float MinThickness = 0.01;

	UPROPERTY(EditAnywhere, Category = AutoDetect)
	bool bDetectBoxes = true;

	UPROPERTY(EditAnywhere, Category = AutoDetect)
	bool bDetectSpheres = true;

	UPROPERTY(EditAnywhere, Category = AutoDetect)
	bool bDetectCapsules = true;

	UPROPERTY(EditAnywhere, Category = ConvexHulls, meta = (EditConditionHides, EditCondition = "GeometryType == ECollisionGeometryType::ConvexHulls"))
	bool bSimplifyHulls = true;

	UPROPERTY(EditAnywhere, Category = ConvexHulls, meta = (UIMin = "4", UIMax = "100", ClampMin = "4", ClampMax = "9999999",
		EditConditionHides, EditCondition = "GeometryType == ECollisionGeometryType::ConvexHulls"))
	int32 HullTargetFaceCount = 20;

	/** How many convex hulls can be used to approximate each mesh */
	UPROPERTY(EditAnywhere, Category = ConvexHulls, meta = (UIMin = "1", UIMax = "100", ClampMin = "1",
		EditConditionHides, EditCondition = "GeometryType == ECollisionGeometryType::ConvexHulls"))
	int32 MaxHullsPerMesh = 1;

	/** How much to search the space of possible decompositions beyond MaxHullsPerMesh; for larger values, will do additional work to try to better approximate mesh features (but resulting hulls may overlap more) */
	UPROPERTY(EditAnywhere, Category = ConvexHulls, meta = (UIMin = "0", UIMax = "2", ClampMin = "0",
		EditConditionHides, EditCondition = "GeometryType == ECollisionGeometryType::ConvexHulls && MaxHullsPerMesh > 1"))
	float ConvexDecompositionSearchFactor = .5;

	/** Error tolerance for adding more convex hulls, in cm.  For volumetric errors, the value will be cubed (so a value of 10 indicates a 10x10x10 volume worth of error is acceptable). */
	UPROPERTY(EditAnywhere, Category = ConvexHulls, meta = (UIMin = "0", UIMax = "1000", ClampMin = "0",
		EditConditionHides, EditCondition = "GeometryType == ECollisionGeometryType::ConvexHulls && MaxHullsPerMesh > 1"))
	float AddHullsErrorTolerance = 0;

	/** Minimum part thickness for convex decomposition (in cm); hulls thinner than this will be merged into adjacent hulls, if possible. */
	UPROPERTY(EditAnywhere, Category = ConvexHulls, meta = (UIMin = "0", UIMax = "1", ClampMin = "0",
		EditConditionHides, EditCondition = "GeometryType == ECollisionGeometryType::ConvexHulls && MaxHullsPerMesh > 1"))
	float MinPartThickness = 0.1;

	UPROPERTY(EditAnywhere, Category = SweptHulls, meta = (EditConditionHides, EditCondition = "GeometryType == ECollisionGeometryType::SweptHulls"))
	bool bSimplifyPolygons = true;

	UPROPERTY(EditAnywhere, Category = SweptHulls, meta = (UIMin = "0", UIMax = "10", ClampMin = "0", ClampMax = "100000",
		EditConditionHides, EditCondition = "GeometryType == ECollisionGeometryType::SweptHulls"))
	float HullTolerance = 0.1;

	UPROPERTY(EditAnywhere, Category = SweptHulls, meta = (UIMin = "0", UIMax = "10", ClampMin = "0", ClampMax = "100000",
		EditConditionHides, EditCondition = "GeometryType == ECollisionGeometryType::SweptHulls"))
	EProjectedHullAxis SweepAxis = EProjectedHullAxis::SmallestVolume;

	// Level Set settings

	/** Level set grid resolution along longest grid axis */
	UPROPERTY(EditAnywhere, Category = LevelSets, meta = (UIMin = "3", UIMax = "100", ClampMin = "3", ClampMax = "1000",
		EditConditionHides, EditCondition = "GeometryType == ECollisionGeometryType::LevelSets"))
	int32 LevelSetResolution = 10;

	UPROPERTY(EditAnywhere, Category = OutputOptions)
	bool bAppendToExisting = false;

	UPROPERTY(EditAnywhere, Category = OutputOptions)
	ECollisionGeometryMode SetCollisionType = ECollisionGeometryMode::SimpleAndComplex;
};





/**
 * Mesh Inspector Tool for visualizing mesh information
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API USetCollisionGeometryTool : public UMultiSelectionMeshEditingTool, public UE::Geometry::IGenericDataOperatorFactory<FPhysicsDataCollection>
{
	GENERATED_BODY()
public:
	virtual void Setup() override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;

	virtual void OnTick(float DeltaTime) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override
	{
		// allow accept when we're showing the current, valid result
		return Super::CanAccept() && bInputMeshesValid && Compute && Compute->HaveValidResult() && !bVisualizationDirty;
	}

	// Begin IGenericDataOperatorFactory interface
	virtual TUniquePtr<UE::Geometry::TGenericDataOperator<FPhysicsDataCollection>> MakeNewOperator() override;
	// End IGenericDataOperatorFactory interface

protected:

	UPROPERTY()
	TObjectPtr<USetCollisionGeometryToolProperties> Settings = nullptr;

	UPROPERTY()
	TObjectPtr<UPolygroupLayersProperties> PolygroupLayerProperties = nullptr;

	UPROPERTY()
	TObjectPtr<UCollisionGeometryVisualizationProperties> VizSettings = nullptr;

	UPROPERTY()
	TObjectPtr<UPhysicsObjectToolPropertySet> CollisionProps;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> LineMaterial = nullptr;

	//
	// Background compute
	//
	TUniquePtr<TGenericDataBackgroundCompute<FPhysicsDataCollection>> Compute = nullptr;

protected:
	UPROPERTY()
	TObjectPtr<UPreviewGeometry> PreviewGeom;

	TArray<int32> SourceObjectIndices;
	bool bSourcesHidden = false;

	TArray<FDynamicMesh3> InitialSourceMeshes;

	void OnInputModeChanged();

	/**
	 * Invalidates the background compute operator.
	 */
	void InvalidateCompute();

	enum class EDetectedCollisionGeometry
	{
		None,
		Sphere = 2,
		Box = 4,
		Capsule = 8,
		Convex = 16
	};

	struct FSourceMesh
	{
		FDynamicMesh3 Mesh;

		EDetectedCollisionGeometry DetectedType = EDetectedCollisionGeometry::None;

		UE::Geometry::FSphere3d DetectedSphere;
		UE::Geometry::FOrientedBox3d DetectedBox;
		UE::Geometry::FCapsule3d DetectedCapsule;
	};
	
	bool bInputMeshesValid = false;
	TArray<TSharedPtr<FDynamicMesh3, ESPMode::ThreadSafe>> InputMeshes;
	TArray<TSharedPtr<FDynamicMesh3, ESPMode::ThreadSafe>> CombinedInputMeshes;
	TArray<TSharedPtr<FDynamicMesh3, ESPMode::ThreadSafe>> SeparatedInputMeshes;
	TArray<TSharedPtr<FDynamicMesh3, ESPMode::ThreadSafe>> PerGroupInputMeshes;

	TSharedPtr<UE::Geometry::FMeshSimpleShapeApproximation, ESPMode::ThreadSafe> InputMeshesApproximator;
	TSharedPtr<UE::Geometry::FMeshSimpleShapeApproximation, ESPMode::ThreadSafe> CombinedInputMeshesApproximator;
	TSharedPtr<UE::Geometry::FMeshSimpleShapeApproximation, ESPMode::ThreadSafe> SeparatedMeshesApproximator;
	TSharedPtr<UE::Geometry::FMeshSimpleShapeApproximation, ESPMode::ThreadSafe> PerGroupMeshesApproximator;

	void PrecomputeInputMeshes();
	void InitializeDerivedMeshSet(
		const TArray<TSharedPtr<FDynamicMesh3, ESPMode::ThreadSafe>>& FromInputMeshes,
		TArray<TSharedPtr<FDynamicMesh3, ESPMode::ThreadSafe>>& ToMeshes,
		TFunctionRef<bool(const FDynamicMesh3*, int32, int32)> TrisConnectedPredicate);

	TUniquePtr<UE::Geometry::FPolygroupSet> ActiveGroupSet;
	void OnSelectedGroupLayerChanged();
	void UpdateActiveGroupLayer();

	FTransform OrigTargetTransform;
	FVector TargetScale3D;

	TSharedPtr<FPhysicsDataCollection, ESPMode::ThreadSafe> InitialCollision;
	TSharedPtr<FPhysicsDataCollection, ESPMode::ThreadSafe> GeneratedCollision;


	bool bVisualizationDirty = false;
	void UpdateVisualization();
};

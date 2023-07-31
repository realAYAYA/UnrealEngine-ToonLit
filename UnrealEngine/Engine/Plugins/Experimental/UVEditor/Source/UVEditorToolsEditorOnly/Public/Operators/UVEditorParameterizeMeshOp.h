// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Util/ProgressCancel.h"
#include "ModelingOperators.h"
#include "Polygroups/PolygroupSet.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "InteractiveTool.h"

#include "UVEditorParameterizeMeshOp.generated.h"

UENUM()
enum class EUVEditorParameterizeMeshUVMethod
{
	// Keep values the same as UE::Geometry::EUVEditorParamOpBackend!

	/** Compute automatic UVs using the Patch Builder technique */
	PatchBuilder = 0,
	/** Compute automatic UVs using the UVAtlas technique */
	UVAtlas = 1,
	/** Compute automatic UVs using the XAtlas technique */
	XAtlas = 2,
};


UCLASS()
class UVEDITORTOOLSEDITORONLY_API UUVEditorParameterizeMeshToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** Automatic UV generation technique to use */
	UPROPERTY(EditAnywhere, Category = AutoUV)
	EUVEditorParameterizeMeshUVMethod Method = EUVEditorParameterizeMeshUVMethod::PatchBuilder;
};


/**
 * Settings for the UVAtlas Automatic UV Generation Method
 */
UCLASS()
class UVEDITORTOOLSEDITORONLY_API UUVEditorParameterizeMeshToolUVAtlasProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** Maximum amount of stretch, from none to unbounded. If zero stretch is specified, each triangle will likely be its own UV island. */
	UPROPERTY(EditAnywhere, Category = UVAtlas, meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float IslandStretch = 0.11f;

	/** Hint at number of UV islands. The default of 0 means it is determined automatically. */
	UPROPERTY(EditAnywhere, Category = UVAtlas, meta = (UIMin = "0", UIMax = "100", ClampMin = "0", ClampMax = "10000"))
	int NumIslands = 0;

	/** Expected resolution of the output textures; this controls spacing left between UV islands to avoid interpolation artifacts. */
	UPROPERTY(EditAnywhere, Category = UVAtlas, meta = (UIMin = "64", UIMax = "2048", ClampMin = "2", ClampMax = "4096"))
	int TextureResolution = 1024;

	/** Generate new UVs based on polygroups from specified layer. */
	UPROPERTY(EditAnywhere, Category = UVAtlas, meta = (DisplayName = "Constrain to Polygroups", EditCondition = bPolygroupsEnabled, EditConditionHides, HideEditConditionToggle))
	bool bUsePolygroups;

	/** Layout resulting islands on UDIMs based on polygroups. */
	UPROPERTY(EditAnywhere, Category = UVAtlas, meta = (EditCondition = "bUsePolygroups && bUDIMsEnabled", EditConditionHides, HideEditConditionToggle))
	bool bLayoutUDIMPerPolygroup;

	/** Controls if polygroup options are available to the user. */
	UPROPERTY()
	bool bPolygroupsEnabled;

	/** Controls if UDIM options are available to the user. */
	UPROPERTY()
	bool bUDIMsEnabled;
};


UCLASS()
class UVEDITORTOOLSEDITORONLY_API UUVEditorParameterizeMeshToolXAtlasProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** Number of solve iterations; higher values generally result in better UV islands. */
	UPROPERTY(EditAnywhere, Category = XAtlas, meta = (UIMin = "1", UIMax = "10", ClampMin = "1", ClampMax = "1000"))
	int MaxIterations = 1;
};


UCLASS()
class UVEDITORTOOLSEDITORONLY_API UUVEditorParameterizeMeshToolPatchBuilderProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** Number of initial patches the mesh will be split into before island merging. */
	UPROPERTY(EditAnywhere, Category = PatchBuilder, meta = (UIMin = "1", UIMax = "1000", ClampMin = "1", ClampMax = "99999999"))
	int InitialPatches = 100;

	/** Alignment of the initial patches to creases in the mesh.*/
	UPROPERTY(EditAnywhere, Category = PatchBuilder, meta = (UIMin = "0.1", UIMax = "2.0", ClampMin = "0.01", ClampMax = "100.0"))
	float CurvatureAlignment = 1.0f;

	/** Threshold for stretching and distortion below which island merging is allowed; larger values increase the allowable UV distortion. */
	UPROPERTY(EditAnywhere, Category = PatchBuilder, meta = (DisplayName = "Distortion Threshold", UIMin = "1.0", UIMax = "5.0", ClampMin = "1.0"))
	float MergingDistortionThreshold = 1.5f;

	/** Threshold for the average face normal deviation below which island merging is allowed. */
	UPROPERTY(EditAnywhere, Category = PatchBuilder, meta = (DisplayName = "Angle Threshold", UIMin = "0.0", UIMax = "90.0", ClampMin = "0.0", ClampMax = "180.0"))
	float MergingAngleThreshold = 45.0f;

	/** Number of smoothing steps to apply; this slightly increases distortion but produces more stable results. */
	UPROPERTY(EditAnywhere, Category = PatchBuilder, meta = (UIMin = "0", UIMax = "25", ClampMin = "0", ClampMax = "1000"))
	int SmoothingSteps = 5;

	/** Smoothing parameter; larger values result in faster smoothing in each step. */
	UPROPERTY(EditAnywhere, Category = PatchBuilder, meta = (UIMin = "0", UIMax = "1.0", ClampMin = "0", ClampMax = "1.0"))
	float SmoothingAlpha = 0.25f;

	/** Automatically pack result UVs into the unit square, i.e. fit between 0 and 1 with no overlap. */
	UPROPERTY(EditAnywhere, Category = PatchBuilder)
	bool bRepack = true;

	/** Expected resolution of the output textures; this controls spacing left between UV islands to avoid interpolation artifacts. This is only enabled when Repack is enabled. */
	UPROPERTY(EditAnywhere, Category = PatchBuilder, meta = (UIMin = "64", UIMax = "2048", ClampMin = "2", ClampMax = "4096", EditCondition = bRepack))
	int TextureResolution = 1024;

	/** Generate new UVs based on polygroups from specified layer. */
	UPROPERTY(EditAnywhere, Category = PatchBuilder, meta = (DisplayName = "Constrain to Polygroups", EditCondition = bPolygroupsEnabled, EditConditionHides, HideEditConditionToggle))
	bool bUsePolygroups;

	/** Layout resulting islands on UDIMs based on polygroups. */
	UPROPERTY(EditAnywhere, Category = PatchBuilder, meta = (EditCondition = "bUsePolygroups && bUDIMsEnabled", EditConditionHides, HideEditConditionToggle))
	bool bLayoutUDIMPerPolygroup;

	/** Controls if polygroup options are available to the user. */
	UPROPERTY()
	bool bPolygroupsEnabled;

	/** Controls if UDIM options are available to the user. */
	UPROPERTY()
	bool bUDIMsEnabled;
};


namespace UE
{
	namespace Geometry
	{

		enum class EUVEditorParamOpBackend
		{
			PatchBuilder = 0,
			UVAtlas = 1,
			XAtlas = 2
		};

		class UVEDITORTOOLSEDITORONLY_API FUVEditorParameterizeMeshOp : public FDynamicMeshOperator
		{
		public:
			virtual ~FUVEditorParameterizeMeshOp() {}

			//
			// Inputs
			// 

			// source mesh
			TSharedPtr<FDynamicMesh3, ESPMode::ThreadSafe> InputMesh;

			// source groups (optional)
			TSharedPtr<UE::Geometry::FPolygroupSet, ESPMode::ThreadSafe> InputGroups;

			// UVAtlas generation parameters
			float Stretch = 0.11f;
			int32 NumCharts = 0;

			// XAtlas generation parameters
			int32 XAtlasMaxIterations = 1;

			// PatchBuilder generation parameters
			int32 InitialPatchCount = 100;
			bool bRespectInputGroups = false;
			float PatchCurvatureAlignmentWeight = 1.0;
			float PatchMergingMetricThresh = 1.5;
			float PatchMergingAngleThresh = 45.0;
			int ExpMapNormalSmoothingSteps = 5;
			float ExpMapNormalSmoothingAlpha = 0.25;

			// UV layer
			int32 UVLayer = 0;

			// UDIM support
			bool bPackToUDIMSByOriginPolygroup = false;

			// Atlas Packing parameters
			bool bEnablePacking = true;
			int32 Height = 512;
			int32 Width = 512;
			float Gutter = 2.5;

			EUVEditorParamOpBackend Method = EUVEditorParamOpBackend::UVAtlas;

			// set ability on protected transform.
			void SetTransform(const FTransformSRT3d& XForm)
			{
				ResultTransform = XForm;
			}

			//
			// FDynamicMeshOperator implementation
			// 

			virtual void CalculateResult(FProgressCancel* Progress) override;


		protected:

			// dense index/vertex buffer based representation of the data needed for parameterization
			struct FLinearMesh
			{
				FLinearMesh(const FDynamicMesh3& Mesh, UE::Geometry::FPolygroupSet* InputGroups);

				// Stripped down mesh
				TArray<int32>   IndexBuffer;
				TArray<FVector3f> VertexBuffer;

				// Map from offset in the VertexBuffer to the VertexID in the FDynamicMesh
				TArray<int32> VertToID;

				// Adjacency[tri], Adjacency[tri+1], Adjacency[tri+2] 
				// are the three tris adjacent to tri.
				TArray<int32>   AdjacencyBuffer;

			};

			FGeometryResult NewResultInfo;

			bool ComputeUVs_UVAtlas(FDynamicMesh3& InOutMesh, TFunction<bool(float)>& Interrupter);
			bool ComputeUVs_XAtlas(FDynamicMesh3& InOutMesh, TFunction<bool(float)>& Interrupter);
			bool ComputeUVs_PatchBuilder(FDynamicMesh3& InOutMesh, FProgressCancel* Progress);

			void CopyNewUVsToMesh(
				FDynamicMesh3& Mesh,
				const FLinearMesh& LinearMesh,
				const FDynamicMesh3& FlippedMesh,
				const TArray<FVector2D>& UVVertexBuffer,
				const TArray<int32>& UVIndexBuffer,
				const TArray<int32>& VertexRemapArray,
				bool bReverseOrientation);

			void LayoutToUDIMByPolygroup(FDynamicMesh3& InOutMesh, UE::Geometry::FPolygroupSet& InputGroups);
		};

	} // end namespace UE::Geometry
} // end namespace UE

/**
 * Can be hooked up to a UMeshOpPreviewWithBackgroundCompute to perform UV parameterization operations.
 *
 * Inherits from UObject so that it can hold a strong pointer to the settings UObject, which
 * needs to be a UObject to be displayed in the details panel.
 */
UCLASS()
class UVEDITORTOOLSEDITORONLY_API UUVEditorParameterizeMeshOperatorFactory : public UObject, public UE::Geometry::IDynamicMeshOperatorFactory
{
	GENERATED_BODY()

public:
	// IDynamicMeshOperatorFactory API
	virtual TUniquePtr<UE::Geometry::FDynamicMeshOperator> MakeNewOperator() override;

	UPROPERTY()
	TObjectPtr<UUVEditorParameterizeMeshToolProperties> Settings = nullptr;

	UPROPERTY()
	TObjectPtr<UUVEditorParameterizeMeshToolUVAtlasProperties> UVAtlasProperties = nullptr;

	UPROPERTY()
	TObjectPtr<UUVEditorParameterizeMeshToolXAtlasProperties> XAtlasProperties = nullptr;

	UPROPERTY()
	TObjectPtr<UUVEditorParameterizeMeshToolPatchBuilderProperties> PatchBuilderProperties = nullptr;

	TSharedPtr<UE::Geometry::FPolygroupSet, ESPMode::ThreadSafe> InputGroups;
	TSharedPtr<UE::Geometry::FDynamicMesh3, ESPMode::ThreadSafe> OriginalMesh;
	TUniqueFunction<int32()> GetSelectedUVChannel = []() { return 0; };
	FTransform TargetTransform;
};
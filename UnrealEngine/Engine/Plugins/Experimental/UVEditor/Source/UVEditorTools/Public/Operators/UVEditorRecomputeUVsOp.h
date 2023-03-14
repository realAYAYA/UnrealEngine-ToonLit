// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Util/ProgressCancel.h"
#include "ModelingOperators.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Polygroups/PolygroupSet.h"
#include "InteractiveTool.h"

#include "UVEditorRecomputeUVsOp.generated.h"

class URecomputeUVsToolProperties;

/*
 * TODO: This code is mostly copy and pasted from the original RecomputeUVsOp, for the
 * purposes of exploring UDIM workflows in the UV Editor. Once a decision has been made
 * regarding the path we want UDIMs to take, this can be merged back into the original
 * Modeling Mode operator. 
 */

UENUM()
enum class EUVEditorRecomputeUVsPropertiesUnwrapType
{
	// Values must match UE::Geometry::ERecomputeUVsUnwrapType

	/** ExpMap UV flattening is very fast but has limited abilities to reduce stretching and distortion */
	ExpMap = 0 UMETA(DisplayName = "ExpMap"),
	/** Conformal UV flattening is increasingly expensive on large islands but reduces distortion */
	Conformal = 1,
	/** Compared to the default Conformal method does not pin two vertices along the boundary which reduces the distortion but is more expensive to compute. */
	SpectralConformal = 2,
	/** UV islands will be merged into larger islands if it does not increase stretching and distortion beyond defined limits */
	IslandMerging = 3
};


UENUM()
enum class EUVEditorRecomputeUVsPropertiesIslandMode
{
	// Values must match UE::Geometry::ERecomputeUVsIslandMode

	/** Use Polygroups defined by Active PolyGroup property to define initial UV islands. */
	PolyGroups = 0 UMETA(DisplayName = "PolyGroups"),
	/** Use existing UV Layer to define UV islands, i.e. re-solve UV flattening based on existing UVs */
	ExistingUVs = 1
};


UENUM()
enum class EUVEditorRecomputeUVsToolOrientationMode
{
	/** Do not rotate UV islands */
	None,
	/** Automatically rotate each UV island to reduce its bounding box size */
	MinBounds
};


UENUM()
enum class EUVEditorRecomputeUVsPropertiesLayoutType
{
	/** Do not apply additional layout options to unwrapped UVs */
	None,
	/** Uniformly scale and translate UV islands collectively to pack them into the unit square, i.e. fit between 0 and 1 with no overlap */
	Repack,
	/** Scale and center all islands to fit within their original bounding boxes. Only applicable if using existing UVs. */
	NormalizeToExistingBounds,
	/** Uniformly scale UV islands such that they have constant relative area, relative to object bounds */
	NormalizeToBounds,
	/** Uniformly scale UV islands such that they have constant relative area, relative to world space */
	NormalizeToWorld
};


UCLASS()
class UVEDITORTOOLS_API  UUVEditorRecomputeUVsToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:

	UPROPERTY(meta = (TransientToolProperty))
	bool bEnablePolygroupSupport = true;

	/** Generation method for initial UV islands.*/
	UPROPERTY(EditAnywhere, Category = "UV Unwrap", meta = (EditCondition = "bEnablePolygroupSupport", EditConditionHides, HideEditConditionToggle = true))
	EUVEditorRecomputeUVsPropertiesIslandMode IslandGeneration = EUVEditorRecomputeUVsPropertiesIslandMode::ExistingUVs;

	/** Type of UV flattening algorithm to use */
	UPROPERTY(EditAnywhere, Category = "UV Unwrap")
	EUVEditorRecomputeUVsPropertiesUnwrapType UnwrapType = EUVEditorRecomputeUVsPropertiesUnwrapType::SpectralConformal;

	/** Type of automatic rotation applied to each UV island */
	UPROPERTY(EditAnywhere, Category = "UV Unwrap")
	EUVEditorRecomputeUVsToolOrientationMode AutoRotation = EUVEditorRecomputeUVsToolOrientationMode::MinBounds;

	/**  If enabled, reduces distortion for meshes with triangles of vastly different sizes, This is only enabled if the Unwrap Type is set to Spectral Conformal. */
	UPROPERTY(EditAnywhere, Category = "UV Unwrap", meta = (EditCondition = "UnwrapType == EUVEditorRecomputeUVsPropertiesUnwrapType::SpectralConformal", EditConditionHides, HideEditConditionToggle))
	bool bPreserveIrregularity = true;

	/** Number of smoothing steps to apply; this slightly increases distortion but produces more stable results. This is only enabled if the Unwrap Type is set to ExpMap or Island Merging. */
	UPROPERTY(EditAnywhere, Category = "UV Unwrap", meta = (UIMin = "0", UIMax = "25", ClampMin = "0", ClampMax = "1000",
		EditCondition = "UnwrapType == EUVEditorRecomputeUVsPropertiesUnwrapType::ExpMap || UnwrapType == EUVEditorRecomputeUVsPropertiesUnwrapType::IslandMerging", EditConditionHides, HideEditConditionToggle))
	int SmoothingSteps = 5;

	/** Smoothing parameter; larger values result in faster smoothing in each step. This is only enabled if the Unwrap Type is set to ExpMap or Island Merging. */
	UPROPERTY(EditAnywhere, Category = "UV Unwrap", meta = (UIMin = "0", UIMax = "1.0", ClampMin = "0", ClampMax = "1.0",
		EditCondition = "UnwrapType == EUVEditorRecomputeUVsPropertiesUnwrapType::ExpMap || UnwrapType == EUVEditorRecomputeUVsPropertiesUnwrapType::IslandMerging", EditConditionHides, HideEditConditionToggle))
	float SmoothingAlpha = 0.25f;

	/** Threshold for stretching and distortion below which island merging is allowed; larger values increase the allowable UV distortion. This is only enabled if the Unwrap Type is set to Island Merging. */
	UPROPERTY(EditAnywhere, Category = "UV Unwrap", meta = (DisplayName = "Distortion Threshold", UIMin = "1.0", UIMax = "5.0", ClampMin = "1.0",
		EditCondition = "UnwrapType == EUVEditorRecomputeUVsPropertiesUnwrapType::IslandMerging", EditConditionHides, HideEditConditionToggle))
	float MergingDistortionThreshold = 1.5f;

	/** Threshold for the average face normal deviation below  which island merging is allowed. This is only enabled if the Unwrap Type is set to Island Merging. */
	UPROPERTY(EditAnywhere, Category = "UV Unwrap", meta = (DisplayName = "Angle Threshold", UIMin = "0.0", UIMax = "90.0", ClampMin = "0.0", ClampMax = "180.0",
		EditCondition = "UnwrapType == EUVEditorRecomputeUVsPropertiesUnwrapType::IslandMerging", EditConditionHides, HideEditConditionToggle))
	float MergingAngleThreshold = 45.0f;

	/** Uniformly scale and translate UV islands collectively to pack them into the unit square, i.e. fit between 0 and 1 with no overlap. */
	UPROPERTY(EditAnywhere, Category = "UV Layout")
	EUVEditorRecomputeUVsPropertiesLayoutType LayoutType = EUVEditorRecomputeUVsPropertiesLayoutType::Repack;

	/** Expected resolution of the output textures; this controls spacing left between UV islands to avoid interpolation artifacts. This is only enabled when the Layout Type is set to Repack. */
	UPROPERTY(EditAnywhere, Category = "UV Layout", meta = (UIMin = "64", UIMax = "2048", ClampMin = "2", ClampMax = "4096",
		EditCondition = "LayoutType == EUVEditorRecomputeUVsPropertiesLayoutType::Repack", EditConditionHides, HideEditConditionToggle))
	int TextureResolution = 1024;

	/** Scaling factor used for UV island normalization/scaling. This is only enabled when the Layout Type is set to Normalize to Bounds or Normalize to World. */
	UPROPERTY(EditAnywhere, Category = "UV Layout", meta = (UIMin = "0.001", UIMax = "10", ClampMin = "0.00001", ClampMax = "1000000.0",
		EditCondition = "LayoutType == EUVEditorRecomputeUVsPropertiesLayoutType::NormalizeToBounds || LayoutType == EUVEditorRecomputeUVsPropertiesLayoutType::NormalizeToWorld", EditConditionHides, HideEditConditionToggle))
	float NormalizeScale = 1.0f;

	/** Enable UDIM aware layout and keep islands within their originating UDIM tiles when laying out.*/
	UPROPERTY(EditAnywhere, Category = "UV Layout", meta = (DisplayName = "Preserve UDIMs", EditCondition = "bUDIMCVAREnabled", EditConditionHides, HideEditConditionToggle = true))
	bool bEnableUDIMLayout = false;

	UPROPERTY(Transient)
	bool bUDIMCVAREnabled = false;
};


namespace UE
{
namespace Geometry
{

	struct TileConnectedComponents;

enum class EUVEditorRecomputeUVsUnwrapType
{
	ExpMap = 0,
	ConformalFreeBoundary = 1,
	SpectralConformal = 2
};



enum class EUVEditorRecomputeUVsIslandMode
{
	PolyGroups = 0,
	UVIslands = 1
};


class UVEDITORTOOLS_API FUVEditorRecomputeUVsOp : public FDynamicMeshOperator
{
public:
	virtual ~FUVEditorRecomputeUVsOp() {}

	//
	// Inputs
	// 

	// source mesh
	TSharedPtr<FDynamicMesh3, ESPMode::ThreadSafe> InputMesh;
	
	// source groups (optional)
	TSharedPtr<UE::Geometry::FPolygroupSet, ESPMode::ThreadSafe> InputGroups;

	// orientation control
	bool bAutoRotate = true;

	// Individual packing
	bool bPackToOriginalBounds = false;

	// area scaling
	bool bNormalizeAreas = true;
	float AreaScaling = 1.0;

	// UV layer
	int32 UVLayer = 0;

	// Atlas Packing parameters
	bool bPackUVs = true;	
	int32 PackingTextureResolution = 512;
	float PackingGutterWidth = 1.0f;

	EUVEditorRecomputeUVsUnwrapType UnwrapType = EUVEditorRecomputeUVsUnwrapType::ExpMap;
	EUVEditorRecomputeUVsIslandMode IslandMode = EUVEditorRecomputeUVsIslandMode::PolyGroups;

	//
	// Spectral Conformal Map Options 
	//
	bool bPreserveIrregularity = false;

	// 
	// ExpMap Options
	//
	int32 NormalSmoothingRounds = 0;
	double NormalSmoothingAlpha = 0.25;

	//
	// Patch Merging options
	//
	bool bMergingOptimization = false;
	double MergingThreshold = 1.5;
	double CompactnessThreshold = 9999999.0;		// effectively disabled as it usually is not a good idea
	double MaxNormalDeviationDeg = 45.0;

	// UDIM options	
	bool bUDIMsEnabled = false;

	// Selection
	TOptional<TSet<int32>> Selection;

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

	/*
	* Check if operation is valid to do work. It may not be valid if there are conflicting or inconsistent options passed to it from the user.
	*/
	bool IsValid() const;

	FGeometryResult NewResultInfo;

	void NormalizeUVAreas(const FDynamicMesh3& Mesh, FDynamicMeshUVOverlay* Overlay, const TileConnectedComponents& TileComponents, float GlobalScale = 1.0f);

	void CollectIslandComponentsPerTile(const FDynamicMeshUVOverlay& UVOverlay, TArray< TileConnectedComponents >& ComponentsPerTile, bool& UseExistingUVTopology);

	virtual bool CalculateResult_Basic(FProgressCancel* Progress);
	virtual bool CalculateResult_RegionOptimization(FProgressCancel* Progress);
};

} // end namespace UE::Geometry
} // end namespace UE


/**
 * Can be hooked up to a UMeshOpPreviewWithBackgroundCompute to perform UV solving operations.
 * 
 * Inherits from UObject so that it can hold a strong pointer to the settings UObject, which
 * needs to be a UObject to be displayed in the details panel.
 */
UCLASS()
class UVEDITORTOOLS_API UUVEditorRecomputeUVsOpFactory : public UObject, public UE::Geometry::IDynamicMeshOperatorFactory
{
	GENERATED_BODY()

public:

	// IDynamicMeshOperatorFactory API
	virtual TUniquePtr<UE::Geometry::FDynamicMeshOperator> MakeNewOperator() override;

	UPROPERTY()
	TObjectPtr<UUVEditorRecomputeUVsToolProperties> Settings = nullptr;

	TOptional<TSet<int32>> Selection;

	TSharedPtr<UE::Geometry::FPolygroupSet, ESPMode::ThreadSafe> InputGroups;
	TSharedPtr<UE::Geometry::FDynamicMesh3, ESPMode::ThreadSafe> OriginalMesh;
	TUniqueFunction<int32()> GetSelectedUVChannel = []() { return 0; };
	UE::Geometry::FTransformSRT3d TargetTransform;
};
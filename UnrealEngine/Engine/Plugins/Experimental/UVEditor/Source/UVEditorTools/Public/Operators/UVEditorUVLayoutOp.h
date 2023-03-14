// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Util/ProgressCancel.h"
#include "ModelingOperators.h"
#include "InteractiveTool.h"

#include "UVEditorUVLayoutOp.generated.h"

class UUVLayoutProperties;

/**
 * UV Layout Strategies for the UV Layout Tool
 */
UENUM()
enum class EUVEditorUVLayoutType
{
	/** Apply Scale and Translation properties to all UV values */
	Transform,
	/** Uniformly scale and translate each UV island individually to pack it into the unit square, i.e. fit between 0 and 1 with overlap */
	Stack,
	/** Uniformly scale and translate UV islands collectively to pack them into the unit square, i.e. fit between 0 and 1 with no overlap */
	Repack
};


/**
 * UV Layout Settings
 */
UCLASS()
class UVEDITORTOOLS_API UUVEditorUVLayoutProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	/** Type of layout applied to input UVs */
	UPROPERTY(EditAnywhere, Category = "UV Layout")
	EUVEditorUVLayoutType LayoutType = EUVEditorUVLayoutType::Repack;

	/** Expected resolution of the output textures; this controls spacing left between UV islands to avoid interpolation artifacts */
	UPROPERTY(EditAnywhere, Category = "UV Layout", meta = (UIMin = "64", UIMax = "2048", ClampMin = "2", ClampMax = "4096"))
	int TextureResolution = 1024;

	/** Uniform scale applied to UVs after packing */
	UPROPERTY(EditAnywhere, Category = "UV Layout", meta = (UIMin = "0.1", UIMax = "5.0", ClampMin = "0.0001", ClampMax = "10000"))
	float Scale = 1;

	/** Translation applied to UVs after packing, and after scaling */
	UPROPERTY(EditAnywhere, Category = "UV Layout")
	FVector2D Translation = FVector2D(0, 0);

	/** Allow the Repack layout type to flip the orientation of UV islands to save space. Note that this may cause problems for downstream operations, and therefore is disabled by default. */
	UPROPERTY(EditAnywhere, Category = "UV Layout", meta = (EditCondition = "LayoutType == EUVEditorUVLayoutType::Repack"))
	bool bAllowFlips = false;

	/** Enable UDIM aware layout and keep islands within their originating UDIM tiles when laying out.*/
	UPROPERTY(EditAnywhere, Category = "UV Layout", meta = (DisplayName = "Preserve UDIMs", EditCondition="bUDIMCVAREnabled", EditConditionHides, HideEditConditionToggle = true))
	bool bEnableUDIMLayout = false;

	UPROPERTY(Transient)
	bool bUDIMCVAREnabled = false;
};


namespace UE
{
namespace Geometry
{
	class FDynamicMesh3;
	class FDynamicMeshUVPacker;

enum class EUVEditorUVLayoutOpLayoutModes
{
	TransformOnly = 0,
	RepackToUnitRect = 1,
	StackInUnitRect = 2
};

/**
 * This is an intentional duplication of the original FUVLayoutOp class from GeometryTools as we explore UDIMs.
 * It may be temporary (with changes folded into the original) or may evolve into a UVEditor specific layout operation.
 */
class UVEDITORTOOLS_API FUVEditorUVLayoutOp : public FDynamicMeshOperator
{
public:
	virtual ~FUVEditorUVLayoutOp() {}

	// inputs
	TSharedPtr<FDynamicMesh3, ESPMode::ThreadSafe> OriginalMesh;

	EUVEditorUVLayoutOpLayoutModes UVLayoutMode = EUVEditorUVLayoutOpLayoutModes::RepackToUnitRect;

	int UVLayerIndex = 0;
	int TextureResolution = 128;
	bool bAllowFlips = false;
	bool bAlwaysSplitBowties = true;
	float UVScaleFactor = 1.0;
	float GutterSize = 1.0;
	bool bMaintainOriginatingUDIM = false;
	TOptional<TSet<int32>> Selection;
	FVector2f UVTranslation = FVector2f::Zero();

	void SetTransform(const FTransformSRT3d& Transform);

	//
	// FDynamicMeshOperator implementation
	// 

	virtual void CalculateResult(FProgressCancel* Progress) override;

protected:
	void ExecutePacker(FDynamicMeshUVPacker& Packer);
};

} // end namespace UE::Geometry
} // end namespace UE

/**
 * Can be hooked up to a UMeshOpPreviewWithBackgroundCompute to perform UV Layout operations.
 *
 * Inherits from UObject so that it can hold a strong pointer to the settings UObject, which
 * needs to be a UObject to be displayed in the details panel.
 */
UCLASS()
class UVEDITORTOOLS_API UUVEditorUVLayoutOperatorFactory : public UObject, public UE::Geometry::IDynamicMeshOperatorFactory
{
	GENERATED_BODY()

public:
	// IDynamicMeshOperatorFactory API
	virtual TUniquePtr<UE::Geometry::FDynamicMeshOperator> MakeNewOperator() override;

	UPROPERTY()
	TObjectPtr<UUVEditorUVLayoutProperties> Settings;

	TOptional<TSet<int32>> Selection;

	TSharedPtr<UE::Geometry::FDynamicMesh3, ESPMode::ThreadSafe> OriginalMesh;
	TUniqueFunction<int32()> GetSelectedUVChannel = []() { return 0; };
	FTransform TargetTransform;
};